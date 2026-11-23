#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace cc {

enum class MediaType : int {
    Video = 0,
    Audio = 1,
    Image = 2,
    Text  = 3
};

enum class TrackType : int {
    Text  = 0,
    Video = 1,
    Audio = 2
};

enum class FilterPreset : int {
    None       = 0,
    Cinematic  = 1,
    Vivid      = 2,
    WarmSunset = 3,
    CoolCyber  = 4,
    NoirBW     = 5,
    Vintage    = 6,
    TealOrange = 7,
    MoodyDark  = 8,
    GoldenHour = 9,
    PastelSoft = 10,
    CyberNeon  = 11,
    FreshLime  = 12,
    VHS        = 13,
    Blur1      = 14,
    Blur2      = 15,
    Fog        = 16,
    AnimeBW    = 17,
    Reindeer   = 18,
    Nebula     = 19,
    Cartoon    = 20,
    FoodFresh  = 21,
    NightGlow  = 22,
    PhotoBooth = 23,
    LifeDay    = 24
};

enum class TransitionType : int {
    None       = 0,
    Fade       = 1,
    Dissolve   = 2,
    BlurTrans  = 3,
    ZoomIn     = 4,
    ZoomOut    = 5,
    SlideLeft  = 6,
    SlideRight = 7,
    SlideUp    = 8,
    SlideDown  = 9,
    Spin       = 10,
    Flip       = 11,
    Shake      = 12,
    PullIn     = 13,
    PullOut    = 14,
    WipeRight  = 15,
    MaskTrans  = 16,
    SplitTrans = 17,
    CubeTurn   = 18,
    MotionGfx  = 19
};

enum class EffectType : int {
    None       = 0,
    Vignette   = 1,
    GlitchRGB  = 2,
    FilmGrain  = 3,
    SoftGlow   = 4,
    RetroCRT   = 5,
    MotionBlur = 6,
    Sparkles   = 7,
    LoveGlow   = 8,
    LensFlare  = 9,
    WaveDistort= 10,
    ZoomBlur   = 11,
    Anaglyph3D = 12,
    RainDrop   = 13,
    NeonEdge   = 14,
    HardShadow   = 15,
    VHSNoise     = 16,
    LightLeaks   = 17,
    Smoke        = 18,
    Fire         = 19,
    Snow         = 20,
    Liquid       = 21,
    Displacement = 22,
    FractalNoise = 23,
    ChromaticAb  = 24,
    DepthMap     = 25,
    Object3D     = 26,
    Particles    = 27
};

enum class BlendMode : int {
    Normal     = 0,
    Multiply   = 1,
    Screen     = 2,
    Overlay    = 3,
    SoftLight  = 4,
    HardLight  = 5,
    ColorDodge = 6,
    ColorBurn  = 7,
    Difference = 8,
    Add        = 9
};

enum class PhotoTool : int {
    None          = 0,
    Heal          = 1,
    Clone         = 2,
    ContentFill   = 3,
    Liquify       = 4,
    Dodge         = 5,
    Burn          = 6,
    BlurBrush     = 7,
    SharpenBrush  = 8,
    SelectBrush   = 9,
    MagneticLasso = 10
};

struct PhotoBrushDot {
    PhotoTool tool = PhotoTool::None;
    float nx = 0.5f;
    float ny = 0.5f;
    float dx = 0.0f;
    float dy = 0.0f;
    float radius = 0.055f;
    float srcNx  = -1.0f; // V32: Manual Clone Stamp source X (Alt+Click)
    float srcNy  = -1.0f; // V32: Manual Clone Stamp source Y
    float pressure = 1.0f;// V32: Liquify / Brush pressure (0.2..1.5)
};

inline const char* photoToolName(PhotoTool t) {
    switch (t) {
        case PhotoTool::Heal:          return "Healing Brush";
        case PhotoTool::Clone:         return "Clone Stamp";
        case PhotoTool::ContentFill:   return "Content Fill";
        case PhotoTool::Liquify:       return "Liquify";
        case PhotoTool::Dodge:         return "Dodge";
        case PhotoTool::Burn:          return "Burn";
        case PhotoTool::BlurBrush:     return "Blur Brush";
        case PhotoTool::SharpenBrush:  return "Sharpen Brush";
        case PhotoTool::SelectBrush:   return "Select Brush";
        case PhotoTool::MagneticLasso: return "Magnetic Lasso";
        default:                       return "OFF";
    }
}

enum class TextEffectType : int {
    None       = 0,
    NeonGlow   = 1,
    Extrude3D  = 2,
    RetroSplit = 3,
    GoldLuxury = 4
};

enum class TextAnimType : int {
    None       = 0,
    Typewriter = 1,
    BouncePop  = 2,
    FadeRise   = 3,
    Pulse      = 4
};

enum class MaskType : int {
    None       = 0,
    Circle     = 1,
    RoundedBox = 2,
    SplitH     = 3,
    CinemaBars = 4,
    MirrorH    = 5,
    RadialFade = 6,
    VectorPath = 7
};

enum class FrameStyle : int {
    None       = 0,
    WhiteClean = 1,
    NeonCyan   = 2,
    CinemaGold = 3,
    FilmStrip  = 4,
    Polaroid   = 5,
    ShadowBox  = 6
};

enum class StudioBgPreset : int {
    None         = 0,
    DarkStudio   = 1,
    NeonGradient = 2,
    SunsetSky    = 3,
    CleanWhite   = 4
};

enum class SpeedRampType : int {
    None      = 0,
    HeroSlow  = 1,
    FlashIn   = 2,
    Bullet    = 3,
    Montage   = 4
};

enum class CurvePreset : int {
    Linear  = 0,
    SCurve  = 1,
    Matte   = 2,
    HighKey = 3,
    FilmLog = 4
};

enum class StabilizationLevel : int {
    Off         = 0,
    Minimal     = 1,
    Recommended = 2,
    High        = 3
};

struct DrawPoint {
    float nx = 0.0f; // normalized 0..1 inside video canvas
    float ny = 0.0f;
};

