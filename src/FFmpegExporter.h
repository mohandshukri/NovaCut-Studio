#pragma once

#include "Types.h"
#include <functional>
#include <string>

namespace cc {

using ExportProgressCallback = std::function<void(float progress01, const std::string& statusLine)>;

class FFmpegExporter {
public:
    // Auto-detects ffmpeg.exe: app directory -> E:\Rea\CapCut\Apps\6.4.0.26\ffmpeg.exe -> system PATH
    static std::string findFFmpegPath();

    // Builds the full FFmpeg command line with filter_complex for video, image, text, color filters, and audio
    static std::string buildCommandLine(
        const std::string& ffmpegExePath,
        const Project& project,
        const ExportSettings& settings,
        double totalDurationSec);

    // Executes FFmpeg synchronously (call from background worker thread) and streams progress 0..1
    // V31: On-disk 640x360 Fast H.264 Proxy Generator
    static bool generateProxyFile(const std::string& srcVideoUtf8, const std::string& outProxyMp4Utf8, std::string& outError);

    static bool exportProject(
        const Project& project,
        const ExportSettings& settings,
        double totalDurationSec,
        ExportProgressCallback onProgress,
        std::string& outError);
};

} // namespace cc
