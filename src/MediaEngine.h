#pragma once

#include "Types.h"
#include <vector>
#include <string>
#include <memory>

namespace cc {

struct FrameBGRA {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels; // Premultiplied BGRA32, size = width * height * 4

    bool valid() const { return width > 0 && height > 0 && pixels.size() == static_cast<size_t>(width) * height * 4; }
};

class MediaEngine {
public:
    MediaEngine();
    ~MediaEngine();

    MediaEngine(const MediaEngine&) = delete;
    MediaEngine& operator=(const MediaEngine&) = delete;

    bool initialize();
    void shutdown();

    bool probeFile(const std::string& filePathUtf8, MediaAsset& outAsset);
    bool extractThumbnail(const std::string& filePathUtf8, MediaType type, int maxW, int maxH, FrameBGRA& outThumb);

    // Synchronous decode (used for export & thumbnails)
    bool decodeFrame(const std::string& filePathUtf8, MediaType type, double sourceTimeSec, FrameBGRA& outFrame);

    // V8 Ultra-Light Zero-Copy Async Video Frame Decoder (UI thread NEVER stalls or copies MBs!)
    void requestFrameAsync(const std::string& filePathUtf8, MediaType type, double sourceTimeSec);
    std::shared_ptr<const FrameBGRA> tryGetLatestFramePtr(const std::string& filePathUtf8, MediaType type,
                                                          double targetSec, double& outDecodedSec);

    // Pro Pipeline: Curves, Exposure, Whites/Blacks, Vibrance, HSL, Skin Smooth, Teeth Whiten, Red Eye,
    // 2-Pass Separable Blur, Sharpness/Clarity, .cube 3D LUT, Chroma Key, FX, Crop & Alpha Masking
    static void applyPipeline(const FrameBGRA& src,
                              const ColorAdjust& adj,
                              const ChromaKeySettings& ck,
                              const EffectSettings& fx,
                              FrameBGRA& dst,
                              MaskType mask = MaskType::None,
                              float cropX = 0.0f,
                              float cropY = 0.0f,
                              float maskFeather = 0.0f,
                              const FrameBGRA* opticalFlowPrev = nullptr,
                              float maskOffX = 0.0f,
                              float maskOffY = 0.0f);

    // Batch 12: Export current clip color grade as standard Adobe .cube 3D LUT (17x17x17)
    static bool exportCubeLutFile(const std::string& path, const ColorAdjust& adj, int size = 17);

    // Batch 12: Gray-World Auto White Balance analyzer (computes Temperature & Tint)
    static void computeAutoWhiteBalance(const FrameBGRA& frame, float& outTemp, float& outTint);

    // V31: Real Block-Matching Object Tracker & 4-Quadrant Planar Surface Tracker
    bool trackObjectPath(const std::string& filePathUtf8, double startSec, double durationSec,
                         int samples, std::vector<Keyframe>& outKeyframes);
    bool trackPlanarSurfacePath(const std::string& filePathUtf8, double startSec, double durationSec,
                                int samples, std::vector<Keyframe>& outKeyframes);

    // V32: Real Histogram-Delta Scene Cut Detector & Optical Trajectory Stabilization
    bool detectSceneCuts(const std::string& filePathUtf8, double startSec, double durationSec,
                         std::vector<double>& outLocalCutTimes);
    bool computeStabilizationKeyframes(const std::string& filePathUtf8, double startSec, double durationSec,
                                       int samples, std::vector<Keyframe>& outKeyframes);
    // V33: 6-DOF Parallax 3D Camera Tracker (pan/tilt/zoom/roll/perspective)
    bool trackCamera3DPath(const std::string& filePathUtf8, double startSec, double durationSec,
                           int samples, std::vector<Keyframe>& outKeyframes);
    // V39: Pixel-True 10-Mode Photoshop Blend Compositor
    static void compositeBlendLayer(FrameBGRA& baseDst, const FrameBGRA& topSrc, BlendMode mode, float opacity);

    bool saveFrameToPng(const FrameBGRA& frame, const std::string& filePathUtf8);
    void clearCache();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace cc
