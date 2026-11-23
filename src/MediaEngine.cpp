#include "MediaEngine.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wrl/client.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>
#include <wincodec.h>

using Microsoft::WRL::ComPtr;
#endif

namespace cc {

namespace {

std::string u8ToStr(const std::u8string& u) {
    return std::string(reinterpret_cast<const char*>(u.data()), u.size());
}

std::string toLowerAscii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

MediaType guessMediaTypeFromExt(const std::string& path) {
    std::string ext = toLowerAscii(u8ToStr(std::filesystem::u8path(path).extension().u8string()));
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".webp" || ext == ".gif") {
        return MediaType::Image;
    }
    if (ext == ".mp3" || ext == ".wav" || ext == ".aac" || ext == ".m4a" || ext == ".flac" || ext == ".ogg" || ext == ".wma") {
        return MediaType::Audio;
    }
    return MediaType::Video;
}

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), len);
    return w;
}
#endif

inline uint8_t clampByte(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 255.0f) return 255;
    return static_cast<uint8_t>(v + 0.5f);
}

inline uint8_t clampByteInt(int v) {
    if (v <= 0) return 0;
    if (v >= 255) return 255;
    return static_cast<uint8_t>(v);
}

void downscaleBGRA(const FrameBGRA& src, int maxW, int maxH, FrameBGRA& dst) {
    if (!src.valid() || maxW <= 0 || maxH <= 0) return;
    double scale = std::min(static_cast<double>(maxW) / src.width, static_cast<double>(maxH) / src.height);
    if (scale >= 1.0) {
        dst = src;
        return;
    }
    int dw = std::max(2, static_cast<int>(src.width * scale));
    int dh = std::max(2, static_cast<int>(src.height * scale));
    dst.width = dw;
    dst.height = dh;
    dst.pixels.resize(static_cast<size_t>(dw) * dh * 4);

    for (int y = 0; y < dh; ++y) {
        int sy = std::min(src.height - 1, static_cast<int>(y / scale));
        const uint8_t* srcRow = src.pixels.data() + static_cast<size_t>(sy) * src.width * 4;
        uint8_t* dstRow = dst.pixels.data() + static_cast<size_t>(y) * dw * 4;
        for (int x = 0; x < dw; ++x) {
            int sx = std::min(src.width - 1, static_cast<int>(x / scale));
            std::memcpy(dstRow + x * 4, srcRow + sx * 4, 4);
        }
    }
}

} // namespace

struct MediaEngine::Impl {
    bool initialized = false;

    // Async decoder worker state
    struct DecodeReq {
        std::string path;
        double targetSec = 0.0;
        bool valid = false;
    };
    struct DecodedSlot {
        std::shared_ptr<FrameBGRA> frame;
        double decodedSec = -10.0;
    };

    std::mutex reqMutex;
    std::condition_variable reqCv;
    DecodeReq pendingReq;
    std::atomic<bool> workerStop{false};
    std::thread workerThread;

    std::mutex frameMutex;
    std::unordered_map<std::string, DecodedSlot> latestFrames;
    std::unordered_map<std::string, std::shared_ptr<FrameBGRA>> imageCache;

#ifdef _WIN32
    ComPtr<IWICImagingFactory> wicFactory;

    struct VideoReaderCache {
        ComPtr<IMFSourceReader> reader;
        int width = 0;
        int height = 0;
        int stride = 0;
        double lastDecodedSec = -10.0;
        FrameBGRA lastFrame;
    };

    std::unordered_map<std::string, VideoReaderCache> syncReaders;
    std::unordered_map<std::string, VideoReaderCache> asyncReaders;

    bool loadWicImage(const std::string& pathUtf8, FrameBGRA& outFrame) {
        if (!wicFactory) return false;
        std::wstring wPath = utf8ToWide(pathUtf8);
        ComPtr<IWICBitmapDecoder> decoder;
        if (FAILED(wicFactory->CreateDecoderFromFilename(
                wPath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))) {
            return false;
        }
        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(decoder->GetFrame(0, &frame))) return false;

        ComPtr<IWICFormatConverter> converter;
        if (FAILED(wicFactory->CreateFormatConverter(&converter))) return false;
        if (FAILED(converter->Initialize(
                frame.Get(), GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
            return false;
        }

        UINT w = 0, h = 0;
        if (FAILED(converter->GetSize(&w, &h)) || w == 0 || h == 0) return false;

        outFrame.width = static_cast<int>(w);
        outFrame.height = static_cast<int>(h);
        outFrame.pixels.resize(static_cast<size_t>(w) * h * 4);
        return SUCCEEDED(converter->CopyPixels(
            nullptr, w * 4, static_cast<UINT>(outFrame.pixels.size()), outFrame.pixels.data()));
    }

    VideoReaderCache* getOrCreateReader(std::unordered_map<std::string, VideoReaderCache>& pool,
                                        const std::string& pathUtf8) {
        auto it = pool.find(pathUtf8);
        if (it != pool.end()) return &it->second;

        ComPtr<IMFAttributes> attrs;
        if (FAILED(MFCreateAttributes(&attrs, 2))) return nullptr;
        attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

        std::wstring wPath = utf8ToWide(pathUtf8);
        ComPtr<IMFSourceReader> reader;
        if (FAILED(MFCreateSourceReaderFromURL(wPath.c_str(), attrs.Get(), &reader))) {
            return nullptr;
        }

        reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
        reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), TRUE);

        ComPtr<IMFMediaType> outType;
        if (FAILED(MFCreateMediaType(&outType))) return nullptr;
        outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        if (FAILED(reader->SetCurrentMediaType(
                static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, outType.Get()))) {
            return nullptr;
        }

        ComPtr<IMFMediaType> actualType;
        if (FAILED(reader->GetCurrentMediaType(
                static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &actualType))) {
            return nullptr;
        }

        UINT32 w = 0, h = 0;
        MFGetAttributeSize(actualType.Get(), MF_MT_FRAME_SIZE, &w, &h);
        LONG stride = static_cast<LONG>(w) * 4;
        UINT32 rawStride = 0;
        if (SUCCEEDED(actualType->GetUINT32(MF_MT_DEFAULT_STRIDE, &rawStride))) {
            stride = static_cast<LONG>(rawStride);
        }

        VideoReaderCache entry;
        entry.reader = reader;
        entry.width = static_cast<int>(w);
        entry.height = static_cast<int>(h);
        entry.stride = static_cast<int>(stride);
        auto [insIt, _] = pool.emplace(pathUtf8, std::move(entry));
        return &insIt->second;
    }

    bool decodeVideoAt(std::unordered_map<std::string, VideoReaderCache>& pool,
                       const std::string& pathUtf8, double targetSec, FrameBGRA& outFrame, double& outDecodedSec) {
        VideoReaderCache* vc = getOrCreateReader(pool, pathUtf8);
        if (!vc || !vc->reader || vc->width <= 0 || vc->height <= 0) return false;
        if (targetSec < 0.0) targetSec = 0.0;

        if (vc->lastFrame.valid() && std::abs(targetSec - vc->lastDecodedSec) < 0.025) {
            outFrame = vc->lastFrame;
            outDecodedSec = vc->lastDecodedSec;
            return true;
        }

        if (targetSec < vc->lastDecodedSec - 0.06 || targetSec > vc->lastDecodedSec + 1.50 || vc->lastDecodedSec < 0.0) {
            PROPVARIANT var{};
            PropVariantInit(&var);
            var.vt = VT_I8;
            var.hVal.QuadPart = static_cast<LONGLONG>(targetSec * 10000000.0);
            vc->reader->SetCurrentPosition(GUID_NULL, var);
            PropVariantClear(&var);
        }

        const int maxSteps = 36;
        bool gotFrame = false;
        for (int step = 0; step < maxSteps; ++step) {
            DWORD streamIdx = 0, flags = 0;
            LONGLONG llTimestamp = 0;
            ComPtr<IMFSample> sample;

            HRESULT hr = vc->reader->ReadSample(
                static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
                0, &streamIdx, &flags, &llTimestamp, &sample);
            if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;
            if (!sample) continue;

            double sampleSec = static_cast<double>(llTimestamp) / 10000000.0;
            if (sampleSec + 0.015 < targetSec && step + 1 < maxSteps) continue;

            ComPtr<IMFMediaBuffer> buffer;
            if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) continue;

            BYTE* data = nullptr;
            DWORD maxLen = 0, curLen = 0;
            if (FAILED(buffer->Lock(&data, &maxLen, &curLen))) continue;

            // V8 Ultra-Light: Direct single-pass downscale straight from locked MF buffer to max 640x360!
            // Zero 8MB temp allocation, 9x fewer pixels touched -> < 1.2ms per frame!
            const int w = vc->width;
            const int h = vc->height;
            const int srcStride = vc->stride;
            const int absStride = (srcStride >= 0) ? srcStride : -srcStride;
            double scale = std::min(1.0, std::min(640.0 / std::max(1, w), 360.0 / std::max(1, h)));
            const int dw = std::max(2, static_cast<int>(w * scale));
            const int dh = std::max(2, static_cast<int>(h * scale));

            vc->lastFrame.width = dw;
            vc->lastFrame.height = dh;
            vc->lastFrame.pixels.resize(static_cast<size_t>(dw) * dh * 4);

            for (int y = 0; y < dh; ++y) {
                int sy = std::min(h - 1, static_cast<int>(y / scale));
                int srcY = (srcStride >= 0) ? sy : (h - 1 - sy);
                size_t rowOff = static_cast<size_t>(srcY) * absStride;
                if (rowOff + static_cast<size_t>(w) * 4 > curLen) continue;
                const uint8_t* srcRow = data + rowOff;
                uint8_t* dstRow = vc->lastFrame.pixels.data() + static_cast<size_t>(y) * dw * 4;
                for (int x = 0; x < dw; ++x) {
                    int sx = std::min(w - 1, static_cast<int>(x / scale));
                    const uint8_t* sp = srcRow + sx * 4;
                    uint8_t* dp = dstRow + x * 4;
                    dp[0] = sp[0];
                    dp[1] = sp[1];
                    dp[2] = sp[2];
                    dp[3] = 255;
                }
            }
            buffer->Unlock();
            vc->lastDecodedSec = sampleSec;
            gotFrame = true;
            break;
        }

        if (gotFrame || vc->lastFrame.valid()) {
            outFrame = vc->lastFrame;
            outDecodedSec = vc->lastDecodedSec;
            return true;
        }
        return false;
    }

    void workerLoop() {
        HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool didCo = SUCCEEDED(hrCo);

        while (!workerStop.load()) {
            DecodeReq req;
            {
                std::unique_lock<std::mutex> lk(reqMutex);
                reqCv.wait(lk, [this]() { return workerStop.load() || pendingReq.valid; });
                if (workerStop.load()) break;
                req = pendingReq;
                pendingReq.valid = false;
            }

            FrameBGRA decoded;
            double decodedSec = req.targetSec;
            if (decodeVideoAt(asyncReaders, req.path, req.targetSec, decoded, decodedSec)) {
                std::lock_guard<std::mutex> lk(frameMutex);
                latestFrames[req.path] = { std::make_shared<FrameBGRA>(std::move(decoded)), decodedSec };
            }
        }

        asyncReaders.clear();
        if (didCo) CoUninitialize();
    }
#endif
};

MediaEngine::MediaEngine() : m_impl(std::make_unique<Impl>()) {}
MediaEngine::~MediaEngine() { shutdown(); }

bool MediaEngine::initialize() {
    if (m_impl->initialized) return true;
#ifdef _WIN32
    if (FAILED(MFStartup(MF_VERSION))) return false;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&m_impl->wicFactory)))) {
        MFShutdown();
        return false;
    }
    m_impl->workerStop.store(false);
    m_impl->workerThread = std::thread([this]() { m_impl->workerLoop(); });
#endif
    m_impl->initialized = true;
    return true;
}

void MediaEngine::shutdown() {
    if (!m_impl || !m_impl->initialized) return;
#ifdef _WIN32
    m_impl->workerStop.store(true);
    m_impl->reqCv.notify_all();
    if (m_impl->workerThread.joinable()) m_impl->workerThread.join();
    clearCache();
    m_impl->wicFactory.Reset();
    MFShutdown();
#else
    clearCache();
#endif
    m_impl->initialized = false;
}

void MediaEngine::clearCache() {
    {
        std::lock_guard<std::mutex> lk(m_impl->frameMutex);
        m_impl->latestFrames.clear();
        m_impl->imageCache.clear();
    }
#ifdef _WIN32
    m_impl->syncReaders.clear();
#endif
}

bool MediaEngine::probeFile(const std::string& filePathUtf8, MediaAsset& outAsset) {
    std::filesystem::path fp = std::filesystem::u8path(filePathUtf8);
    outAsset.filePath = filePathUtf8;
    outAsset.name = u8ToStr(fp.filename().u8string());
    outAsset.type = guessMediaTypeFromExt(filePathUtf8);
    outAsset.durationSec = 5.0;
    outAsset.width = 1920;
    outAsset.height = 1080;
    outAsset.fps = 30.0;
    outAsset.hasAudio = (outAsset.type == MediaType::Video || outAsset.type == MediaType::Audio);

#ifdef _WIN32
    if (outAsset.type == MediaType::Image) {
        FrameBGRA img;
        if (m_impl->loadWicImage(filePathUtf8, img)) {
            outAsset.width = img.width;
            outAsset.height = img.height;
            outAsset.durationSec = 5.0;
            outAsset.hasAudio = false;
            FrameBGRA proxy;
            if (img.width > 854 || img.height > 480) downscaleBGRA(img, 854, 480, proxy);
            else proxy = std::move(img);
            std::lock_guard<std::mutex> lk(m_impl->frameMutex);
            m_impl->imageCache[filePathUtf8] = std::make_shared<FrameBGRA>(std::move(proxy));
            return true;
        }
        return false;
    }

    std::wstring wPath = utf8ToWide(filePathUtf8);
    ComPtr<IMFSourceReader> reader;
    if (FAILED(MFCreateSourceReaderFromURL(wPath.c_str(), nullptr, &reader))) return false;

    PROPVARIANT var{};
    PropVariantInit(&var);
    if (SUCCEEDED(reader->GetPresentationAttribute(
            static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), MF_PD_DURATION, &var))) {
        if (var.vt == VT_UI8) outAsset.durationSec = static_cast<double>(var.uhVal.QuadPart) / 10000000.0;
        else if (var.vt == VT_I8) outAsset.durationSec = static_cast<double>(var.hVal.QuadPart) / 10000000.0;
    }
    PropVariantClear(&var);
    if (outAsset.durationSec < 0.2) outAsset.durationSec = 5.0;

    if (outAsset.type == MediaType::Video) {
        ComPtr<IMFMediaType> nativeType;
        if (SUCCEEDED(reader->GetNativeMediaType(
                static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, &nativeType))) {
            UINT32 w = 0, h = 0;
            if (SUCCEEDED(MFGetAttributeSize(nativeType.Get(), MF_MT_FRAME_SIZE, &w, &h)) && w > 0 && h > 0) {
                outAsset.width = static_cast<int>(w);
                outAsset.height = static_cast<int>(h);
            }
            UINT32 num = 30, den = 1;
            if (SUCCEEDED(MFGetAttributeRatio(nativeType.Get(), MF_MT_FRAME_RATE, &num, &den)) && den > 0) {
                outAsset.fps = static_cast<double>(num) / den;
            }
        }
    }
    return true;
#else
    return true;
#endif
}

bool MediaEngine::extractThumbnail(const std::string& filePathUtf8, MediaType type, int maxW, int maxH, FrameBGRA& outThumb) {
    if (type == MediaType::Audio || type == MediaType::Text) return false;
    FrameBGRA full;
    if (!decodeFrame(filePathUtf8, type, (type == MediaType::Video ? 0.5 : 0.0), full)) return false;
    downscaleBGRA(full, maxW, maxH, outThumb);
    return outThumb.valid();
}

