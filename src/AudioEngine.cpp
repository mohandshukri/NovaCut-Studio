#include "AudioEngine.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <wrl/client.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

using Microsoft::WRL::ComPtr;
#endif

namespace cc {

namespace {

constexpr int kSampleRate = 48000;
constexpr int kChannels   = 2;
constexpr int kWaveformBins = 160;
constexpr size_t kMaxSamplesPerFile = static_cast<size_t>(kSampleRate) * kChannels * 300;

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), len);
    return w;
}
#endif

bool writeWavFile16Stereo(const std::string& pathUtf8, const std::vector<int16_t>& pcm, int sampleRate) {
    if (pcm.empty()) return false;
    std::filesystem::path fp = std::filesystem::u8path(pathUtf8);
    std::ofstream ofs(fp, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;

    uint32_t dataBytes = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
    uint32_t riffSize  = 36 + dataBytes;
    uint16_t audioFmt  = 1;
    uint16_t numCh     = 2;
    uint32_t sr        = static_cast<uint32_t>(sampleRate);
    uint32_t byteRate  = sr * numCh * 2;
    uint16_t blockAln  = numCh * 2;
    uint16_t bitsPS    = 16;
    uint32_t fmtSize   = 16;

    ofs.write("RIFF", 4);
    ofs.write(reinterpret_cast<const char*>(&riffSize), 4);
    ofs.write("WAVEfmt ", 8);
    ofs.write(reinterpret_cast<const char*>(&fmtSize), 4);
    ofs.write(reinterpret_cast<const char*>(&audioFmt), 2);
    ofs.write(reinterpret_cast<const char*>(&numCh), 2);
    ofs.write(reinterpret_cast<const char*>(&sr), 4);
    ofs.write(reinterpret_cast<const char*>(&byteRate), 4);
    ofs.write(reinterpret_cast<const char*>(&blockAln), 2);
    ofs.write(reinterpret_cast<const char*>(&bitsPS), 2);
    ofs.write("data", 4);
    ofs.write(reinterpret_cast<const char*>(&dataBytes), 4);
    ofs.write(reinterpret_cast<const char*>(pcm.data()), dataBytes);
    return ofs.good();
}

} // namespace

struct AudioTrackData {
    std::vector<int16_t> pcm;
    std::vector<float> peaks;
};

struct AudioEngine::Impl {
    bool initialized = false;
    mutable std::mutex cacheMutex;
    std::unordered_map<std::string, AudioTrackData> tracks;
    std::unordered_set<std::string> loadingPaths;

    mutable std::mutex specMutex;
    std::array<float, 16> spectrumBands{};
    std::atomic<bool> playing{false};
    std::atomic<bool> stopRequested{false};
    std::mutex projectMutex;
    Project activeProject;
    double playStartSec = 0.0;
    std::thread playbackThread;