struct DrawStroke {
    uint32_t colorARGB = 0xFF00E5D1;
    float thickness    = 4.0f;
    std::vector<DrawPoint> points;
};

struct ChromaKeySettings {
    bool enabled         = false;
    uint32_t keyColorRGB = 0x00B140;
    float similarity     = 0.32f;
    float smoothness     = 0.12f;
    float spillSuppress  = 0.50f;
};

struct EffectSettings {
    EffectType type = EffectType::None;
    float intensity = 0.65f;
};

struct AudioDspSettings {
    float pan            = 0.0f;  // -1.0 Left .. +1.0 Right
    float pitch          = 1.0f;  //  0.6 .. 1.6 (Voice Changer / Pitch)
    float bassBoost      = 0.0f;  //  0.0 .. 1.0
    float midEq          = 0.0f;  // -1.0 .. +1.0 (3-Band EQ Mid)
    float treble         = 0.0f;  // -1.0 .. +1.0
    float echo           = 0.0f;  //  0.0 .. 1.0
    float noiseReduction = 0.0f;  //  0.0 .. 1.0
    float compressor     = 0.0f;  //  0.0 .. 1.0 (Dynamic Range Compressor)
    float reverb         = 0.0f;  // Batch 14: 0.0 .. 1.0 Multi-tap Room Reverb
    float deEsser        = 0.0f;  // Batch 14: 0.0 .. 1.0 High-Freq Sibilance Suppressor
    float stereoWidth    = 1.0f;  // Batch 14: 0.0 Mono .. 2.0 Ultra-Wide M/S Imager
    float limiter        = 0.95f; // Batch 14: 0.2 .. 1.0 Brickwall Ceiling Limiter
    bool autoDucking     = false;
    bool vocalIsolate    = false; // Mid-channel vocal boost / side suppress
};

struct ColorAdjust {
    float brightness   = 0.0f;   // -1.0 .. +1.0
    float contrast     = 1.0f;   //  0.2 ..  2.0
    float exposure     = 0.0f;   // -1.0 .. +1.0
    float saturation   = 1.0f;   //  0.0 ..  2.0
    float vibrance     = 0.0f;   // -1.0 .. +1.0
    float temperature  = 0.0f;   // -1.0 .. +1.0
    float tint         = 0.0f;   // -1.0 .. +1.0
    float highlights   = 0.0f;   // -1.0 .. +1.0
    float shadows      = 0.0f;   // -1.0 .. +1.0
    float whites       = 0.0f;   // -1.0 .. +1.0
    float blacks       = 0.0f;   // -1.0 .. +1.0
    float sharpness    = 0.0f;   //  0.0 ..  1.0
    float clarity      = 0.0f;   //  0.0 ..  1.0
    float blur         = 0.0f;   //  0.0 ..  1.0
    float hslHue       = 0.0f;   // -180 .. +180 (HSL Hue Shift)
    float hslRedSat    = 0.0f;   // -1.0 .. +1.0 (HSL Red Saturation)
    float hslGreenSat  = 0.0f;   // -1.0 .. +1.0 (HSL Green Saturation)
    float hslBlueSat   = 0.0f;   // -1.0 .. +1.0 (HSL Blue Saturation)
    float wheelShadow  = 0.0f;   // -1.0 Cool .. +1.0 Warm (Color Wheel Shadows)
    float wheelMid     = 0.0f;   // -1.0 Cool .. +1.0 Warm (Color Wheel Midtones)
    float wheelHigh    = 0.0f;   // -1.0 Cool .. +1.0 Warm (Color Wheel Highlights)
    float skinSmooth   = 0.0f;   //  0.0 ..  1.0 (Portrait Smooth)
    float teethWhiten  = 0.0f;   //  0.0 ..  1.0
    bool redEyeRemove  = false;
    bool autoBgRemove  = false;  // Batch 2: Auto corner-sampled Background Remover
    float faceRetouch  = 0.0f;   // Batch 2: Portrait glow & eye pop
    float faceSlim     = 0.0f;   // Batch 2: -0.4 .. +0.4 Face Reshape
    float bodySlim     = 0.0f;   // Batch 2: -0.4 .. +0.4 Body Reshape
    float enhanceHD    = 0.0f;   // Batch 2: 0..1 Super-Clarity / HD Enhance
    float portraitBokeh= 0.0f;   // Batch 2: 0..1 Radial Portrait Bokeh
    float canvasExpand = 0.0f;
    float aiRelight      = 0.0f;
    float lensDistort    = 0.0f; // Batch 11: -0.5..+0.5 Barrel/Pincushion Lens Correction
    float rollingShutter = 0.0f; // Batch 11: -0.25..+0.25 Rolling Shutter Shear Correction
    float curveR         = 0.0f; // Batch 12: -0.5..+0.5 Per-Channel RGB Red Curve
    float curveG         = 0.0f; // Batch 12: -0.5..+0.5 Per-Channel RGB Green Curve
    float curveB         = 0.0f; // Batch 12: -0.5..+0.5 Per-Channel RGB Blue Curve
    float hslTargetHue   = 0.0f; // Batch 12: 0..360 deg HSL Secondary Target Hue
    float hslSecShift    = 0.0f; // Batch 12: -180..+180 deg HSL Secondary Hue Shift
    float hslSecSat      = 0.0f; // Batch 12: -1..+1 HSL Secondary Saturation
    float splitShadowHue = 215.0f; // Batch 12: 0..360 deg Split Toning Shadow Hue
    float splitHighHue   = 38.0f;  // Batch 12: 0..360 deg Split Toning Highlight Hue
    float splitAmount    = 0.0f;   // Batch 12: 0..1 Split Toning Strength
    bool  falseColor     = false;  // Batch 12: Cinema IRE False Color Exposure Overlay
    float photoDenoise   = 0.0f;   // Batch 15: 0..1 Edge-preserving Photo Noise Reduction
    float gradientOverlay= 0.0f;   // Batch 15: 0..1 Cinema Gradient Tool overlay
    float rawFilmicTone  = 0.0f;   // V31: 0..1 RAW ACES Filmic Highlight Roll-Off
    float hslBandSat[8]  = {0,0,0,0,0,0,0,0}; // V32: Full 8-Band HSL Saturation (R,O,Y,G,A,B,P,M)
    float hslBandLum[8]  = {0,0,0,0,0,0,0,0}; // V32: Full 8-Band HSL Luminance
    float curveShadowPt  = 0.0f;   // V32: Multi-point RGB Curve Shadow control (-0.4..+0.4)
    float curveHighPt    = 0.0f;   // V32: Multi-point RGB Curve Highlight control (-0.4..+0.4)
    float perspWarpH     = 0.0f;   // V39: -0.45..+0.45 True Projective Horizontal Keystone Warp
    float perspWarpV     = 0.0f;   // V39: -0.45..+0.45 True Projective Vertical Keystone Warp
    std::vector<PhotoBrushDot> brushDots; // Batch 15: Non-destructive Canvas Retouch Brushes
    CurvePreset curve  = CurvePreset::Linear;
    float opacity      = 1.0f;   //  0.0 ..  1.0
    FilterPreset preset = FilterPreset::None;
    std::string lutPath;