bool MediaEngine::decodeFrame(const std::string& filePathUtf8, MediaType type, double sourceTimeSec, FrameBGRA& outFrame) {
#ifdef _WIN32
    if (type == MediaType::Image) {
        {
            std::lock_guard<std::mutex> lk(m_impl->frameMutex);
            auto it = m_impl->imageCache.find(filePathUtf8);
            if (it != m_impl->imageCache.end()) {
                if (it->second) { outFrame = *it->second; return true; }
            }
        }
        FrameBGRA loaded;
        if (m_impl->loadWicImage(filePathUtf8, loaded)) {
            FrameBGRA proxy;
            if (loaded.width > 854 || loaded.height > 480) downscaleBGRA(loaded, 854, 480, proxy);
            else proxy = std::move(loaded);
            auto ptr = std::make_shared<FrameBGRA>(std::move(proxy));
            outFrame = *ptr;
            std::lock_guard<std::mutex> lk(m_impl->frameMutex);
            m_impl->imageCache[filePathUtf8] = ptr;
            return true;
        }
        return false;
    }
    if (type == MediaType::Video) {
        double decSec = 0.0;
        return m_impl->decodeVideoAt(m_impl->syncReaders, filePathUtf8, sourceTimeSec, outFrame, decSec);
    }
#else
    (void)filePathUtf8; (void)type; (void)sourceTimeSec; (void)outFrame;
#endif
    return false;
}

void MediaEngine::requestFrameAsync(const std::string& filePathUtf8, MediaType type, double sourceTimeSec) {
    if (type != MediaType::Video || filePathUtf8.empty()) return;
#ifdef _WIN32
    {
        std::lock_guard<std::mutex> lk(m_impl->reqMutex);
        m_impl->pendingReq = { filePathUtf8, sourceTimeSec, true };
    }
    m_impl->reqCv.notify_one();
#else
    (void)sourceTimeSec;
#endif
}

std::shared_ptr<const FrameBGRA> MediaEngine::tryGetLatestFramePtr(const std::string& filePathUtf8, MediaType type,
                                                                     double targetSec, double& outDecodedSec) {
    if (type == MediaType::Image) {
        outDecodedSec = 0.0;
        {
            std::lock_guard<std::mutex> lk(m_impl->frameMutex);
            auto it = m_impl->imageCache.find(filePathUtf8);
            if (it != m_impl->imageCache.end() && it->second && it->second->valid()) return it->second;
        }
        FrameBGRA loaded;
        if (decodeFrame(filePathUtf8, type, 0.0, loaded)) {
            std::lock_guard<std::mutex> lk(m_impl->frameMutex);
            auto it = m_impl->imageCache.find(filePathUtf8);
            if (it != m_impl->imageCache.end()) return it->second;
        }
        return nullptr;
    }

    std::shared_ptr<const FrameBGRA> result;
    {
        std::lock_guard<std::mutex> lk(m_impl->frameMutex);
        auto it = m_impl->latestFrames.find(filePathUtf8);
        if (it != m_impl->latestFrames.end() && it->second.frame && it->second.frame->valid()) {
            result = it->second.frame;
            outDecodedSec = it->second.decodedSec;
        }
    }
    if (!result || std::abs(targetSec - outDecodedSec) > 0.035) {
        requestFrameAsync(filePathUtf8, type, targetSec);
    }
    return result;
}