    // V6: Live Microphone Recorder state
    std::atomic<bool> recording{false};
    std::atomic<bool> recStop{false};
    std::mutex recMutex;
    std::vector<int16_t> recBuffer;
    std::thread recThread;

#ifdef _WIN32
    bool decodeAudioFile(const std::string& pathUtf8, AudioTrackData& outData) {
        HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool didCoInit = SUCCEEDED(hrCo);

        bool ok = false;
        {
            std::wstring wPath = utf8ToWide(pathUtf8);
            ComPtr<IMFSourceReader> reader;
            if (SUCCEEDED(MFCreateSourceReaderFromURL(wPath.c_str(), nullptr, &reader))) {
                reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
                if (SUCCEEDED(reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), TRUE))) {
                    ComPtr<IMFMediaType> pcmType;
                    if (SUCCEEDED(MFCreateMediaType(&pcmType))) {
                        pcmType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                        pcmType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
                        pcmType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, kChannels);
                        pcmType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, kSampleRate);
                        pcmType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
                        pcmType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, kChannels * 2);
                        pcmType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, kSampleRate * kChannels * 2);

                        if (SUCCEEDED(reader->SetCurrentMediaType(
                                static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, pcmType.Get()))) {
                            outData.pcm.reserve(kSampleRate * kChannels * 10);
                            while (outData.pcm.size() < kMaxSamplesPerFile) {
                                DWORD streamIdx = 0, flags = 0;
                                LONGLONG ts = 0;
                                ComPtr<IMFSample> sample;
                                HRESULT hr = reader->ReadSample(
                                    static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
                                    0, &streamIdx, &flags, &ts, &sample);
                                if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;
                                if (!sample) continue;

                                ComPtr<IMFMediaBuffer> buf;
                                if (FAILED(sample->ConvertToContiguousBuffer(&buf))) continue;
                                BYTE* data = nullptr;
                                DWORD maxLen = 0, curLen = 0;
                                if (SUCCEEDED(buf->Lock(&data, &maxLen, &curLen)) && curLen >= 2) {
                                    size_t numSamples = std::min(static_cast<size_t>(curLen / sizeof(int16_t)),
                                                                 kMaxSamplesPerFile - outData.pcm.size());
                                    size_t oldSz = outData.pcm.size();
                                    outData.pcm.resize(oldSz + numSamples);
                                    std::memcpy(outData.pcm.data() + oldSz, data, numSamples * sizeof(int16_t));
                                    buf->Unlock();
                                }
                            }
                            ok = !outData.pcm.empty();
                        }
                    }
                }
            }
        }

        if (ok) {
            outData.peaks.assign(kWaveformBins, 0.0f);
            size_t totalFrames = outData.pcm.size() / kChannels;
            size_t framesPerBin = std::max<size_t>(1, totalFrames / kWaveformBins);
            float maxPeak = 0.01f;
            for (int b = 0; b < kWaveformBins; ++b) {
                size_t startF = static_cast<size_t>(b) * framesPerBin;
                size_t endF = std::min(totalFrames, startF + framesPerBin);
                float binPeak = 0.0f;
                for (size_t f = startF; f < endF; f += 4) {
                    float l = std::abs(static_cast<float>(outData.pcm[f * 2])) / 32768.0f;
                    float r = std::abs(static_cast<float>(outData.pcm[f * 2 + 1])) / 32768.0f;
                    binPeak = std::max(binPeak, std::max(l, r));
                }
                outData.peaks[b] = binPeak;
                maxPeak = std::max(maxPeak, binPeak);
            }
            for (float& p : outData.peaks) p = std::clamp(p / maxPeak, 0.05f, 1.0f);
        }

        if (didCoInit) CoUninitialize();
        return ok;
    }

    void playbackLoop() {
        WAVEFORMATEX wfx{};
        wfx.wFormatTag      = WAVE_FORMAT_PCM;
        wfx.nChannels       = kChannels;
        wfx.nSamplesPerSec  = kSampleRate;
        wfx.wBitsPerSample  = 16;
        wfx.nBlockAlign     = kChannels * 2;
        wfx.nAvgBytesPerSec = kSampleRate * wfx.nBlockAlign;

        HWAVEOUT hWaveOut = nullptr;
        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            playing.store(false);
            return;
        }

        constexpr int kNumBufs = 4;
        constexpr int kChunkFrames = kSampleRate / 25; // 40ms
        constexpr int kChunkSamples = kChunkFrames * kChannels;

        std::vector<int16_t> bufs[kNumBufs];
        WAVEHDR hdrs[kNumBufs]{};
        for (int i = 0; i < kNumBufs; ++i) {
            bufs[i].assign(kChunkSamples, 0);
            hdrs[i].lpData = reinterpret_cast<LPSTR>(bufs[i].data());
            hdrs[i].dwBufferLength = static_cast<DWORD>(kChunkSamples * sizeof(int16_t));
            waveOutPrepareHeader(hWaveOut, &hdrs[i], sizeof(WAVEHDR));
        }

        // Real-time DSP state (lowpass for bass/treble + 240ms echo delay buffer)
        float lpL = 0.0f, lpR = 0.0f;
        constexpr size_t kEchoDelay = kSampleRate / 4; // 250ms
        constexpr size_t kRevDelay1 = 1557, kRevDelay2 = 2251, kRevDelay3 = 3109;
        std::vector<float> rev1L(kRevDelay1, 0.0f), rev1R(kRevDelay1, 0.0f);
        std::vector<float> rev2L(kRevDelay2, 0.0f), rev2R(kRevDelay2, 0.0f);
        std::vector<float> rev3L(kRevDelay3, 0.0f), rev3R(kRevDelay3, 0.0f);
        size_t revP1 = 0, revP2 = 0, revP3 = 0;
        // V32: Dedicated 2nd-Order Biquad 6.5kHz Bandpass De-Esser DSP state
        float deZ1L = 0.0f, deZ2L = 0.0f, deZ1R = 0.0f, deZ2R = 0.0f, deEnv = 0.0f;
        constexpr float kDeW0 = 2.0f * 3.14159265f * 6500.0f / 48000.0f;
        const float kDeSin = std::sin(kDeW0), kDeCos = std::cos(kDeW0);
        const float kDeAlpha = kDeSin / (2.0f * 2.2f);
        const float kDeA0 = 1.0f + kDeAlpha;
        const float kDeB0 = kDeAlpha / kDeA0, kDeB2 = -kDeAlpha / kDeA0;
        const float kDeA1 = (-2.0f * kDeCos) / kDeA0, kDeA2 = (1.0f - kDeAlpha) / kDeA0;
        std::vector<float> echoL(kEchoDelay, 0.0f), echoR(kEchoDelay, 0.0f);
        size_t echoPos = 0;

        double currentTime = playStartSec;
        int bufIdx = 0;

        while (!stopRequested.load()) {
            WAVEHDR& hdr = hdrs[bufIdx];
            while ((hdr.dwFlags & WHDR_INQUEUE) && !stopRequested.load()) Sleep(6);
            if (stopRequested.load()) break;

            Project projCopy;
            {
                std::lock_guard<std::mutex> lk(projectMutex);
                projCopy = activeProject;
            }

            // Detect if any V1/V2 video clip has active audio (for Audio Ducking on A1 music!)
            bool voiceActive = false;
            double chunkEnd = currentTime + static_cast<double>(kChunkFrames) / kSampleRate;
            for (const auto& c : projCopy.clips) {
                if (c.type == MediaType::Video && !c.muted && chunkEnd > c.timelineStart && currentTime < c.timelineEnd()) {
                    voiceActive = true;
                    break;
                }
            }

            std::vector<float> mix(kChunkSamples, 0.0f);
            float maxEcho = 0.0f, maxBass = 0.0f, maxTreble = 0.0f, maxReverb = 0.0f, minLimiter = 0.98f;
            {
                std::lock_guard<std::mutex> lk(cacheMutex);
                for (const auto& c : projCopy.clips) {
                    if (c.muted || c.type == MediaType::Text || c.type == MediaType::Image) continue;
                    if (c.trackIndex < 0 || c.trackIndex >= static_cast<int>(projCopy.tracks.size())) continue;
                    if (projCopy.tracks[c.trackIndex].muted) continue;
                    if (chunkEnd <= c.timelineStart || currentTime >= c.timelineEnd()) continue;

                    auto it = tracks.find(c.sourcePath);
                    if (it == tracks.end() || it->second.pcm.empty()) continue;
                    const auto& pcm = it->second.pcm;
                    size_t totalFrames = pcm.size() / kChannels;

                    const float panL = std::clamp(1.0f - c.audioDsp.pan, 0.0f, 1.0f);
                    const float panR = std::clamp(1.0f + c.audioDsp.pan, 0.0f, 1.0f);
                    const float gate = c.audioDsp.noiseReduction * 0.08f;
                    const float duck = (c.audioDsp.autoDucking && c.type == MediaType::Audio && voiceActive) ? 0.30f : 1.0f;
                    const double rateMul = std::clamp(c.speed * static_cast<double>(c.audioDsp.pitch), 0.25, 4.0);

                    maxEcho   = std::max(maxEcho, c.audioDsp.echo);
                    maxReverb = std::max(maxReverb, c.audioDsp.reverb);
                    minLimiter = std::min(minLimiter, std::clamp(c.audioDsp.limiter, 0.2f, 1.0f));
                    maxBass   = std::max(maxBass, c.audioDsp.bassBoost);
                    if (std::abs(c.audioDsp.treble) > std::abs(maxTreble)) maxTreble = c.audioDsp.treble;

                    for (int f = 0; f < kChunkFrames; ++f) {
                        double t = currentTime + static_cast<double>(f) / kSampleRate;
                        if (t < c.timelineStart || t >= c.timelineEnd()) continue;
                        double localT = t - c.timelineStart;
                        double srcSec = c.reversed
                            ? std::max(0.0, c.sourceIn + (c.duration - localT) * rateMul)
                            : (c.sourceIn + localT * rateMul);
                        size_t srcFrame = static_cast<size_t>(srcSec * kSampleRate);
                        if (srcFrame >= totalFrames) continue;

                        float sL = pcm[srcFrame * 2]     / 32768.0f;
                        float sR = pcm[srcFrame * 2 + 1] / 32768.0f;
                        if (gate > 0.001f && std::max(std::abs(sL), std::abs(sR)) < gate) {
                            sL *= 0.1f; sR *= 0.1f;
                        }
                        if (c.audioDsp.vocalIsolate) {
                            float mid = (sL + sR) * 0.5f;
                            sL = mid; sR = mid;
                        }
                        if (std::abs(c.audioDsp.stereoWidth - 1.0f) > 0.02f) {
                            float mid = (sL + sR) * 0.5f;
                            float side = (sL - sR) * 0.5f * c.audioDsp.stereoWidth;
                            sL = mid + side;
                            sR = mid - side;
                        }
                        if (c.audioDsp.deEsser > 0.02f) {
                            float bpL = kDeB0 * sL + deZ1L;
                            deZ1L = deZ2L - kDeA1 * bpL;
                            deZ2L = kDeB2 * sL - kDeA2 * bpL;
                            float bpR = kDeB0 * sR + deZ1R;
                            deZ1R = deZ2R - kDeA1 * bpR;
                            deZ2R = kDeB2 * sR - kDeA2 * bpR;
                            float sib = std::max(std::abs(bpL), std::abs(bpR));
                            deEnv = deEnv * 0.92f + sib * 0.08f;
                            if (deEnv > 0.07f) {
                                float red = std::clamp((deEnv - 0.07f) * 4.5f, 0.0f, 0.80f) * c.audioDsp.deEsser;
                                sL -= bpL * red;
                                sR -= bpR * red;
                            }
                        }
                        if (c.audioDsp.compressor > 0.02f) {
                            float thr = 1.0f - c.audioDsp.compressor * 0.65f;
                            auto comp = [&](float v) {
                                float av = std::abs(v);
                                if (av > thr) {
                                    float excess = (av - thr) * (1.0f - c.audioDsp.compressor * 0.7f);
                                    return (v >= 0 ? 1.0f : -1.0f) * (thr + excess);
                                }
                                return v;
                            };
                            sL = comp(sL) * (1.0f + c.audioDsp.compressor * 0.4f);
                            sR = comp(sR) * (1.0f + c.audioDsp.compressor * 0.4f);
                        }
                        float g = evaluateAudioGain(c, t) * duck;
                        mix[f * 2]     += sL * g * panL;
                        mix[f * 2 + 1] += sR * g * panR;
                    }
                }
            }

            // Master DSP pass: Bass Boost + Treble EQ + Echo + Soft-Knee Limiter (tanh)
            constexpr float alpha = 0.024f; // ~180Hz 1-pole lowpass
            for (int f = 0; f < kChunkFrames; ++f) {
                float l = mix[f * 2];
                float r = mix[f * 2 + 1];

                lpL += alpha * (l - lpL);
                lpR += alpha * (r - lpR);
                if (maxBass > 0.01f) {
                    l += lpL * maxBass * 1.3f;
                    r += lpR * maxBass * 1.3f;
                }
                if (std::abs(maxTreble) > 0.01f) {
                    l += (l - lpL) * maxTreble * 0.8f;
                    r += (r - lpR) * maxTreble * 0.8f;
                }
                if (maxEcho > 0.01f) {
                    float dL = echoL[echoPos];
                    float dR = echoR[echoPos];
                    echoL[echoPos] = l + dL * (0.42f * maxEcho);
                    echoR[echoPos] = r + dR * (0.42f * maxEcho);
                    l += dL * maxEcho * 0.55f;
                    r += dR * maxEcho * 0.55f;
                    echoPos = (echoPos + 1) % kEchoDelay;
                }

                if (maxReverb > 0.01f) {
                    float fb = 0.55f * maxReverb;
                    float r1L = rev1L[revP1], r1R = rev1R[revP1];
                    float r2L = rev2L[revP2], r2R = rev2R[revP2];
                    float r3L = rev3L[revP3], r3R = rev3R[revP3];
                    rev1L[revP1] = l + r1L * fb; rev1R[revP1] = r + r1R * fb;
                    rev2L[revP2] = l + r2L * fb; rev2R[revP2] = r + r2R * fb;
                    rev3L[revP3] = l + r3L * fb; rev3R[revP3] = r + r3R * fb;
                    revP1 = (revP1 + 1) % kRevDelay1;
                    revP2 = (revP2 + 1) % kRevDelay2;
                    revP3 = (revP3 + 1) % kRevDelay3;
                    l += (r1L + r2L + r3L) * (0.24f * maxReverb);
                    r += (r1R + r2R + r3R) * (0.24f * maxReverb);
                }

                // Controllable brickwall + soft-knee limiter prevents digital clipping
                float ceilL = minLimiter;
                l = std::tanh(l / std::max(0.2f, ceilL)) * ceilL;
                r = std::tanh(r / std::max(0.2f, ceilL)) * ceilL;
                bufs[bufIdx][f * 2]     = static_cast<int16_t>(std::clamp(l, -1.0f, 1.0f) * 32767.0f);
                bufs[bufIdx][f * 2 + 1] = static_cast<int16_t>(std::clamp(r, -1.0f, 1.0f) * 32767.0f);
            }

            // Compute live 16-band Audio Spectrum via fast Goertzel bank (60Hz .. 12.5kHz)
            {
                static constexpr float kBandHz[16] = {
                    60, 100, 160, 250, 380, 560, 820, 1200,
                    1750, 2500, 3500, 4800, 6400, 8200, 10000, 12500
                };
                std::array<float, 16> bands{};
                const int N = std::min(kChunkFrames, 256);
                for (int bi = 0; bi < 16; ++bi) {
                    float w = 2.0f * 3.14159265f * kBandHz[bi] / static_cast<float>(kSampleRate);
                    float coeff = 2.0f * std::cos(w);
                    float s1 = 0.0f, s2 = 0.0f;
                    for (int n = 0; n < N; ++n) {
                        float x = (mix[n * 2] + mix[n * 2 + 1]) * 0.5f;
                        float s0 = x + coeff * s1 - s2;
                        s2 = s1; s1 = s0;
                    }
                    float mag = std::sqrt(std::max(0.0f, s1 * s1 + s2 * s2 - coeff * s1 * s2)) / (N * 0.35f);
                    bands[bi] = std::clamp(std::sqrt(mag), 0.0f, 1.0f);
                }
                std::lock_guard<std::mutex> slk(specMutex);
                for (int bi = 0; bi < 16; ++bi) {
                    spectrumBands[bi] = spectrumBands[bi] * 0.45f + bands[bi] * 0.55f;
                }
            }

            waveOutWrite(hWaveOut, &hdr, sizeof(WAVEHDR));
            currentTime += static_cast<double>(kChunkFrames) / kSampleRate;
            bufIdx = (bufIdx + 1) % kNumBufs;
        }

        waveOutReset(hWaveOut);
        for (int i = 0; i < kNumBufs; ++i) waveOutUnprepareHeader(hWaveOut, &hdrs[i], sizeof(WAVEHDR));
        waveOutClose(hWaveOut);
        playing.store(false);
    }

    void recordLoop() {
        WAVEFORMATEX wfx{};
        wfx.wFormatTag      = WAVE_FORMAT_PCM;
        wfx.nChannels       = kChannels;
        wfx.nSamplesPerSec  = kSampleRate;
        wfx.wBitsPerSample  = 16;
        wfx.nBlockAlign     = kChannels * 2;
        wfx.nAvgBytesPerSec = kSampleRate * wfx.nBlockAlign;

        HWAVEIN hWaveIn = nullptr;
        if (waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            recording.store(false);
            return;
        }

        constexpr int kRecBufs = 4;
        constexpr int kRecSamples = (kSampleRate / 10) * kChannels; // 100ms chunks
        std::vector<int16_t> rbufs[kRecBufs];
        WAVEHDR rhdrs[kRecBufs]{};
        for (int i = 0; i < kRecBufs; ++i) {
            rbufs[i].assign(kRecSamples, 0);
            rhdrs[i].lpData = reinterpret_cast<LPSTR>(rbufs[i].data());
            rhdrs[i].dwBufferLength = static_cast<DWORD>(kRecSamples * sizeof(int16_t));
            waveInPrepareHeader(hWaveIn, &rhdrs[i], sizeof(WAVEHDR));
            waveInAddBuffer(hWaveIn, &rhdrs[i], sizeof(WAVEHDR));
        }

        waveInStart(hWaveIn);
        while (!recStop.load()) {
            for (int i = 0; i < kRecBufs; ++i) {
                if (rhdrs[i].dwFlags & WHDR_DONE) {
                    size_t n = rhdrs[i].dwBytesRecorded / sizeof(int16_t);
                    if (n > 0) {
                        std::lock_guard<std::mutex> lk(recMutex);
                        if (recBuffer.size() + n < kMaxSamplesPerFile) {
                            recBuffer.insert(recBuffer.end(), rbufs[i].begin(), rbufs[i].begin() + n);
                        }
                    }
                    rhdrs[i].dwFlags &= ~WHDR_DONE;
                    waveInAddBuffer(hWaveIn, &rhdrs[i], sizeof(WAVEHDR));
                }
            }
            Sleep(15);
        }

        waveInStop(hWaveIn);
        waveInReset(hWaveIn);
        for (int i = 0; i < kRecBufs; ++i) waveInUnprepareHeader(hWaveIn, &rhdrs[i], sizeof(WAVEHDR));
        waveInClose(hWaveIn);
        recording.store(false);
    }