    uint64_t hash(const ChromaKeySettings& ck, const EffectSettings& fx, MaskType mask, float cropX, float cropY, float maskFeather = 0.0f) const {
        auto q = [](float v) -> uint64_t {
            return static_cast<uint64_t>(static_cast<int32_t>(std::round(v * 200.0f)) & 0xFFFF);
        };
        uint64_t h1 = (q(brightness)) | (q(contrast) << 16) | (q(saturation) << 32) | (q(temperature) << 48);
        uint64_t h2 = (q(exposure)) ^ (q(vibrance) << 10) ^ (q(tint) << 20) ^ (q(highlights) << 30) ^ (q(shadows) << 40) ^
                      (q(whites) << 5) ^ (q(blacks) << 15) ^ (q(clarity) << 25) ^ (q(hslHue) << 35) ^ (q(skinSmooth) << 45) ^ (q(teethWhiten) << 50) ^ (q(hslRedSat) << 7) ^ (q(hslGreenSat) << 17) ^ (q(hslBlueSat) << 27) ^ (q(wheelShadow) << 11) ^ (q(wheelMid) << 21) ^ (q(wheelHigh) << 31) ^ (q(maskFeather) << 41);
        uint64_t h3 = (q(sharpness)) ^ (q(blur) << 16) ^ (q(cropX) << 32) ^ (q(cropY) << 48) ^
                      (static_cast<uint64_t>(preset) * 2654435761ULL) ^
                      (static_cast<uint64_t>(curve) * 65537ULL) ^
                      (static_cast<uint64_t>((redEyeRemove ? 1 : 0) | (autoBgRemove ? 2 : 0)) * 131071ULL) ^ (q(faceRetouch) << 3) ^ (q(faceSlim) << 13) ^ (q(bodySlim) << 23) ^ (q(enhanceHD) << 33) ^ (q(portraitBokeh) << 43) ^ (q(canvasExpand) << 53) ^ (q(aiRelight) << 19) ^ (q(lensDistort) << 29) ^ (q(rollingShutter) << 39) ^ (q(curveR) << 4) ^ (q(curveG) << 14) ^ (q(curveB) << 24) ^ (q(hslTargetHue) << 6) ^ (q(hslSecShift) << 16) ^ (q(hslSecSat) << 26) ^ (q(splitShadowHue) << 9) ^ (q(splitHighHue) << 21) ^ (q(splitAmount) << 31) ^ (static_cast<uint64_t>(falseColor ? 1 : 0) * 999983ULL) ^
                      (static_cast<uint64_t>(fx.type) * 40503ULL) ^
                      (static_cast<uint64_t>(mask) * 11400714819323198485ULL) ^
                      (q(fx.intensity) << 8);
        if (ck.enabled) {
            h3 ^= (static_cast<uint64_t>(ck.keyColorRGB) << 24) ^
                  (q(ck.similarity) << 4) ^ (q(ck.smoothness) << 20) ^ 0x9E3779B97F4A7C15ULL;
        }
        h3 ^= (q(photoDenoise) << 12) ^ (q(gradientOverlay) << 28) ^ (q(rawFilmicTone) << 40) ^ (q(curveShadowPt) << 8) ^ (q(curveHighPt) << 20) ^ (q(perspWarpH) << 15) ^ (q(perspWarpV) << 35) ^ (static_cast<uint64_t>(brushDots.size()) * 2654435761ULL);
        for (int bi = 0; bi < 8; ++bi) h3 ^= (q(hslBandSat[bi]) << (bi * 4)) ^ (q(hslBandLum[bi]) << (bi * 4 + 2));
        for (char ch : lutPath) h3 = (h3 * 131ULL) ^ static_cast<uint64_t>(ch);
        return h1 ^ (h2 * 1099511628211ULL) ^ h3;
    }
};

struct Transform2D {
    float posX        = 0.0f;
    float posY        = 0.0f;
    float scale       = 1.0f;
    float rotationDeg = 0.0f;
    float skewX       = 0.0f;   // Perspective tilt X (-30..30 deg)
    float skewY       = 0.0f;   // Perspective tilt Y (-30..30 deg)
    bool flipH        = false;
    bool flipV        = false;
    float cropX       = 0.0f;
    float cropY       = 0.0f;
};

enum class KeyframeEasing : int {
    Linear    = 0,
    EaseIn    = 1,
    EaseOut   = 2,
    EaseInOut = 3,
    Bounce    = 4
};

inline const char* keyframeEasingName(KeyframeEasing e) {
    switch (e) {
        case KeyframeEasing::Linear:    return "Linear";
        case KeyframeEasing::EaseIn:    return "Ease In";
        case KeyframeEasing::EaseOut:   return "Ease Out";
        case KeyframeEasing::Bounce:    return "Bounce";
        default:                        return "Bezier";
    }
}

