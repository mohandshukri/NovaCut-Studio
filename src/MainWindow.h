#pragma once

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include "Types.h"
#include "TimelineEngine.h"
#include "MediaEngine.h"
#include "AudioEngine.h"
#include "AICloudBridge.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <functional>
#include <array>

namespace cc {

enum class LeftTab : int {
    Media       = 0,
    Text        = 1,
    Filters     = 2,
    Effects     = 3,
    Transitions = 4,
    Stickers    = 5,
    Audio       = 6,
    AI          = 7
};

enum class InspectorTab : int {
    Basic = 0,
    Color = 1,
    Audio = 2,
    FX    = 3
};

enum class DragMode : int {
    None = 0,
    ScrubPlayhead,
    MoveClip,
    TrimClipLeft,
    TrimClipRight,
    Slider,
    FreehandDraw,
    ScrollTimelineH,
    MoveCanvasClip,
    PhotoBrush
};

struct UiRect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
    D2D1_RECT_F d2d() const { return D2D1::RectF(x, y, x + w, y + h); }
};

struct SliderDef {
    int id = 0;
    std::wstring label;
    float minVal = 0.0f;
    float maxVal = 1.0f;
    float value  = 0.0f;
    std::wstring suffix;
    UiRect trackRect;
};

struct CachedClipBitmap {
    Microsoft::WRL::ComPtr<ID2D1Bitmap> bmp;
    double decodedSec = -999.0;
    uint64_t pipelineHash = 0;
    int width = 0;
    int height = 0;
};

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool create(HINSTANCE hInst, int nCmdShow);
    int runMessageLoop();

private:
    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    bool initGraphics();
    bool createDeviceResources();
    void discardDeviceResources();

    void render();
    void renderTopBar();
    void renderLeftPanel();
    void renderPreviewPanel();
    void renderInspectorPanel();
    void renderTimelinePanel();
    void renderAllToolsModal();
    void renderAssetHubModal();
    void renderExportSettingsModal();
    void renderExportOverlay();
    void renderHintBar();
    void renderToast();

    void computeLayout();

    void onMouseDown(float x, float y);
    void onMouseMove(float x, float y);
    void onMouseUp(float x, float y);
    void onMouseWheel(float x, float y, short delta, bool ctrlDown);
    void onKeyDown(WPARAM vk);
    void onDropFiles(HDROP hDrop);
    void onTimer();

    void setPlaying(bool play);
    void actionNewProject();
    void actionOpenProject();
    void actionSaveProject();
    void actionImportMedia(bool audioOnly = false);
    void importFilePath(const std::string& utf8Path);
    void actionSplitSelected();
    void actionDeleteSelected();
    void actionDuplicateSelected();
    void actionEditSelectedText();
    void actionToggleKeyframe();
    void actionImportSrt();
    void actionAutoCaptions();
    void actionCycleResolution();
    void actionCycleAspectRatio();
    void actionFreezeFrame();
    void actionExtractAudio();
    void actionCycleTextAnim();
    void actionToggleVoiceRec();
    void actionExtractPngFrame();
    void actionLoadCubeLut();
    void actionExportCubeLut();
    void actionAutoWhiteBalance();
    void updateVideoScopes(const FrameBGRA& frame);
    void renderVideoScopesOverlay();
    void renderAudioSpectrumOverlay();
    void actionBatchEdit();
    void actionBatchExport();
    void actionAutoSaveDraft(bool silent = true);
    void actionRecoverDraft();
    void actionSaveVersionSnapshot();
    void actionCustomPreset(bool loadMode);
    void actionCloudSyncBackup();
    void actionSearchAssets();
    void actionAddToRenderQueue();
    void actionRunRenderQueue();
    void postUiTask(std::function<void()> fn);
    void runAiTaskAsync(const std::wstring& busyMsg, std::function<void()> bgWork);
    void actionRestoreVersionHistory();
    void actionGenerateDiskProxies();
    void actionRunObjectTrack(bool planarMode);
    void actionDetectSceneCuts();
    void actionRunOpticalStabilization();
    void actionCopyEffects();
    void actionPasteEffects();
    void actionDetectBeats();
    void actionToggleFullScreen();
    void actionStartExport();
    const wchar_t* tr(const wchar_t* en, const wchar_t* ar) const { return m_arabicUi ? ar : en; }
    void showToast(const std::wstring& msg);

    void applySliderValue(int sliderId, float val);
    void rebuildInspectorSliders();

    void drawText(const std::wstring& text, const UiRect& r, IDWriteTextFormat* fmt,
                  D2D1_COLOR_F color, DWRITE_TEXT_ALIGNMENT hAlign = DWRITE_TEXT_ALIGNMENT_LEADING);
    void drawText(const std::wstring& text, const UiRect& r, const Microsoft::WRL::ComPtr<IDWriteTextFormat>& fmt,
                  D2D1_COLOR_F color, DWRITE_TEXT_ALIGNMENT hAlign = DWRITE_TEXT_ALIGNMENT_LEADING) {
        drawText(text, r, fmt.Get(), color, hAlign);
    }
    void drawRoundRect(const UiRect& r, float radius, D2D1_COLOR_F fill,
                       bool hasBorder = false, D2D1_COLOR_F border = {}, float borderW = 1.0f);
    bool uploadFrameToBitmap(const FrameBGRA& frame, Microsoft::WRL::ComPtr<ID2D1Bitmap>& outBmp);

    float timeToTimelineX(double sec) const;
    double timelineXToTime(float x) const;
    int timelineYToTrack(float y) const;
    UiRect getClipRect(const Clip& c) const;
    bool promptTextDialog(const std::wstring& title, std::wstring& ioText);