#endif
};

AudioEngine::AudioEngine() : m_impl(std::make_unique<Impl>()) {}
AudioEngine::~AudioEngine() { shutdown(); }

bool AudioEngine::initialize() {
    m_impl->initialized = true;
    return true;
}

void AudioEngine::shutdown() {
    if (!m_impl) return;
    stopPlayback();
    m_impl->recStop.store(true);
    if (m_impl->recThread.joinable()) m_impl->recThread.join();
    clearCache();
    m_impl->initialized = false;
}

void AudioEngine::clearCache() {
    std::lock_guard<std::mutex> lk(m_impl->cacheMutex);
    m_impl->tracks.clear();
    m_impl->loadingPaths.clear();
}

void AudioEngine::preloadAudioAsync(const std::string& filePathUtf8) {
    if (filePathUtf8.empty()) return;
#ifdef _WIN32
    {
        std::lock_guard<std::mutex> lk(m_impl->cacheMutex);
        if (m_impl->tracks.count(filePathUtf8) || m_impl->loadingPaths.count(filePathUtf8)) return;
        m_impl->loadingPaths.insert(filePathUtf8);
    }
    Impl* impl = m_impl.get();
    std::thread([impl, filePathUtf8]() {
        AudioTrackData data;
        bool ok = impl->decodeAudioFile(filePathUtf8, data);
        std::lock_guard<std::mutex> lk(impl->cacheMutex);
        impl->loadingPaths.erase(filePathUtf8);
        if (ok) impl->tracks[filePathUtf8] = std::move(data);
    }).detach();
#endif
}