struct Keyframe {
    double localTime  = 0.0;
    float posX        = 0.0f;
    float posY        = 0.0f;
    float scale       = 1.0f;
    float rotationDeg = 0.0f;
    float opacity     = 1.0f;
    float skewX       = 0.0f;
    float skewY       = 0.0f;
    KeyframeEasing easing = KeyframeEasing::EaseInOut;
};

struct EvaluatedPose {
    Transform2D transform;
    float opacity = 1.0f;
    float wipeProgress = 1.0f;
    float textReveal01 = 1.0f;
};

struct TextStyle {
    std::string content   = "CapCutVS Title";
    std::string fontName  = "Segoe UI";
    float fontSize        = 52.0f;
    uint32_t colorARGB    = 0xFFFFFFFF;
    uint32_t bgARGB       = 0xAA101218;
    uint32_t strokeARGB   = 0xFF000000;
    bool bold             = true;
    bool shadow           = true;
    bool hasBackground    = false;
    float strokeWidth     = 0.0f;   // 0..6 px Text Stroke
    float tracking        = 0.0f;   // 0..10 Text Tracking (letter spacing)
    float curve           = 0.0f;   // -1..+1 Text Curve arch
    TextEffectType effect = TextEffectType::None;
    TextAnimType anim     = TextAnimType::None;
    bool isStickerBadge   = false;
};

struct MediaAsset {
    uint64_t id = 0;
    std::string name;
    std::string filePath;
    MediaType type = MediaType::Video;
    double durationSec = 5.0;
    int width = 1920;
    int height = 1080;
    double fps = 30.0;
    bool hasAudio = true;
};

struct Clip {
    uint64_t id = 0;
    uint64_t assetId = 0;
    int trackIndex = 0;
    MediaType type = MediaType::Video;
    std::string name;
    std::string sourcePath;

    double timelineStart = 0.0;
    double sourceIn = 0.0;
    double duration = 5.0;
    double speed = 1.0;
    SpeedRampType speedRamp = SpeedRampType::None;
    float speedStart = 1.0f; // V33: Manual Speed Ramp start multiplier (0.25..4.0)
    float speedMid   = 1.0f; // V33: Manual Speed Ramp midpoint multiplier
    float speedEnd   = 1.0f; // V33: Manual Speed Ramp end multiplier
    float volume = 1.0f;
    bool muted = false;
    bool freezeFrame = false;
    bool reversed    = false;
    bool bgBlur      = false;   // V7: Blurred canvas background fill

    float audioFadeIn  = 0.0f;
    float audioFadeOut = 0.0f;
    AudioDspSettings audioDsp;

    Transform2D transform;
    ColorAdjust color;
    TextStyle text;

    TransitionType transitionIn = TransitionType::None;
    double transitionDuration   = 0.6;
    double fadeOutDuration      = 0.0;
    std::vector<Keyframe> keyframes;

    ChromaKeySettings chromaKey;
    EffectSettings effect;
    MaskType mask     = MaskType::None;
    float maskFeather = 0.0f;    // 0.0 .. 1.0 soft feathered mask edge
    FrameStyle frame  = FrameStyle::None;
    BlendMode blendMode = BlendMode::Normal;
    bool clippingMask   = false; // Batch 15: Clipping Mask to underlying layer
    KeyframeEasing keyframeEasing = KeyframeEasing::EaseInOut; // V31: Keyframe motion curve
    std::string proxyPath;       // V31: Generated low-res disk proxy path
    bool isSmartObject  = true;  // V39: Non-destructive Smart Object Layer
    StabilizationLevel stabilization = StabilizationLevel::Off;
    bool opticalFlow  = false;
    StudioBgPreset studioBg = StudioBgPreset::None;

    double timelineEnd() const { return timelineStart + duration; }

    double sourceTimeAt(double timelineTime) const {
        if (type == MediaType::Image) return 0.0;
        if (freezeFrame) return sourceIn;
        double localT = std::clamp(timelineTime - timelineStart, 0.0, duration);
        double u = (duration > 1e-4) ? (localT / duration) : 0.0;
        double warpedU = u;
        switch (speedRamp) {
            case SpeedRampType::HeroSlow: warpedU = u < 0.3 ? u * 1.5 : (u < 0.7 ? 0.45 + (u - 0.3) * 0.4 : 0.61 + (u - 0.7) * 1.3); break;
            case SpeedRampType::FlashIn:  warpedU = std::pow(u, 0.55); break;
            case SpeedRampType::Bullet:   warpedU = 0.5 + 0.5 * std::sin((u - 0.5) * 3.14159); break;
            case SpeedRampType::Montage:  warpedU = std::pow(u, 1.45); break;
            default: break;
        }
        // V36: Exact analytical integral of manual Speed Ramp (speedStart -> speedMid -> speedEnd)
        if (std::abs(speedStart - 1.0f) > 0.02f || std::abs(speedMid - 1.0f) > 0.02f || std::abs(speedEnd - 1.0f) > 0.02f) {
            double s0 = std::clamp(static_cast<double>(speedStart), 0.2, 4.0);
            double s1 = std::clamp(static_cast<double>(speedMid),   0.2, 4.0);
            double s2 = std::clamp(static_cast<double>(speedEnd),   0.2, 4.0);
            double totalArea = 0.25 * (s0 + 2.0 * s1 + s2);
            double area = 0.0;
            if (warpedU <= 0.5) {
                area = s0 * warpedU + (s1 - s0) * warpedU * warpedU;
            } else {
                double v = warpedU - 0.5;
                area = 0.25 * (s0 + s1) + s1 * v + (s2 - s1) * v * v;
            }
            warpedU = area / std::max(1e-4, totalArea);
        }
        double mappedLocal = std::clamp(warpedU, 0.0, 1.0) * duration;
        if (reversed) {
            return std::max(0.0, sourceIn + (duration - mappedLocal) * speed);
        }
        return std::max(0.0, sourceIn + mappedLocal * speed);
    }
};

struct Track {
    int index = 0;
    std::string label;
    TrackType type = TrackType::Video;
    bool muted = false;
    bool hidden = false;
    bool locked = false;
};