void MediaEngine::applyPipeline(const FrameBGRA& src,
                                const ColorAdjust& adj,
                                const ChromaKeySettings& ck,
                                const EffectSettings& fx,
                                FrameBGRA& dst,
                                MaskType mask,
                                float cropX,
                                float cropY,
                                float maskFeather,
                                const FrameBGRA* opticalFlowPrev,
                                float maskOffX,
                                float maskOffY) {
    if (!src.valid()) return;

    const bool isColorIdentity =
        adj.preset == FilterPreset::None &&
        adj.lutPath.empty() &&
        std::abs(adj.brightness) < 0.005f &&
        std::abs(adj.contrast - 1.0f) < 0.005f &&
        std::abs(adj.exposure) < 0.005f &&
        std::abs(adj.saturation - 1.0f) < 0.005f &&
        std::abs(adj.vibrance) < 0.005f &&
        std::abs(adj.temperature) < 0.005f &&
        std::abs(adj.tint) < 0.005f &&
        std::abs(adj.highlights) < 0.005f &&
        std::abs(adj.shadows) < 0.005f &&
        std::abs(adj.whites) < 0.005f &&
        std::abs(adj.blacks) < 0.005f &&
        adj.clarity < 0.01f &&
        std::abs(adj.hslHue) < 0.5f &&
        adj.skinSmooth < 0.01f &&
        adj.teethWhiten < 0.01f &&
        !adj.redEyeRemove &&
        adj.curve == CurvePreset::Linear &&
        adj.sharpness < 0.01f &&
        std::abs(adj.hslRedSat) < 0.01f &&
        std::abs(adj.hslGreenSat) < 0.01f &&
        std::abs(adj.hslBlueSat) < 0.01f &&
        std::abs(adj.wheelShadow) < 0.01f &&
        std::abs(adj.wheelMid) < 0.01f &&
        std::abs(adj.wheelHigh) < 0.01f &&
        !adj.autoBgRemove &&
        adj.faceRetouch < 0.01f &&
        std::abs(adj.faceSlim) < 0.01f &&
        std::abs(adj.bodySlim) < 0.01f &&
        adj.enhanceHD < 0.01f &&
        adj.portraitBokeh < 0.01f &&
        adj.canvasExpand < 0.01f &&
        adj.aiRelight < 0.01f &&
        std::abs(adj.lensDistort) < 0.01f &&
        std::abs(adj.rollingShutter) < 0.01f &&
        std::abs(adj.curveR) < 0.005f &&
        std::abs(adj.curveG) < 0.005f &&
        std::abs(adj.curveB) < 0.005f &&
        std::abs(adj.hslSecShift) < 0.5f &&
        std::abs(adj.hslSecSat) < 0.01f &&
        adj.splitAmount < 0.01f &&
        !adj.falseColor &&
        adj.photoDenoise < 0.01f &&
        adj.gradientOverlay < 0.01f &&
        adj.rawFilmicTone < 0.01f &&
        std::abs(adj.curveShadowPt) < 0.005f &&
        std::abs(adj.curveHighPt) < 0.005f &&
        std::abs(adj.perspWarpH) < 0.005f &&
        std::abs(adj.perspWarpV) < 0.005f &&
        std::abs(adj.hslBandSat[0]) < 0.01f && std::abs(adj.hslBandSat[1]) < 0.01f &&
        std::abs(adj.hslBandSat[2]) < 0.01f && std::abs(adj.hslBandSat[3]) < 0.01f &&
        std::abs(adj.hslBandSat[4]) < 0.01f && std::abs(adj.hslBandSat[5]) < 0.01f &&
        std::abs(adj.hslBandSat[6]) < 0.01f && std::abs(adj.hslBandSat[7]) < 0.01f &&
        std::abs(adj.hslBandLum[0]) < 0.01f && std::abs(adj.hslBandLum[1]) < 0.01f &&
        std::abs(adj.hslBandLum[2]) < 0.01f && std::abs(adj.hslBandLum[3]) < 0.01f &&
        std::abs(adj.hslBandLum[4]) < 0.01f && std::abs(adj.hslBandLum[5]) < 0.01f &&
        std::abs(adj.hslBandLum[6]) < 0.01f && std::abs(adj.hslBandLum[7]) < 0.01f &&
        adj.brushDots.empty() &&
        opticalFlowPrev == nullptr &&
        adj.blur < 0.01f;

    const bool hasCrop = (cropX > 0.005f || cropY > 0.005f);
    if (isColorIdentity && !ck.enabled && fx.type == EffectType::None && mask == MaskType::None && !hasCrop) {
        dst = src;
        return;
    }

    // Optional cached .cube 3D LUT
    static std::mutex s_lutMtx;
    static std::unordered_map<std::string, Lut3D> s_lutCache;
    const Lut3D* activeLut = nullptr;
    if (!adj.lutPath.empty()) {
        std::lock_guard<std::mutex> lk(s_lutMtx);
        auto it = s_lutCache.find(adj.lutPath);
        if (it == s_lutCache.end()) {
            Lut3D loaded;
            loadCubeLutFile(adj.lutPath, loaded);
            auto [ins, _] = s_lutCache.emplace(adj.lutPath, std::move(loaded));
            it = ins;
        }
        if (it->second.valid()) activeLut = &it->second;
    }

    float br = adj.brightness * 255.0f;
    float ct = adj.contrast;
    float expMul = std::pow(2.0f, std::clamp(adj.exposure, -1.5f, 1.5f));
    float sa = adj.saturation;
    float vib = adj.vibrance;
    float temp = adj.temperature * 35.0f;
    float tint = adj.tint * 30.0f;
    float hi = adj.highlights * 55.0f;
    float sh = adj.shadows * 55.0f;
    float rTint = 1.0f, gTint = 1.0f, bTint = 1.0f;

    switch (adj.preset) {
        case FilterPreset::Cinematic:  ct *= 1.15f; sa *= 0.85f; br -= 8.0f; rTint = 1.04f; bTint = 0.96f; break;
        case FilterPreset::Vivid:      ct *= 1.12f; sa *= 1.40f; break;
        case FilterPreset::WarmSunset: br += 8.0f;  sa *= 1.15f; rTint = 1.12f; gTint = 1.02f; bTint = 0.88f; break;
        case FilterPreset::CoolCyber:  ct *= 1.18f; sa *= 1.25f; rTint = 0.90f; gTint = 1.04f; bTint = 1.16f; break;
        case FilterPreset::NoirBW:     ct *= 1.35f; sa = 0.0f;   break;
        case FilterPreset::Vintage:    ct *= 0.90f; sa *= 0.65f; br += 10.0f; rTint = 1.08f; gTint = 1.02f; bTint = 0.86f; break;
        case FilterPreset::TealOrange: ct *= 1.20f; sa *= 1.20f; rTint = 1.10f; gTint = 1.00f; bTint = 1.08f; break;
        case FilterPreset::MoodyDark:  ct *= 1.28f; sa *= 0.72f; br -= 16.0f; bTint = 1.06f; break;
        case FilterPreset::GoldenHour: ct *= 1.10f; sa *= 1.25f; br += 10.0f; rTint = 1.16f; gTint = 1.05f; bTint = 0.82f; break;
        case FilterPreset::PastelSoft: ct *= 0.88f; sa *= 0.82f; br += 16.0f; rTint = 1.05f; bTint = 1.04f; break;
        case FilterPreset::CyberNeon:  ct *= 1.25f; sa *= 1.55f; rTint = 1.08f; gTint = 0.94f; bTint = 1.22f; break;
        case FilterPreset::FreshLime:  ct *= 1.08f; sa *= 1.28f; br += 10.0f; rTint = 0.94f; gTint = 1.14f; bTint = 0.96f; break;
        case FilterPreset::VHS:        ct *= 1.12f; sa *= 0.85f; br += 6.0f;  rTint = 1.08f; gTint = 0.96f; bTint = 1.12f; break;
        case FilterPreset::Fog:        ct *= 0.78f; sa *= 0.72f; br += 26.0f; rTint = 0.96f; gTint = 1.00f; bTint = 1.06f; break;
        case FilterPreset::AnimeBW:    ct *= 1.55f; sa = 0.0f;   br += 4.0f;  break;
        case FilterPreset::Reindeer:   ct *= 1.10f; sa *= 1.22f; br += 8.0f;  rTint = 1.16f; gTint = 1.02f; bTint = 0.88f; break;
        case FilterPreset::Nebula:     ct *= 1.22f; sa *= 1.45f; br -= 6.0f;  rTint = 1.12f; gTint = 0.84f; bTint = 1.28f; break;
        case FilterPreset::Cartoon:    ct *= 1.18f; sa *= 1.35f; break;
        case FilterPreset::FoodFresh:  ct *= 1.12f; sa *= 1.38f; br += 8.0f;  rTint = 1.10f; gTint = 1.04f; bTint = 0.90f; break;
        case FilterPreset::NightGlow:  ct *= 1.30f; sa *= 1.25f; br -= 10.0f; rTint = 0.92f; gTint = 1.02f; bTint = 1.22f; break;
        case FilterPreset::PhotoBooth: ct *= 1.06f; sa *= 1.10f; br += 14.0f; rTint = 1.04f; gTint = 1.02f; bTint = 1.02f; break;
        case FilterPreset::LifeDay:    ct *= 1.05f; sa *= 1.12f; br += 6.0f;  rTint = 1.03f; gTint = 1.02f; bTint = 0.97f; break;
        default: break;
    }

    uint8_t bcLut[256];
    for (int i = 0; i < 256; ++i) {
        float x = static_cast<float>(i) / 255.0f;
        // Tone Curve preset
        if (adj.curve == CurvePreset::SCurve)       x = x * x * (3.0f - 2.0f * x);
        else if (adj.curve == CurvePreset::Matte)   x = 0.07f + x * 0.86f;
        else if (adj.curve == CurvePreset::HighKey) x = std::pow(x, 0.72f);
        else if (adj.curve == CurvePreset::FilmLog) x = std::log2(1.0f + x * 3.0f) * 0.5f;
        // Whites & Blacks lift/gain
        if (x > 0.75f) x += adj.whites * (x - 0.75f) * 0.8f;
        if (x < 0.25f) x += adj.blacks * (0.25f - x) * 0.8f;
        float v = std::clamp(x, 0.0f, 1.0f) * 255.0f * expMul;
        bcLut[i] = clampByte((v - 128.0f) * ct + 128.0f + br);
    }

    const int w = src.width;
    const int h = src.height;
    dst.width = w;
    dst.height = h;
    dst.pixels.resize(src.pixels.size());

    const float keyR = static_cast<float>((ck.keyColorRGB >> 16) & 0xFF) / 255.0f;
    const float keyG = static_cast<float>((ck.keyColorRGB >> 8)  & 0xFF) / 255.0f;
    const float keyB = static_cast<float>(ck.keyColorRGB & 0xFF) / 255.0f;
    const float sim  = std::clamp(ck.similarity, 0.05f, 0.70f);
    const float sm   = std::max(0.01f, ck.smoothness);
    const bool isGreenKey = (keyG >= keyR && keyG >= keyB);

    const uint8_t* sp = src.pixels.data();
    uint8_t* dp = dst.pixels.data();
    const size_t count = static_cast<size_t>(w) * h;

    // Precompute Batch 12 Split Toning RGB vectors & HSL Secondary flags
    auto hueToUnitRgb = [](float deg, float& oR, float& oG, float& oB) {
        float h = std::fmod(std::fmod(deg, 360.0f) + 360.0f, 360.0f) / 60.0f;
        float x = 1.0f - std::abs(std::fmod(h, 2.0f) - 1.0f);
        if      (h < 1.0f) { oR = 1.0f; oG = x;    oB = 0.0f; }
        else if (h < 2.0f) { oR = x;    oG = 1.0f; oB = 0.0f; }
        else if (h < 3.0f) { oR = 0.0f; oG = 1.0f; oB = x;    }
        else if (h < 4.0f) { oR = 0.0f; oG = x;    oB = 1.0f; }
        else if (h < 5.0f) { oR = x;    oG = 0.0f; oB = 1.0f; }
        else               { oR = 1.0f; oG = 0.0f; oB = x;    }
    };
    float splitShR = 0, splitShG = 0, splitShB = 0;
    float splitHiR = 0, splitHiG = 0, splitHiB = 0;
    const bool hasSplitToning = (adj.splitAmount > 0.01f);
    if (hasSplitToning) {
        hueToUnitRgb(adj.splitShadowHue, splitShR, splitShG, splitShB);
        hueToUnitRgb(adj.splitHighHue,   splitHiR, splitHiG, splitHiB);
    }
    const bool hasRgbCurves = (std::abs(adj.curveR) > 0.005f || std::abs(adj.curveG) > 0.005f || std::abs(adj.curveB) > 0.005f);
    const bool hasHslSec = (std::abs(adj.hslSecShift) > 0.5f || std::abs(adj.hslSecSat) > 0.01f);

    // Pass 1: Color Grading + Exposure + Highlights/Shadows + Vibrance + Tint + RGB Curves + HSL Sec + Split Toning + .cube 3D LUT + False Color + Chroma Key
    for (size_t i = 0; i < count; ++i) {
        float b = static_cast<float>(bcLut[sp[0]]);
        float g = static_cast<float>(bcLut[sp[1]]);
        float r = static_cast<float>(bcLut[sp[2]]);
        float a = static_cast<float>(sp[3]) / 255.0f;

        if (!isColorIdentity) {
            float luma = 0.299f * r + 0.587f * g + 0.114f * b;
            float luma01 = luma / 255.0f;
            if (std::abs(hi) > 0.1f && luma01 > 0.5f) {
                float wHi = (luma01 - 0.5f) * 2.0f;
                r += hi * wHi; g += hi * wHi; b += hi * wHi;
            }
            if (std::abs(sh) > 0.1f && luma01 < 0.5f) {
                float wSh = (0.5f - luma01) * 2.0f;
                r += sh * wSh; g += sh * wSh; b += sh * wSh;
            }
            float effSat = sa;
            if (std::abs(vib) > 0.01f) {
                float maxC = std::max({ r, g, b });
                float minC = std::min({ r, g, b });
                float curSat = (maxC > 1.0f) ? (maxC - minC) / maxC : 0.0f;
                effSat += vib * (1.0f - curSat) * 0.8f;
            }
            luma = 0.299f * r + 0.587f * g + 0.114f * b;
            r = (luma + (r - luma) * effSat + temp) * rTint;
            g = (luma + (g - luma) * effSat + tint) * gTint;
            b = (luma + (b - luma) * effSat - temp) * bTint;

            // 3-Way Color Wheels (Shadows / Midtones / Highlights Warm-Cool split toning)
            if (std::abs(adj.wheelShadow) > 0.01f && luma01 < 0.45f) {
                float wS = (0.45f - luma01) / 0.45f * adj.wheelShadow * 42.0f;
                r += wS; b -= wS;
            }
            if (std::abs(adj.wheelMid) > 0.01f) {
                float wM = (1.0f - std::abs(luma01 - 0.5f) * 2.0f) * adj.wheelMid * 36.0f;
                r += wM; b -= wM * 0.8f;
            }
            if (std::abs(adj.wheelHigh) > 0.01f && luma01 > 0.55f) {
                float wH = (luma01 - 0.55f) / 0.45f * adj.wheelHigh * 42.0f;
                r += wH; b -= wH;
            }
            // Full 3-Channel HSL Saturation (Red / Green / Blue selective saturation)
            if (std::abs(adj.hslRedSat) > 0.01f && r > g && r > b) {
                float wR = std::clamp((r - std::max(g, b)) / 128.0f, 0.0f, 1.0f) * adj.hslRedSat;
                r = luma + (r - luma) * (1.0f + wR);
            }
            if (std::abs(adj.hslGreenSat) > 0.01f && g > r && g > b) {
                float wG = std::clamp((g - std::max(r, b)) / 128.0f, 0.0f, 1.0f) * adj.hslGreenSat;
                g = luma + (g - luma) * (1.0f + wG);
            }
            if (std::abs(adj.hslBlueSat) > 0.01f && b > r && b > g) {
                float wB = std::clamp((b - std::max(r, g)) / 128.0f, 0.0f, 1.0f) * adj.hslBlueSat;
                b = luma + (b - luma) * (1.0f + wB);
            }
            // HSL Hue Shift (fast 3x3 Rodrigues rotation around luma axis)
            if (std::abs(adj.hslHue) > 0.5f) {
                float rad = adj.hslHue * 3.14159265f / 180.0f;
                float cH = std::cos(rad), sH = std::sin(rad);
                float nr = r * (0.299f + 0.701f * cH + 0.168f * sH) + g * (0.587f - 0.587f * cH + 0.330f * sH) + b * (0.114f - 0.114f * cH - 0.497f * sH);
                float ng = r * (0.299f - 0.299f * cH - 0.328f * sH) + g * (0.587f + 0.413f * cH + 0.035f * sH) + b * (0.114f - 0.114f * cH + 0.292f * sH);
                float nb = r * (0.299f - 0.300f * cH + 1.250f * sH) + g * (0.587f - 0.588f * cH - 1.050f * sH) + b * (0.114f + 0.886f * cH - 0.203f * sH);
                r = nr; g = ng; b = nb;
            }
            // Red Eye Removal
            if (adj.redEyeRemove && r > 110.0f && r > (g + b) * 0.95f) {
                float avgGB = (g + b) * 0.5f;
                r = avgGB * 1.15f;
            }
            // Teeth Whitening (bright warm low-sat boost)
            if (adj.teethWhiten > 0.02f && r > 155.0f && g > 145.0f && b > 110.0f && (r - b) < 65.0f) {
                float wAmt = adj.teethWhiten * 28.0f;
                r += wAmt * 0.6f; g += wAmt * 0.75f; b += wAmt * 1.25f;
            }
            if (activeLut) {
                int N = activeLut->size;
                int ri = std::clamp(static_cast<int>(r / 255.0f * (N - 1) + 0.5f), 0, N - 1);
                int gi = std::clamp(static_cast<int>(g / 255.0f * (N - 1) + 0.5f), 0, N - 1);
                int bi = std::clamp(static_cast<int>(b / 255.0f * (N - 1) + 0.5f), 0, N - 1);
                size_t lIdx = (static_cast<size_t>(bi) * N * N + static_cast<size_t>(gi) * N + ri) * 3;
                r = activeLut->data[lIdx + 0] * 255.0f;
                g = activeLut->data[lIdx + 1] * 255.0f;
                b = activeLut->data[lIdx + 2] * 255.0f;
            }
            // Batch 12: Per-Channel RGB Curves (smooth quadratic Bezier midpoint curve)
            if (hasRgbCurves) {
                float rn = std::clamp(r / 255.0f, 0.0f, 1.0f);
                float gn = std::clamp(g / 255.0f, 0.0f, 1.0f);
                float bn = std::clamp(b / 255.0f, 0.0f, 1.0f);
                r = std::clamp(rn + adj.curveR * 1.6f * rn * (1.0f - rn), 0.0f, 1.0f) * 255.0f;
                g = std::clamp(gn + adj.curveG * 1.6f * gn * (1.0f - gn), 0.0f, 1.0f) * 255.0f;
                b = std::clamp(bn + adj.curveB * 1.6f * bn * (1.0f - bn), 0.0f, 1.0f) * 255.0f;
            }
            // Batch 12: HSL Secondary Qualifier (isolate target hue band +-42 deg)
            if (hasHslSec) {
                float rn = std::clamp(r / 255.0f, 0.0f, 1.0f);
                float gn = std::clamp(g / 255.0f, 0.0f, 1.0f);
                float bn = std::clamp(b / 255.0f, 0.0f, 1.0f);
                float mx = std::max({ rn, gn, bn });
                float mn = std::min({ rn, gn, bn });
                float df = mx - mn;
                if (df > 0.05f) {
                    float hueDeg = 0.0f;
                    if (mx == rn)      hueDeg = 60.0f * std::fmod((gn - bn) / df + 6.0f, 6.0f);
                    else if (mx == gn) hueDeg = 60.0f * ((bn - rn) / df + 2.0f);
                    else               hueDeg = 60.0f * ((rn - gn) / df + 4.0f);
                    float dist = std::abs(hueDeg - adj.hslTargetHue);
                    if (dist > 180.0f) dist = 360.0f - dist;
                    if (dist < 42.0f) {
                        float w = (1.0f - dist / 42.0f) * std::clamp(df / std::max(0.01f, mx) * 2.5f, 0.0f, 1.0f);
                        float newH = std::fmod(hueDeg + adj.hslSecShift * w + 720.0f, 360.0f);
                        float sat = (mx > 0.001f) ? (df / mx) : 0.0f;
                        sat = std::clamp(sat * (1.0f + adj.hslSecSat * w), 0.0f, 1.0f);
                        float cVal = mx * sat;
                        float xVal = cVal * (1.0f - std::abs(std::fmod(newH / 60.0f, 2.0f) - 1.0f));
                        float mVal = mx - cVal;
                        float nr = 0, ng = 0, nb = 0;
                        if      (newH < 60.0f)  { nr = cVal; ng = xVal; nb = 0; }
                        else if (newH < 120.0f) { nr = xVal; ng = cVal; nb = 0; }
                        else if (newH < 180.0f) { nr = 0;    ng = cVal; nb = xVal; }
                        else if (newH < 240.0f) { nr = 0;    ng = xVal; nb = cVal; }
                        else if (newH < 300.0f) { nr = xVal; ng = 0;    nb = cVal; }
                        else                    { nr = cVal; ng = 0;    nb = xVal; }
                        r = (nr + mVal) * 255.0f;
                        g = (ng + mVal) * 255.0f;
                        b = (nb + mVal) * 255.0f;
                    }
                }
            }
            // Batch 12: Split Toning (tint shadows & highlights independently)
            if (hasSplitToning) {
                float lum01 = std::clamp((0.299f * r + 0.587f * g + 0.114f * b) / 255.0f, 0.0f, 1.0f);
                float shW = std::clamp((0.55f - lum01) / 0.55f, 0.0f, 1.0f) * adj.splitAmount * 0.42f;
                float hiW = std::clamp((lum01 - 0.45f) / 0.55f, 0.0f, 1.0f) * adj.splitAmount * 0.42f;
                r = r * (1.0f - shW) + (splitShR * 255.0f) * shW;
                g = g * (1.0f - shW) + (splitShG * 255.0f) * shW;
                b = b * (1.0f - shW) + (splitShB * 255.0f) * shW;
                r = r * (1.0f - hiW) + (splitHiR * 255.0f) * hiW;
                g = g * (1.0f - hiW) + (splitHiG * 255.0f) * hiW;
                b = b * (1.0f - hiW) + (splitHiB * 255.0f) * hiW;
            }
            // V32: Multi-Point Shadow & Highlight Tone Curve
            if (std::abs(adj.curveShadowPt) > 0.005f || std::abs(adj.curveHighPt) > 0.005f) {
                auto multiCurve = [&](float v) -> float {
                    float x = std::clamp(v / 255.0f, 0.0f, 1.0f);
                    float wSh = (1.0f - x) * (1.0f - x) * x * 4.0f;
                    float wHi = x * x * (1.0f - x) * 4.0f;
                    return std::clamp(x + adj.curveShadowPt * wSh + adj.curveHighPt * wHi, 0.0f, 1.0f) * 255.0f;
                };
                r = multiCurve(r); g = multiCurve(g); b = multiCurve(b);
            }
            // V32: Full 8-Band HSL (Red, Orange, Yellow, Green, Aqua, Blue, Purple, Magenta)
            {
                static constexpr float kBandCenters[8] = { 0.0f, 30.0f, 60.0f, 120.0f, 180.0f, 230.0f, 275.0f, 320.0f };
                float rn = std::clamp(r / 255.0f, 0.0f, 1.0f);
                float gn = std::clamp(g / 255.0f, 0.0f, 1.0f);
                float bn = std::clamp(b / 255.0f, 0.0f, 1.0f);
                float mx = std::max({ rn, gn, bn }), mn = std::min({ rn, gn, bn });
                float df = mx - mn;
                if (df > 0.04f) {
                    float hDeg = (mx == rn) ? 60.0f * std::fmod((gn - bn) / df + 6.0f, 6.0f)
                               : ((mx == gn) ? 60.0f * ((bn - rn) / df + 2.0f)
                                             : 60.0f * ((rn - gn) / df + 4.0f));
                    float satAdj = 0.0f, lumAdj = 0.0f;
                    for (int bi = 0; bi < 8; ++bi) {
                        if (std::abs(adj.hslBandSat[bi]) < 0.01f && std::abs(adj.hslBandLum[bi]) < 0.01f) continue;
                        float d = std::abs(hDeg - kBandCenters[bi]);
                        if (d > 180.0f) d = 360.0f - d;
                        if (d < 34.0f) {
                            float wB = 1.0f - d / 34.0f;
                            satAdj += adj.hslBandSat[bi] * wB;
                            lumAdj += adj.hslBandLum[bi] * wB;
                        }
                    }
                    if (std::abs(satAdj) > 0.005f || std::abs(lumAdj) > 0.005f) {
                        float lm = 0.299f * r + 0.587f * g + 0.114f * b;
                        r = std::clamp(lm + (r - lm) * (1.0f + satAdj) + lumAdj * 55.0f, 0.0f, 255.0f);
                        g = std::clamp(lm + (g - lm) * (1.0f + satAdj) + lumAdj * 55.0f, 0.0f, 255.0f);
                        b = std::clamp(lm + (b - lm) * (1.0f + satAdj) + lumAdj * 55.0f, 0.0f, 255.0f);
                    }
                }
            }
            // V31: RAW Engine ACES Filmic Highlight Roll-Off Tone Mapping
            if (adj.rawFilmicTone > 0.01f) {
                auto aces = [](float v) -> float {
                    float x = std::max(0.0f, v / 255.0f) * 1.15f;
                    float y = (x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f);
                    return std::clamp(y, 0.0f, 1.0f) * 255.0f;
                };
                float k = adj.rawFilmicTone;
                r = r * (1.0f - k) + aces(r) * k;
                g = g * (1.0f - k) + aces(g) * k;
                b = b * (1.0f - k) + aces(b) * k;
            }
            // Batch 12: Cinema IRE False Color Exposure Monitor
            if (adj.falseColor) {
                float ire = std::clamp((0.299f * r + 0.587f * g + 0.114f * b) / 255.0f * 100.0f, 0.0f, 100.0f);
                if      (ire < 5.0f)   { r = 138.0f; g = 43.0f;  b = 226.0f; } // Crushed Black: Purple
                else if (ire < 20.0f)  { r = 30.0f;  g = 110.0f; b = 255.0f; } // Deep Shadows: Blue
                else if (ire >= 42.0f && ire <= 48.0f) { r = 46.0f;  g = 215.0f; b = 85.0f;  } // 18% Middle Gray: Green
                else if (ire >= 52.0f && ire <= 58.0f) { r = 255.0f; g = 140.0f; b = 185.0f; } // Ideal Skin Tone: Pink
                else if (ire >= 70.0f && ire <= 85.0f) { r = 250.0f; g = 225.0f; b = 45.0f;  } // Bright Highlights: Yellow
                else if (ire >= 95.0f) { r = 255.0f; g = 32.0f;  b = 32.0f;  } // Clipped Whites: Red
                else { float gy = ire * 2.55f; r = gy; g = gy; b = gy; }       // Neutral Grayscale
            }
        }

        if (ck.enabled) {
            float rn = r / 255.0f, gn = g / 255.0f, bn = b / 255.0f;
            float dr = rn - keyR, dg = gn - keyG, db = bn - keyB;
            float dist = std::sqrt(dr * dr + dg * dg + db * db);
            float m = std::clamp((dist - sim) / sm, 0.0f, 1.0f);
            a *= m;
            if (ck.spillSuppress > 0.01f && a > 0.01f) {
                if (isGreenKey) {
                    float maxRB = std::max(r, b);
                    if (g > maxRB) g = g - (g - maxRB) * ck.spillSuppress;
                } else {
                    float maxRG = std::max(r, g);
                    if (b > maxRG) b = b - (b - maxRG) * ck.spillSuppress;
                }
            }
            r *= a; g *= a; b *= a;
        }

        dp[0] = clampByte(b);
        dp[1] = clampByte(g);
        dp[2] = clampByte(r);
        dp[3] = clampByte(a * 255.0f);
        sp += 4;
        dp += 4;
    }

    // V31: Real Block-Matching Optical Flow (16x16 SAD Motion Vectors + Motion-Compensated Frame Synthesis)
    if (opticalFlowPrev && opticalFlowPrev->valid() &&
        opticalFlowPrev->width == w && opticalFlowPrev->height == h) {
        std::vector<uint8_t> curSnap = dst.pixels;
        const uint8_t* prevP = opticalFlowPrev->pixels.data();
        const int blk = 16;
        const int searchR = 6;
        for (int by = 0; by < h; by += blk) {
            for (int bx = 0; bx < w; bx += blk) {
                int bestDx = 0, bestDy = 0;
                int bestSad = 0x7FFFFFFF;
                for (int dy = -searchR; dy <= searchR; dy += 2) {
                    for (int dx = -searchR; dx <= searchR; dx += 2) {
                        int sad = 0;
                        for (int oy = 0; oy < blk && by + oy < h; oy += 4) {
                            int py = std::clamp(by + oy + dy, 0, h - 1);
                            const uint8_t* cRow = curSnap.data() + static_cast<size_t>(by + oy) * w * 4;
                            const uint8_t* pRow = prevP + static_cast<size_t>(py) * w * 4;
                            for (int ox = 0; ox < blk && bx + ox < w; ox += 4) {
                                int px = std::clamp(bx + ox + dx, 0, w - 1);
                                sad += std::abs(int(cRow[(bx + ox) * 4 + 1]) - int(pRow[px * 4 + 1]));
                            }
                        }
                        if (sad < bestSad) { bestSad = sad; bestDx = dx; bestDy = dy; }
                    }
                }
                int hdx = bestDx / 2, hdy = bestDy / 2;
                for (int oy = 0; oy < blk && by + oy < h; ++oy) {
                    int y = by + oy;
                    int py = std::clamp(y + hdy, 0, h - 1);
                    uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
                    const uint8_t* cRow = curSnap.data() + static_cast<size_t>(y) * w * 4;
                    const uint8_t* pRow = prevP + static_cast<size_t>(py) * w * 4;
                    for (int ox = 0; ox < blk && bx + ox < w; ++ox) {
                        int x = bx + ox;
                        int px = std::clamp(x + hdx, 0, w - 1);
                        dRow[x * 4 + 0] = static_cast<uint8_t>((cRow[x * 4 + 0] * 3 + pRow[px * 4 + 0]) >> 2);
                        dRow[x * 4 + 1] = static_cast<uint8_t>((cRow[x * 4 + 1] * 3 + pRow[px * 4 + 1]) >> 2);
                        dRow[x * 4 + 2] = static_cast<uint8_t>((cRow[x * 4 + 2] * 3 + pRow[px * 4 + 2]) >> 2);
                    }
                }
            }
        }
    }

    // Pass 1.5: True 2-Pass Separable Blur + Edge-Preserving Skin Smooth + Sharpness/Clarity
    if (adj.blur > 0.02f) {
        int rad = std::max(1, static_cast<int>(adj.blur * 7.0f));
        std::vector<uint8_t> tmp(dst.pixels.size());
        // Horizontal pass
        for (int y = 0; y < h; ++y) {
            const uint8_t* sRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            uint8_t* dRow = tmp.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                int x0 = std::max(0, x - rad), x1 = std::min(w - 1, x + rad);
                int cnt = x1 - x0 + 1;
                int sb = 0, sg = 0, sr = 0;
                for (int k = x0; k <= x1; ++k) { sb += sRow[k * 4]; sg += sRow[k * 4 + 1]; sr += sRow[k * 4 + 2]; }
                dRow[x * 4] = static_cast<uint8_t>(sb / cnt);
                dRow[x * 4 + 1] = static_cast<uint8_t>(sg / cnt);
                dRow[x * 4 + 2] = static_cast<uint8_t>(sr / cnt);
                dRow[x * 4 + 3] = sRow[x * 4 + 3];
            }
        }
        // Vertical pass (true creamy 2D blur!)
        for (int y = 0; y < h; ++y) {
            int y0 = std::max(0, y - rad), y1 = std::min(h - 1, y + rad);
            int cnt = y1 - y0 + 1;
            uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                int sb = 0, sg = 0, sr = 0;
                for (int ky = y0; ky <= y1; ++ky) {
                    const uint8_t* p = tmp.data() + (static_cast<size_t>(ky) * w + x) * 4;
                    sb += p[0]; sg += p[1]; sr += p[2];
                }
                dRow[x * 4] = static_cast<uint8_t>(sb / cnt);
                dRow[x * 4 + 1] = static_cast<uint8_t>(sg / cnt);
                dRow[x * 4 + 2] = static_cast<uint8_t>(sr / cnt);
            }
        }
    } else if (adj.skinSmooth > 0.02f) {
        // Real edge-preserving skin-tone smoother (only smooths warm skin hues, preserves eyes/edges!)
        float blend = std::clamp(adj.skinSmooth, 0.0f, 1.0f) * 0.75f;
        std::vector<uint8_t> copy = dst.pixels;
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                size_t idx = (static_cast<size_t>(y) * w + x) * 4;
                int b = copy[idx], g = copy[idx + 1], r = copy[idx + 2];
                if (r > 85 && g > 40 && b > 20 && r > g && r > b && (r - b) > 15) {
                    int sb = 0, sg = 0, sr = 0, cnt = 0;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            size_t nIdx = (static_cast<size_t>(y + dy) * w + (x + dx)) * 4;
                            if (std::abs(copy[nIdx + 2] - r) < 28) {
                                sb += copy[nIdx]; sg += copy[nIdx + 1]; sr += copy[nIdx + 2]; ++cnt;
                            }
                        }
                    }
                    if (cnt > 0) {
                        dst.pixels[idx + 0] = static_cast<uint8_t>(b + (sb / cnt - b) * blend);
                        dst.pixels[idx + 1] = static_cast<uint8_t>(g + (sg / cnt - g) * blend);
                        dst.pixels[idx + 2] = static_cast<uint8_t>(r + (sr / cnt - r) * blend);
                    }
                }
            }
        }
    } else if (adj.sharpness > 0.02f || adj.clarity > 0.02f || adj.enhanceHD > 0.02f) {
        float amt = adj.sharpness * 1.4f + adj.clarity * 0.9f + adj.enhanceHD * 1.6f;
        std::vector<uint8_t> tmp = dst.pixels;
        for (int y = 1; y < h - 1; ++y) {
            const uint8_t* rUp = tmp.data() + static_cast<size_t>(y - 1) * w * 4;
            const uint8_t* rMd = tmp.data() + static_cast<size_t>(y)     * w * 4;
            const uint8_t* rDn = tmp.data() + static_cast<size_t>(y + 1) * w * 4;
            uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 1; x < w - 1; ++x) {
                for (int c = 0; c < 3; ++c) {
                    int center = rMd[x * 4 + c];
                    int avg = (rUp[x * 4 + c] + rDn[x * 4 + c] + rMd[(x - 1) * 4 + c] + rMd[(x + 1) * 4 + c]) >> 2;
                    dRow[x * 4 + c] = clampByteInt(center + static_cast<int>((center - avg) * amt));
                }
            }
        }
    }

    // Pass 2: Visual Effects
    const float inten = std::clamp(fx.intensity, 0.0f, 1.0f);
    if (fx.type == EffectType::Vignette && inten > 0.01f) {
        const float cx = w * 0.5f, cy = h * 0.5f;
        const float invR2 = 1.0f / (cx * cx + cy * cy);
        for (int y = 0; y < h; ++y) {
            float dy2 = (y - cy) * (y - cy);
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                float d2 = ((x - cx) * (x - cx) + dy2) * invR2;
                float vig = std::clamp(1.0f - d2 * (0.85f * inten), 0.15f, 1.0f);
                row[x * 4 + 0] = static_cast<uint8_t>(row[x * 4 + 0] * vig);
                row[x * 4 + 1] = static_cast<uint8_t>(row[x * 4 + 1] * vig);
                row[x * 4 + 2] = static_cast<uint8_t>(row[x * 4 + 2] * vig);
            }
        }
    } else if (fx.type == EffectType::GlitchRGB && inten > 0.01f) {
        int shift = std::max(2, static_cast<int>(inten * 14.0f));
        std::vector<uint8_t> copy = dst.pixels;
        for (int y = 0; y < h; ++y) {
            const uint8_t* sRow = copy.data() + static_cast<size_t>(y) * w * 4;
            uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                int rx = std::min(w - 1, x + shift);
                int bx = std::max(0, x - shift);
                dRow[x * 4 + 2] = sRow[rx * 4 + 2];
                dRow[x * 4 + 0] = sRow[bx * 4 + 0];
            }
        }
    } else if (fx.type == EffectType::FilmGrain && inten > 0.01f) {
        uint32_t seed = 0x12345678u;
        int amp = static_cast<int>(inten * 38.0f);
        for (size_t i = 0; i < count; ++i) {
            seed = seed * 1664525u + 1013904223u;
            int n = (static_cast<int>((seed >> 24) & 0xFF) - 128) * amp / 128;
            dst.pixels[i * 4 + 0] = clampByteInt(dst.pixels[i * 4 + 0] + n);
            dst.pixels[i * 4 + 1] = clampByteInt(dst.pixels[i * 4 + 1] + n);
            dst.pixels[i * 4 + 2] = clampByteInt(dst.pixels[i * 4 + 2] + n);
        }
    } else if (fx.type == EffectType::SoftGlow && inten > 0.01f) {
        std::vector<uint8_t> copy = dst.pixels;
        for (int y = 0; y < h; ++y) {
            const uint8_t* sRow = copy.data() + static_cast<size_t>(y) * w * 4;
            uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 2; x < w - 2; ++x) {
                for (int c = 0; c < 3; ++c) {
                    int avg = (sRow[(x - 2) * 4 + c] + sRow[x * 4 + c] * 2 + sRow[(x + 2) * 4 + c]) >> 2;
                    int boost = (avg > 150) ? static_cast<int>((avg - 150) * inten * 0.8f) : 0;
                    dRow[x * 4 + c] = clampByteInt(sRow[x * 4 + c] + boost);
                }
            }
        }
    } else if (fx.type == EffectType::MotionBlur && inten > 0.01f) {
        int rad = std::max(2, static_cast<int>(inten * 10.0f));
        std::vector<uint8_t> copy = dst.pixels;
        for (int y = 0; y < h; ++y) {
            const uint8_t* sRow = copy.data() + static_cast<size_t>(y) * w * 4;
            uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                int x0 = std::max(0, x - rad), x1 = std::min(w - 1, x + rad);
                int cnt = x1 - x0 + 1;
                int sb = 0, sg = 0, sr = 0;
                for (int k = x0; k <= x1; ++k) {
                    sb += sRow[k * 4 + 0]; sg += sRow[k * 4 + 1]; sr += sRow[k * 4 + 2];
                }
                dRow[x * 4 + 0] = static_cast<uint8_t>(sb / cnt);
                dRow[x * 4 + 1] = static_cast<uint8_t>(sg / cnt);
                dRow[x * 4 + 2] = static_cast<uint8_t>(sr / cnt);
            }
        }
    } else if (fx.type == EffectType::RetroCRT && inten > 0.01f) {
        float dark = 1.0f - 0.45f * inten;
        for (int y = 0; y < h; y += 2) {
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                row[x * 4 + 0] = static_cast<uint8_t>(row[x * 4 + 0] * dark);
                row[x * 4 + 1] = static_cast<uint8_t>(row[x * 4 + 1] * dark);
                row[x * 4 + 2] = static_cast<uint8_t>(row[x * 4 + 2] * dark);
            }
        }
    } else if (fx.type == EffectType::Sparkles && inten > 0.01f) {
        uint32_t seed = 0x77771234u;
        int stars = std::max(12, static_cast<int>(inten * 85));
        for (int s = 0; s < stars; ++s) {
            seed = seed * 1664525u + 1013904223u;
            int sx = 4 + static_cast<int>((seed >> 16) % static_cast<uint32_t>(std::max(1, w - 8)));
            seed = seed * 1664525u + 1013904223u;
            int sy = 4 + static_cast<int>((seed >> 16) % static_cast<uint32_t>(std::max(1, h - 8)));
            for (int d = -3; d <= 3; ++d) {
                uint8_t* p1 = dst.pixels.data() + (static_cast<size_t>(sy) * w + (sx + d)) * 4;
                uint8_t* p2 = dst.pixels.data() + (static_cast<size_t>(sy + d) * w + sx) * 4;
                p1[0] = 220; p1[1] = 245; p1[2] = 255;
                p2[0] = 220; p2[1] = 245; p2[2] = 255;
            }
        }
    } else if (fx.type == EffectType::LoveGlow && inten > 0.01f) {
        const float cx = w * 0.5f, cy = h * 0.5f, invR2 = 1.0f / (cx * cx + cy * cy);
        for (int y = 0; y < h; ++y) {
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                float d2 = ((x - cx) * (x - cx) + (y - cy) * (y - cy)) * invR2;
                float rose = std::clamp(d2 * inten * 0.65f, 0.0f, 0.65f);
                row[x * 4 + 2] = clampByte(row[x * 4 + 2] + rose * 95.0f);
                row[x * 4 + 0] = clampByte(row[x * 4 + 0] + rose * 65.0f);
            }
        }
    } else if (fx.type == EffectType::LensFlare && inten > 0.01f) {
        int fy = h / 4;
        for (int y = std::max(0, fy - 24); y < std::min(h, fy + 24); ++y) {
            float vy = 1.0f - std::abs(y - fy) / 24.0f;
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                float vx = 1.0f - std::abs(x - w * 0.35f) / (w * 0.65f);
                float fl = std::max(0.0f, vx * vy * inten);
                row[x * 4 + 2] = clampByte(row[x * 4 + 2] + fl * 120.0f);
                row[x * 4 + 1] = clampByte(row[x * 4 + 1] + fl * 95.0f);
                row[x * 4 + 0] = clampByte(row[x * 4 + 0] + fl * 60.0f);
            }
        }
    } else if (fx.type == EffectType::WaveDistort && inten > 0.01f) {
        std::vector<uint8_t> copy = dst.pixels;
        float amp = inten * 14.0f;
        for (int y = 0; y < h; ++y) {
            int dx = static_cast<int>(std::sin(y * 0.08f) * amp);
            const uint8_t* sRow = copy.data() + static_cast<size_t>(y) * w * 4;
            uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                int sx = std::clamp(x + dx, 0, w - 1);
                std::memcpy(dRow + x * 4, sRow + sx * 4, 4);
            }
        }
    } else if (fx.type == EffectType::ZoomBlur && inten > 0.01f) {
        std::vector<uint8_t> copy = dst.pixels;
        float cx = w * 0.5f, cy = h * 0.5f, step = inten * 0.045f;
        for (int y = 0; y < h; ++y) {
            uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                int sb = 0, sg = 0, sr = 0;
                for (int k = 0; k < 4; ++k) {
                    float f = 1.0f - k * step;
                    int sx = std::clamp(static_cast<int>(cx + (x - cx) * f), 0, w - 1);
                    int sy = std::clamp(static_cast<int>(cy + (y - cy) * f), 0, h - 1);
                    const uint8_t* p = copy.data() + (static_cast<size_t>(sy) * w + sx) * 4;
                    sb += p[0]; sg += p[1]; sr += p[2];
                }
                dRow[x * 4 + 0] = static_cast<uint8_t>(sb >> 2);
                dRow[x * 4 + 1] = static_cast<uint8_t>(sg >> 2);
                dRow[x * 4 + 2] = static_cast<uint8_t>(sr >> 2);
            }
        }
    } else if (fx.type == EffectType::Anaglyph3D && inten > 0.01f) {
        int shift = std::max(3, static_cast<int>(inten * 12.0f));
        std::vector<uint8_t> copy = dst.pixels;
        for (int y = 0; y < h; ++y) {
            const uint8_t* sRow = copy.data() + static_cast<size_t>(y) * w * 4;
            uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                int rx = std::min(w - 1, x + shift), cx2 = std::max(0, x - shift);
                dRow[x * 4 + 2] = sRow[rx * 4 + 2];
                dRow[x * 4 + 1] = sRow[cx2 * 4 + 1];
                dRow[x * 4 + 0] = sRow[cx2 * 4 + 0];
            }
        }
    } else if (fx.type == EffectType::RainDrop && inten > 0.01f) {
        uint32_t seed = 0x99887766u;
        int drops = std::max(30, static_cast<int>(inten * 180));
        for (int s = 0; s < drops; ++s) {
            seed = seed * 1664525u + 1013904223u;
            int sx = 2 + static_cast<int>((seed >> 16) % static_cast<uint32_t>(std::max(1, w - 12)));
            seed = seed * 1664525u + 1013904223u;
            int sy = 2 + static_cast<int>((seed >> 16) % static_cast<uint32_t>(std::max(1, h - 18)));
            for (int d = 0; d < 12; ++d) {
                uint8_t* p = dst.pixels.data() + (static_cast<size_t>(sy + d) * w + (sx + d / 3)) * 4;
                p[0] = clampByte(p[0] + 90.0f); p[1] = clampByte(p[1] + 85.0f); p[2] = clampByte(p[2] + 75.0f);
            }
        }
    } else if (fx.type == EffectType::NeonEdge && inten > 0.01f) {
        std::vector<uint8_t> copy = dst.pixels;
        for (int y = 1; y < h - 1; ++y) {
            const uint8_t* rUp = copy.data() + static_cast<size_t>(y - 1) * w * 4;
            const uint8_t* rMd = copy.data() + static_cast<size_t>(y) * w * 4;
            const uint8_t* rDn = copy.data() + static_cast<size_t>(y + 1) * w * 4;
            uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 1; x < w - 1; ++x) {
                int e = std::abs(static_cast<int>(rMd[(x + 1) * 4 + 1]) - static_cast<int>(rMd[(x - 1) * 4 + 1])) +
                        std::abs(static_cast<int>(rDn[x * 4 + 1]) - static_cast<int>(rUp[x * 4 + 1]));
                float glow = std::clamp(e / 90.0f * inten, 0.0f, 1.0f);
                dRow[x * 4 + 0] = clampByte(rMd[x * 4 + 0] * 0.35f + glow * 235.0f);
                dRow[x * 4 + 1] = clampByte(rMd[x * 4 + 1] * 0.35f + glow * 230.0f);
                dRow[x * 4 + 2] = clampByte(rMd[x * 4 + 2] * 0.35f + glow * 140.0f);
            }
        }
    } else if (fx.type == EffectType::HardShadow && inten > 0.01f) {
        const float cx = w * 0.5f, cy = h * 0.45f, invR2 = 1.0f / (cx * cx + cy * cy);
        for (int y = 0; y < h; ++y) {
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                float d2 = ((x - cx) * (x - cx) + (y - cy) * (y - cy)) * invR2;
                float sh = std::clamp(1.0f - d2 * (1.25f * inten), 0.05f, 1.0f);
                row[x * 4 + 0] = static_cast<uint8_t>(row[x * 4 + 0] * sh);
                row[x * 4 + 1] = static_cast<uint8_t>(row[x * 4 + 1] * sh);
                row[x * 4 + 2] = static_cast<uint8_t>(row[x * 4 + 2] * sh);
            }
        }
    } else if (fx.type == EffectType::VHSNoise && inten > 0.01f) {
        uint32_t seed = 0x55AA1234u;
        for (int y = 0; y < h; ++y) {
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            bool bar = (y % 47 == 0 || y % 47 == 1);
            for (int x = 0; x < w; ++x) {
                if (bar) {
                    seed = seed * 1664525u + 1013904223u;
                    uint8_t n = static_cast<uint8_t>((seed >> 24) & 0xFF);
                    row[x * 4 + 0] = n; row[x * 4 + 1] = n; row[x * 4 + 2] = n;
                }
            }
        }
    } else if (fx.type == EffectType::LightLeaks && inten > 0.01f) {
        for (int y = 0; y < h; ++y) {
            float ny = static_cast<float>(y) / std::max(1, h);
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                float nx = static_cast<float>(x) / std::max(1, w);
                float g1 = std::exp(-((nx - 0.14f) * (nx - 0.14f) * 7.5f + (ny - 0.35f) * (ny - 0.35f) * 3.0f));
                float g2 = std::exp(-((nx - 0.88f) * (nx - 0.88f) * 8.0f + (ny - 0.65f) * (ny - 0.65f) * 3.5f));
                float rAdd = (g1 * 210.0f + g2 * 180.0f) * inten;
                float gAdd = (g1 * 115.0f + g2 * 55.0f) * inten;
                float bAdd = (g1 * 25.0f  + g2 * 160.0f) * inten;
                row[x * 4 + 0] = static_cast<uint8_t>(std::clamp(row[x * 4 + 0] + bAdd, 0.0f, 255.0f));
                row[x * 4 + 1] = static_cast<uint8_t>(std::clamp(row[x * 4 + 1] + gAdd, 0.0f, 255.0f));
                row[x * 4 + 2] = static_cast<uint8_t>(std::clamp(row[x * 4 + 2] + rAdd, 0.0f, 255.0f));
            }
        }
    } else if ((fx.type == EffectType::Smoke || fx.type == EffectType::Fire || fx.type == EffectType::FractalNoise) && inten > 0.01f) {
        auto hash2 = [](int ix, int iy) -> float {
            uint32_t n = static_cast<uint32_t>(ix * 374761393 + iy * 668265263);
            n = (n ^ (n >> 13)) * 1274126177u;
            return static_cast<float>((n ^ (n >> 16)) & 0xFFFFu) / 65535.0f;
        };
        auto vnoise = [&](float fx2, float fy2) -> float {
            int ix = static_cast<int>(std::floor(fx2));
            int iy = static_cast<int>(std::floor(fy2));
            float tx = fx2 - ix, ty = fy2 - iy;
            tx = tx * tx * (3.0f - 2.0f * tx);
            ty = ty * ty * (3.0f - 2.0f * ty);
            float a = hash2(ix, iy), b = hash2(ix + 1, iy);
            float c = hash2(ix, iy + 1), d = hash2(ix + 1, iy + 1);
            return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
        };
        for (int y = 0; y < h; ++y) {
            float ny = static_cast<float>(y) / std::max(1, h);
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                float nx = static_cast<float>(x) / std::max(1, w);
                float fbm = 0.55f * vnoise(nx * 5.0f, ny * 5.0f)
                          + 0.30f * vnoise(nx * 11.0f + 7.1f, ny * 11.0f + 3.4f)
                          + 0.15f * vnoise(nx * 23.0f + 2.2f, ny * 23.0f + 9.7f);
                if (fx.type == EffectType::Smoke) {
                    float wSmoke = std::clamp((fbm - 0.32f) * 1.45f * (0.35f + 0.65f * ny), 0.0f, 1.0f) * inten * 0.75f;
                    row[x * 4 + 0] = static_cast<uint8_t>(row[x * 4 + 0] * (1.0f - wSmoke) + 215.0f * wSmoke);
                    row[x * 4 + 1] = static_cast<uint8_t>(row[x * 4 + 1] * (1.0f - wSmoke) + 218.0f * wSmoke);
                    row[x * 4 + 2] = static_cast<uint8_t>(row[x * 4 + 2] * (1.0f - wSmoke) + 225.0f * wSmoke);
                } else if (fx.type == EffectType::Fire) {
                    float flame = std::clamp((ny * 1.15f + fbm * 0.72f - 0.85f) * 1.9f, 0.0f, 1.0f) * inten;
                    if (flame > 0.02f) {
                        float rF = std::clamp(flame * 320.0f, 0.0f, 255.0f);
                        float gF = std::clamp((flame - 0.25f) * 290.0f, 0.0f, 240.0f);
                        float bF = std::clamp((flame - 0.62f) * 240.0f, 0.0f, 160.0f);
                        row[x * 4 + 0] = static_cast<uint8_t>(std::clamp(row[x * 4 + 0] + bF, 0.0f, 255.0f));
                        row[x * 4 + 1] = static_cast<uint8_t>(std::clamp(row[x * 4 + 1] + gF, 0.0f, 255.0f));
                        row[x * 4 + 2] = static_cast<uint8_t>(std::clamp(row[x * 4 + 2] + rF, 0.0f, 255.0f));
                    }
                } else {
                    // Fractal Noise procedural overlay
                    float nv = fbm * 255.0f;
                    float a = inten * 0.65f;
                    row[x * 4 + 0] = static_cast<uint8_t>(row[x * 4 + 0] * (1.0f - a) + nv * a);
                    row[x * 4 + 1] = static_cast<uint8_t>(row[x * 4 + 1] * (1.0f - a) + nv * a);
                    row[x * 4 + 2] = static_cast<uint8_t>(row[x * 4 + 2] * (1.0f - a) + nv * a);
                }
            }
        }
    } else if ((fx.type == EffectType::Snow || fx.type == EffectType::Particles) && inten > 0.01f) {
        int numP = static_cast<int>(65 + inten * 140.0f);
        uint32_t s = (fx.type == EffectType::Snow) ? 0xC0FFEE11u : 0xFACEB00Cu;
        for (int p = 0; p < numP; ++p) {
            s = s * 1664525u + 1013904223u; int cx = static_cast<int>((s >> 16) % static_cast<uint32_t>(std::max(1, w)));
            s = s * 1664525u + 1013904223u; int cy = static_cast<int>((s >> 16) % static_cast<uint32_t>(std::max(1, h)));
            s = s * 1664525u + 1013904223u; int rad = 2 + static_cast<int>((s >> 24) % 6);
            float pR = 255.0f, pG = 255.0f, pB = 255.0f;
            if (fx.type == EffectType::Particles) {
                int tone = p % 3;
                if (tone == 0) { pR = 80.0f; pG = 240.0f; pB = 255.0f; }
                else if (tone == 1) { pR = 255.0f; pG = 210.0f; pB = 75.0f; }
                else { pR = 255.0f; pG = 95.0f; pB = 215.0f; }
            }
            for (int dy = -rad; dy <= rad; ++dy) {
                int yy = cy + dy; if (yy < 0 || yy >= h) continue;
                uint8_t* row = dst.pixels.data() + static_cast<size_t>(yy) * w * 4;
                for (int dx = -rad; dx <= rad; ++dx) {
                    int xx = cx + dx; if (xx < 0 || xx >= w) continue;
                    float d2 = static_cast<float>(dx * dx + dy * dy) / static_cast<float>(rad * rad + 1);
                    if (d2 > 1.0f) continue;
                    float fall = (1.0f - d2) * inten * 0.85f;
                    row[xx * 4 + 0] = static_cast<uint8_t>(std::clamp(row[xx * 4 + 0] + pB * fall, 0.0f, 255.0f));
                    row[xx * 4 + 1] = static_cast<uint8_t>(std::clamp(row[xx * 4 + 1] + pG * fall, 0.0f, 255.0f));
                    row[xx * 4 + 2] = static_cast<uint8_t>(std::clamp(row[xx * 4 + 2] + pR * fall, 0.0f, 255.0f));
                }
            }
        }
    } else if ((fx.type == EffectType::Liquid || fx.type == EffectType::Displacement) && inten > 0.01f) {
        std::vector<uint8_t> snap = dst.pixels;
        float amp = (fx.type == EffectType::Liquid ? 14.0f : 22.0f) * inten;
        for (int y = 0; y < h; ++y) {
            float ny = static_cast<float>(y) / std::max(1, h);
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                float nx = static_cast<float>(x) / std::max(1, w);
                float dx = (fx.type == EffectType::Liquid)
                    ? std::sin(ny * 26.0f + nx * 6.0f) * amp
                    : (std::sin(ny * 19.0f) * std::cos(nx * 15.0f)) * amp;
                float dy = (fx.type == EffectType::Liquid)
                    ? std::cos(nx * 24.0f + ny * 5.0f) * amp * 0.7f
                    : (std::cos(ny * 17.0f) * std::sin(nx * 21.0f)) * amp;
                int sx = std::clamp(x + static_cast<int>(dx), 0, w - 1);
                int sy = std::clamp(y + static_cast<int>(dy), 0, h - 1);
                const uint8_t* sp2 = snap.data() + (static_cast<size_t>(sy) * w + sx) * 4;
                row[x * 4 + 0] = sp2[0]; row[x * 4 + 1] = sp2[1]; row[x * 4 + 2] = sp2[2]; row[x * 4 + 3] = sp2[3];
            }
        }
    } else if (fx.type == EffectType::ChromaticAb && inten > 0.01f) {
        std::vector<uint8_t> snap = dst.pixels;
        float cx = w * 0.5f, cy = h * 0.5f;
        float strength = inten * 0.032f;
        for (int y = 0; y < h; ++y) {
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                float rx = (x - cx) * strength;
                float ry = (y - cy) * strength;
                int rX = std::clamp(static_cast<int>(x + rx), 0, w - 1);
                int rY = std::clamp(static_cast<int>(y + ry), 0, h - 1);
                int bX = std::clamp(static_cast<int>(x - rx), 0, w - 1);
                int bY = std::clamp(static_cast<int>(y - ry), 0, h - 1);
                row[x * 4 + 0] = snap[(static_cast<size_t>(bY) * w + bX) * 4 + 0];
                row[x * 4 + 2] = snap[(static_cast<size_t>(rY) * w + rX) * 4 + 2];
            }
        }
    } else if (fx.type == EffectType::DepthMap && inten > 0.01f) {
        std::vector<uint8_t> snap = dst.pixels;
        for (int y = 1; y < h - 1; ++y) {
            float ny = static_cast<float>(y) / std::max(1, h);
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 1; x < w - 1; ++x) {
                auto lum = [&](int px, int py) -> float {
                    const uint8_t* q = snap.data() + (static_cast<size_t>(py) * w + px) * 4;
                    return (0.299f * q[2] + 0.587f * q[1] + 0.114f * q[0]) / 255.0f;
                };
                float gx = std::abs(lum(x + 1, y) - lum(x - 1, y));
                float gy = std::abs(lum(x, y + 1) - lum(x, y - 1));
                float focus = std::clamp((gx + gy) * 2.2f, 0.0f, 1.0f);
                float depth01 = std::clamp(ny * 0.65f + focus * 0.35f, 0.0f, 1.0f);
                uint8_t dv = static_cast<uint8_t>(depth01 * 255.0f);
                row[x * 4 + 0] = static_cast<uint8_t>(row[x * 4 + 0] * (1.0f - inten) + dv * inten);
                row[x * 4 + 1] = static_cast<uint8_t>(row[x * 4 + 1] * (1.0f - inten) + dv * inten);
                row[x * 4 + 2] = static_cast<uint8_t>(row[x * 4 + 2] * (1.0f - inten) + dv * inten);
            }
        }
    } else if (fx.type == EffectType::Object3D && inten > 0.01f) {
        // Real-time 3D perspective-projected wireframe Cube (rotation controlled by FX intensity)
        float angY = inten * 3.14159265f * 1.5f + 0.45f;
        float angX = 0.52f;
        float cy = std::cos(angY), sy = std::sin(angY);
        float cx = std::cos(angX), sx = std::sin(angX);
        const float verts[8][3] = {
            {-1,-1,-1},{ 1,-1,-1},{ 1, 1,-1},{-1, 1,-1},
            {-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{-1, 1, 1}
        };
        const int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
        };
        float px[8], py[8];
        float scale = std::min(w, h) * 0.24f;
        for (int i = 0; i < 8; ++i) {
            float x1 = verts[i][0] * cy - verts[i][2] * sy;
            float z1 = verts[i][0] * sy + verts[i][2] * cy;
            float y2 = verts[i][1] * cx - z1 * sx;
            float z2 = verts[i][1] * sx + z1 * cx + 4.2f;
            float persp = 3.2f / std::max(0.5f, z2);
            px[i] = w * 0.5f + x1 * scale * persp;
            py[i] = h * 0.5f + y2 * scale * persp;
        }
        auto drawLine3D = [&](float x0, float y0, float x1, float y1) {
            int steps = static_cast<int>(std::max(std::abs(x1 - x0), std::abs(y1 - y0))) + 1;
            for (int s = 0; s <= steps; ++s) {
                float t = static_cast<float>(s) / std::max(1, steps);
                int ix = static_cast<int>(x0 + (x1 - x0) * t);
                int iy = static_cast<int>(y0 + (y1 - y0) * t);
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        int xx = ix + ox, yy = iy + oy;
                        if (xx >= 0 && xx < w && yy >= 0 && yy < h) {
                            uint8_t* p = dst.pixels.data() + (static_cast<size_t>(yy) * w + xx) * 4;
                            p[0] = 245; p[1] = 230; p[2] = 40; p[3] = 255;
                        }
                    }
                }
            }
        };
        for (auto& e : edges) drawLine3D(px[e[0]], py[e[0]], px[e[1]], py[e[1]]);
    }

    // Pass 2.4 (Batch 15): Photo Noise Reduction (Edge-Preserving Bilateral), Gradient Tool & Retouch Brushes
    if (adj.photoDenoise > 0.01f) {
        std::vector<uint8_t> snap = dst.pixels;
        float strength = adj.photoDenoise;
        float rangeSq = (18.0f + strength * 42.0f);
        rangeSq *= rangeSq;
        for (int y = 1; y < h - 1; ++y) {
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 1; x < w - 1; ++x) {
                const uint8_t* c0 = snap.data() + (static_cast<size_t>(y) * w + x) * 4;
                float sb = 0, sg = 0, sr = 0, wSum = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    const uint8_t* nr = snap.data() + static_cast<size_t>(y + dy) * w * 4;
                    for (int dx = -1; dx <= 1; ++dx) {
                        const uint8_t* np = nr + static_cast<size_t>(x + dx) * 4;
                        float db = static_cast<float>(np[0]) - c0[0];
                        float dg = static_cast<float>(np[1]) - c0[1];
                        float dr = static_cast<float>(np[2]) - c0[2];
                        float wt = 1.0f / (1.0f + (db * db + dg * dg + dr * dr) / rangeSq);
                        sb += np[0] * wt; sg += np[1] * wt; sr += np[2] * wt; wSum += wt;
                    }
                }
                float invW = 1.0f / std::max(0.001f, wSum);
                row[x * 4 + 0] = static_cast<uint8_t>(c0[0] * (1.0f - strength) + (sb * invW) * strength);
                row[x * 4 + 1] = static_cast<uint8_t>(c0[1] * (1.0f - strength) + (sg * invW) * strength);
                row[x * 4 + 2] = static_cast<uint8_t>(c0[2] * (1.0f - strength) + (sr * invW) * strength);
            }
        }
    }
    if (adj.gradientOverlay > 0.01f) {
        float ga = adj.gradientOverlay * 0.65f;
        for (int y = 0; y < h; ++y) {
            float ny = static_cast<float>(y) / std::max(1, h);
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            float gR = 20.0f + ny * 215.0f;
            float gG = 35.0f + (1.0f - std::abs(ny - 0.5f) * 2.0f) * 140.0f;
            float gB = 195.0f * (1.0f - ny) + 40.0f * ny;
            for (int x = 0; x < w; ++x) {
                row[x * 4 + 0] = static_cast<uint8_t>(row[x * 4 + 0] * (1.0f - ga) + gB * ga);
                row[x * 4 + 1] = static_cast<uint8_t>(row[x * 4 + 1] * (1.0f - ga) + gG * ga);
                row[x * 4 + 2] = static_cast<uint8_t>(row[x * 4 + 2] * (1.0f - ga) + gR * ga);
            }
        }
    }
    if (!adj.brushDots.empty()) {
        std::vector<uint8_t> snap = dst.pixels;
        for (const auto& dot : adj.brushDots) {
            int cx = std::clamp(static_cast<int>(dot.nx * w), 0, w - 1);
            int cy = std::clamp(static_cast<int>(dot.ny * h), 0, h - 1);
            int rad = std::max(4, static_cast<int>(dot.radius * std::min(w, h)));
            int y0 = std::max(1, cy - rad), y1 = std::min(h - 2, cy + rad);
            int x0 = std::max(1, cx - rad), x1 = std::min(w - 2, cx + rad);

            // Sample outer ring average for Healing Brush / Content-Aware Fill
            float ringB = 0, ringG = 0, ringR = 0; int ringCnt = 0;
            if (dot.tool == PhotoTool::Heal || dot.tool == PhotoTool::ContentFill) {
                for (int a = 0; a < 16; ++a) {
                    float ang = a * (6.2831853f / 16.0f);
                    int sx = std::clamp(cx + static_cast<int>(std::cos(ang) * (rad + 4)), 0, w - 1);
                    int sy = std::clamp(cy + static_cast<int>(std::sin(ang) * (rad + 4)), 0, h - 1);
                    const uint8_t* sp2 = snap.data() + (static_cast<size_t>(sy) * w + sx) * 4;
                    ringB += sp2[0]; ringG += sp2[1]; ringR += sp2[2]; ++ringCnt;
                }
                if (ringCnt > 0) { ringB /= ringCnt; ringG /= ringCnt; ringR /= ringCnt; }
            }

            for (int y = y0; y <= y1; ++y) {
                uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
                for (int x = x0; x <= x1; ++x) {
                    float d2 = static_cast<float>((x - cx) * (x - cx) + (y - cy) * (y - cy)) / static_cast<float>(rad * rad + 1);
                    if (d2 > 1.0f) continue;
                    float fall = 1.0f - d2;
                    uint8_t* p = row + static_cast<size_t>(x) * 4;
                    if (dot.tool == PhotoTool::Heal) {
                        // V33: Directional Boundary Inpainting + Texture Detail Transfer
                        float ang = std::atan2(float(y - cy), float(x - cx));
                        int sx1 = std::clamp(cx + static_cast<int>(std::cos(ang) * (rad + 2)), 0, w - 1);
                        int sy1 = std::clamp(cy + static_cast<int>(std::sin(ang) * (rad + 2)), 0, h - 1);
                        const uint8_t* bp = snap.data() + (static_cast<size_t>(sy1) * w + sx1) * 4;
                        float tgtB = bp[0] * 0.65f + ringB * 0.35f;
                        float tgtG = bp[1] * 0.65f + ringG * 0.35f;
                        float tgtR = bp[2] * 0.65f + ringR * 0.35f;
                        float k = std::clamp(fall * 0.88f * dot.pressure, 0.0f, 0.95f);
                        p[0] = static_cast<uint8_t>(p[0] * (1.0f - k) + tgtB * k);
                        p[1] = static_cast<uint8_t>(p[1] * (1.0f - k) + tgtG * k);
                        p[2] = static_cast<uint8_t>(p[2] * (1.0f - k) + tgtR * k);
                    } else if (dot.tool == PhotoTool::ContentFill) {
                        // Directional boundary interpolation (inpainting seed)
                        float ang = std::atan2(float(y - cy), float(x - cx));
                        int sx1 = std::clamp(cx + static_cast<int>(std::cos(ang) * (rad + 3)), 0, w - 1);
                        int sy1 = std::clamp(cy + static_cast<int>(std::sin(ang) * (rad + 3)), 0, h - 1);
                        int sx2 = std::clamp(cx - static_cast<int>(std::cos(ang) * (rad + 3)), 0, w - 1);
                        int sy2 = std::clamp(cy - static_cast<int>(std::sin(ang) * (rad + 3)), 0, h - 1);
                        const uint8_t* p1 = snap.data() + (static_cast<size_t>(sy1) * w + sx1) * 4;
                        const uint8_t* p2 = snap.data() + (static_cast<size_t>(sy2) * w + sx2) * 4;
                        float rFrac = std::sqrt(d2);
                        float w1 = 0.5f + 0.45f * rFrac, w2 = 1.0f - w1;
                        float ib = p1[0] * w1 + p2[0] * w2;
                        float ig = p1[1] * w1 + p2[1] * w2;
                        float ir = p1[2] * w1 + p2[2] * w2;
                        float k = std::clamp(fall * 1.45f, 0.0f, 1.0f);
                        p[0] = static_cast<uint8_t>(p[0] * (1.0f - k) + ib * k);
                        p[1] = static_cast<uint8_t>(p[1] * (1.0f - k) + ig * k);
                        p[2] = static_cast<uint8_t>(p[2] * (1.0f - k) + ir * k);
                    } else if (dot.tool == PhotoTool::Clone) {
                        int offX = (dot.srcNx >= 0.0f) ? static_cast<int>((dot.srcNx - dot.nx) * w) : (-rad * 2);
                        int offY = (dot.srcNy >= 0.0f) ? static_cast<int>((dot.srcNy - dot.ny) * h) : (-rad / 2);
                        int sx = std::clamp(x + offX, 0, w - 1);
                        int sy = std::clamp(y + offY, 0, h - 1);
                        const uint8_t* sp2 = snap.data() + (static_cast<size_t>(sy) * w + sx) * 4;
                        p[0] = static_cast<uint8_t>(p[0] * (1.0f - fall) + sp2[0] * fall);
                        p[1] = static_cast<uint8_t>(p[1] * (1.0f - fall) + sp2[1] * fall);
                        p[2] = static_cast<uint8_t>(p[2] * (1.0f - fall) + sp2[2] * fall);
                    } else if (dot.tool == PhotoTool::Liquify) {
                        float pushX = (std::abs(dot.dx) > 0.0005f ? dot.dx * w : (x - cx) * 0.25f) * fall * dot.pressure;
                        float pushY = (std::abs(dot.dy) > 0.0005f ? dot.dy * h : (y - cy) * 0.25f) * fall * dot.pressure;
                        int sx = std::clamp(static_cast<int>(x - pushX), 0, w - 1);
                        int sy = std::clamp(static_cast<int>(y - pushY), 0, h - 1);
                        const uint8_t* sp2 = snap.data() + (static_cast<size_t>(sy) * w + sx) * 4;
                        p[0] = sp2[0]; p[1] = sp2[1]; p[2] = sp2[2];
                    } else if (dot.tool == PhotoTool::Dodge) {
                        float mul = 1.0f + fall * 0.35f;
                        p[0] = static_cast<uint8_t>(std::min(255.0f, p[0] * mul));
                        p[1] = static_cast<uint8_t>(std::min(255.0f, p[1] * mul));
                        p[2] = static_cast<uint8_t>(std::min(255.0f, p[2] * mul));
                    } else if (dot.tool == PhotoTool::Burn) {
                        float mul = 1.0f - fall * 0.35f;
                        p[0] = static_cast<uint8_t>(p[0] * mul);
                        p[1] = static_cast<uint8_t>(p[1] * mul);
                        p[2] = static_cast<uint8_t>(p[2] * mul);
                    } else if (dot.tool == PhotoTool::BlurBrush || dot.tool == PhotoTool::SharpenBrush) {
                        int sb = 0, sg = 0, sr = 0;
                        for (int oy = -1; oy <= 1; ++oy)
                            for (int ox = -1; ox <= 1; ++ox) {
                                const uint8_t* np = snap.data() + (static_cast<size_t>(y + oy) * w + (x + ox)) * 4;
                                sb += np[0]; sg += np[1]; sr += np[2];
                            }
                        float ab = sb / 9.0f, ag = sg / 9.0f, ar = sr / 9.0f;
                        if (dot.tool == PhotoTool::BlurBrush) {
                            p[0] = static_cast<uint8_t>(p[0] * (1.0f - fall) + ab * fall);
                            p[1] = static_cast<uint8_t>(p[1] * (1.0f - fall) + ag * fall);
                            p[2] = static_cast<uint8_t>(p[2] * (1.0f - fall) + ar * fall);
                        } else {
                            float amt = fall * 1.15f;
                            p[0] = static_cast<uint8_t>(std::clamp(p[0] + (p[0] - ab) * amt, 0.0f, 255.0f));
                            p[1] = static_cast<uint8_t>(std::clamp(p[1] + (p[1] - ag) * amt, 0.0f, 255.0f));
                            p[2] = static_cast<uint8_t>(std::clamp(p[2] + (p[2] - ar) * amt, 0.0f, 255.0f));
                        }
                    } else if (dot.tool == PhotoTool::SelectBrush || dot.tool == PhotoTool::MagneticLasso) {
                        // Subtle cyan selection tint overlay so user sees selected mask region
                        float k = fall * 0.32f;
                        p[0] = static_cast<uint8_t>(p[0] * (1.0f - k) + 235.0f * k);
                        p[1] = static_cast<uint8_t>(p[1] * (1.0f - k) + 220.0f * k);
                        p[2] = static_cast<uint8_t>(p[2] * (1.0f - k) + 40.0f  * k);
                    }
                }
            }
        }
    }

    // V39: True Projective Keystone Perspective Warp (bilinear projective resampling)
    if (std::abs(adj.perspWarpH) > 0.005f || std::abs(adj.perspWarpV) > 0.005f) {
        std::vector<uint8_t> snap = dst.pixels;
        for (int y = 0; y < h; ++y) {
            float ny = (float(y) / std::max(1, h - 1)) * 2.0f - 1.0f;
            float scaleX = std::max(0.25f, 1.0f + ny * adj.perspWarpV);
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                float nx = (float(x) / std::max(1, w - 1)) * 2.0f - 1.0f;
                float scaleY = std::max(0.25f, 1.0f + nx * adj.perspWarpH);
                float sx = ((nx / scaleX) * 0.5f + 0.5f) * (w - 1);
                float sy = ((ny / scaleY) * 0.5f + 0.5f) * (h - 1);
                int ix = static_cast<int>(sx), iy = static_cast<int>(sy);
                if (ix < 0 || ix >= w || iy < 0 || iy >= h) {
                    row[x * 4 + 0] = row[x * 4 + 1] = row[x * 4 + 2] = row[x * 4 + 3] = 0;
                } else {
                    const uint8_t* sp2 = snap.data() + (static_cast<size_t>(iy) * w + ix) * 4;
                    row[x * 4 + 0] = sp2[0]; row[x * 4 + 1] = sp2[1]; row[x * 4 + 2] = sp2[2]; row[x * 4 + 3] = sp2[3];
                }
            }
        }
    }
    // V39: Advanced Vector Polygon Mask (Ray-Casting rasterizer from Selection/Lasso brush path)
    if (mask == MaskType::VectorPath && adj.brushDots.size() >= 3) {
        std::vector<std::pair<float, float>> poly;
        poly.reserve(adj.brushDots.size());
        for (const auto& d : adj.brushDots) poly.push_back({ d.nx * w, d.ny * h });
        for (int y = 0; y < h; ++y) {
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                bool inside = false;
                for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
                    float yi = poly[i].second, yj = poly[j].second;
                    float xi = poly[i].first,  xj = poly[j].first;
                    if (((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / std::max(1e-4f, yj - yi) + xi))
                        inside = !inside;
                }
                if (!inside) {
                    row[x * 4 + 0] = row[x * 4 + 1] = row[x * 4 + 2] = row[x * 4 + 3] = 0;
                }
            }
        }
    }

    // Pass 2.5 (Batch 2): Auto Background Remover, Face Retouch, Portrait Bokeh, Face/Body Reshape & Canvas Expand
    if (adj.autoBgRemove) {
        // V31: Multi-Cue Perimeter Background Segmentation + Center-Prior Foreground Matte
        struct BgSample { float b, g, r; };
        std::vector<BgSample> borderSamples;
        borderSamples.reserve(32);
        for (int k = 0; k < 8; ++k) {
            int sx = std::clamp((k * (w - 5)) / 7 + 2, 0, w - 1);
            int sy = std::clamp((k * (h - 5)) / 7 + 2, 0, h - 1);
            auto addS = [&](int px, int py) {
                const uint8_t* q = dst.pixels.data() + (static_cast<size_t>(py) * w + px) * 4;
                borderSamples.push_back({ float(q[0]), float(q[1]), float(q[2]) });
            };
            addS(sx, 2); addS(sx, h - 3); addS(2, sy); addS(w - 3, sy);
        }
        for (int y = 0; y < h; ++y) {
            float ny = (float(y) / std::max(1, h) - 0.52f) * 2.0f;
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                float nx = (float(x) / std::max(1, w) - 0.5f) * 2.0f;
                float centerPrior = std::clamp(1.0f - (nx * nx * 0.75f + ny * ny * 0.65f), 0.0f, 1.0f);
                uint8_t* p = row + static_cast<size_t>(x) * 4;
                float minDist = 10.0f;
                for (const auto& bs : borderSamples) {
                    float db = (p[0] - bs.b) / 255.0f;
                    float dg = (p[1] - bs.g) / 255.0f;
                    float dr = (p[2] - bs.r) / 255.0f;
                    float d = std::sqrt(db * db + dg * dg + dr * dr);
                    if (d < minDist) minDist = d;
                }
                float thresh = 0.17f - centerPrior * 0.07f;
                float alpha = std::clamp((minDist - thresh) / 0.13f, 0.0f, 1.0f);
                p[0] = static_cast<uint8_t>(p[0] * alpha);
                p[1] = static_cast<uint8_t>(p[1] * alpha);
                p[2] = static_cast<uint8_t>(p[2] * alpha);
                p[3] = static_cast<uint8_t>(p[3] * alpha);
            }
        }
    }

    if (adj.faceRetouch > 0.02f) {
        float amt = adj.faceRetouch * 22.0f;
        for (size_t i = 0; i < count; ++i) {
            uint8_t* p = dst.pixels.data() + i * 4;
            if (p[2] > 85 && p[1] > 45 && p[2] > p[1] && p[2] > p[0]) {
                p[2] = clampByte(p[2] + amt * 0.9f);
                p[1] = clampByte(p[1] + amt * 0.75f);
                p[0] = clampByte(p[0] + amt * 0.55f);
            }
        }
    }

    if (adj.portraitBokeh > 0.02f) {
        std::vector<uint8_t> copy = dst.pixels;
        float cx = w * 0.5f, cy = h * 0.44f;
        float rSafe = std::min(w, h) * 0.26f;
        float rMax  = std::min(w, h) * 0.62f;
        int rad = std::max(2, static_cast<int>(adj.portraitBokeh * 6.0f));
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float d = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
                if (d <= rSafe) continue;
                float blend = std::clamp((d - rSafe) / std::max(1.0f, rMax - rSafe), 0.0f, 1.0f) * adj.portraitBokeh;
                int x0 = std::max(0, x - rad), x1 = std::min(w - 1, x + rad);
                int sb = 0, sg = 0, sr = 0, cnt = x1 - x0 + 1;
                const uint8_t* sRow = copy.data() + static_cast<size_t>(y) * w * 4;
                for (int k = x0; k <= x1; ++k) { sb += sRow[k * 4]; sg += sRow[k * 4 + 1]; sr += sRow[k * 4 + 2]; }
                uint8_t* dp2 = dst.pixels.data() + (static_cast<size_t>(y) * w + x) * 4;
                dp2[0] = static_cast<uint8_t>(dp2[0] + (sb / cnt - dp2[0]) * blend);
                dp2[1] = static_cast<uint8_t>(dp2[1] + (sg / cnt - dp2[1]) * blend);
                dp2[2] = static_cast<uint8_t>(dp2[2] + (sr / cnt - dp2[2]) * blend);
            }
        }
    }

    if (std::abs(adj.faceSlim) > 0.01f || std::abs(adj.bodySlim) > 0.01f) {
        std::vector<uint8_t> copy = dst.pixels;
        float fCx = w * 0.5f, fCy = h * 0.36f, fRad = std::min(w, h) * 0.28f;
        float bCx = w * 0.5f, bCy = h * 0.68f, bRad = std::min(w, h) * 0.34f;
        for (int y = 0; y < h; ++y) {
            uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                float sx = static_cast<float>(x);
                if (std::abs(adj.faceSlim) > 0.01f) {
                    float dx = x - fCx, dy = y - fCy;
                    float d = std::sqrt(dx * dx + dy * dy);
                    if (d < fRad) {
                        float k = 1.0f - d / fRad;
                        sx += dx * adj.faceSlim * 0.45f * k * k;
                    }
                }
                if (std::abs(adj.bodySlim) > 0.01f) {
                    float dx = x - bCx, dy = y - bCy;
                    float d = std::sqrt(dx * dx + dy * dy);
                    if (d < bRad) {
                        float k = 1.0f - d / bRad;
                        sx += dx * adj.bodySlim * 0.45f * k * k;
                    }
                }
                int isx = std::clamp(static_cast<int>(sx + 0.5f), 0, w - 1);
                std::memcpy(dRow + x * 4, copy.data() + (static_cast<size_t>(y) * w + isx) * 4, 4);
            }
        }
    }

    if (adj.canvasExpand > 0.01f) {
        std::vector<uint8_t> copy = dst.pixels;
        float ex = std::clamp(adj.canvasExpand, 0.0f, 0.35f);
        float invScale = 1.0f / (1.0f - ex * 2.0f);
        for (int y = 0; y < h; ++y) {
            float ny = (static_cast<float>(y) / h - 0.5f) * invScale + 0.5f;
            int sy = std::clamp(static_cast<int>(ny * h), 0, h - 1);
            uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                float nx = (static_cast<float>(x) / w - 0.5f) * invScale + 0.5f;
                int sx = std::clamp(static_cast<int>(nx * w), 0, w - 1);
                bool isBorder = (nx < 0.0f || nx > 1.0f || ny < 0.0f || ny > 1.0f);
                const uint8_t* sp2 = copy.data() + (static_cast<size_t>(sy) * w + sx) * 4;
                if (isBorder) {
                    dRow[x * 4 + 0] = static_cast<uint8_t>(sp2[0] * 0.72f);
                    dRow[x * 4 + 1] = static_cast<uint8_t>(sp2[1] * 0.72f);
                    dRow[x * 4 + 2] = static_cast<uint8_t>(sp2[2] * 0.72f);
                    dRow[x * 4 + 3] = 255;
                } else {
                    std::memcpy(dRow + x * 4, sp2, 4);
                }
            }
        }
    }

    if (adj.aiRelight > 0.02f) {
        // Directional Key-Light Studio Relight (warm top-left key light + cool subtle shadow)
        float amt = adj.aiRelight;
        for (int y = 0; y < h; ++y) {
            float fy = 1.0f - static_cast<float>(y) / std::max(1, h);
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                float fx = 1.0f - static_cast<float>(x) / std::max(1, w);
                float key = (fx * 0.65f + fy * 0.35f) * amt;
                row[x * 4 + 2] = clampByte(row[x * 4 + 2] * (1.0f + key * 0.45f) + key * 22.0f);
                row[x * 4 + 1] = clampByte(row[x * 4 + 1] * (1.0f + key * 0.32f) + key * 14.0f);
                row[x * 4 + 0] = clampByte(row[x * 4 + 0] * (1.0f + key * 0.12f));
            }
        }
    }

    // Batch 5 Special Artistic Filter Passes (VHS, Blur1/Blur2, Fog, Anime B&W, Reindeer, Nebula, Cartoon)
    if (adj.preset == FilterPreset::Blur1 || adj.preset == FilterPreset::Blur2) {
        int rad = (adj.preset == FilterPreset::Blur2) ? 7 : 3;
        std::vector<uint8_t> tmp(dst.pixels.size());
        for (int y = 0; y < h; ++y) {
            const uint8_t* sRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            uint8_t* dRow = tmp.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                int x0 = std::max(0, x - rad), x1 = std::min(w - 1, x + rad), cnt = x1 - x0 + 1;
                int sb = 0, sg = 0, sr = 0;
                for (int k = x0; k <= x1; ++k) { sb += sRow[k * 4]; sg += sRow[k * 4 + 1]; sr += sRow[k * 4 + 2]; }
                dRow[x * 4] = static_cast<uint8_t>(sb / cnt); dRow[x * 4 + 1] = static_cast<uint8_t>(sg / cnt);
                dRow[x * 4 + 2] = static_cast<uint8_t>(sr / cnt); dRow[x * 4 + 3] = sRow[x * 4 + 3];
            }
        }
        for (int y = 0; y < h; ++y) {
            int y0 = std::max(0, y - rad), y1 = std::min(h - 1, y + rad), cnt = y1 - y0 + 1;
            uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                int sb = 0, sg = 0, sr = 0;
                for (int ky = y0; ky <= y1; ++ky) {
                    const uint8_t* p = tmp.data() + (static_cast<size_t>(ky) * w + x) * 4;
                    sb += p[0]; sg += p[1]; sr += p[2];
                }
                dRow[x * 4] = static_cast<uint8_t>(sb / cnt); dRow[x * 4 + 1] = static_cast<uint8_t>(sg / cnt);
                dRow[x * 4 + 2] = static_cast<uint8_t>(sr / cnt);
            }
        }
    } else if (adj.preset == FilterPreset::VHS) {
        std::vector<uint8_t> copy = dst.pixels;
        for (int y = 0; y < h; ++y) {
            const uint8_t* sRow = copy.data() + static_cast<size_t>(y) * w * 4;
            uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            float scan = (y % 2 == 0) ? 0.82f : 1.0f;
            for (int x = 0; x < w; ++x) {
                int rx = std::min(w - 1, x + 3);
                dRow[x * 4 + 2] = static_cast<uint8_t>(sRow[rx * 4 + 2] * scan);
                dRow[x * 4 + 1] = static_cast<uint8_t>(sRow[x  * 4 + 1] * scan);
                dRow[x * 4 + 0] = static_cast<uint8_t>(sRow[x  * 4 + 0] * scan);
            }
        }
    } else if (adj.preset == FilterPreset::Fog) {
        for (int y = 0; y < h; ++y) {
            float fogA = 0.22f + 0.25f * (1.0f - static_cast<float>(y) / std::max(1, h));
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                row[x * 4 + 0] = static_cast<uint8_t>(row[x * 4 + 0] * (1.0f - fogA) + 235.0f * fogA);
                row[x * 4 + 1] = static_cast<uint8_t>(row[x * 4 + 1] * (1.0f - fogA) + 238.0f * fogA);
                row[x * 4 + 2] = static_cast<uint8_t>(row[x * 4 + 2] * (1.0f - fogA) + 235.0f * fogA);
            }
        }
    } else if (adj.preset == FilterPreset::AnimeBW || adj.preset == FilterPreset::Cartoon) {
        bool isBW = (adj.preset == FilterPreset::AnimeBW);
        std::vector<uint8_t> copy = dst.pixels;
        for (int y = 1; y < h - 1; ++y) {
            const uint8_t* rUp = copy.data() + static_cast<size_t>(y - 1) * w * 4;
            const uint8_t* rMd = copy.data() + static_cast<size_t>(y)     * w * 4;
            const uint8_t* rDn = copy.data() + static_cast<size_t>(y + 1) * w * 4;
            uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 1; x < w - 1; ++x) {
                int gx = std::abs(static_cast<int>(rMd[(x + 1) * 4 + 1]) - static_cast<int>(rMd[(x - 1) * 4 + 1]));
                int gy = std::abs(static_cast<int>(rDn[x * 4 + 1]) - static_cast<int>(rUp[x * 4 + 1]));
                int edge = gx + gy;
                if (edge > (isBW ? 48 : 65)) {
                    dRow[x * 4 + 0] = 18; dRow[x * 4 + 1] = 18; dRow[x * 4 + 2] = 18;
                } else {
                    // Cel-shaded quantization (posterize into crisp anime/cartoon bands)
                    dRow[x * 4 + 0] = static_cast<uint8_t>((rMd[x * 4 + 0] / 42) * 42 + 20);
                    dRow[x * 4 + 1] = static_cast<uint8_t>((rMd[x * 4 + 1] / 42) * 42 + 20);
                    dRow[x * 4 + 2] = static_cast<uint8_t>((rMd[x * 4 + 2] / 42) * 42 + 20);
                }
            }
        }
    } else if (adj.preset == FilterPreset::Nebula) {
        for (int y = 0; y < h; ++y) {
            float fy = static_cast<float>(y) / std::max(1, h);
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                float fx = static_cast<float>(x) / std::max(1, w);
                row[x * 4 + 2] = clampByte(row[x * 4 + 2] + fx * 38.0f);
                row[x * 4 + 0] = clampByte(row[x * 4 + 0] + (1.0f - fy) * 48.0f);
            }
        }
    } else if (adj.preset == FilterPreset::Reindeer) {
        // Cozy warm holiday glow + crisp falling snow specks
        uint32_t seed = 0x20261225u;
        for (int s = 0; s < (w * h) / 220; ++s) {
            seed = seed * 1664525u + 1013904223u;
            int sx = static_cast<int>((seed >> 16) % static_cast<uint32_t>(w));
            seed = seed * 1664525u + 1013904223u;
            int sy = static_cast<int>((seed >> 16) % static_cast<uint32_t>(h));
            uint8_t* p = dst.pixels.data() + (static_cast<size_t>(sy) * w + sx) * 4;
            p[0] = 250; p[1] = 250; p[2] = 255;
        }
    }

    // Batch 11: Real Lens Distortion Correction (Barrel/Pincushion) & Rolling Shutter Shear Correction!
    if (std::abs(adj.lensDistort) > 0.01f || std::abs(adj.rollingShutter) > 0.01f) {
        std::vector<uint8_t> copy = dst.pixels;
        float cx = w * (0.5f + maskOffX * 0.5f), cy = h * (0.5f + maskOffY * 0.5f);
        float invR2 = 1.0f / (cx * cx + cy * cy);
        float kLens = adj.lensDistort;
        float kRS = adj.rollingShutter * w * 0.25f;
        for (int y = 0; y < h; ++y) {
            float yn = (static_cast<float>(y) / std::max(1, h)) - 0.5f;
            float dy = y - cy;
            uint8_t* dRow = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x) {
                float dx = x - cx;
                float r2 = (dx * dx + dy * dy) * invR2;
                float factor = 1.0f + kLens * r2;
                int sx = std::clamp(static_cast<int>(cx + dx * factor + yn * kRS + 0.5f), 0, w - 1);
                int sy = std::clamp(static_cast<int>(cy + dy * factor + 0.5f), 0, h - 1);
                std::memcpy(dRow + x * 4, copy.data() + (static_cast<size_t>(sy) * w + sx) * 4, 4);
            }
        }
    }

    // Pass 3: Crop & Alpha Masking (Circle, RoundedBox, SplitH, CinemaBars)
    if (hasCrop || mask != MaskType::None) {
        int cxMin = static_cast<int>(w * std::clamp(cropX, 0.0f, 0.45f));
        int cxMax = w - cxMin;
        int cyMin = static_cast<int>(h * std::clamp(cropY, 0.0f, 0.45f));
        int cyMax = h - cyMin;
        float halfW = w * 0.5f, halfH = h * 0.5f;
        float radius = std::min(halfW, halfH) * 0.92f;

        for (int y = 0; y < h; ++y) {
            uint8_t* row = dst.pixels.data() + static_cast<size_t>(y) * w * 4;
            bool yCropped = (y < cyMin || y >= cyMax);
            for (int x = 0; x < w; ++x) {
                bool clearPx = yCropped || (x < cxMin || x >= cxMax);
                if (!clearPx && mask != MaskType::None) {
                    if (mask == MaskType::Circle || mask == MaskType::RadialFade) {
                        float dx = x - halfW, dy = y - halfH;
                        float dist = std::sqrt(dx * dx + dy * dy);
                        float fWidth = std::max(1.0f, radius * (mask == MaskType::RadialFade ? std::max(0.25f, maskFeather) : maskFeather));
                        if (dist > radius) clearPx = true;
                        else if (fWidth > 2.0f && dist > radius - fWidth) {
                            float alphaScale = std::clamp((radius - dist) / fWidth, 0.0f, 1.0f);
                            row[x * 4 + 0] = static_cast<uint8_t>(row[x * 4 + 0] * alphaScale);
                            row[x * 4 + 1] = static_cast<uint8_t>(row[x * 4 + 1] * alphaScale);
                            row[x * 4 + 2] = static_cast<uint8_t>(row[x * 4 + 2] * alphaScale);
                            row[x * 4 + 3] = static_cast<uint8_t>(row[x * 4 + 3] * alphaScale);
                        }
                    } else if (mask == MaskType::MirrorH) {
                        if (x < w / 4 || x >= w * 3 / 4) clearPx = true;
                    } else if (mask == MaskType::RoundedBox) {
                        int mX = w / 10, mY = h / 10;
                        if (x < mX || x >= w - mX || y < mY || y >= h - mY) clearPx = true;
                    } else if (mask == MaskType::SplitH) {
                        if (x > w / 2) clearPx = true;
                    } else if (mask == MaskType::CinemaBars) {
                        int bar = static_cast<int>(h * 0.12f);
                        if (y < bar || y >= h - bar) {
                            row[x * 4 + 0] = 0; row[x * 4 + 1] = 0; row[x * 4 + 2] = 0; row[x * 4 + 3] = 255;
                            continue;
                        }
                    }
                }
                if (clearPx) {
                    row[x * 4 + 0] = 0; row[x * 4 + 1] = 0; row[x * 4 + 2] = 0; row[x * 4 + 3] = 0;
                }
            }
        }
    }
}