std::vector<float> AudioEngine::getWaveformPeaks(const std::string& filePathUtf8) const {
    std::lock_guard<std::mutex> lk(m_impl->cacheMutex);
    auto it = m_impl->tracks.find(filePathUtf8);
    if (it != m_impl->tracks.end()) return it->second.peaks;
    return {};
}

std::vector<SubtitleEntry> AudioEngine::detectSpeechSegments(const std::string& filePathUtf8,
                                                             double clipTimelineStart,
                                                             double clipDuration) const {
    std::vector<SubtitleEntry> segs;
    auto peaks = getWaveformPeaks(filePathUtf8);
    if (peaks.empty() || clipDuration <= 0.5) {
        int idx = 1;
        for (double t = 0.0; t < clipDuration; t += 3.0) {
            double dur = std::min(2.6, clipDuration - t);
            if (dur >= 0.4) segs.push_back({ clipTimelineStart + t, clipTimelineStart + t + dur, "Caption " + std::to_string(idx++) });
        }
        return segs;
    }

    const double binDur = clipDuration / static_cast<double>(peaks.size());
    bool inSpeech = false;
    double segStart = 0.0;
    int idx = 1;
    for (size_t i = 0; i < peaks.size(); ++i) {
        bool loud = peaks[i] >= 0.22f;
        double t = clipTimelineStart + i * binDur;
        if (loud && !inSpeech) { inSpeech = true; segStart = t; }
        else if ((!loud || i + 1 == peaks.size()) && inSpeech) {
            double segEnd = t + binDur;
            if (segEnd - segStart >= 0.45) segs.push_back({ segStart, segEnd, "Caption " + std::to_string(idx++) });
            inSpeech = false;
        }
    }
    return segs;
}