struct ExportSettings {
    std::string outputPath = "output.mp4";
    int width = 1920;
    int height = 1080;
    int fps = 30;
    int bitrateMbps = 8;        // 3, 8, 16, 32, 64 Mbps (Custom Bitrate)
    bool formatMov = false;     // false = MP4, true = MOV
    int hdrMode = 0;            // 0 = Off, 1 = HDR, 2 = Smart HDR
    int codec = 0;              // Batch 17: 0=H.264, 1=H.265/HEVC, 2=AV1, 3=ProRes, 4=VP9, 5=GIF, 6=PNG Seq, 7=Alpha PNG
    bool variableFps = false;   // Batch 17: false=CFR (Constant), true=VFR (Variable)
    int audioSampleRate = 48000;// Batch 17: 44100 or 48000 Hz
    int audioBitDepth = 16;     // Batch 17: 16 or 24 bit
    int colorSpace = 0;         // Batch 17: 0=Rec.709, 1=sRGB, 2=Rec.2020, 3=DCI-P3
    int crf = 20;
    std::string preset = "medium";
    std::string audioBitrate = "192k";
};

struct Project {
    std::string name = "Untitled Project";
    int canvasWidth = 1920;
    int canvasHeight = 1080;
    double fps = 30.0;
    uint64_t nextAssetId = 1;
    uint64_t nextClipId = 1;
    std::vector<MediaAsset> assets;
    std::vector<Track> tracks;
    std::vector<Clip> clips;
    std::vector<DrawStroke> drawings;
    std::vector<DrawStroke> eraserStrokes; // Batch 2: Healing Eraser strokes
    std::vector<double> markers;           // V26: Timeline chapter/beat markers (seconds)

    void initDefaultTracks() {
        tracks.clear();
        tracks.push_back({0, "T1", TrackType::Text,  false, false, false});
        tracks.push_back({1, "V2", TrackType::Video, false, false, false});
        tracks.push_back({2, "V1", TrackType::Video, false, false, false});
        tracks.push_back({3, "A1", TrackType::Audio, false, false, false});
    }
};

struct SubtitleEntry {
    double startSec = 0.0;
    double endSec   = 2.0;
    std::string text;
};

struct Lut3D {
    int size = 0;
    std::vector<float> data;
    bool valid() const { return size >= 2 && data.size() == static_cast<size_t>(size) * size * size * 3; }
};

inline bool loadCubeLutFile(const std::string& pathUtf8, Lut3D& outLut) {
    outLut = Lut3D{};
    std::ifstream ifs(pathUtf8);
    if (!ifs) return false;
    std::string line;
    int n = 0;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("LUT_3D_SIZE", 0) == 0) {
            std::istringstream iss(line.substr(11));
            iss >> n;
            if (n >= 2 && n <= 65) {
                outLut.size = n;
                outLut.data.reserve(static_cast<size_t>(n) * n * n * 3);
            }
            continue;
        }
        if (n >= 2 && (std::isdigit(static_cast<unsigned char>(line[0])) || line[0] == '-' || line[0] == '.')) {
            std::istringstream iss(line);
            float r = 0, g = 0, b = 0;
            if (iss >> r >> g >> b) {
                outLut.data.push_back(std::clamp(r, 0.0f, 1.0f));
                outLut.data.push_back(std::clamp(g, 0.0f, 1.0f));
                outLut.data.push_back(std::clamp(b, 0.0f, 1.0f));
            }
        }
    }
    return outLut.valid();
}

inline std::string formatTimecode(double sec, double fps = 30.0) {
    if (sec < 0.0) sec = 0.0;
    int totalSec = static_cast<int>(std::floor(sec));
    int hours = totalSec / 3600;
    int mins  = (totalSec % 3600) / 60;
    int secs  = totalSec % 60;
    int frames = static_cast<int>(std::floor((sec - totalSec) * fps));
    if (frames < 0) frames = 0;
    if (frames >= static_cast<int>(fps)) frames = static_cast<int>(fps) - 1;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d:%02d", hours, mins, secs, frames);
    return std::string(buf);
}

inline const char* filterPresetName(FilterPreset p) {
    switch (p) {
        case FilterPreset::None:       return "None";
        case FilterPreset::Cinematic:  return "Cinematic";
        case FilterPreset::Vivid:      return "Vivid";
        case FilterPreset::WarmSunset: return "Warm Sunset";
        case FilterPreset::CoolCyber:  return "Cool Cyber";
        case FilterPreset::NoirBW:     return "Noir B&W";
        case FilterPreset::Vintage:    return "Vintage";
        case FilterPreset::TealOrange: return "Teal & Orange";
        case FilterPreset::MoodyDark:  return "Moody Dark";
        case FilterPreset::GoldenHour: return "Golden Hour";
        case FilterPreset::PastelSoft: return "Pastel Soft";
        case FilterPreset::CyberNeon:  return "Cyber Neon";
        case FilterPreset::FreshLime:  return "Fresh Lime";
        case FilterPreset::VHS:        return "VHS Retro";
        case FilterPreset::Blur1:      return "Blur";
        case FilterPreset::Blur2:      return "Blur 2";
        case FilterPreset::Fog:        return "Fog";
        case FilterPreset::AnimeBW:    return "Anime B&W";
        case FilterPreset::Reindeer:   return "Reindeer";
        case FilterPreset::Nebula:     return "Nebula";
        case FilterPreset::Cartoon:    return "Cartoon";
        case FilterPreset::FoodFresh:  return "Food Tasty";
        case FilterPreset::NightGlow:  return "Night Scene";
        case FilterPreset::PhotoBooth: return "Photo Booth";
        case FilterPreset::LifeDay:    return "Life Natural";
        default:                       return "None";
    }
}