private:
    HWND m_hwnd = nullptr;
    HINSTANCE m_hInst = nullptr;
    float m_width = 1440.0f;
    float m_height = 860.0f;

    Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> m_rt;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brush;

    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fontTitle;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fontBold;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fontRegular;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fontSmall;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fontMono;

    TimelineEngine m_engine;
    MediaEngine m_media;
    AudioEngine m_audio;

    std::unordered_map<uint64_t, Microsoft::WRL::ComPtr<ID2D1Bitmap>> m_thumbBitmaps;
    std::unordered_map<uint64_t, CachedClipBitmap> m_previewCache;
    FrameBGRA m_tempFrame;
    FrameBGRA m_gradedFrame;

    UiRect m_rcTopBar;
    UiRect m_rcLeftPanel;
    UiRect m_rcPreviewPanel;
    UiRect m_rcInspectorPanel;
    UiRect m_rcTimelinePanel;
    UiRect m_rcTimelineToolbar;
    UiRect m_rcTimelineRuler;
    UiRect m_rcTimelineTracks;
    UiRect m_rcTimelineScrollH;
    UiRect m_rcVideoCanvas;
    UiRect m_rcHintBar;

    struct ButtonHit {
        int id = 0;
        UiRect rect;
        uint64_t param = 0;
    };
    std::vector<ButtonHit> m_buttons;
    std::vector<SliderDef> m_sliders;

    LeftTab m_leftTab = LeftTab::Media;
    bool m_playing = false;
    bool m_snapEnabled = true;
    double m_pixelsPerSec = 70.0;
    double m_scrollSec = 0.0;
    double m_lastTickSec = 0.0;
    std::string m_currentProjectPath;
    bool m_ffmpegFound = false;

    // Part 3: Aspect Ratio & Resolution Preset & Live FPS counter
    bool m_arabicUi  = true;
    bool m_lightMode = false;
    bool m_beforeAfter = false;
    bool m_fullScreenPreview = false;
    InspectorTab m_inspTab = InspectorTab::Basic;
    int m_aspectMode = 0; // 0 = 16:9, 1 = 9:16, 2 = 1:1
    int m_resPreset  = 2; // 0=480p, 1=720p, 2=1080p, 3=2K, 4=4K
    int m_fpsPreset  = 1; // 0=24, 1=30, 2=60
    int m_bitrateIdx = 1; // 0=Low(3M), 1=Rec(8M), 2=High(16M), 3=Ultra(32M)
    bool m_exportMov = false;
    int m_hdrMode    = 0; // 0=Off, 1=HDR, 2=Smart HDR
    bool m_showExportModal = false;
    DWORD m_lastAutoSaveTick = 0;
    int m_voiceRecCounter = 1;
    bool m_hasCopiedFx = false;
    Clip m_copiedFx;
    std::vector<double> m_beatMarkers;
    bool m_showAllToolsModal = false;
    bool m_drawMode = false;
    float m_inspScrollY = 0.0f;
    float m_leftScrollY = 0.0f;
    int m_filterCategory = 0;
    int m_fxCategory = 0; // 0=All, 1=Hits, 2=Life, 3=Movies, 4=Photo, 5=Mono, 6=Retro, 7=Style, 8=Food, 9=Night
    float m_trackScrollY = 0.0f;
    AICloudConfig m_aiConfig;
    int m_aiGenCounter = 1;
    double timelineTotalRangeSec() const { return std::max(180.0, m_engine.totalDuration() + 30.0); }
    int m_fpsCounter = 0;
    int m_displayedFps = 60;
    // Batch 12: Live Video Scopes (0=Off, 1=Histogram, 2=Waveform, 3=Vectorscope, 4=All 3)
    int m_scopeMode = 0;
    bool m_showAudioSpectrum = false;
    PhotoTool m_photoTool = PhotoTool::None;
    float m_lastBrushNx = 0.5f;
    float m_lastBrushNy = 0.5f;
    // Batch 16: Studio Workflow (Auto-Save, Version History, Offline Mode, Preview Quality, Search & Favorites)
    int   m_versionCounter   = 1;
    int   m_previewQuality   = 1; // 0=1/4 Fast, 1=1/2 Proxy, 2=Full HQ
    bool  m_offlineMode      = true;
    bool  m_hasCustomPreset  = false;
    ColorAdjust m_savedPresetColor;
    EffectSettings m_savedPresetFx;
    std::string m_assetSearchQuery;
    std::vector<int> m_favEffects;
    // Batch 17: Broadcast Export & Render Queue
    int  m_exportCodec     = 0;     // 0=H.264, 1=H.265, 2=AV1, 3=ProRes, 4=VP9, 5=GIF, 6=PNG Seq, 7=Alpha PNG
    bool m_exportVfr       = false; // false=CFR, true=VFR
    int  m_exportAudioCfg  = 0;     // 0=48k/16b, 1=48k/24b, 2=44.1k/16b
    int  m_exportColorSp   = 0;     // 0=Rec.709, 1=sRGB, 2=Rec.2020, 3=DCI-P3
    std::vector<ExportSettings> m_renderQueue;
    // V26: 2-Page All Tools 160+ Hub & Keyboard Shortcuts Modal
    int  m_allToolsPage = 0;
    // Built-In Unified 626+ Asset Hub Modal (520 Stickers + 64 SFX + 42 Filters — 100% Native Direct2D)
    bool m_showAssetHubModal = false;
    int  m_hubTab  = 0; // 0=Stickers(520), 1=Audio(64), 2=Filters(42)
    int  m_hubCat  = 0; // 0=All, 1..N=Category
    int  m_hubPage = 0;
    bool m_showShortcutsModal = false;
    // V30: Non-Blocking Async AI Worker (Zero UI Freeze / Zero Not Responding)
    std::atomic<bool> m_aiBusy{false};
    // V32: 8-Band HSL active band, Manual Clone Stamp Source & Brush Size/Pressure
    int   m_hslBandIdx    = 0;
    float m_cloneSrcNx    = -1.0f;
    float m_cloneSrcNy    = -1.0f;
    float m_brushRadius   = 0.055f;
    float m_brushPressure = 1.0f;
    std::mutex m_uiTaskMutex;
    std::vector<std::function<void()>> m_pendingUiTasks;
    std::array<uint32_t, 64> m_histR{};
    std::array<uint32_t, 64> m_histG{};
    std::array<uint32_t, 64> m_histB{};
    std::array<uint32_t, 64> m_histL{};
    std::array<uint32_t, 64 * 32> m_waveBins{};
    std::array<uint32_t, 48 * 48> m_vecBins{};
    uint64_t m_scopeStamp = 0;
    DWORD m_fpsLastTick = 0;

    DragMode m_dragMode = DragMode::None;
    uint64_t m_dragClipId = 0;
    int m_activeSliderId = 0;
    float m_dragStartX = 0.0f;
    float m_dragStartY = 0.0f;
    float m_dragOrigPosX = 0.0f;
    float m_dragOrigPosY = 0.0f;
    DWORD m_lastPlayRedrawTick = 0;
    double m_dragOrigStart = 0.0;
    double m_dragOrigEnd = 0.0;
    float m_hoverX = -1.0f;
    float m_hoverY = -1.0f;
    int m_lastHoveredBtn = -1;

    std::atomic<bool> m_exporting{false};
    std::atomic<float> m_exportProgress{0.0f};
    std::mutex m_exportMutex;
    std::string m_exportStatus;
    std::string m_exportError;
    std::thread m_exportThread;

    std::mutex m_toastMutex;
    std::wstring m_toastText;
    DWORD m_toastExpireTick = 0;
};

} // namespace cc

#endif // _WIN32