std::vector<double> AudioEngine::detectBeats(const std::string& filePathUtf8,
                                             double clipTimelineStart,
                                             double clipDuration) const {
    std::vector<double> beats;
    auto peaks = getWaveformPeaks(filePathUtf8);
    if (peaks.size() < 4 || clipDuration <= 0.5) return beats;
    const double binDur = clipDuration / static_cast<double>(peaks.size());
    double lastBeat = -1.0;
    for (size_t i = 1; i + 1 < peaks.size(); ++i) {
        if (peaks[i] > 0.65f && peaks[i] >= peaks[i - 1] && peaks[i] >= peaks[i + 1]) {
            double t = clipTimelineStart + i * binDur;
            if (t - lastBeat >= 0.45) {
                beats.push_back(t);
                lastBeat = t;
            }
        }
    }
    return beats;
}

void AudioEngine::startPlayback(const Project& projectSnapshot, double startTimeSec) {
    stopPlayback();
#ifdef _WIN32
    {
        std::lock_guard<std::mutex> lk(m_impl->projectMutex);
        m_impl->activeProject = projectSnapshot;
        m_impl->playStartSec = startTimeSec;
    }
    m_impl->stopRequested.store(false);
    m_impl->playing.store(true);
    m_impl->playbackThread = std::thread([this]() { m_impl->playbackLoop(); });
#else
    (void)projectSnapshot; (void)startTimeSec;
#endif
}