inline const char* transitionName(TransitionType t) {
    switch (t) {
        case TransitionType::None:       return "None";
        case TransitionType::Fade:       return "Fade";
        case TransitionType::Dissolve:   return "Dissolve";
        case TransitionType::BlurTrans:  return "Blur";
        case TransitionType::ZoomIn:     return "Zoom In";
        case TransitionType::ZoomOut:    return "Zoom Out";
        case TransitionType::SlideLeft:  return "Slide Left";
        case TransitionType::SlideRight: return "Slide Right";
        case TransitionType::SlideUp:    return "Slide Up";
        case TransitionType::SlideDown:  return "Slide Down";
        case TransitionType::Spin:       return "Spin";
        case TransitionType::Flip:       return "Flip";
        case TransitionType::Shake:      return "Shake";
        case TransitionType::PullIn:     return "Pull In";
        case TransitionType::PullOut:    return "Pull Out";
        case TransitionType::WipeRight:  return "Wipe";
        case TransitionType::MaskTrans:  return "Mask";
        case TransitionType::SplitTrans: return "Split";
        case TransitionType::CubeTurn:   return "Cube 3D";
        case TransitionType::MotionGfx:  return "MG Pop";
        default:                         return "None";
    }
}

inline const char* effectName(EffectType e) {
    switch (e) {
        case EffectType::None:       return "None";
        case EffectType::Vignette:   return "Vignette";
        case EffectType::GlitchRGB:  return "RGB Glitch";
        case EffectType::FilmGrain:  return "Film Grain";
        case EffectType::SoftGlow:   return "Soft Glow";
        case EffectType::RetroCRT:   return "Retro CRT";
        case EffectType::MotionBlur: return "Motion Blur";
        case EffectType::Sparkles:   return "Sparkles";
        case EffectType::LoveGlow:   return "Love Glow";
        case EffectType::LensFlare:  return "Lens Flare";
        case EffectType::WaveDistort:return "Wave Distort";
        case EffectType::ZoomBlur:   return "Radial Blur";
        case EffectType::Anaglyph3D: return "3D Anaglyph";
        case EffectType::RainDrop:   return "Nature Rain";
        case EffectType::NeonEdge:   return "Neon AI Edge";
        case EffectType::HardShadow:   return "Noir Shadow";
        case EffectType::VHSNoise:     return "VHS Tracking";
        case EffectType::LightLeaks:   return "Light Leaks";
        case EffectType::Smoke:        return "Smoke VFX";
        case EffectType::Fire:         return "Fire VFX";
        case EffectType::Snow:         return "Snow Fall";
        case EffectType::Liquid:       return "Liquid Wave";
        case EffectType::Displacement: return "Displace Map";
        case EffectType::FractalNoise: return "Fractal Noise";
        case EffectType::ChromaticAb:  return "Chromatic Ab.";
        case EffectType::DepthMap:     return "Depth Map";
        case EffectType::Object3D:     return "3D Object";
        case EffectType::Particles:    return "Particles";
        default:                     return "None";
    }
}

inline const char* blendModeName(BlendMode b) {
    switch (b) {
        case BlendMode::Multiply:   return "Multiply";
        case BlendMode::Screen:     return "Screen";
        case BlendMode::Overlay:    return "Overlay";
        case BlendMode::SoftLight:  return "Soft Light";
        case BlendMode::HardLight:  return "Hard Light";
        case BlendMode::ColorDodge: return "Color Dodge";
        case BlendMode::ColorBurn:  return "Color Burn";
        case BlendMode::Difference: return "Difference";
        case BlendMode::Add:        return "Linear Add";
        default:                    return "Normal";
    }
}

inline const char* textEffectName(TextEffectType e) {
    switch (e) {
        case TextEffectType::None:       return "None";
        case TextEffectType::NeonGlow:   return "Neon Glow";
        case TextEffectType::Extrude3D:  return "3D Extrude";
        case TextEffectType::RetroSplit: return "Retro 3D";
        case TextEffectType::GoldLuxury: return "Gold";
        default:                         return "None";
    }
}

inline const char* textAnimName(TextAnimType a) {
    switch (a) {
        case TextAnimType::None:       return "None";
        case TextAnimType::Typewriter: return "Typewriter";
        case TextAnimType::BouncePop:  return "Bounce Pop";
        case TextAnimType::FadeRise:   return "Fade Rise";
        case TextAnimType::Pulse:      return "Pulse";
        default:                       return "None";
    }
}

inline const char* maskName(MaskType m) {
    switch (m) {
        case MaskType::None:       return "None";
        case MaskType::Circle:     return "Circle";
        case MaskType::RoundedBox: return "Rounded";
        case MaskType::SplitH:     return "Split";
        case MaskType::CinemaBars: return "2.35:1 Bars";
        case MaskType::MirrorH:    return "Mirror";
        case MaskType::RadialFade: return "Soft Oval";
        default:                   return "None";
    }
}

inline const char* frameStyleName(FrameStyle f) {
    switch (f) {
        case FrameStyle::None:       return "None";
        case FrameStyle::WhiteClean: return "White";
        case FrameStyle::NeonCyan:   return "Neon Cyan";
        case FrameStyle::CinemaGold: return "Gold";
        case FrameStyle::FilmStrip:  return "Film Strip";
        case FrameStyle::Polaroid:   return "Polaroid";
        case FrameStyle::ShadowBox:  return "Shadow Box";
        default:                     return "None";
    }
}

inline const char* speedRampName(SpeedRampType r) {
    switch (r) {
        case SpeedRampType::None:     return "None";
        case SpeedRampType::HeroSlow: return "Hero Slow";
        case SpeedRampType::FlashIn:  return "Flash In";
        case SpeedRampType::Bullet:   return "Bullet";
        case SpeedRampType::Montage:  return "Montage";
        default:                      return "None";
    }
}

inline const char* curvePresetName(CurvePreset c) {
    switch (c) {
        case CurvePreset::Linear:  return "Linear";
        case CurvePreset::SCurve:  return "S-Curve";
        case CurvePreset::Matte:   return "Matte Fade";
        case CurvePreset::HighKey: return "High Key";
        case CurvePreset::FilmLog: return "Film Log";
        default:                   return "Linear";
    }
}