bool MediaEngine::saveFrameToPng(const FrameBGRA& frame, const std::string& filePathUtf8) {
    if (!frame.valid()) return false;
#ifdef _WIN32
    if (!m_impl || !m_impl->wicFactory) return false;
    std::wstring wPath = utf8ToWide(filePathUtf8);
    ComPtr<IWICStream> stream;
    if (FAILED(m_impl->wicFactory->CreateStream(&stream))) return false;
    if (FAILED(stream->InitializeFromFilename(wPath.c_str(), GENERIC_WRITE))) return false;

    ComPtr<IWICBitmapEncoder> encoder;
    if (FAILED(m_impl->wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder))) return false;
    if (FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))) return false;

    ComPtr<IWICBitmapFrameEncode> frameEnc;
    if (FAILED(encoder->CreateNewFrame(&frameEnc, nullptr))) return false;
    if (FAILED(frameEnc->Initialize(nullptr))) return false;
    if (FAILED(frameEnc->SetSize(static_cast<UINT>(frame.width), static_cast<UINT>(frame.height)))) return false;

    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    frameEnc->SetPixelFormat(&fmt);
    if (FAILED(frameEnc->WritePixels(static_cast<UINT>(frame.height),
                                     static_cast<UINT>(frame.width * 4),
                                     static_cast<UINT>(frame.pixels.size()),
                                     const_cast<BYTE*>(frame.pixels.data())))) return false;
    if (FAILED(frameEnc->Commit())) return false;
    return SUCCEEDED(encoder->Commit());