void AudioEngine::stopPlayback() {
    m_impl->stopRequested.store(true);
    if (m_impl->playbackThread.joinable()) m_impl->playbackThread.join();
    m_impl->playing.store(false);
}

void AudioEngine::updateProjectSnapshot(const Project& projectSnapshot) {
    std::lock_guard<std::mutex> lk(m_impl->projectMutex);
    m_impl->activeProject = projectSnapshot;
}

bool AudioEngine::isPlaying() const { return m_impl->playing.load(); }

bool AudioEngine::startRecording() {
#ifdef _WIN32
    if (m_impl->recording.load()) return false;
    {
        std::lock_guard<std::mutex> lk(m_impl->recMutex);
        m_impl->recBuffer.clear();
    }
    m_impl->recStop.store(false);
    m_impl->recording.store(true);
    m_impl->recThread = std::thread([this]() { m_impl->recordLoop(); });
    return true;
#else
    return false;
#endif
}

bool AudioEngine::stopRecordingToWav(const std::string& outWavPathUtf8) {
    if (!m_impl->recording.load() && !m_impl->recThread.joinable()) return false;
    m_impl->recStop.store(true);
    if (m_impl->recThread.joinable()) m_impl->recThread.join();
    std::lock_guard<std::mutex> lk(m_impl->recMutex);
    return writeWavFile16Stereo(outWavPathUtf8, m_impl->recBuffer, kSampleRate);
}

bool AudioEngine::isRecording() const { return m_impl->recording.load(); }


bool AudioEngine::generateSfxWav(int sfxIndex, const std::string& outWavPathUtf8) {
    constexpr double kDur = 1.0;
    const size_t nFrames = static_cast<size_t>(kSampleRate * kDur);
    std::vector<int16_t> pcm(nFrames * 2, 0);
    uint32_t seed = 0x51A7F00Du + static_cast<uint32_t>(sfxIndex * 997);

    for (size_t i = 0; i < nFrames; ++i) {
        double t = static_cast<double>(i) / kSampleRate;
        double env = 0.0, s = 0.0;
        switch (sfxIndex % 9) {
            case 0: // Whoosh
                seed = seed * 1664525u + 1013904223u;
                env = std::sin(t * 3.14159) * std::sin(t * 3.14159);
                s = ((static_cast<int>(seed >> 16) % 2000) / 1000.0 - 1.0) * env * 0.7;
                break;
            case 1: // Pop
                env = (t < 0.18) ? std::exp(-t * 28.0) : 0.0;
                s = std::sin(2.0 * 3.14159 * (420.0 - t * 1400.0) * t) * env * 0.9;
                break;
            case 2: // Click
                env = (t < 0.05) ? std::exp(-t * 80.0) : 0.0;
                s = std::sin(2.0 * 3.14159 * 1800.0 * t) * env * 0.85;
                break;
            case 3: // Beep
                env = (t < 0.35) ? 1.0 : 0.0;
                s = std::sin(2.0 * 3.14159 * 1000.0 * t) * env * 0.6;
                break;
            case 4: // Riser
                env = t;
                s = std::sin(2.0 * 3.14159 * (120.0 + t * t * 900.0) * t) * env * 0.75;
                break;
            case 5: // Bass Drop
                env = std::exp(-t * 3.5);
                s = std::sin(2.0 * 3.14159 * (140.0 * std::exp(-t * 3.0) + 32.0) * t) * env * 0.95;
                break;
            case 6: // Notification Ding
                env = std::exp(-t * 4.5);
                s = (std::sin(2.0 * 3.14159 * 1046.5 * t) * 0.6 + std::sin(2.0 * 3.14159 * 1568.0 * t) * 0.4) * env;
                break;
            case 7: // Laser Zap
                env = (t < 0.35) ? std::exp(-t * 8.0) : 0.0;
                s = std::sin(2.0 * 3.14159 * (1600.0 * std::exp(-t * 9.0) + 180.0) * t) * env * 0.85;
                break;
            case 8: // Applause / Crowd burst
                seed = seed * 1664525u + 1013904223u;
                env = std::sin(t * 3.14159) * (0.6 + 0.4 * std::sin(t * 45.0));
                s = ((static_cast<int>(seed >> 16) % 2000) / 1000.0 - 1.0) * env * 0.55;
                break;
        }
        int16_t v = static_cast<int16_t>(std::clamp(s, -1.0, 1.0) * 32767.0);
        pcm[i * 2] = v;
        pcm[i * 2 + 1] = v;
    }
    return writeWavFile16Stereo(outWavPathUtf8, pcm, kSampleRate);
}