inline const char* studioBgName(StudioBgPreset b) {
    switch (b) {
        case StudioBgPreset::None:         return "None";
        case StudioBgPreset::DarkStudio:   return "Dark Studio";
        case StudioBgPreset::NeonGradient: return "Neon Glow";
        case StudioBgPreset::SunsetSky:    return "Sunset Sky";
        case StudioBgPreset::CleanWhite:   return "Clean White";
        default:                           return "None";
    }
}

inline const char* stabLevelName(StabilizationLevel s) {
    switch (s) {
        case StabilizationLevel::Off:         return "Off";
        case StabilizationLevel::Minimal:     return "Minimal";
        case StabilizationLevel::Recommended: return "Balanced";
        case StabilizationLevel::High:        return "High";
        default:                              return "Off";
    }
}

inline float smoothstep01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float evaluateAudioGain(const Clip& c, double timelineTime) {
    float gain = c.volume;
    double localT = timelineTime - c.timelineStart;
    double remain = c.timelineEnd() - timelineTime;
    if (c.audioFadeIn > 0.01f && localT < c.audioFadeIn) {
        gain *= std::clamp(static_cast<float>(localT / c.audioFadeIn), 0.0f, 1.0f);
    }
    if (c.audioFadeOut > 0.01f && remain < c.audioFadeOut) {
        gain *= std::clamp(static_cast<float>(remain / c.audioFadeOut), 0.0f, 1.0f);
    }
    return std::max(0.0f, gain);
}

inline EvaluatedPose evaluateClipPose(const Clip& c, double timelineTime) {
    EvaluatedPose pose;
    pose.transform = c.transform;
    pose.opacity = c.color.opacity;
    pose.wipeProgress = 1.0f;
    pose.textReveal01 = 1.0f;

    double localT = std::max(0.0, timelineTime - c.timelineStart);

    if (!c.keyframes.empty()) {
        if (localT <= c.keyframes.front().localTime) {
            const auto& k = c.keyframes.front();
            pose.transform.posX = k.posX; pose.transform.posY = k.posY;
            pose.transform.scale = k.scale; pose.transform.rotationDeg = k.rotationDeg;
            pose.opacity = k.opacity;
        } else if (localT >= c.keyframes.back().localTime) {
            const auto& k = c.keyframes.back();
            pose.transform.posX = k.posX; pose.transform.posY = k.posY;
            pose.transform.scale = k.scale; pose.transform.rotationDeg = k.rotationDeg;
            pose.opacity = k.opacity;
        } else {
            for (size_t i = 0; i + 1 < c.keyframes.size(); ++i) {
                const auto& a = c.keyframes[i];
                const auto& b = c.keyframes[i + 1];
                if (localT >= a.localTime && localT <= b.localTime) {
                    double span = std::max(1e-6, b.localTime - a.localTime);
                    float raw = std::clamp(static_cast<float>((localT - a.localTime) / span), 0.0f, 1.0f);
                    float u = raw;
                    switch (c.keyframeEasing) {
                        case KeyframeEasing::Linear:    u = raw; break;
                        case KeyframeEasing::EaseIn:    u = raw * raw * raw; break;
                        case KeyframeEasing::EaseOut: { float inv = 1.0f - raw; u = 1.0f - inv * inv * inv; break; }
                        case KeyframeEasing::Bounce: {
                            if (raw < 0.727f) u = 1.89f * raw * raw;
                            else { float d = raw - 0.863f; u = 1.0f - 4.5f * (0.0185f - d * d); u = std::clamp(u, 0.0f, 1.0f); }
                            break;
                        }
                        default: u = raw * raw * (3.0f - 2.0f * raw); break; // Cubic Bezier EaseInOut
                    }
                    pose.transform.posX        = a.posX + (b.posX - a.posX) * u;
                    pose.transform.posY        = a.posY + (b.posY - a.posY) * u;
                    pose.transform.scale       = a.scale + (b.scale - a.scale) * u;
                    pose.transform.rotationDeg = a.rotationDeg + (b.rotationDeg - a.rotationDeg) * u;
                    pose.transform.skewX       = a.skewX + (b.skewX - a.skewX) * u;
                    pose.transform.skewY       = a.skewY + (b.skewY - a.skewY) * u;
                    pose.opacity               = a.opacity + (b.opacity - a.opacity) * u;
                    break;
                }
            }
        }
    }

    if (c.transitionIn != TransitionType::None && c.transitionDuration > 0.01 && localT < c.transitionDuration) {
        float rawU = std::clamp(static_cast<float>(localT / c.transitionDuration), 0.0f, 1.0f);
        float u = smoothstep01(rawU);
        float inv = 1.0f - u;
        switch (c.transitionIn) {
            case TransitionType::Fade:       pose.opacity *= u; break;
            case TransitionType::Dissolve:   pose.opacity *= rawU; break;
            case TransitionType::BlurTrans:  pose.transform.scale *= (1.12f - 0.12f * u); pose.opacity *= u; break;
            case TransitionType::ZoomIn:     pose.transform.scale *= (0.35f + 0.65f * u); pose.opacity *= u; break;
            case TransitionType::ZoomOut:    pose.transform.scale *= (1.85f - 0.85f * u); pose.opacity *= u; break;
            case TransitionType::SlideLeft:  pose.transform.posX += inv * 1.4f; pose.opacity *= std::min(1.0f, u * 1.6f); break;
            case TransitionType::SlideRight: pose.transform.posX -= inv * 1.4f; pose.opacity *= std::min(1.0f, u * 1.6f); break;
            case TransitionType::SlideUp:    pose.transform.posY += inv * 1.2f; pose.opacity *= std::min(1.0f, u * 1.6f); break;
            case TransitionType::SlideDown:  pose.transform.posY -= inv * 1.2f; pose.opacity *= std::min(1.0f, u * 1.6f); break;
            case TransitionType::Spin:       pose.transform.rotationDeg -= inv * 180.0f; pose.transform.scale *= (0.5f + 0.5f * u); pose.opacity *= u; break;
            case TransitionType::Flip:       pose.transform.skewX += inv * 38.0f; pose.transform.scale *= (0.25f + 0.75f * u); pose.opacity *= u; break;
            case TransitionType::Shake:      pose.transform.posX += std::sin(rawU * 38.0f) * inv * 0.10f; pose.transform.posY += std::cos(rawU * 31.0f) * inv * 0.07f; pose.transform.scale *= (1.08f - 0.08f * u); break;
            case TransitionType::PullIn:     pose.transform.scale *= (2.2f - 1.2f * u); pose.transform.rotationDeg += inv * 14.0f; pose.opacity *= std::min(1.0f, u * 1.8f); break;
            case TransitionType::PullOut:    pose.transform.scale *= (0.2f + 0.8f * u); pose.transform.rotationDeg -= inv * 14.0f; pose.opacity *= std::min(1.0f, u * 1.8f); break;
            case TransitionType::WipeRight:  pose.wipeProgress = u; break;
            case TransitionType::MaskTrans:  pose.wipeProgress = std::pow(u, 0.7f); pose.transform.scale *= (0.85f + 0.15f * u); break;
            case TransitionType::SplitTrans: pose.wipeProgress = u; pose.transform.posY += inv * 0.25f; break;
            case TransitionType::CubeTurn:   pose.transform.posX += inv * 0.9f; pose.transform.skewY += inv * 24.0f; pose.opacity *= std::min(1.0f, u * 1.5f); break;
            case TransitionType::MotionGfx:  pose.transform.scale *= (0.4f + 0.6f * u + std::sin(rawU * 3.14159f) * 0.18f * inv); pose.transform.skewX -= inv * 18.0f; pose.opacity *= std::min(1.0f, u * 2.0f); break;
            default: break;
        }
    }

    if (c.type == MediaType::Text && c.text.anim != TextAnimType::None) {
        constexpr double kAnimDur = 0.75;
        switch (c.text.anim) {
            case TextAnimType::Typewriter: {
                double twDur = std::min(std::max(0.5, c.duration * 0.65), 2.5);
                pose.textReveal01 = std::clamp(static_cast<float>(localT / twDur), 0.05f, 1.0f);
                break;
            }
            case TextAnimType::BouncePop: {
                if (localT < kAnimDur) {
                    float t = static_cast<float>(localT / kAnimDur);
                    float overshoot = 1.0f + std::sin(t * 3.14159f) * 0.28f * (1.0f - t);
                    float s = (t < 0.25f) ? (t / 0.25f) * 1.15f : overshoot;
                    pose.transform.scale *= std::max(0.1f, s);
                    pose.opacity *= std::min(1.0f, t * 4.0f);
                }
                break;
            }
            case TextAnimType::FadeRise: {
                if (localT < kAnimDur) {
                    float u = smoothstep01(static_cast<float>(localT / kAnimDur));
                    pose.transform.posY += (1.0f - u) * 0.14f;
                    pose.opacity *= u;
                }
                break;
            }
            case TextAnimType::Pulse: {
                float p = 1.0f + 0.07f * std::sin(static_cast<float>(localT * 5.0));
                pose.transform.scale *= p;
                break;
            }
            default: break;
        }
    }

    if (c.fadeOutDuration > 0.01) {
        double remain = c.timelineEnd() - timelineTime;
        if (remain < c.fadeOutDuration) {
            pose.opacity *= smoothstep01(static_cast<float>(remain / c.fadeOutDuration));
        }
    }
    // Stabilization: crops shaky borders smoothly & dampens micro-jitter
    if (c.stabilization != StabilizationLevel::Off) {
        float stabZoom = (c.stabilization == StabilizationLevel::Minimal) ? 1.06f
                       : (c.stabilization == StabilizationLevel::Recommended ? 1.12f : 1.18f);
        pose.transform.scale *= stabZoom;
        float damp = (c.stabilization == StabilizationLevel::High) ? 0.004f : 0.002f;
        pose.transform.posX += std::sin(static_cast<float>(localT * 1.4)) * damp;
        pose.transform.posY += std::cos(static_cast<float>(localT * 1.1)) * damp;
    }
    pose.opacity = std::clamp(pose.opacity, 0.0f, 1.0f);
    return pose;
}

