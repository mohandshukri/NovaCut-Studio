#pragma once

#include "Types.h"
#include <memory>
#include <string>
#include <vector>
#include <array>

namespace cc {

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool initialize();
    void shutdown();

    void preloadAudioAsync(const std::string& filePathUtf8);
    std::vector<float> getWaveformPeaks(const std::string& filePathUtf8) const;

    // Auto Captions speech segmentation & Beat Detection
    std::vector<SubtitleEntry> detectSpeechSegments(const std::string& filePathUtf8,
                                                    double clipTimelineStart,
                                                    double clipDuration) const;
    std::vector<double> detectBeats(const std::string& filePathUtf8,
                                    double clipTimelineStart,
                                    double clipDuration) const;

    // Live synchronized multi-track audio mixer with DSP (Bass, Treble, Echo, Noise Gate, Pan, Pitch, Ducking, Limiter)
    void startPlayback(const Project& projectSnapshot, double startTimeSec);
    void stopPlayback();
    void updateProjectSnapshot(const Project& projectSnapshot);
    bool isPlaying() const;

    // V6: Live Microphone Voice Recorder / Voice Over (saves standard 48kHz Stereo WAV)
    bool startRecording();
    bool stopRecordingToWav(const std::string& outWavPathUtf8);
    bool isRecording() const;

    // Batch 3: 9 Built-in Sound Effects + 4 Royalty-Free Music Loops + Windows SAPI Text-to-Speech!
    static bool generateSfxWav(int sfxIndex, const std::string& outWavPathUtf8);
    static bool generateHubSoundWav(int hubIdx0to63, const std::string& outWavPathUtf8);
    static bool generateMusicLoopWav(int trackIndex, const std::string& outWavPathUtf8);
    static bool synthesizeTextToSpeechWav(const std::string& textUtf8, const std::string& outWavPathUtf8);

    // Batch 14: Live 16-Band Audio Spectrum Visualizer + Auto Silence Trim Detector
    void getSpectrumBands(std::array<float, 16>& outBands) const;
    bool detectNonSilentRange(const std::string& filePathUtf8, double clipDuration,
                              double& outTrimInSec, double& outTrimOutSec) const;

    void clearCache();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace cc