bool AudioEngine::generateMusicLoopWav(int trackIndex, const std::string& outWavPathUtf8) {
    constexpr double kDur = 12.0;
    const size_t nFrames = static_cast<size_t>(kSampleRate * kDur);
    std::vector<int16_t> pcm(nFrames * 2, 0);

    // 4 Musical Chord Progressions (Hz)
    const double chords[4][4][3] = {
        { {261.63, 329.63, 392.00}, {220.00, 261.63, 329.63}, {174.61, 220.00, 261.63}, {196.00, 246.94, 293.66} }, // Lo-Fi Chill (C-Am-F-G)
        { {146.83, 220.00, 261.63}, {130.81, 196.00, 246.94}, {116.54, 174.61, 220.00}, {130.81, 196.00, 261.63} }, // Cyber Synth
        { {110.00, 164.81, 220.00}, {130.81, 196.00, 261.63}, {146.83, 220.00, 293.66}, {164.81, 246.94, 329.63} }, // Epic Cinema
        { {293.66, 369.99, 440.00}, {246.94, 293.66, 369.99}, {196.00, 246.94, 293.66}, {220.00, 277.18, 329.63} }  // Upbeat Pop
    };
    int style = trackIndex % 4;

    for (size_t i = 0; i < nFrames; ++i) {
        double t = static_cast<double>(i) / kSampleRate;
        int bar = static_cast<int>(t / 3.0) % 4;
        double beatFrac = std::fmod(t, 0.5) / 0.5;
        double kick = (beatFrac < 0.18) ? std::sin(2.0 * 3.14159 * (110.0 * std::exp(-beatFrac * 18.0) + 42.0) * beatFrac) * std::exp(-beatFrac * 14.0) * 0.45 : 0.0;

        double pad = 0.0;
        for (int n = 0; n < 3; ++n) {
            double f = chords[style][bar][n];
            pad += std::sin(2.0 * 3.14159 * f * t) * 0.16;
            if (style == 1 || style == 3) pad += std::sin(2.0 * 3.14159 * f * 2.0 * t) * 0.06 * (1.0 - beatFrac);
        }
        double fade = std::min(1.0, std::min(t / 0.5, (kDur - t) / 0.8));
        double l = std::tanh((pad + kick) * fade);
        double r = std::tanh((pad * 0.95 + kick) * fade);
        pcm[i * 2]     = static_cast<int16_t>(l * 30000.0);
        pcm[i * 2 + 1] = static_cast<int16_t>(r * 30000.0);
    }
    return writeWavFile16Stereo(outWavPathUtf8, pcm, kSampleRate);
}

bool AudioEngine::synthesizeTextToSpeechWav(const std::string& textUtf8, const std::string& outWavPathUtf8) {
    if (textUtf8.empty() || outWavPathUtf8.empty()) return false;
#ifdef _WIN32
    std::string safeText;
    for (char c : textUtf8) {
        if (c == '\'' || c == '"' || c == '`' || c == '$') safeText.push_back(' ');
        else safeText.push_back(c);
    }
    std::string psCmd =
        "powershell.exe -NoProfile -NonInteractive -WindowStyle Hidden -Command \""
        "Add-Type -AssemblyName System.Speech; "
        "$s = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
        "$s.SetOutputToWaveFile('" + outWavPathUtf8 + "'); "
        "$s.Speak('" + safeText + "'); "
        "$s.Dispose()\"";

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::wstring wCmd = utf8ToWide(psCmd);
    std::vector<wchar_t> buf(wCmd.begin(), wCmd.end());
    buf.push_back(L'\0');

    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return false;
    }
    WaitForSingleObject(pi.hProcess, 15000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode == 0 && std::filesystem::exists(std::filesystem::u8path(outWavPathUtf8));
#else
    return false;
#endif
}


void AudioEngine::getSpectrumBands(std::array<float, 16>& outBands) const {
    outBands.fill(0.0f);
    if (!m_impl) return;
    if (!m_impl->playing.load()) return;
    std::lock_guard<std::mutex> lk(m_impl->specMutex);
    outBands = m_impl->spectrumBands;
}

bool AudioEngine::detectNonSilentRange(const std::string& filePathUtf8, double clipDuration,
                                       double& outTrimInSec, double& outTrimOutSec) const {
    outTrimInSec = 0.0;
    outTrimOutSec = clipDuration;
    if (clipDuration <= 0.25) return false;
    auto peaks = getWaveformPeaks(filePathUtf8);
    if (peaks.size() < 4) return false;

    const float thresh = 0.045f;
    size_t firstNonSilent = 0;
    while (firstNonSilent < peaks.size() && peaks[firstNonSilent] < thresh) ++firstNonSilent;
    if (firstNonSilent >= peaks.size()) return false;

    size_t lastNonSilent = peaks.size() - 1;
    while (lastNonSilent > firstNonSilent && peaks[lastNonSilent] < thresh) --lastNonSilent;

    double inFrac  = static_cast<double>(firstNonSilent) / static_cast<double>(peaks.size());
    double outFrac = static_cast<double>(lastNonSilent + 1) / static_cast<double>(peaks.size());
    outTrimInSec  = std::clamp(inFrac * clipDuration, 0.0, clipDuration - 0.15);
    outTrimOutSec = std::clamp(outFrac * clipDuration, outTrimInSec + 0.15, clipDuration);
    return (outTrimInSec > 0.05 || (clipDuration - outTrimOutSec) > 0.05);
}