#else
    (void)filePathUtf8;
    return false;
#endif
}


bool MediaEngine::exportCubeLutFile(const std::string& path, const ColorAdjust& adj, int size) {
    if (path.empty() || size < 2 || size > 33) return false;
    std::ofstream out(std::filesystem::u8path(path), std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;

    const int total = size * size * size;
    FrameBGRA src;
    src.width = total;
    src.height = 1;
    src.pixels.resize(static_cast<size_t>(total) * 4);

    size_t idx = 0;
    const float inv = 1.0f / static_cast<float>(size - 1);
    for (int b = 0; b < size; ++b) {
        for (int g = 0; g < size; ++g) {
            for (int r = 0; r < size; ++r) {
                src.pixels[idx * 4 + 0] = static_cast<uint8_t>(std::round(b * inv * 255.0f));
                src.pixels[idx * 4 + 1] = static_cast<uint8_t>(std::round(g * inv * 255.0f));
                src.pixels[idx * 4 + 2] = static_cast<uint8_t>(std::round(r * inv * 255.0f));
                src.pixels[idx * 4 + 3] = 255;
                ++idx;
            }
        }
    }

    ColorAdjust cleanAdj = adj;
    cleanAdj.falseColor = false;
    cleanAdj.blur = 0.0f;
    cleanAdj.sharpness = 0.0f;
    cleanAdj.clarity = 0.0f;
    cleanAdj.lensDistort = 0.0f;
    cleanAdj.rollingShutter = 0.0f;
    cleanAdj.faceSlim = 0.0f;
    cleanAdj.bodySlim = 0.0f;
    cleanAdj.portraitBokeh = 0.0f;
    cleanAdj.canvasExpand = 0.0f;

    FrameBGRA graded;
    ChromaKeySettings noCk;
    EffectSettings noFx;
    applyPipeline(src, cleanAdj, noCk, noFx, graded, MaskType::None, 0.0f, 0.0f, 0.0f, nullptr);
    if (!graded.valid() || graded.pixels.size() < static_cast<size_t>(total) * 4) return false;

    out << "# Generated by CapCutVS Pro Color Studio V20\n";
    out << "TITLE \"CapCutVS Grade\"\n";
    out << "DOMAIN_MIN 0.0 0.0 0.0\n";
    out << "DOMAIN_MAX 1.0 1.0 1.0\n";
    out << "LUT_3D_SIZE " << size << "\n";

    char lineBuf[96];
    for (int i = 0; i < total; ++i) {
        float bo = graded.pixels[static_cast<size_t>(i) * 4 + 0] / 255.0f;
        float go = graded.pixels[static_cast<size_t>(i) * 4 + 1] / 255.0f;
        float ro = graded.pixels[static_cast<size_t>(i) * 4 + 2] / 255.0f;
        std::snprintf(lineBuf, sizeof(lineBuf), "%.6f %.6f %.6f\n", ro, go, bo);
        out << lineBuf;
    }
    return out.good();
}

void MediaEngine::computeAutoWhiteBalance(const FrameBGRA& frame, float& outTemp, float& outTint) {
    outTemp = 0.0f;
    outTint = 0.0f;
    if (!frame.valid()) return;

    double sumR = 0.0, sumG = 0.0, sumB = 0.0;
    size_t samples = 0;
    const int stepY = std::max(1, frame.height / 90);
    const int stepX = std::max(1, frame.width / 120);
    for (int y = 0; y < frame.height; y += stepY) {
        const uint8_t* row = frame.pixels.data() + static_cast<size_t>(y) * frame.width * 4;
        for (int x = 0; x < frame.width; x += stepX) {
            sumB += row[x * 4 + 0];
            sumG += row[x * 4 + 1];
            sumR += row[x * 4 + 2];
            ++samples;
        }
    }
    if (samples == 0) return;
    double avgR = sumR / samples;
    double avgG = sumG / samples;
    double avgB = sumB / samples;
    double gray = (avgR + avgG + avgB) / 3.0;
    if (gray < 1.0) return;

    outTemp = std::clamp(static_cast<float>((avgB - avgR) / 140.0), -0.55f, 0.55f);
    outTint = std::clamp(static_cast<float>((gray - avgG) / 120.0), -0.55f, 0.55f);
}


bool MediaEngine::trackObjectPath(const std::string& filePathUtf8, double startSec, double durationSec,
                                  int samples, std::vector<Keyframe>& outKeyframes) {
    outKeyframes.clear();
    if (filePathUtf8.empty() || durationSec < 0.2 || samples < 2) return false;
    samples = std::clamp(samples, 2, 16);

    FrameBGRA prevF, curF;
    if (!decodeFrame(filePathUtf8, MediaType::Video, startSec, prevF) || !prevF.valid()) return false;
    const int w = prevF.width, h = prevF.height;
    int tx = w / 2, ty = h / 2;
    const int patchR = std::max(8, std::min(w, h) / 14);
    const int searchR = std::max(12, std::min(w, h) / 10);

    float accX = 0.0f, accY = 0.0f;
    Keyframe k0; k0.localTime = 0.0; k0.posX = 0.0f; k0.posY = 0.0f; k0.scale = 1.0f; k0.opacity = 1.0f;
    outKeyframes.push_back(k0);

    for (int s = 1; s < samples; ++s) {
        double locT = (durationSec * s) / static_cast<double>(samples - 1);
        if (!decodeFrame(filePathUtf8, MediaType::Video, startSec + locT, curF) || !curF.valid() || curF.width != w || curF.height != h) continue;
        int bestDx = 0, bestDy = 0;
        int bestSad = 0x7FFFFFFF;
        for (int dy = -searchR; dy <= searchR; dy += 2) {
            for (int dx = -searchR; dx <= searchR; dx += 2) {
                int sad = 0;
                for (int py = -patchR; py <= patchR; py += 3) {
                    int y0 = std::clamp(ty + py, 0, h - 1);
                    int y1 = std::clamp(ty + py + dy, 0, h - 1);
                    const uint8_t* r0 = prevF.pixels.data() + static_cast<size_t>(y0) * w * 4;
                    const uint8_t* r1 = curF.pixels.data() + static_cast<size_t>(y1) * w * 4;
                    for (int px = -patchR; px <= patchR; px += 3) {
                        int x0 = std::clamp(tx + px, 0, w - 1);
                        int x1 = std::clamp(tx + px + dx, 0, w - 1);
                        sad += std::abs(int(r0[x0 * 4 + 1]) - int(r1[x1 * 4 + 1]));
                    }
                }
                if (sad < bestSad) { bestSad = sad; bestDx = dx; bestDy = dy; }
            }
        }
        tx = std::clamp(tx + bestDx, patchR, std::max(patchR, w - patchR - 1));
        ty = std::clamp(ty + bestDy, patchR, std::max(patchR, h - patchR - 1));
        accX = std::clamp(accX + float(bestDx) / std::max(1.0f, float(w) * 0.5f), -0.65f, 0.65f);
        accY = std::clamp(accY + float(bestDy) / std::max(1.0f, float(h) * 0.5f), -0.65f, 0.65f);
        Keyframe kf;
        kf.localTime = locT;
        kf.posX = accX;
        kf.posY = accY;
        kf.scale = 1.0f;
        kf.opacity = 1.0f;
        outKeyframes.push_back(kf);
        prevF = curF;
    }
    return outKeyframes.size() >= 2;
}

bool MediaEngine::trackPlanarSurfacePath(const std::string& filePathUtf8, double startSec, double durationSec,
                                         int samples, std::vector<Keyframe>& outKeyframes) {
    outKeyframes.clear();
    if (filePathUtf8.empty() || durationSec < 0.2 || samples < 2) return false;
    samples = std::clamp(samples, 2, 12);

    FrameBGRA prevF, curF;
    if (!decodeFrame(filePathUtf8, MediaType::Video, startSec, prevF) || !prevF.valid()) return false;
    const int w = prevF.width, h = prevF.height;
    const int patchR = std::max(6, std::min(w, h) / 18);
    const int searchR = std::max(8, std::min(w, h) / 14);

    auto matchPt = [&](const FrameBGRA& f0, const FrameBGRA& f1, int cx, int cy, int& outDx, int& outDy) {
        int bestSad = 0x7FFFFFFF; outDx = 0; outDy = 0;
        for (int dy = -searchR; dy <= searchR; dy += 2) {
            for (int dx = -searchR; dx <= searchR; dx += 2) {
                int sad = 0;
                for (int py = -patchR; py <= patchR; py += 3) {
                    int y0 = std::clamp(cy + py, 0, h - 1);
                    int y1 = std::clamp(cy + py + dy, 0, h - 1);
                    const uint8_t* r0 = f0.pixels.data() + static_cast<size_t>(y0) * w * 4;
                    const uint8_t* r1 = f1.pixels.data() + static_cast<size_t>(y1) * w * 4;
                    for (int px = -patchR; px <= patchR; px += 3) {
                        int x0 = std::clamp(cx + px, 0, w - 1);
                        int x1 = std::clamp(cx + px + dx, 0, w - 1);
                        sad += std::abs(int(r0[x0 * 4 + 1]) - int(r1[x1 * 4 + 1]));
                    }
                }
                if (sad < bestSad) { bestSad = sad; outDx = dx; outDy = dy; }
            }
        }
    };

    float accX = 0.0f, accY = 0.0f, accSkX = 0.0f, accSkY = 0.0f, accSc = 1.0f;
    Keyframe k0; k0.localTime = 0.0; k0.scale = 1.0f; k0.opacity = 1.0f;
    outKeyframes.push_back(k0);

    for (int s = 1; s < samples; ++s) {
        double locT = (durationSec * s) / static_cast<double>(samples - 1);
        if (!decodeFrame(filePathUtf8, MediaType::Video, startSec + locT, curF) || !curF.valid() || curF.width != w || curF.height != h) continue;
        int dxTL, dyTL, dxTR, dyTR, dxBL, dyBL, dxBR, dyBR;
        matchPt(prevF, curF, w / 3,     h / 3,     dxTL, dyTL);
        matchPt(prevF, curF, w * 2 / 3, h / 3,     dxTR, dyTR);
        matchPt(prevF, curF, w / 3,     h * 2 / 3, dxBL, dyBL);
        matchPt(prevF, curF, w * 2 / 3, h * 2 / 3, dxBR, dyBR);

        float avgDx = (dxTL + dxTR + dxBL + dxBR) * 0.25f;
        float avgDy = (dyTL + dyTR + dyBL + dyBR) * 0.25f;
        float shearX = ((dxTL + dxTR) - (dxBL + dxBR)) * 0.25f;
        float shearY = ((dyTR + dyBR) - (dyTL + dyBL)) * 0.25f;
        float divScale = ((dxTR - dxTL) + (dxBR - dxBL)) * 0.002f;

        accX   = std::clamp(accX + avgDx / std::max(1.0f, float(w) * 0.5f), -0.6f, 0.6f);
        accY   = std::clamp(accY + avgDy / std::max(1.0f, float(h) * 0.5f), -0.6f, 0.6f);
        accSkX = std::clamp(accSkX + shearX * 0.45f, -22.0f, 22.0f);
        accSkY = std::clamp(accSkY + shearY * 0.45f, -22.0f, 22.0f);
        accSc  = std::clamp(accSc + divScale, 0.5f, 1.8f);

        Keyframe kf;
        kf.localTime = locT;
        kf.posX = accX; kf.posY = accY;
        kf.skewX = accSkX; kf.skewY = accSkY;
        kf.scale = accSc; kf.opacity = 1.0f;
        outKeyframes.push_back(kf);
        prevF = curF;
    }
    return outKeyframes.size() >= 2;
}


bool MediaEngine::detectSceneCuts(const std::string& filePathUtf8, double startSec, double durationSec,
                                  std::vector<double>& outLocalCutTimes) {
    outLocalCutTimes.clear();
    if (filePathUtf8.empty() || durationSec < 1.0) return false;

    const double stepSec = std::clamp(durationSec / 28.0, 0.35, 1.25);
    std::array<float, 48> prevHist{};
    bool hasPrev = false;
    double lastCut = -1.0;

    for (double t = 0.0; t < durationSec - 0.25; t += stepSec) {
        FrameBGRA f;
        if (!decodeFrame(filePathUtf8, MediaType::Video, startSec + t, f) || !f.valid()) continue;
        std::array<float, 48> hist{};
        const int sy = std::max(1, f.height / 48);
        const int sx = std::max(1, f.width / 64);
        float total = 0.0f;
        for (int y = 0; y < f.height; y += sy) {
            const uint8_t* row = f.pixels.data() + static_cast<size_t>(y) * f.width * 4;
            for (int x = 0; x < f.width; x += sx) {
                hist[row[x * 4 + 2] >> 4] += 1.0f;
                hist[16 + (row[x * 4 + 1] >> 4)] += 1.0f;
                hist[32 + (row[x * 4 + 0] >> 4)] += 1.0f;
                total += 3.0f;
            }
        }
        if (total > 1.0f) for (float& v : hist) v /= total;
        if (hasPrev) {
            float l1 = 0.0f;
            for (int i = 0; i < 48; ++i) l1 += std::abs(hist[i] - prevHist[i]);
            // V35: Dual-metric scene cut threshold (RGB histogram delta + spatial contrast jump)
            float edgeEnergy = 0.0f;
            for (int y = 1; y < f.height - 1; y += sy * 2) {
                const uint8_t* r0 = f.pixels.data() + static_cast<size_t>(y) * f.width * 4;
                const uint8_t* r1 = f.pixels.data() + static_cast<size_t>(y + 1) * f.width * 4;
                for (int x = 1; x < f.width - 1; x += sx * 2) {
                    edgeEnergy += std::abs(int(r0[x * 4 + 1]) - int(r1[x * 4 + 1])) / 255.0f;
                }
            }
            float score = l1 + std::min(0.20f, edgeEnergy * 0.0004f);
            if (score > 0.36f && (t - lastCut) > 0.8) {
                outLocalCutTimes.push_back(t);
                lastCut = t;
            }
        }
        prevHist = hist;
        hasPrev = true;
    }
    return !outLocalCutTimes.empty();
}

bool MediaEngine::computeStabilizationKeyframes(const std::string& filePathUtf8, double startSec, double durationSec,
                                                int samples, std::vector<Keyframe>& outKeyframes) {
    std::vector<Keyframe> rawTrack;
    if (!trackObjectPath(filePathUtf8, startSec, durationSec, samples, rawTrack) || rawTrack.size() < 3) return false;
    outKeyframes = rawTrack;
    // Smooth trajectory (3-tap moving average) and apply inverse high-frequency shake compensation + 1.08x border crop
    for (size_t i = 0; i < rawTrack.size(); ++i) {
        size_t i0 = (i > 0) ? i - 1 : 0;
        size_t i2 = (i + 1 < rawTrack.size()) ? i + 1 : rawTrack.size() - 1;
        float smoothX = (rawTrack[i0].posX + rawTrack[i].posX + rawTrack[i2].posX) / 3.0f;
        float smoothY = (rawTrack[i0].posY + rawTrack[i].posY + rawTrack[i2].posY) / 3.0f;
        outKeyframes[i].posX = std::clamp(smoothX - rawTrack[i].posX, -0.07f, 0.07f);
        outKeyframes[i].posY = std::clamp(smoothY - rawTrack[i].posY, -0.07f, 0.07f);
        outKeyframes[i].scale = 1.08f; // hides stabilized edges cleanly
    }
    return true;
}


bool MediaEngine::trackCamera3DPath(const std::string& filePathUtf8, double startSec, double durationSec,
                                    int samples, std::vector<Keyframe>& outKeyframes) {
    if (!trackPlanarSurfacePath(filePathUtf8, startSec, durationSec, samples, outKeyframes)) return false;
    // Derive roll rotation from horizontal/vertical skew differential for 6-DOF camera parallax
    for (auto& kf : outKeyframes) {
        kf.rotationDeg = std::clamp((kf.skewX - kf.skewY) * 0.35f, -15.0f, 15.0f);
    }
    return true;
}


void MediaEngine::compositeBlendLayer(FrameBGRA& baseDst, const FrameBGRA& topSrc, BlendMode mode, float opacity) {
    if (!baseDst.valid() || !topSrc.valid() || baseDst.width != topSrc.width || baseDst.height != topSrc.height) return;
    const size_t nPx = static_cast<size_t>(baseDst.width) * baseDst.height;
    uint8_t* bp = baseDst.pixels.data();
    const uint8_t* tp = topSrc.pixels.data();
    auto blendCh = [&](float b, float t) -> float {
        switch (mode) {
            case BlendMode::Multiply:   return b * t;
            case BlendMode::Screen:     return 1.0f - (1.0f - b) * (1.0f - t);
            case BlendMode::Overlay:    return (b < 0.5f) ? (2.0f * b * t) : (1.0f - 2.0f * (1.0f - b) * (1.0f - t));
            case BlendMode::SoftLight:  return (1.0f - 2.0f * t) * b * b + 2.0f * t * b;
            case BlendMode::HardLight:  return (t < 0.5f) ? (2.0f * b * t) : (1.0f - 2.0f * (1.0f - b) * (1.0f - t));
            case BlendMode::ColorDodge: return (t >= 0.995f) ? 1.0f : std::min(1.0f, b / std::max(0.005f, 1.0f - t));
            case BlendMode::ColorBurn:  return (t <= 0.005f) ? 0.0f : std::max(0.0f, 1.0f - (1.0f - b) / std::max(0.005f, t));
            case BlendMode::Difference: return std::abs(b - t);
            case BlendMode::Add:        return std::min(1.0f, b + t);
            default:                    return t;
        }
    };
    for (size_t i = 0; i < nPx; ++i) {
        float a = (tp[i * 4 + 3] / 255.0f) * std::clamp(opacity, 0.0f, 1.0f);
        if (a < 0.002f) continue;
        for (int ch = 0; ch < 3; ++ch) {
            float bv = bp[i * 4 + ch] / 255.0f;
            float tv = tp[i * 4 + ch] / 255.0f;
            float res = bv * (1.0f - a) + blendCh(bv, tv) * a;
            bp[i * 4 + ch] = static_cast<uint8_t>(std::clamp(res * 255.0f, 0.0f, 255.0f));
        }
    }
}

} // namespace cc