inline std::vector<SubtitleEntry> parseSrtText(const std::string& raw) {
    std::vector<SubtitleEntry> out;
    size_t pos = 0;
    if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF &&
        static_cast<unsigned char>(raw[1]) == 0xBB && static_cast<unsigned char>(raw[2]) == 0xBF) {
        pos = 3;
    }

    auto readLine = [&](std::string& line) -> bool {
        if (pos >= raw.size()) return false;
        size_t nl = raw.find('\n', pos);
        if (nl == std::string::npos) { line = raw.substr(pos); pos = raw.size(); }
        else { line = raw.substr(pos, nl - pos); pos = nl + 1; }
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return true;
    };

    auto parseTs = [](const std::string& ts) -> double {
        int nums[4] = { 0, 0, 0, 0 };
        int idx = 0;
        bool inNum = false;
        for (char c : ts) {
            if (c >= '0' && c <= '9') {
                if (idx < 4) nums[idx] = nums[idx] * 10 + (c - '0');
                inNum = true;
            } else if (inNum) {
                ++idx;
                inNum = false;
                if (idx >= 4) break;
            }
        }
        if (inNum) ++idx;
        if (idx >= 4) return nums[0] * 3600.0 + nums[1] * 60.0 + nums[2] + nums[3] / 1000.0;
        return -1.0;
    };

    std::string line;
    while (readLine(line)) {
        size_t arrow = line.find("-->");
        if (arrow != std::string::npos) {
            double st = parseTs(line.substr(0, arrow));
            double en = parseTs(line.substr(arrow + 3));
            if (st >= 0.0 && en > st) {
                std::string text;
                std::string tline;
                while (readLine(tline) && !tline.empty()) {
                    if (!text.empty()) text += " ";
                    text += tline;
                }
                if (!text.empty()) out.push_back({ st, en, text });
            }
        }
    }
    return out;
}

} // namespace cc