bool AudioEngine::generateHubSoundWav(int hubIdx0to63, const std::string& outWavPathUtf8) {
    int idx = std::clamp(hubIdx0to63, 0, 119);
    constexpr int sr = 48000;
    // Synthesize distinct duration, base frequency & waveform family for all 64 popular SFX & music loops
    double dur = (idx >= 96) ? 2.4 : ((idx >= 48 && idx < 72) ? 1.35 : 0.60);
    if (idx >= 24 && idx < 48) dur = 0.35;
    size_t n = static_cast<size_t>(dur * sr);
    std::vector<int16_t> pcm(n * 2, 0);

    uint32_t seed = static_cast<uint32_t>((idx + 1) * 2654435761u);
    auto rnd = [&]() -> float {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>((seed >> 8) & 0xFFFFFFu) / 8388608.0f - 1.0f;
    };

    const float baseFreqs[16] = { 900, 420, 1400, 680, 540, 1046, 1318, 880, 68, 82, 220, 58, 988, 523, 220, 55 };
    float f0 = baseFreqs[idx % 16] * (0.85f + 0.08f * (idx / 16));

    for (size_t i = 0; i < n; ++i) {
        double t = static_cast<double>(i) / sr;
        double u = static_cast<double>(i) / std::max<size_t>(1, n);
        float v = 0.0f;
        if (idx < 24) { // Whoosh & Transitions
            float env = static_cast<float>(std::sin(3.14159265 * u) * std::sin(3.14159265 * u));
            v = (rnd() * 0.55f + static_cast<float>(std::sin(6.2831853 * f0 * (0.4 + u) * t)) * 0.3f) * env;
        } else if (idx < 48) { // Pop, Bell, Click, UI & YouTube
            float env = static_cast<float>(std::exp(-u * 6.5));
            v = (static_cast<float>(std::sin(6.2831853 * f0 * (1.5 - u * 0.7) * t))
               + 0.45f * static_cast<float>(std::sin(6.2831853 * f0 * 1.5 * t))) * env * 0.65f;
        } else if (idx < 72) { // Cinema Booms, Braams & Risers
            bool isRiser = (idx % 3 == 2);
            float env = isRiser ? static_cast<float>(u * u) : static_cast<float>(std::exp(-u * 2.5));
            float freq = isRiser ? f0 * static_cast<float>(1.0 + u * 3.8) : (62.0f + (idx % 5) * 10.0f) * static_cast<float>(1.0 + std::exp(-u * 8.0) * 1.4);
            v = std::tanh((static_cast<float>(std::sin(6.2831853 * freq * t)) + 0.35f * static_cast<float>(std::sin(12.56637 * freq * t)) + rnd() * 0.15f) * 1.4f) * env * 0.85f;
        } else if (idx < 96) { // Gaming, Coins, Level Up & Alerts
            int step = std::min(3, static_cast<int>(u * 4.0));
            const float mults[4] = { 1.0f, 1.25f, 1.5f, 2.0f };
            v = static_cast<float>(std::sin(6.2831853 * f0 * mults[step] * t)) * static_cast<float>(std::exp(-(u * 4.0 - step) * 2.8)) * 0.65f;
        } else { // Music Loops (Lo-Fi, Trap 808, Synthwave, Cinema Pads)
            const float chord[4] = { 1.0f, 1.25f, 1.5f, 1.875f };
            float arp = chord[static_cast<int>(t * 4.0) % 4];
            float fade = static_cast<float>(std::sin(3.14159265 * std::min(1.0, u * 12.0)) * std::sin(3.14159265 * std::min(1.0, (1.0 - u) * 12.0)));
            double beat = std::fmod(t * 2.5, 1.0);
            float kick = (idx % 2 == 0) ? static_cast<float>(std::sin(6.2831853 * 58.0 * (1.0 + std::exp(-beat * 16.0) * 2.5) * t) * std::exp(-beat * 6.0)) * 0.45f : 0.0f;
            float pad = (static_cast<float>(std::sin(6.2831853 * 220.0 * t)) * 0.3f
                       + static_cast<float>(std::sin(6.2831853 * 220.0 * arp * t)) * 0.28f);
            v = std::tanh((kick + pad) * 1.15f) * fade * 0.8f;
        }
        int16_t sL = static_cast<int16_t>(std::clamp(v, -0.98f, 0.98f) * 32767.0f);
        int16_t sR = static_cast<int16_t>(std::clamp(v * 0.96f, -0.98f, 0.98f) * 32767.0f);
        pcm[i * 2] = sL;
        pcm[i * 2 + 1] = sR;
    }
    return writeWavFile16Stereo(outWavPathUtf8, pcm, sr);
}

} // namespace cc
