#ifdef _WIN32

#include "MainWindow.h"
#include "ProjectIO.h"
#include "FFmpegExporter.h"
#include "resource.h"

#include <commdlg.h>
#include <shlwapi.h>
#include <windowsx.h>
#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

using Microsoft::WRL::ComPtr;

namespace cc {

namespace {

D2D1_COLOR_F kColBg         = { 0.078f, 0.078f, 0.086f, 1.0f };
D2D1_COLOR_F kColPanel      = { 0.118f, 0.118f, 0.133f, 1.0f };
D2D1_COLOR_F kColCard       = { 0.157f, 0.157f, 0.180f, 1.0f };
D2D1_COLOR_F kColCardHover  = { 0.200f, 0.204f, 0.235f, 1.0f };
D2D1_COLOR_F kColBorder     = { 0.196f, 0.196f, 0.227f, 1.0f };
D2D1_COLOR_F kColAccent     = { 0.000f, 0.898f, 0.820f, 1.0f };
D2D1_COLOR_F kColAccentDim  = { 0.000f, 0.620f, 0.565f, 1.0f };
D2D1_COLOR_F kColGold       = { 1.000f, 0.847f, 0.302f, 1.0f };
D2D1_COLOR_F kColPurple     = { 0.650f, 0.420f, 0.960f, 1.0f };
D2D1_COLOR_F kColTextMain   = { 0.949f, 0.957f, 0.973f, 1.0f };
D2D1_COLOR_F kColTextMuted  = { 0.596f, 0.616f, 0.659f, 1.0f };
D2D1_COLOR_F kColDarkText   = { 0.060f, 0.070f, 0.080f, 1.0f };
D2D1_COLOR_F kColClipVideo  = { 0.118f, 0.360f, 0.478f, 1.0f };
D2D1_COLOR_F kColClipImage  = { 0.150f, 0.420f, 0.520f, 1.0f };
D2D1_COLOR_F kColClipText   = { 0.431f, 0.247f, 0.541f, 1.0f };
D2D1_COLOR_F kColClipAudio  = { 0.133f, 0.439f, 0.353f, 1.0f };
D2D1_COLOR_F kColDanger     = { 0.880f, 0.280f, 0.320f, 1.0f };
D2D1_COLOR_F kColGreen      = { 0.160f, 0.780f, 0.460f, 1.0f };
D2D1_COLOR_F kColAmber      = { 0.960f, 0.680f, 0.180f, 1.0f };

void applyThemePalette(bool lightMode) {
    if (!lightMode) {
        // Obsidian Cinema Dark Theme
        kColBg        = { 0.075f, 0.078f, 0.090f, 1.0f };
        kColPanel     = { 0.112f, 0.116f, 0.133f, 1.0f };
        kColCard      = { 0.157f, 0.162f, 0.188f, 1.0f };
        kColCardHover = { 0.205f, 0.212f, 0.245f, 1.0f };
        kColBorder    = { 0.205f, 0.212f, 0.245f, 1.0f };
        kColAccent    = { 0.000f, 0.898f, 0.820f, 1.0f };
        kColAccentDim = { 0.000f, 0.560f, 0.515f, 1.0f };
        kColGold      = { 1.000f, 0.847f, 0.302f, 1.0f };
        kColPurple    = { 0.650f, 0.420f, 0.960f, 1.0f };
        kColTextMain  = { 0.949f, 0.957f, 0.973f, 1.0f };
        kColTextMuted = { 0.620f, 0.645f, 0.690f, 1.0f };
        kColDarkText  = { 0.050f, 0.060f, 0.075f, 1.0f };
        kColClipVideo = { 0.118f, 0.360f, 0.478f, 1.0f };
        kColClipImage = { 0.150f, 0.420f, 0.520f, 1.0f };
        kColClipText  = { 0.431f, 0.247f, 0.541f, 1.0f };
        kColClipAudio = { 0.133f, 0.439f, 0.353f, 1.0f };
    } else {
        // Studio Daylight Light Theme (100% crisp contrast across every panel!)
        kColBg        = { 0.922f, 0.935f, 0.956f, 1.0f };
        kColPanel     = { 0.985f, 0.988f, 0.996f, 1.0f };
        kColCard      = { 0.945f, 0.955f, 0.972f, 1.0f };
        kColCardHover = { 0.885f, 0.905f, 0.940f, 1.0f };
        kColBorder    = { 0.780f, 0.810f, 0.860f, 1.0f };
        kColAccent    = { 0.000f, 0.580f, 0.530f, 1.0f };
        kColAccentDim = { 0.000f, 0.450f, 0.415f, 1.0f };
        kColGold      = { 0.780f, 0.530f, 0.020f, 1.0f };
        kColPurple    = { 0.480f, 0.250f, 0.820f, 1.0f };
        kColTextMain  = { 0.090f, 0.105f, 0.135f, 1.0f };
        kColTextMuted = { 0.360f, 0.400f, 0.470f, 1.0f };
        kColDarkText  = { 0.990f, 0.995f, 1.000f, 1.0f };
        kColClipVideo = { 0.680f, 0.860f, 0.940f, 1.0f };
        kColClipImage = { 0.720f, 0.890f, 0.950f, 1.0f };
        kColClipText  = { 0.850f, 0.760f, 0.940f, 1.0f };
        kColClipAudio = { 0.700f, 0.900f, 0.820f, 1.0f };
    }
}

constexpr UINT_PTR kPlaybackTimerId = 1;
constexpr float kTrackHeaderW = 72.0f;
constexpr float kTrackRowH = 46.0f;

enum BtnId : int {
    BTN_NEW = 100,
    BTN_OPEN,
    BTN_SAVE,
    BTN_EXPORT,
    BTN_RES_CYCLE,
    BTN_ASPECT_CYCLE,
    BTN_TAB_MEDIA,
    BTN_TAB_TEXT,
    BTN_TAB_FILTERS,
    BTN_TAB_EFFECTS,
    BTN_TAB_TRANSITIONS,
    BTN_TAB_STICKERS,
    BTN_TAB_AUDIO,
    BTN_LANG_TOGGLE,
    BTN_STICKER_PRESET,
    BTN_FREEZE_FRAME,
    BTN_EXTRACT_AUDIO,
    BTN_CYCLE_TEXT_ANIM,
    BTN_FLIP_H,
    BTN_FLIP_V,
    BTN_ROT_90,
    BTN_FILL_SCREEN,
    BTN_AUTO_ENHANCE,
    BTN_SPEED_PRESET,
    BTN_TRIM_LEFT,
    BTN_TRIM_RIGHT,
    BTN_THEME_TOGGLE,
    BTN_BEFORE_AFTER,
    BTN_VOICE_REC,
    BTN_EXTRACT_PNG,
    BTN_FPS_CYCLE,
    BTN_FULLSCREEN,
    BTN_INSP_BASIC,
    BTN_INSP_COLOR,
    BTN_INSP_AUDIO,
    BTN_INSP_FX,
    BTN_TOGGLE_REVERSE,
    BTN_CYCLE_MASK,
    BTN_LOAD_LUT,
    BTN_COPY_FX,
    BTN_PASTE_FX,
    BTN_TOGGLE_DUCKING,
    BTN_DETECT_BEATS,
    BTN_ALL_TOOLS,
    BTN_DRAW_MODE,
    BTN_CLEAR_DRAW,
    BTN_SFX_ADD,
    BTN_TEMPLATE_ADD,
    BTN_CYCLE_RAMP,
    BTN_CYCLE_FRAME,
    BTN_CYCLE_CURVE,
    BTN_TOGGLE_BGBLUR,
    BTN_TOGGLE_REDEYE,
    BTN_TOGGLE_VOCAL,
    BTN_VOICE_PRESET,
    BTN_TOOL_JUMP,
    BTN_ADD_TRACK_T,
    BTN_ADD_TRACK_V,
    BTN_ADD_TRACK_A,
    BTN_RIPPLE_DEL,
    BTN_MERGE_NEXT,
    BTN_CYCLE_STAB,
    BTN_TOGGLE_OPTFLOW,
    BTN_TOGGLE_AUTOBG,
    BTN_CYCLE_STUDIOBG,
    BTN_CYCLE_FONT,
    BTN_TTS_GENERATE,
    BTN_MUSIC_ADD,
    BTN_AUDIO_ENHANCE,
    BTN_AUDIO_SYNC,
    BTN_TAB_AI,
    BTN_AI_APIKEY,
    BTN_AI_TOOL,
    BTN_FILTER_CAT,
    BTN_FX_CAT,
    BTN_MANUAL_CAPTION,
    BTN_TOGGLE_TXTBG,
    BTN_TOGGLE_TXTSHADOW,
    BTN_CYCLE_TXTEFFECT,
    BTN_SPEAK_TEXTCLIP,
    BTN_EXP_SET_RES,
    BTN_EXP_SET_FPS,
    BTN_EXP_SET_BITRATE,
    BTN_EXP_SET_FMT,
    BTN_EXP_SET_HDR,
    BTN_EXP_CONFIRM,
    BTN_EXP_CANCEL,
    BTN_NLE_JCUT,
    BTN_NLE_LCUT,
    BTN_NLE_SLIP,
    BTN_NLE_COMPOUND,
    BTN_NLE_NEST,
    BTN_NLE_MULTICAM,
    BTN_EXPORT_LUT,
    BTN_AUTO_WB,
    BTN_FALSE_COLOR,
    BTN_CYCLE_SCOPES,
    BTN_CYCLE_BLEND,
    BTN_REMOVE_SILENCE,
    BTN_AUDIO_RESTORE,
    BTN_TOGGLE_SPECTRUM,
    BTN_CYCLE_PHOTO_TOOL,
    BTN_CLEAR_BRUSHES,
    BTN_CLIPPING_MASK,
    BTN_BATCH_EDIT,
    BTN_BATCH_EXPORT,
    BTN_RECOVER_DRAFT,
    BTN_SAVE_VERSION,
    BTN_CUSTOM_PRESET,
    BTN_TOGGLE_OFFLINE,
    BTN_PREVIEW_QUAL,
    BTN_SEARCH_ASSETS,
    BTN_TOGGLE_FAV,
    BTN_CLOUD_SYNC,
    BTN_EXP_SET_CODEC,
    BTN_EXP_SET_VFR,
    BTN_EXP_SET_AUDIOCFG,
    BTN_EXP_SET_COLORSP,
    BTN_EXP_ADD_QUEUE,
    BTN_EXP_RUN_QUEUE,
    BTN_TOOLS_PAGE,
    BTN_ADD_MARKER,
    BTN_SHOW_SHORTCUTS,
    BTN_AI_LOCAL_MODEL,
    BTN_AI_TEST_LOCAL,
    BTN_CYCLE_EASING,
    BTN_OBJ_TRACK,
    BTN_PLANAR_TRACK,
    BTN_ROTO_KEYFRAME,
    BTN_RESTORE_VERSION,
    BTN_GEN_PROXIES,
    BTN_EXP_CLEAR_QUEUE,
    BTN_SCENE_DETECT,
    BTN_OPTICAL_STAB,
    BTN_CYCLE_HSL_BAND,
    BTN_CAM3D_TRACK,
    BTN_CLEAR_LUT,
    BTN_OPEN_ASSETHUB,
    BTN_HUB_TAB,
    BTN_HUB_CAT,
    BTN_HUB_PAGE,
    BTN_HUB_INSERT_STICKER,
    BTN_HUB_INSERT_AUDIO,
    BTN_HUB_APPLY_FILTER,
    BTN_IMPORT_MEDIA,
    BTN_IMPORT_AUDIO,
    BTN_ASSET_ADD,
    BTN_TEXT_PRESET,
    BTN_IMPORT_SRT,
    BTN_AUTO_CAPTIONS,
    BTN_FILTER_PRESET,
    BTN_EFFECT_PRESET,
    BTN_TRANS_PRESET,
    BTN_PLAY_PAUSE,
    BTN_JUMP_START,
    BTN_STEP_BACK,
    BTN_STEP_FWD,
    BTN_UNDO,
    BTN_REDO,
    BTN_SPLIT,
    BTN_DUPLICATE,
    BTN_DELETE,
    BTN_SNAP,
    BTN_ZOOM_OUT,
    BTN_ZOOM_IN,
    BTN_TRACK_HIDE,
    BTN_TRACK_MUTE,
    BTN_RESET_COLOR,
    BTN_TOGGLE_KEYFRAME,
    BTN_TOGGLE_CHROMA,
    BTN_CHROMA_COLOR,
    BTN_EDIT_TEXT,
    BTN_TEXT_COLOR
};

enum SldId : int {
    SLD_SCALE = 1,
    SLD_POS_X,
    SLD_POS_Y,
    SLD_ROTATION,
    SLD_OPACITY,
    SLD_VOLUME,
    SLD_FADE_IN,
    SLD_FADE_OUT,
    SLD_SPEED,
    SLD_TRANS_DUR,
    SLD_FX_INTENSITY,
    SLD_CHROMA_SIM,
    SLD_CHROMA_SMOOTH,
    SLD_BRIGHTNESS,
    SLD_CONTRAST,
    SLD_SATURATION,
    SLD_TEMP,
    SLD_FONTSIZE,
    SLD_CROP_X,
    SLD_CROP_Y,
    SLD_EXPOSURE,
    SLD_VIBRANCE,
    SLD_TINT,
    SLD_HIGHLIGHTS,
    SLD_SHADOWS,
    SLD_SHARPNESS,
    SLD_BLUR,
    SLD_AUDIO_PAN,
    SLD_AUDIO_PITCH,
    SLD_AUDIO_BASS,
    SLD_AUDIO_TREBLE,
    SLD_AUDIO_ECHO,
    SLD_AUDIO_NOISE,
    SLD_AUDIO_COMP,
    SLD_SKEW_X,
    SLD_SKEW_Y,
    SLD_WHITES,
    SLD_BLACKS,
    SLD_CLARITY,
    SLD_HSL_HUE,
    SLD_SKIN_SMOOTH,
    SLD_TEETH_WHITEN,
    SLD_FADE_OUT_VIS,
    SLD_MASK_FEATHER,
    SLD_HSL_RED,
    SLD_HSL_GREEN,
    SLD_HSL_BLUE,
    SLD_WHEEL_SHADOW,
    SLD_WHEEL_MID,
    SLD_WHEEL_HIGH,
    SLD_FACE_RETOUCH,
    SLD_FACE_SLIM,
    SLD_BODY_SLIM,
    SLD_ENHANCE_HD,
    SLD_PORTRAIT_BOKEH,
    SLD_CANVAS_EXPAND,
    SLD_AUDIO_MID,
    SLD_TXT_STROKE,
    SLD_TXT_TRACKING,
    SLD_TXT_CURVE,
    SLD_LENS_DISTORT,
    SLD_ROLL_SHUTTER,
    SLD_CURVE_R,
    SLD_CURVE_G,
    SLD_CURVE_B,
    SLD_HSL_TARGET,
    SLD_HSL_SECSHIFT,
    SLD_HSL_SECSAT,
    SLD_SPLIT_SH_HUE,
    SLD_SPLIT_HI_HUE,
    SLD_SPLIT_AMT,
    SLD_AUDIO_REVERB,
    SLD_AUDIO_DEESSER,
    SLD_AUDIO_WIDTH,
    SLD_AUDIO_LIMITER,
    SLD_PHOTO_DENOISE,
    SLD_GRADIENT_OVL,
    SLD_RAW_FILMIC,
    SLD_HSL_BAND_SAT,
    SLD_HSL_BAND_LUM,
    SLD_CURVE_SH_PT,
    SLD_CURVE_HI_PT,
    SLD_BRUSH_SIZE,
    SLD_BRUSH_PRESS,
    SLD_SPEED_START,
    SLD_SPEED_MID,
    SLD_SPEED_END,
    SLD_PERSP_WARP_H,
    SLD_PERSP_WARP_V
};

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), len);
    return w;
}

std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), len, nullptr, nullptr);
    return s;
}

D2D1_COLOR_F argbToD2D(uint32_t argb) {
    float a = ((argb >> 24) & 0xFF) / 255.0f;
    float r = ((argb >> 16) & 0xFF) / 255.0f;
    float g = ((argb >> 8)  & 0xFF) / 255.0f;
    float b = (argb & 0xFF) / 255.0f;
    return D2D1::ColorF(r, g, b, a);
}

struct TextPromptState {
    std::wstring result;
    bool accepted = false;
    HWND hEdit = nullptr;
};

LRESULT CALLBACK TextPromptProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<TextPromptState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            st = reinterpret_cast<TextPromptState*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));

            HFONT hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

            st->hEdit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", st->result.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                16, 18, 368, 30, hwnd, reinterpret_cast<HMENU>(101), nullptr, nullptr);
            SendMessageW(st->hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            SendMessageW(st->hEdit, EM_SETSEL, 0, -1);

            HWND hOk = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                     214, 64, 80, 30, hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
            HWND hCancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
                                         304, 64, 80, 30, hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
            SendMessageW(hOk, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            SendMessageW(hCancel, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            SetFocus(st->hEdit);
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDOK && st) {
                int len = GetWindowTextLengthW(st->hEdit);
                std::wstring buf(len + 1, L'\0');
                GetWindowTextW(st->hEdit, buf.data(), len + 1);
                buf.resize(len);
                st->result = buf;
                st->accepted = true;
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

MainWindow::MainWindow() = default;

MainWindow::~MainWindow() {
    setPlaying(false);
    if (m_exportThread.joinable()) m_exportThread.join();
    m_audio.shutdown();
    m_media.shutdown();
    discardDeviceResources();
}

bool MainWindow::create(HINSTANCE hInst, int nCmdShow) {
    m_hInst = hInst;
    m_media.initialize();
    m_audio.initialize();
    m_ffmpegFound = !FFmpegExporter::findFFmpegPath().empty();
    m_fpsLastTick = GetTickCount();
    m_lastAutoSaveTick = GetTickCount();
    m_aiConfig = AICloudBridge::loadConfig();

    if (!initGraphics()) return false;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = &MainWindow::WndProcStatic;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"CapCutVSMainWindow";
    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        wc.lpszClassName,
        L"NovaCut Studio 1.0 — Pro Video, Audio & Photo NLE",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1460, 880,
        nullptr, nullptr, hInst, this);

    if (!m_hwnd) return false;

    BOOL useDark = TRUE;
    typedef HRESULT (WINAPI* DwmSetAttrFn)(HWND, DWORD, LPCVOID, DWORD);
    if (HMODULE dwm = LoadLibraryW(L"dwmapi.dll")) {
        auto fn = reinterpret_cast<DwmSetAttrFn>(GetProcAddress(dwm, "DwmSetWindowAttribute"));
        if (fn) fn(m_hwnd, 20, &useDark, sizeof(useDark));
        FreeLibrary(dwm);
    }

    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);
    SetTimer(m_hwnd, kPlaybackTimerId, 16, nullptr);

    showToast(tr(L"CapCutVS Ultimate Ready! Fast GPU Export + Stickers + Text Anim + Freeze/Audio", L"جاهز! تصدير سريع GPU + ملصقات + حركات النصوص + تجميد وفصل الصوت"));
    return true;
}

int MainWindow::runMessageLoop() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindow::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->handleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE: {
            UINT w = LOWORD(lParam);
            UINT h = HIWORD(lParam);
            if (w > 0 && h > 0) {
                m_width = static_cast<float>(w);
                m_height = static_cast<float>(h);
                if (m_rt) m_rt->Resize(D2D1::SizeU(w, h));
                computeLayout();
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(m_hwnd, &ps);
            render();
            EndPaint(m_hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_LBUTTONDOWN:
            SetCapture(m_hwnd);
            onMouseDown(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)));
            return 0;
        case WM_MOUSEMOVE:
            onMouseMove(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)));
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            onMouseUp(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)));
            return 0;
        case WM_MOUSEWHEEL: {
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(m_hwnd, &pt);
            onMouseWheel(static_cast<float>(pt.x), static_cast<float>(pt.y),
                         GET_WHEEL_DELTA_WPARAM(wParam), (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0);
            return 0;
        }
        case WM_KEYDOWN:
            onKeyDown(wParam);
            return 0;
        case WM_DROPFILES:
            onDropFiles(reinterpret_cast<HDROP>(wParam));
            return 0;
        case WM_TIMER:
            if (wParam == kPlaybackTimerId) onTimer();
            return 0;
        case WM_DESTROY:
            setPlaying(false);
            KillTimer(m_hwnd, kPlaybackTimerId);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

bool MainWindow::initGraphics() {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2dFactory.GetAddressOf()))) return false;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf())))) return false;

    auto makeFmt = [&](float size, DWRITE_FONT_WEIGHT weight, const wchar_t* family, ComPtr<IDWriteTextFormat>& out) {
        m_dwriteFactory->CreateTextFormat(
            family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            size, L"en-us", &out);
        if (out) {
            out->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            out->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
    };

    makeFmt(15.5f, DWRITE_FONT_WEIGHT_BOLD,      L"Segoe UI", m_fontTitle);
    makeFmt(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, L"Segoe UI", m_fontBold);
    makeFmt(12.0f, DWRITE_FONT_WEIGHT_NORMAL,    L"Segoe UI", m_fontRegular);
    makeFmt(10.8f, DWRITE_FONT_WEIGHT_NORMAL,    L"Segoe UI", m_fontSmall);
    makeFmt(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, L"Consolas", m_fontMono);
    return true;
}

bool MainWindow::createDeviceResources() {
    if (m_rt) return true;
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    D2D1_SIZE_U sz = D2D1::SizeU(std::max<LONG>(1, rc.right - rc.left), std::max<LONG>(1, rc.bottom - rc.top));
    m_width = static_cast<float>(sz.width);
    m_height = static_cast<float>(sz.height);

    HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_hwnd, sz, D2D1_PRESENT_OPTIONS_IMMEDIATELY),
        &m_rt);
    if (FAILED(hr)) return false;

    m_rt->CreateSolidColorBrush(kColTextMain, &m_brush);
    computeLayout();
    return true;
}

void MainWindow::discardDeviceResources() {
    m_thumbBitmaps.clear();
    m_previewCache.clear();
    m_brush.Reset();
    m_rt.Reset();
}

void MainWindow::computeLayout() {
    const float topH = 48.0f;
    const float hintH = 24.0f;
    const float timelineH = std::clamp(m_height * 0.31f, 215.0f, 290.0f);
    const float middleH = std::max(180.0f, m_height - topH - timelineH - hintH);
    const float leftW = std::clamp(m_width * 0.24f, 295.0f, 360.0f);
    const float rightW = std::clamp(m_width * 0.23f, 275.0f, 340.0f);
    const float centerW = std::max(280.0f, m_width - leftW - rightW);

    m_rcTopBar         = { 0, 0, m_width, topH };
    m_rcLeftPanel      = { 0, topH, leftW, middleH };
    m_rcPreviewPanel   = { leftW, topH, centerW, middleH };
    m_rcInspectorPanel = { leftW + centerW, topH, rightW, middleH };
    m_rcTimelinePanel  = { 0, topH + middleH, m_width, timelineH };
    m_rcHintBar        = { 0, m_height - hintH, m_width, hintH };

    m_rcTimelineToolbar = { 0, m_rcTimelinePanel.y, m_width, 36.0f };
    m_rcTimelineRuler   = { kTrackHeaderW, m_rcTimelinePanel.y + 36.0f, std::max(100.0f, m_width - kTrackHeaderW), 26.0f };
    const float scrollBarH = 14.0f;
    m_rcTimelineTracks  = { 0, m_rcTimelineRuler.y + m_rcTimelineRuler.h, m_width,
                            std::max(80.0f, m_rcTimelinePanel.h - 62.0f - scrollBarH) };
    m_rcTimelineScrollH = { kTrackHeaderW, m_rcTimelinePanel.y + m_rcTimelinePanel.h - scrollBarH,
                            std::max(100.0f, m_width - kTrackHeaderW - 8.0f), scrollBarH - 2.0f };

    float availW = std::max(120.0f, m_rcPreviewPanel.w - 32.0f);
    float availH = std::max(90.0f, m_rcPreviewPanel.h - 86.0f);
    float aspect = (m_aspectMode == 1) ? (9.0f / 16.0f) : (m_aspectMode == 2 ? 1.0f : (16.0f / 9.0f));
    float cw = availW;
    float ch = cw / aspect;
    if (ch > availH) {
        ch = availH;
        cw = ch * aspect;
    }
    if (m_fullScreenPreview) {
        m_rcPreviewPanel = { 0, topH, m_width, std::max(200.0f, m_height - topH - hintH) };
        availW = std::max(120.0f, m_rcPreviewPanel.w - 24.0f);
        availH = std::max(90.0f, m_rcPreviewPanel.h - 80.0f);
        cw = availW; ch = cw / aspect;
        if (ch > availH) { ch = availH; cw = ch * aspect; }
    }
    m_rcVideoCanvas = {
        m_rcPreviewPanel.x + (m_rcPreviewPanel.w - cw) * 0.5f,
        m_rcPreviewPanel.y + 34.0f + (availH - ch) * 0.5f,
        cw, ch
    };
}

void MainWindow::drawText(const std::wstring& text, const UiRect& r, IDWriteTextFormat* fmt,
                          D2D1_COLOR_F color, DWRITE_TEXT_ALIGNMENT hAlign) {
    if (!m_rt || !fmt || !m_brush) return;
    fmt->SetReadingDirection(m_arabicUi ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    fmt->SetTextAlignment(hAlign);
    m_brush->SetColor(color);
    m_rt->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), fmt, r.d2d(), m_brush.Get(),
                    D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void MainWindow::drawRoundRect(const UiRect& r, float radius, D2D1_COLOR_F fill,
                               bool hasBorder, D2D1_COLOR_F border, float borderW) {
    if (!m_rt || !m_brush) return;
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(r.d2d(), radius, radius);
    m_brush->SetColor(fill);
    m_rt->FillRoundedRectangle(rr, m_brush.Get());
    if (hasBorder) {
        m_brush->SetColor(border);
        m_rt->DrawRoundedRectangle(rr, m_brush.Get(), borderW);
    }
}

bool MainWindow::uploadFrameToBitmap(const FrameBGRA& frame, ComPtr<ID2D1Bitmap>& outBmp) {
    if (!m_rt || !frame.valid()) return false;
    D2D1_SIZE_U sz = D2D1::SizeU(static_cast<UINT32>(frame.width), static_cast<UINT32>(frame.height));
    if (outBmp) {
        D2D1_SIZE_U cur = outBmp->GetPixelSize();
        if (cur.width == sz.width && cur.height == sz.height) {
            D2D1_RECT_U dstRect = D2D1::RectU(0, 0, sz.width, sz.height);
            return SUCCEEDED(outBmp->CopyFromMemory(&dstRect, frame.pixels.data(), static_cast<UINT32>(frame.width * 4)));
        }
        outBmp.Reset();
    }
    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    return SUCCEEDED(m_rt->CreateBitmap(sz, frame.pixels.data(), static_cast<UINT32>(frame.width * 4), props, &outBmp));
}

void MainWindow::render() {
    if (!createDeviceResources()) return;
    m_buttons.clear();

    // Update live FPS counter
    ++m_fpsCounter;
    DWORD nowTick = GetTickCount();
    if (nowTick - m_fpsLastTick >= 1000) {
        m_displayedFps = m_fpsCounter;
        m_fpsCounter = 0;
        m_fpsLastTick = nowTick;
    }

    m_rt->BeginDraw();
    applyThemePalette(m_lightMode);
    m_rt->Clear(kColBg);

    renderTopBar();
    if (m_fullScreenPreview) {
        renderPreviewPanel();
    } else {
        renderLeftPanel();
        renderPreviewPanel();
        renderInspectorPanel();
        renderTimelinePanel();
    }
    renderHintBar();
    if (m_showAllToolsModal) renderAllToolsModal();
    if (m_showAssetHubModal) renderAssetHubModal();
    if (m_showExportModal) renderExportSettingsModal();
    if (m_exporting.load()) renderExportOverlay();
    renderToast();

    HRESULT hr = m_rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        discardDeviceResources();
    }
}

void MainWindow::renderTopBar() {
    m_brush->SetColor(kColPanel);
    m_rt->FillRectangle(m_rcTopBar.d2d(), m_brush.Get());

    UiRect emblem{ 14, 11, 26, 26 };
    drawRoundRect(emblem, 6.0f, kColBg, true, kColAccent, 1.5f);
    drawText(L"NC", emblem, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);

    drawText(L"NovaCut Pro", { 48, 0, 120, 48 }, m_fontTitle, kColTextMain);

    auto addTopBtn = [&](int id, const wchar_t* label, float x, float w) {
        UiRect r{ x, 10, w, 28 };
        bool hov = r.contains(m_hoverX, m_hoverY);
        drawRoundRect(r, 6.0f, hov ? kColCardHover : kColCard, true, kColBorder, 1.0f);
        drawText(label, r, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ id, r, 0 });
    };
    addTopBtn(BTN_NEW,         tr(L"New",  L"جديد"),  170, 52);
    addTopBtn(BTN_OPEN,        tr(L"Open", L"فتح"),   228, 52);
    addTopBtn(BTN_SAVE,        tr(L"Save", L"حفظ"),   286, 52);
    addTopBtn(BTN_LANG_TOGGLE,  m_arabicUi ? L"EN" : L"عربي", 344, 44);
    addTopBtn(BTN_THEME_TOGGLE, m_lightMode ? tr(L"Dark", L"ليلي") : tr(L"Light", L"نهاري"), 394, 48);
    addTopBtn(BTN_BEFORE_AFTER, m_beforeAfter ? tr(L"Before", L"قبل") : tr(L"After", L"بعد"), 448, 56);
    addTopBtn(BTN_EXTRACT_PNG,  tr(L"PNG", L"صورة"), 510, 48);

    // Live Voice Recorder button (turns red when recording!)
    {
        UiRect rRec{ 564, 10, 74, 28 };
        bool rec = m_audio.isRecording();
        drawRoundRect(rRec, 6.0f, rec ? kColDanger : (rRec.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard),
                      true, rec ? kColDanger : kColBorder, 1.0f);
        drawText(rec ? tr(L"■ Stop", L"■ إيقاف") : tr(L"● Voice", L"● تسجيل"), rRec, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_VOICE_REC, rRec, 0 });
    }
    // Draw Mode toggle & All Tools Directory button
    {
        UiRect rDraw{ 644, 10, 62, 28 };
        drawRoundRect(rDraw, 6.0f, m_drawMode ? kColPurple : (rDraw.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard),
                      true, m_drawMode ? kColPurple : kColBorder, 1.0f);
        drawText(tr(L"Draw", L"رسم"), rDraw, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_DRAW_MODE, rDraw, 0 });

        UiRect rTools{ 712, 10, 96, 28 };
        drawRoundRect(rTools, 6.0f, m_showAllToolsModal ? kColAccent : kColCard, true, kColAccent, 1.2f);
        drawText(tr(L"All Tools 160+", L"كل الأدوات 160+"), rTools, m_fontSmall,
                 m_showAllToolsModal ? kColDarkText : kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_ALL_TOOLS, rTools, 0 });
    }
    // Batch 16: Workflow Quick Pills (Recover Draft, Version++, Preset, Search, Offline Mode, Preview Quality)
    if (m_width >= 1520.0f) {
        auto addSmallPill = [&](int id, const wchar_t* lbl, float x, float w, D2D1_COLOR_F bg, D2D1_COLOR_F fg, D2D1_COLOR_F brd) {
            UiRect r{ x, 11, w, 26 };
            drawRoundRect(r, 5.0f, bg, true, brd, 1.0f);
            drawText(lbl, r, m_fontSmall, fg, DWRITE_TEXT_ALIGNMENT_CENTER);
            m_buttons.push_back({ id, r, 0 });
        };
        addSmallPill(BTN_RECOVER_DRAFT, tr(L"Recover", L"استعادة"), 814, 58, kColCard, kColTextMain, kColBorder);
        addSmallPill(BTN_SAVE_VERSION,  tr(L"Ver++", L"نسخة+"),     876, 48, kColCard, kColGold,     kColGold);
        addSmallPill(BTN_CUSTOM_PRESET, m_hasCustomPreset ? tr(L"LoadPre", L"تطبيق") : tr(L"SavePre", L"قالب"), 928, 54, kColCard, kColPurple, kColPurple);
        addSmallPill(BTN_SEARCH_ASSETS, tr(L"Find", L"بحث"),        986, 44, !m_assetSearchQuery.empty() ? kColAccentDim : kColCard, kColTextMain, kColBorder);
        const wchar_t* qLblEn[3] = { L"1/4 Fast", L"Proxy", L"Full HQ" };
        const wchar_t* qLblAr[3] = { L"ربع", L"بروكسي", L"كامل" };
        addSmallPill(BTN_PREVIEW_QUAL, tr(qLblEn[m_previewQuality % 3], qLblAr[m_previewQuality % 3]), 1034, 54, kColCard, kColAccent, kColBorder);
        addSmallPill(BTN_TOGGLE_OFFLINE, m_offlineMode ? tr(L"Offline", L"أوفلاين") : tr(L"Online", L"أونلاين"), 1092, 54,
                     m_offlineMode ? kColGreen : kColCard, m_offlineMode ? kColDarkText : kColTextMain, m_offlineMode ? kColGreen : kColBorder);
    }

    float rightX = m_width - 14.0f;
    UiRect btnExport{ rightX - 102.0f, 9, 102.0f, 30.0f };
    bool expHov = btnExport.contains(m_hoverX, m_hoverY);
    drawRoundRect(btnExport, 7.0f, expHov ? D2D1::ColorF(0.15f, 0.96f, 0.88f, 1.0f) : kColAccent);
    drawText(tr(L"Export", L"تصدير MP4"), btnExport, m_fontBold, kColDarkText, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_buttons.push_back({ BTN_EXPORT, btnExport, 0 });

    const wchar_t* resLabels[5] = { L"480P", L"720P", L"1080P", L"2K", L"4K" };
    const wchar_t* fpsLabels[3] = { L"24 FPS", L"30 FPS", L"60 FPS" };
    UiRect badgeRes{ btnExport.x - 66.0f, 11, 58.0f, 26.0f };
    drawRoundRect(badgeRes, 5.0f, badgeRes.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColBorder, 1.0f);
    drawText(resLabels[m_resPreset % 5], badgeRes, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_buttons.push_back({ BTN_RES_CYCLE, badgeRes, 0 });

    UiRect badgeFps{ badgeRes.x - 64.0f, 11, 58.0f, 26.0f };
    drawRoundRect(badgeFps, 5.0f, badgeFps.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColBorder, 1.0f);
    drawText(fpsLabels[m_fpsPreset % 3], badgeFps, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_buttons.push_back({ BTN_FPS_CYCLE, badgeFps, 0 });

    UiRect badgeFf{ badgeFps.x - 84.0f, 11, 78.0f, 26.0f };
    drawRoundRect(badgeFf, 5.0f, kColCard, true, m_ffmpegFound ? kColGreen : kColAmber, 1.0f);
    drawText(L"H.264 GPU", badgeFf, m_fontSmall,
             m_ffmpegFound ? kColGreen : kColAmber, DWRITE_TEXT_ALIGNMENT_CENTER);

    m_brush->SetColor(kColBorder);
    m_rt->DrawLine(D2D1::Point2F(0, m_rcTopBar.h), D2D1::Point2F(m_width, m_rcTopBar.h), m_brush.Get(), 1.0f);
}

void MainWindow::renderLeftPanel() {
    m_brush->SetColor(kColPanel);
    m_rt->FillRectangle(m_rcLeftPanel.d2d(), m_brush.Get());

    // 8 Tabs: Media | Text | Filter | FX | Trans | Stick | Audio | AI
    const wchar_t* tabLabels[8] = {
        tr(L"Media", L"وسائط"), tr(L"Text", L"نص"), tr(L"Filter", L"فلتر"),
        tr(L"FX", L"مؤثر"), tr(L"Trans", L"انتقال"), tr(L"Stick", L"ملصق"),
        tr(L"Audio", L"صوت"), tr(L"AI", L"ذكاء")
    };
    const int tabIds[8] = {
        BTN_TAB_MEDIA, BTN_TAB_TEXT, BTN_TAB_FILTERS, BTN_TAB_EFFECTS,
        BTN_TAB_TRANSITIONS, BTN_TAB_STICKERS, BTN_TAB_AUDIO, BTN_TAB_AI
    };
    float tabW = m_rcLeftPanel.w / 8.0f;
    for (int i = 0; i < 8; ++i) {
        UiRect tr{ m_rcLeftPanel.x + i * tabW, m_rcLeftPanel.y, tabW, 38.0f };
        bool active = (static_cast<int>(m_leftTab) == i);
        drawText(tabLabels[i], tr, m_fontSmall, active ? kColAccent : kColTextMuted, DWRITE_TEXT_ALIGNMENT_CENTER);
        if (active) {
            m_brush->SetColor(kColAccent);
            m_rt->FillRectangle(D2D1::RectF(tr.x + 5, tr.y + tr.h - 2.5f, tr.x + tr.w - 5, tr.y + tr.h), m_brush.Get());
        }
        m_buttons.push_back({ tabIds[i], tr, 0 });
    }

    float bodyY = m_rcLeftPanel.y + 44.0f;
    float pad = 12.0f;

    if (m_leftTab == LeftTab::Media || m_leftTab == LeftTab::Audio) {
        bool audioOnly = (m_leftTab == LeftTab::Audio);
        UiRect btnImp{ m_rcLeftPanel.x + pad, bodyY, m_rcLeftPanel.w - pad * 2, 32.0f };
        bool hov = btnImp.contains(m_hoverX, m_hoverY);
        drawRoundRect(btnImp, 7.0f, hov ? D2D1::ColorF(0.15f, 0.96f, 0.88f, 1.0f) : kColAccent);
        drawText(audioOnly ? tr(L"+ Import Audio", L"+ استيراد صوت") : tr(L"+ Import Media", L"+ استيراد وسائط"),
                 btnImp, m_fontBold, kColDarkText, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ audioOnly ? BTN_IMPORT_AUDIO : BTN_IMPORT_MEDIA, btnImp, 0 });

        float gridY = bodyY + 42.0f;
        if (audioOnly) {
            // Text-to-Speech (TTS) 1-click button
            UiRect ttsBtn{ m_rcLeftPanel.x + pad, gridY, m_rcLeftPanel.w - pad * 2, 26.0f };
            drawRoundRect(ttsBtn, 6.0f, ttsBtn.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColPurple, 1.0f);
            drawText(tr(L"+ Text to Speech (TTS Voice)", L"+ تحويل النص إلى صوت (TTS)"), ttsBtn, m_fontSmall, kColPurple, DWRITE_TEXT_ALIGNMENT_CENTER);
            m_buttons.push_back({ BTN_TTS_GENERATE, ttsBtn, 0 });
            gridY += 32.0f;

            // 9 Built-in Sound Effects (SFX)
            const wchar_t* sfxNames[9] = { L"Whoosh", L"Pop", L"Click", L"Beep", L"Riser", L"Drop", L"Ding", L"Laser", L"Clap" };
            float sw = (m_rcLeftPanel.w - pad * 2 - 10.0f) / 3.0f;
            for (int si = 0; si < 9; ++si) {
                UiRect sr{ m_rcLeftPanel.x + pad + (si % 3) * (sw + 5.0f), gridY + (si / 3) * 26.0f, sw, 22.0f };
                drawRoundRect(sr, 5.0f, sr.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColAccentDim, 1.0f);
                drawText(sfxNames[si], sr, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
                m_buttons.push_back({ BTN_SFX_ADD, sr, static_cast<uint64_t>(si) });
            }
            gridY += 84.0f;

            // 4 Royalty-Free Built-in Music Library Tracks
            const wchar_t* musNames[4] = { L"♫ Lo-Fi Chill", L"♫ Cyber Synth", L"♫ Epic Cinema", L"♫ Upbeat Pop" };
            float mw = (m_rcLeftPanel.w - pad * 2 - 6.0f) * 0.5f;
            for (int mi = 0; mi < 4; ++mi) {
                UiRect mr{ m_rcLeftPanel.x + pad + (mi % 2) * (mw + 6.0f), gridY + (mi / 2) * 26.0f, mw, 22.0f };
                drawRoundRect(mr, 5.0f, mr.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColGold, 1.0f);
                drawText(musNames[mi], mr, m_fontSmall, kColGold, DWRITE_TEXT_ALIGNMENT_CENTER);
                m_buttons.push_back({ BTN_MUSIC_ADD, mr, static_cast<uint64_t>(mi) });
            }
            gridY += 58.0f;
        } else {
            // 1-Click Starter Templates strip
            const wchar_t* tplNames[3] = { tr(L"YT Intro", L"قالب يوتيوب"), tr(L"TikTok", L"قالب تيك توك"), tr(L"Cinema", L"قالب سينما") };
            float tw = (m_rcLeftPanel.w - pad * 2 - 10.0f) / 3.0f;
            for (int ti = 0; ti < 3; ++ti) {
                UiRect trc{ m_rcLeftPanel.x + pad + ti * (tw + 5.0f), gridY, tw, 23.0f };
                drawRoundRect(trc, 5.0f, trc.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColPurple, 1.0f);
                drawText(tplNames[ti], trc, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
                m_buttons.push_back({ BTN_TEMPLATE_ADD, trc, static_cast<uint64_t>(ti) });
            }
            gridY += 32.0f;
        }
        int col = 0, row = 0, shown = 0;
        float cardW = (m_rcLeftPanel.w - pad * 3) * 0.5f;
        float cardH = 86.0f;

        for (const auto& asset : m_engine.project().assets) {
            if (audioOnly && asset.type != MediaType::Audio) continue;
            if (!audioOnly && asset.type == MediaType::Audio) continue;

            float cx = m_rcLeftPanel.x + pad + col * (cardW + pad);
            float cy = gridY + row * (cardH + 10.0f);
            if (cy + cardH > m_rcLeftPanel.y + m_rcLeftPanel.h - 8.0f) break;

            UiRect card{ cx, cy, cardW, cardH };
            bool chov = card.contains(m_hoverX, m_hoverY);
            drawRoundRect(card, 7.0f, chov ? kColCardHover : kColCard, true, chov ? kColAccent : kColBorder, 1.0f);

            UiRect thumbRc{ cx + 4, cy + 4, cardW - 8, cardH - 26 };
            auto it = m_thumbBitmaps.find(asset.id);
            if (it != m_thumbBitmaps.end() && it->second) {
                m_rt->DrawBitmap(it->second.Get(), thumbRc.d2d(), 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            } else {
                drawRoundRect(thumbRc, 4.0f, asset.type == MediaType::Audio ? kColClipAudio : kColBg);
                drawText(asset.type == MediaType::Audio ? L"AUDIO" : L"MEDIA", thumbRc, m_fontSmall, kColTextMuted, DWRITE_TEXT_ALIGNMENT_CENTER);
            }

            char durBuf[24];
            std::snprintf(durBuf, sizeof(durBuf), "%.1fs", asset.durationSec);
            UiRect durRc{ cx + cardW - 44, cy + 6, 38, 16 };
            drawRoundRect(durRc, 3.0f, D2D1::ColorF(0, 0, 0, 0.72f));
            drawText(utf8ToWide(durBuf), durRc, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);

            drawText(utf8ToWide(asset.name), { cx + 6, cy + cardH - 20, cardW - 12, 18 }, m_fontSmall, kColTextMain);
            m_buttons.push_back({ BTN_ASSET_ADD, card, asset.id });

            ++shown;
            if (++col >= 2) { col = 0; ++row; }
        }

        if (shown == 0) {
            UiRect emptyRc{ m_rcLeftPanel.x + pad, gridY + 12, m_rcLeftPanel.w - pad * 2, 100 };
            drawRoundRect(emptyRc, 8.0f, kColBg, true, kColBorder, 1.0f);
            drawText(tr(L"Drag & drop videos, images,\nor click + Import above", L"اسحب وأفلت الفيديوهات أو الصور هنا\nأو اضغط + استيراد وسائط بالأعلى"), emptyRc, m_fontSmall,
                     kColTextMuted, DWRITE_TEXT_ALIGNMENT_CENTER);
        }
    } else if (m_leftTab == LeftTab::Text) {
        // Part 3: Auto Captions & SRT Import buttons at top of Text tab
        float halfW = (m_rcLeftPanel.w - pad * 3) * 0.5f;
        UiRect btnAuto{ m_rcLeftPanel.x + pad, bodyY, halfW, 34.0f };
        drawRoundRect(btnAuto, 7.0f, btnAuto.contains(m_hoverX, m_hoverY) ? kColAccent : kColAccentDim);
        drawText(tr(L"Auto Captions", L"ترجمة تلقائية"), btnAuto, m_fontSmall, kColDarkText, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_AUTO_CAPTIONS, btnAuto, 0 });

        UiRect btnSrt{ m_rcLeftPanel.x + pad * 2 + halfW, bodyY, halfW, 34.0f };
        drawRoundRect(btnSrt, 7.0f, btnSrt.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColAccent, 1.0f);
        drawText(tr(L"+ Import .SRT", L"+ استيراد SRT"), btnSrt, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_IMPORT_SRT, btnSrt, 0 });

        UiRect btnMan{ m_rcLeftPanel.x + pad, bodyY + 40.0f, m_rcLeftPanel.w - pad * 2, 28.0f };
        drawRoundRect(btnMan, 6.0f, btnMan.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColGold, 1.0f);
        drawText(tr(L"+ Manual Caption at Playhead", L"+ كتابة ترجمة يدوية عند المؤشر"), btnMan, m_fontSmall, kColGold, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_MANUAL_CAPTION, btnMan, 0 });

        const wchar_t* titlesEn[4] = { L"Default Title", L"Cinematic Headline", L"Neon Cyber Tag", L"Minimal Lower Third" };
        const wchar_t* titlesAr[4] = { L"عنوان افتراضي", L"عنوان سينمائي", L"شعار نيون سايبر", L"شريط سفلي أنيق" };
        for (int i = 0; i < 4; ++i) {
            UiRect card{ m_rcLeftPanel.x + pad, bodyY + 76.0f + i * 50.0f, m_rcLeftPanel.w - pad * 2, 42.0f };
            bool hov = card.contains(m_hoverX, m_hoverY);
            drawRoundRect(card, 7.0f, hov ? kColCardHover : kColCard, true, hov ? kColAccent : kColBorder, 1.0f);
            if (m_arabicUi) {
                drawText(titlesAr[i], { card.x + 64, card.y, card.w - 76, card.h }, m_fontBold, kColTextMain, DWRITE_TEXT_ALIGNMENT_LEADING);
                drawText(L"+ إضافة",  { card.x + 10, card.y, 52, card.h }, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_TRAILING);
            } else {
                drawText(titlesEn[i], { card.x + 12, card.y, card.w - 68, card.h }, m_fontBold, kColTextMain, DWRITE_TEXT_ALIGNMENT_LEADING);
                drawText(L"+ Add",    { card.x + card.w - 56, card.y, 46, card.h }, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
            m_buttons.push_back({ BTN_TEXT_PRESET, card, static_cast<uint64_t>(i) });
        }
    } else if (m_leftTab == LeftTab::Filters) {
        // 10 Filter Category Pills (All + 9 CapCut Categories: Hits, Life, Movies, Photo Booth, Mono, Retro, Style, Food, Night)
        const wchar_t* catNames[10] = {
            tr(L"All", L"الكل"), tr(L"Hits", L"رائج"), tr(L"Life", L"حياة"), tr(L"Movies", L"سينما"), tr(L"Photo", L"بورتريه"),
            tr(L"Mono", L"أبيض/أسود"), tr(L"Retro", L"قديم"), tr(L"Style", L"فني"), tr(L"Food", L"طعام"), tr(L"Night", L"ليلي")
        };
        float pillW = (m_rcLeftPanel.w - pad * 2 - 16.0f) / 5.0f;
        for (int ci = 0; ci < 10; ++ci) {
            UiRect pr{ m_rcLeftPanel.x + pad + (ci % 5) * (pillW + 4.0f), bodyY + (ci / 5) * 24.0f, pillW, 20.0f };
            bool act = (m_filterCategory == ci);
            drawRoundRect(pr, 4.0f, act ? kColAccent : kColCard);
            drawText(catNames[ci], pr, m_fontSmall, act ? kColDarkText : kColTextMuted, DWRITE_TEXT_ALIGNMENT_CENTER);
            m_buttons.push_back({ BTN_FILTER_CAT, pr, static_cast<uint64_t>(ci) });
        }
        float listTop = bodyY + 54.0f;

        const D2D1_COLOR_F swatchCols[25] = {
            { 0.30f, 0.30f, 0.34f, 1.0f }, { 0.16f, 0.34f, 0.42f, 1.0f }, { 0.85f, 0.30f, 0.48f, 1.0f },
            { 0.88f, 0.46f, 0.22f, 1.0f }, { 0.12f, 0.68f, 0.82f, 1.0f }, { 0.45f, 0.45f, 0.45f, 1.0f },
            { 0.66f, 0.54f, 0.36f, 1.0f }, { 0.08f, 0.58f, 0.54f, 1.0f }, { 0.14f, 0.16f, 0.22f, 1.0f },
            { 0.92f, 0.62f, 0.20f, 1.0f }, { 0.86f, 0.72f, 0.80f, 1.0f }, { 0.75f, 0.16f, 0.88f, 1.0f },
            { 0.46f, 0.88f, 0.28f, 1.0f }, { 0.68f, 0.32f, 0.58f, 1.0f }, { 0.42f, 0.54f, 0.66f, 1.0f },
            { 0.30f, 0.44f, 0.58f, 1.0f }, { 0.72f, 0.76f, 0.80f, 1.0f }, { 0.22f, 0.22f, 0.24f, 1.0f },
            { 0.82f, 0.26f, 0.24f, 1.0f }, { 0.42f, 0.18f, 0.74f, 1.0f }, { 0.96f, 0.72f, 0.16f, 1.0f },
            { 0.94f, 0.48f, 0.18f, 1.0f }, { 0.10f, 0.22f, 0.52f, 1.0f }, { 0.90f, 0.76f, 0.70f, 1.0f },
            { 0.52f, 0.78f, 0.62f, 1.0f }
        };
        // Category mapping per filter (1=Hits, 2=Life, 3=Movies, 4=Photo, 5=Mono, 6=Retro, 7=Style, 8=Food, 9=Night)
        const int fltCats[25] = {
            0, 3, 1, 2, 9, 5, 6, 3, 9, 2, 4, 9, 1, 6, 7, 7, 7, 5, 7, 7, 7, 8, 9, 4, 2
        };

        Clip* sel = m_engine.selectedClip();
        float cardW = (m_rcLeftPanel.w - pad * 3) * 0.5f;
        float cardH = 48.0f;
        int visibleIdx = 0;
        for (int i = 0; i < 25; ++i) {
            if (m_filterCategory != 0 && i != 0 && fltCats[i] != m_filterCategory) continue;
            int col = visibleIdx % 2, row = visibleIdx / 2;
            ++visibleIdx;
            UiRect card{ m_rcLeftPanel.x + pad + col * (cardW + pad), listTop + row * (cardH + 8.0f) - m_leftScrollY, cardW, cardH };
            if (card.y < listTop - 2.0f || card.y + card.h > m_rcLeftPanel.y + m_rcLeftPanel.h - 4.0f) continue;
            bool active = sel && static_cast<int>(sel->color.preset) == i;
            bool hov = card.contains(m_hoverX, m_hoverY);
            drawRoundRect(card, 7.0f, hov ? kColCardHover : kColCard, true,
                          active ? kColAccent : kColBorder, active ? 2.0f : 1.0f);
            drawRoundRect({ card.x + 8, card.y + 6, card.w - 16, 22 }, 4.0f, swatchCols[i]);
            drawText(utf8ToWide(filterPresetName(static_cast<FilterPreset>(i))),
                     { card.x + 6, card.y + 30, card.w - 12, 16 }, m_fontSmall,
                     active ? kColAccent : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
            m_buttons.push_back({ BTN_FILTER_PRESET, card, static_cast<uint64_t>(i) });
        }
    } else if (m_leftTab == LeftTab::Effects) {
        const wchar_t* fxCats[8] = {
            tr(L"All", L"الكل"), tr(L"Trend", L"رائج"), tr(L"Light", L"إضاءة"), tr(L"Glitch", L"تشويش"),
            tr(L"Blur", L"ضبابية"), tr(L"Retro", L"قديم"), tr(L"Nature", L"طبيعة"), tr(L"AI/3D", L"3D/ذكاء")
        };
        float pillW = (m_rcLeftPanel.w - pad * 2 - 12.0f) / 4.0f;
        for (int ci = 0; ci < 8; ++ci) {
            UiRect pr{ m_rcLeftPanel.x + pad + (ci % 4) * (pillW + 4.0f), bodyY + (ci / 4) * 24.0f, pillW, 20.0f };
            bool act = (m_fxCategory == ci);
            drawRoundRect(pr, 4.0f, act ? kColPurple : kColCard);
            drawText(fxCats[ci], pr, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
            m_buttons.push_back({ BTN_FX_CAT, pr, static_cast<uint64_t>(ci) });
        }
        float listTop = bodyY + 54.0f;

        Clip* sel = m_engine.selectedClip();
        float cardW = (m_rcLeftPanel.w - pad * 3) * 0.5f;
        float cardH = 54.0f;
        const wchar_t* badges[28] = {
            L"OFF", L"SHADOW", L"RGB GLITCH", L"35MM FILM", L"SOFT GLOW", L"RETRO CRT", L"MOTION BLUR",
            L"★ SPARK", L"♥ LOVE", L"LENS FLARE", L"DISTORT", L"ZOOM BLUR", L"3D ANAGLYPH",
            L"RAIN VFX", L"NEON VFX", L"NOIR SHADOW", L"VHS TAPE",
            L"LIGHT LEAK", L"SMOKE VFX", L"FIRE VFX", L"SNOW FALL", L"LIQUID",
            L"DISPLACE", L"FRACTAL", L"CHROMATIC", L"DEPTH MAP", L"3D OBJECT", L"PARTICLES"
        };
        const int fxCatMap[28] = {
            0, 2, 3, 5, 2, 5, 4, 1, 1, 2, 3, 4, 7, 6, 7, 2, 5,
            2, 6, 6, 6, 3, 3, 5, 3, 7, 7, 1
        };
        int vis = 0;
        for (int i = 0; i < 28; ++i) {
            if (m_fxCategory != 0 && i != 0 && fxCatMap[i] != m_fxCategory) continue;
            if (!m_assetSearchQuery.empty() && i != 0) {
                std::string en = effectName(static_cast<EffectType>(i));
                std::string loEn = en, loQ = m_assetSearchQuery;
                for (char& ch : loEn) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                for (char& ch : loQ)  ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                if (loEn.find(loQ) == std::string::npos) continue;
            }
            int col = vis % 2, row = vis / 2;
            ++vis;
            UiRect card{ m_rcLeftPanel.x + pad + col * (cardW + pad), listTop + row * (cardH + 8.0f) - m_leftScrollY, cardW, cardH };
            if (card.y < listTop - 2.0f || card.y + card.h > m_rcLeftPanel.y + m_rcLeftPanel.h - 4.0f) continue;
            bool active = sel && static_cast<int>(sel->effect.type) == i;
            bool hov = card.contains(m_hoverX, m_hoverY);
            drawRoundRect(card, 7.0f, hov ? kColCardHover : kColCard, true,
                          active ? kColPurple : kColBorder, active ? 2.0f : 1.0f);
            drawRoundRect({ card.x + 8, card.y + 6, card.w - 16, 22 }, 4.0f, kColBg);
            drawText(badges[i], { card.x + 8, card.y + 6, card.w - 16, 22 }, m_fontSmall, kColPurple, DWRITE_TEXT_ALIGNMENT_CENTER);
            drawText(utf8ToWide(effectName(static_cast<EffectType>(i))),
                     { card.x + 6, card.y + 32, card.w - 12, 18 }, m_fontSmall,
                     active ? kColPurple : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
            m_buttons.push_back({ BTN_EFFECT_PRESET, card, static_cast<uint64_t>(i) });
        }
    } else if (m_leftTab == LeftTab::Transitions) {
        Clip* sel = m_engine.selectedClip();
        float cardW = (m_rcLeftPanel.w - pad * 3) * 0.5f;
        float cardH = 50.0f;
        const wchar_t* icons[20] = {
            L"OFF", L"FADE", L"DISSOLVE", L"BLUR", L"[+] IN", L"[-] OUT",
            L"<< LEFT", L">> RIGHT", L"^^ UP", L"vv DOWN", L"↻ SPIN", L"⇄ FLIP",
            L"≈ SHAKE", L">|< PULL IN", L"<|> PULL OUT", L">>| WIPE", L"◯ MASK",
            L"|| SPLIT", L"▣ CUBE 3D", L"★ MG POP"
        };
        for (int i = 0; i < 20; ++i) {
            int col = i % 2, row = i / 2;
            UiRect card{ m_rcLeftPanel.x + pad + col * (cardW + pad), bodyY + row * (cardH + 8.0f) - m_leftScrollY, cardW, cardH };
            if (card.y < bodyY - 2.0f || card.y + card.h > m_rcLeftPanel.y + m_rcLeftPanel.h - 4.0f) continue;
            bool active = sel && static_cast<int>(sel->transitionIn) == i;
            bool hov = card.contains(m_hoverX, m_hoverY);
            drawRoundRect(card, 7.0f, hov ? kColCardHover : kColCard, true,
                          active ? kColAccent : kColBorder, active ? 2.0f : 1.0f);
            drawRoundRect({ card.x + 8, card.y + 6, card.w - 16, 20 }, 4.0f, kColBg);
            drawText(icons[i], { card.x + 8, card.y + 6, card.w - 16, 20 }, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
            drawText(utf8ToWide(transitionName(static_cast<TransitionType>(i))),
                     { card.x + 6, card.y + 29, card.w - 12, 18 }, m_fontSmall,
                     active ? kColAccent : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
            m_buttons.push_back({ BTN_TRANS_PRESET, card, static_cast<uint64_t>(i) });
        }
    } else if (m_leftTab == LeftTab::Stickers) {
        UiRect hubBtn{ m_rcLeftPanel.x + pad, bodyY, m_rcLeftPanel.w - pad * 2, 26.0f };
        drawRoundRect(hubBtn, 6.0f, kColAccent, true, kColAccent, 1.0f);
        drawText(tr(L"Built-In Asset Library", L"مكتبة الإدراج الداخلية"), hubBtn, m_fontSmall, kColDarkText, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_OPEN_ASSETHUB, hubBtn, 0 });
        bodyY += 32.0f;
        const wchar_t* stLabels[12] = { L"SUBSCRIBE", L"LIKE", L"LIVE ●", L"★ VIP", L"FIRE", L"100%", L"REC ●", L"NEW!", L"★ TOP", L"SALE", L"PRO", L"LOVE ♥" };
        const uint32_t stColors[12] = { 0xFFE5253A, 0xFF1877F2, 0xFFE5253A, 0xFFD49B00, 0xFFFF5722, 0xFF00B894, 0xFFD63031, 0xFF6C5CE7, 0xFFE17055, 0xFF00CEC9, 0xFF0984E3, 0xFFE84393 };
        float cardW = (m_rcLeftPanel.w - pad * 3) * 0.5f;
        float cardH = 44.0f;
        for (int i = 0; i < 12; ++i) {
            int col = i % 2, row = i / 2;
            UiRect card{ m_rcLeftPanel.x + pad + col * (cardW + pad), bodyY + row * (cardH + 10.0f), cardW, cardH };
            bool hov = card.contains(m_hoverX, m_hoverY);
            drawRoundRect(card, 7.0f, hov ? kColCardHover : kColCard, true, hov ? kColAccent : kColBorder, 1.0f);
            UiRect pill{ card.x + 12, card.y + 12, card.w - 24, card.h - 24 };
            drawRoundRect(pill, 10.0f, argbToD2D(stColors[i]));
            drawText(stLabels[i], pill, m_fontBold, D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), DWRITE_TEXT_ALIGNMENT_CENTER);
            m_buttons.push_back({ BTN_STICKER_PRESET, card, static_cast<uint64_t>(i) });
        }
    } else if (m_leftTab == LeftTab::AI) {
        float hwA = (m_rcLeftPanel.w - pad * 2 - 6.0f) * 0.5f;
        UiRect modBtn{ m_rcLeftPanel.x + pad, bodyY, hwA, 24.0f };
        drawRoundRect(modBtn, 5.0f, kColCard, true, kColAccent, 1.0f);
        drawText(std::wstring(tr(L"LM: ", L"نموذج: ")) + utf8ToWide(m_aiConfig.localModel), modBtn, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_AI_LOCAL_MODEL, modBtn, 0 });

        UiRect tstBtn{ m_rcLeftPanel.x + pad + hwA + 6.0f, bodyY, hwA, 24.0f };
        drawRoundRect(tstBtn, 5.0f, kColCard, true, kColGreen, 1.0f);
        drawText(tr(L"Test LM Studio", L"فحص LM Studio"), tstBtn, m_fontSmall, kColGreen, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_AI_TEST_LOCAL, tstBtn, 0 });

        UiRect keyBtn{ m_rcLeftPanel.x + pad, bodyY + 28.0f, m_rcLeftPanel.w - pad * 2, 22.0f };
        drawRoundRect(keyBtn, 5.0f, m_aiConfig.geminiApiKey.empty() ? kColCard : kColAccentDim, true, kColBorder, 1.0f);
        drawText(m_aiConfig.geminiApiKey.empty()
                    ? tr(L"Optional: Cloud Gemini Key", L"اختياري: مفتاح Gemini السحابي")
                    : tr(L"Gemini Cloud Key Connected ✓", L"مفتاح Gemini السحابي متصل ✓"),
                 keyBtn, m_fontSmall, kColTextMuted, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_AI_APIKEY, keyBtn, 0 });

        const wchar_t* aiLabels[20] = {
            tr(L"AI Image (Net)",     L"توليد صورة (نت)"),
            tr(L"AI Video (Net)",     L"توليد فيديو (نت)"),
            tr(L"AI Script 2B",       L"كتابة سكربت 2B"),
            tr(L"AI Translate 2B",    L"ترجمة محلية 2B"),
            tr(L"AI Voiceover",       L"تعليق صوتي ذكي"),
            tr(L"AI Avatar (Net)",    L"شخصية افتراضية"),
            tr(L"AI Lip Sync",        L"مزامنة الشفاه"),
            tr(L"Auto Captions",      L"تفريغ وترجمة"),
            tr(L"Remove BG",          L"إزالة الخلفية"),
            tr(L"AI Relight",         L"تعديل الإضاءة"),
            tr(L"AI Enhance",         L"تحسين الجودة"),
            tr(L"Auto Reframe",       L"ضبط الإطار 9:16"),
            tr(L"Motion Track",       L"تتبع الحركة"),
            tr(L"Remove Object",      L"إزالة العناصر"),
            tr(L"AI Retouch",         L"تحسين المظهر"),
            tr(L"AI Color Mood",      L"تلوين ذكي 2B"),
            tr(L"AI Audio Master",    L"مكساج ذكي 2B"),
            tr(L"AI Viral Pack",      L"عناوين وهاشتاغ"),
            tr(L"AI Director",        L"مخرج ذكي 2B"),
            tr(L"AI Voice Script",    L"سكربت + صوت 2B")
        };
        float cardW = (m_rcLeftPanel.w - pad * 3) * 0.5f;
        float cardH = 30.0f;
        float startY = bodyY + 56.0f;
        for (int i = 0; i < 20; ++i) {
            int col = i % 2, row = i / 2;
            UiRect card{ m_rcLeftPanel.x + pad + col * (cardW + pad), startY + row * (cardH + 5.0f) - m_leftScrollY, cardW, cardH };
            if (card.y < startY - 2.0f || card.y + card.h > m_rcLeftPanel.y + m_rcLeftPanel.h - 4.0f) continue;
            bool isLocal2B = (i == 2 || i == 3 || i == 4 || i >= 15);
            bool hov = card.contains(m_hoverX, m_hoverY);
            drawRoundRect(card, 5.0f, hov ? kColCardHover : kColCard, true,
                          isLocal2B ? kColAccent : kColBorder, isLocal2B ? 1.0f : 0.8f);
            drawText(aiLabels[i], card, m_fontSmall, isLocal2B ? kColAccent : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
            m_buttons.push_back({ BTN_AI_TOOL, card, static_cast<uint64_t>(i) });
        }
    }
}

void MainWindow::renderPreviewPanel() {
    m_brush->SetColor(kColBg);
    m_rt->FillRectangle(m_rcPreviewPanel.d2d(), m_brush.Get());

    wchar_t hudBuf[64];
    std::swprintf(hudBuf, 64, tr(L"Player  •  %d FPS", L"معاينة  •  %d إطار/ث"), std::max(1, m_displayedFps));
    drawText(hudBuf, { m_rcPreviewPanel.x + 16, m_rcPreviewPanel.y + 4, 210, 26 }, m_fontBold, kColTextMuted);

    const wchar_t* aspectLabels[3] = { L"16:9 YouTube", L"9:16 TikTok", L"1:1 Square" };
    UiRect fsBtn{ m_rcPreviewPanel.x + m_rcPreviewPanel.w - 44, m_rcPreviewPanel.y + 4, 34, 24 };
    drawRoundRect(fsBtn, 5.0f, m_fullScreenPreview ? kColAccentDim : (fsBtn.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard), true, kColBorder, 1.0f);
    drawText(m_fullScreenPreview ? L"×" : L"[ ]", fsBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_buttons.push_back({ BTN_FULLSCREEN, fsBtn, 0 });

    UiRect aspBtn{ fsBtn.x - 102, m_rcPreviewPanel.y + 4, 96, 24 };
    drawRoundRect(aspBtn, 5.0f, aspBtn.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColBorder, 1.0f);
    drawText(aspectLabels[m_aspectMode % 3], aspBtn, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_buttons.push_back({ BTN_ASPECT_CYCLE, aspBtn, 0 });

    m_brush->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f));
    m_rt->FillRectangle(m_rcVideoCanvas.d2d(), m_brush.Get());

    m_rt->PushAxisAlignedClip(m_rcVideoCanvas.d2d(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    auto activeClips = m_engine.activeVisualClipsAt(m_engine.playhead());
    if (activeClips.empty()) {
        float cx = m_rcVideoCanvas.x + m_rcVideoCanvas.w * 0.5f;
        float cy = m_rcVideoCanvas.y + m_rcVideoCanvas.h * 0.5f;
        drawText(tr(L"1. Import or Drop Video   •   2. Edit, FX & Stickers   •   3. Fast Export MP4",
                    L"1. اسحب أو استورد فيديو   •   2. قص ومؤثرات وملصقات   •   3. تصدير MP4 سريع"),
                 { m_rcVideoCanvas.x + 12, cy - 52, m_rcVideoCanvas.w - 24, 28 }, m_fontSmall, kColTextMuted, DWRITE_TEXT_ALIGNMENT_CENTER);
        UiRect quickImp{ cx - 105, cy - 10, 210, 38 };
        drawRoundRect(quickImp, 8.0f, quickImp.contains(m_hoverX, m_hoverY) ? D2D1::ColorF(0.15f, 0.96f, 0.88f, 1.0f) : kColAccent);
        drawText(tr(L"+ Import Video / Photo", L"+ استيراد فيديو أو صورة"), quickImp, m_fontBold, kColDarkText, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_IMPORT_MEDIA, quickImp, 0 });
    } else {
        for (const Clip* c : activeClips) {
            EvaluatedPose pose = evaluateClipPose(*c, m_engine.playhead());
            float cx = m_rcVideoCanvas.x + m_rcVideoCanvas.w * 0.5f + pose.transform.posX * (m_rcVideoCanvas.w * 0.5f);
            float cy = m_rcVideoCanvas.y + m_rcVideoCanvas.h * 0.5f + pose.transform.posY * (m_rcVideoCanvas.h * 0.5f);

            bool hasWipe = (pose.wipeProgress < 0.999f);
            if (hasWipe) {
                D2D1_RECT_F wipeRc = D2D1::RectF(
                    m_rcVideoCanvas.x, m_rcVideoCanvas.y,
                    m_rcVideoCanvas.x + m_rcVideoCanvas.w * std::clamp(pose.wipeProgress, 0.0f, 1.0f),
                    m_rcVideoCanvas.y + m_rcVideoCanvas.h);
                m_rt->PushAxisAlignedClip(wipeRc, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            }

            float sx = pose.transform.scale * (pose.transform.flipH ? -1.0f : 1.0f);
            float sy = pose.transform.scale * (pose.transform.flipV ? -1.0f : 1.0f);
            D2D1::Matrix3x2F xform =
                D2D1::Matrix3x2F::Skew(pose.transform.skewX, pose.transform.skewY, D2D1::Point2F(cx, cy)) *
                D2D1::Matrix3x2F::Scale(sx, sy, D2D1::Point2F(cx, cy)) *
                D2D1::Matrix3x2F::Rotation(pose.transform.rotationDeg, D2D1::Point2F(cx, cy));
            m_rt->SetTransform(xform);

            if (c->type == MediaType::Video || c->type == MediaType::Image) {
                double srcTime = c->sourceTimeAt(m_engine.playhead());
                uint64_t phash = m_beforeAfter ? 0xBEEFULL : c->color.hash(c->chromaKey, c->effect, c->mask, c->transform.cropX, c->transform.cropY, c->maskFeather);

                CachedClipBitmap& cache = m_previewCache[c->id];
                double latestDecSec = -999.0;
                const std::string& playSrc = (!c->proxyPath.empty() && m_previewQuality < 2) ? c->proxyPath : c->sourcePath;
                auto framePtr = m_media.tryGetLatestFramePtr(playSrc, c->type, srcTime, latestDecSec);

                if (framePtr && (!cache.bmp || std::abs(latestDecSec - cache.decodedSec) > 0.005 || cache.pipelineHash != phash)) {
                    if (m_beforeAfter) {
                        if (uploadFrameToBitmap(*framePtr, cache.bmp)) {
                            cache.decodedSec = latestDecSec;
                            cache.pipelineHash = phash;
                            cache.width = framePtr->width;
                            cache.height = framePtr->height;
                        }
                    } else {
                        const FrameBGRA* prevPtr = (c->opticalFlow && m_gradedFrame.valid() && m_gradedFrame.width == framePtr->width) ? &m_gradedFrame : nullptr;
                        float mOffX = (!c->keyframes.empty() && c->mask != MaskType::None) ? (pose.transform.posX - c->transform.posX) : 0.0f;
                        float mOffY = (!c->keyframes.empty() && c->mask != MaskType::None) ? (pose.transform.posY - c->transform.posY) : 0.0f;
                        MediaEngine::applyPipeline(*framePtr, c->color, c->chromaKey, c->effect, m_gradedFrame,
                                                   c->mask, c->transform.cropX, c->transform.cropY, c->maskFeather, prevPtr, mOffX, mOffY);
                        if (uploadFrameToBitmap(m_gradedFrame, cache.bmp)) {
                            cache.decodedSec = latestDecSec;
                            cache.pipelineHash = phash;
                            cache.width = m_gradedFrame.width;
                            cache.height = m_gradedFrame.height;
                            if (m_scopeMode > 0) updateVideoScopes(m_gradedFrame);
                        }
                    }
                }

                if (cache.bmp && cache.width > 0 && cache.height > 0) {
                    float imgAspect = static_cast<float>(cache.width) / std::max(1, cache.height);
                    float dw = m_rcVideoCanvas.w;
                    float dh = dw / imgAspect;
                    if (dh > m_rcVideoCanvas.h) {
                        dh = m_rcVideoCanvas.h;
                        dw = dh * imgAspect;
                    }
                    if (c->studioBg != StudioBgPreset::None) {
                        D2D1_COLOR_F sbg = (c->studioBg == StudioBgPreset::DarkStudio) ? D2D1::ColorF(0.10f, 0.11f, 0.14f, 1.0f)
                                         : (c->studioBg == StudioBgPreset::NeonGradient ? D2D1::ColorF(0.08f, 0.25f, 0.36f, 1.0f)
                                         : (c->studioBg == StudioBgPreset::SunsetSky ? D2D1::ColorF(0.42f, 0.18f, 0.22f, 1.0f)
                                         : D2D1::ColorF(0.95f, 0.96f, 0.98f, 1.0f)));
                        m_brush->SetColor(sbg);
                        m_rt->FillRectangle(m_rcVideoCanvas.d2d(), m_brush.Get());
                    }
                    if (c->bgBlur) {
                        // Blurred full-canvas background fill behind scaled/vertical video
                        m_rt->DrawBitmap(cache.bmp.Get(), m_rcVideoCanvas.d2d(), 0.35f * pose.opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                    }
                    D2D1_RECT_F dst = D2D1::RectF(cx - dw * 0.5f, cy - dh * 0.5f, cx + dw * 0.5f, cy + dh * 0.5f);
                    float compAlpha = pose.opacity;
                    if (c->blendMode == BlendMode::Screen || c->blendMode == BlendMode::Add) compAlpha *= 0.82f;
                    else if (c->blendMode == BlendMode::Overlay) compAlpha *= 0.72f;
                    else if (c->blendMode == BlendMode::Multiply) compAlpha *= 0.88f;
                    m_rt->DrawBitmap(cache.bmp.Get(), dst, compAlpha, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                    if (c->frame != FrameStyle::None) {
                        D2D1_COLOR_F fc = (c->frame == FrameStyle::NeonCyan) ? kColAccent :
                                          (c->frame == FrameStyle::CinemaGold ? kColGold : kColTextMain);
                        m_brush->SetColor(fc);
                        m_rt->DrawRectangle(dst, m_brush.Get(), (c->frame == FrameStyle::FilmStrip) ? 8.0f : 4.0f);
                    }
                }
            } else if (c->type == MediaType::Text) {
                float scaledFont = std::max(12.0f, c->text.fontSize * (m_rcVideoCanvas.h / 1080.0f));
                ComPtr<IDWriteTextFormat> txtFmt;
                m_dwriteFactory->CreateTextFormat(
                    utf8ToWide(c->text.fontName).c_str(), nullptr,
                    c->text.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                    scaledFont, L"en-us", &txtFmt);
                if (txtFmt) {
                    txtFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                    txtFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                    std::wstring wTxt = utf8ToWide(c->text.content);
                    if (pose.textReveal01 < 0.999f && !wTxt.empty()) {
                        size_t keep = std::max<size_t>(1, static_cast<size_t>(std::ceil(wTxt.size() * pose.textReveal01)));
                        wTxt.resize(std::min(wTxt.size(), keep));
                    }
                    UiRect tr{ cx - m_rcVideoCanvas.w * 0.48f, cy - scaledFont * 1.4f,
                               m_rcVideoCanvas.w * 0.96f, scaledFont * 2.8f };
                    // Text Tracking (letter spacing)
                    if (c->text.tracking > 0.5f && wTxt.size() > 1) {
                        std::wstring spaced;
                        int spCount = static_cast<int>(c->text.tracking * 0.5f);
                        for (size_t ci = 0; ci < wTxt.size(); ++ci) {
                            spaced.push_back(wTxt[ci]);
                            if (ci + 1 < wTxt.size()) spaced.append(spCount, L' ');
                        }
                        wTxt = std::move(spaced);
                    }
                    // Text Background or Sticker Badge
                    if (c->text.isStickerBadge || c->text.hasBackground) {
                        float bw = std::max(scaledFont * 3.8f, static_cast<float>(wTxt.size()) * scaledFont * 0.58f + 28.0f);
                        float bh = scaledFont * 1.65f;
                        D2D1_COLOR_F bgc = argbToD2D(c->text.bgARGB != 0 ? c->text.bgARGB : 0xCC101218);
                        bgc.a *= pose.opacity;
                        drawRoundRect({ cx - bw * 0.5f, cy - bh * 0.5f, bw, bh }, c->text.isStickerBadge ? (bh * 0.35f) : 8.0f, bgc);
                    }
                    // Text Effects (Neon Glow / 3D Extrude / Retro Split / Gold)
                    if (c->text.effect == TextEffectType::NeonGlow) {
                        m_brush->SetColor(D2D1::ColorF(0.0f, 0.92f, 0.85f, 0.45f * pose.opacity));
                        for (float dx : {-3.0f, 0.0f, 3.0f}) for (float dy : {-3.0f, 0.0f, 3.0f}) {
                            UiRect gr = tr; gr.x += dx; gr.y += dy;
                            m_rt->DrawTextW(wTxt.c_str(), static_cast<UINT32>(wTxt.size()), txtFmt.Get(), gr.d2d(), m_brush.Get());
                        }
                    } else if (c->text.effect == TextEffectType::Extrude3D) {
                        for (int k = 7; k >= 1; --k) {
                            float shade = 0.08f + (7 - k) * 0.045f;
                            m_brush->SetColor(D2D1::ColorF(shade * 0.7f, shade * 1.15f, shade * 1.45f, 0.92f * pose.opacity));
                            UiRect er = tr; er.x += k * 1.6f; er.y += k * 1.6f;
                            m_rt->DrawTextW(wTxt.c_str(), static_cast<UINT32>(wTxt.size()), txtFmt.Get(), er.d2d(), m_brush.Get());
                        }
                    } else if (c->text.effect == TextEffectType::RetroSplit) {
                        UiRect rr = tr; rr.x -= 2.5f;
                        m_brush->SetColor(D2D1::ColorF(1.0f, 0.18f, 0.38f, 0.8f * pose.opacity));
                        m_rt->DrawTextW(wTxt.c_str(), static_cast<UINT32>(wTxt.size()), txtFmt.Get(), rr.d2d(), m_brush.Get());
                        UiRect br2 = tr; br2.x += 2.5f;
                        m_brush->SetColor(D2D1::ColorF(0.0f, 0.90f, 0.95f, 0.8f * pose.opacity));
                        m_rt->DrawTextW(wTxt.c_str(), static_cast<UINT32>(wTxt.size()), txtFmt.Get(), br2.d2d(), m_brush.Get());
                    }
                    // Text Stroke (crisp outline)
                    if (c->text.strokeWidth > 0.2f) {
                        float sw = c->text.strokeWidth;
                        D2D1_COLOR_F sc = argbToD2D(c->text.strokeARGB);
                        sc.a *= pose.opacity;
                        m_brush->SetColor(sc);
                        const float offs[8][2] = {{-sw,0},{sw,0},{0,-sw},{0,sw},{-sw,-sw},{sw,-sw},{-sw,sw},{sw,sw}};
                        for (auto& o : offs) {
                            UiRect sr2 = tr; sr2.x += o[0]; sr2.y += o[1];
                            m_rt->DrawTextW(wTxt.c_str(), static_cast<UINT32>(wTxt.size()), txtFmt.Get(), sr2.d2d(), m_brush.Get());
                        }
                    }
                    // Text Shadow
                    if (c->text.shadow) {
                        UiRect sr = tr; sr.x += 2.5f; sr.y += 2.5f;
                        m_brush->SetColor(D2D1::ColorF(0, 0, 0, 0.75f * pose.opacity));
                        m_rt->DrawTextW(wTxt.c_str(), static_cast<UINT32>(wTxt.size()), txtFmt.Get(), sr.d2d(), m_brush.Get());
                    }
                    // Main Text (supports curved arch when c->text.curve != 0!)
                    D2D1_COLOR_F col = (c->text.effect == TextEffectType::GoldLuxury) ? kColGold : argbToD2D(c->text.colorARGB);
                    col.a *= pose.opacity;
                    m_brush->SetColor(col);
                    if (std::abs(c->text.curve) > 0.05f && wTxt.size() > 1) {
                        float arch = c->text.curve * 48.0f;
                        float stepX = scaledFont * 0.62f;
                        float totalW = stepX * static_cast<float>(wTxt.size() - 1);
                        for (size_t ci = 0; ci < wTxt.size(); ++ci) {
                            float u = (static_cast<float>(ci) / std::max<size_t>(1, wTxt.size() - 1)) * 2.0f - 1.0f;
                            float px = cx - totalW * 0.5f + ci * stepX;
                            float py = cy + u * u * arch;
                            std::wstring ch(1, wTxt[ci]);
                            UiRect cr2{ px - scaledFont, py - scaledFont, scaledFont * 2.0f, scaledFont * 2.0f };
                            m_rt->DrawTextW(ch.c_str(), 1, txtFmt.Get(), cr2.d2d(), m_brush.Get());
                        }
                    } else {
                        m_rt->DrawTextW(wTxt.c_str(), static_cast<UINT32>(wTxt.size()), txtFmt.Get(), tr.d2d(), m_brush.Get());
                    }
                }
            }

            m_rt->SetTransform(D2D1::Matrix3x2F::Identity());
            if (hasWipe) m_rt->PopAxisAlignedClip();
        }
    }

    // Render Freehand Drawing Strokes on top of preview canvas
    for (const auto& st : m_engine.project().drawings) {
        if (st.points.size() < 2) continue;
        m_brush->SetColor(argbToD2D(st.colorARGB));
        for (size_t pi = 1; pi < st.points.size(); ++pi) {
            m_rt->DrawLine(
                D2D1::Point2F(m_rcVideoCanvas.x + st.points[pi - 1].nx * m_rcVideoCanvas.w,
                              m_rcVideoCanvas.y + st.points[pi - 1].ny * m_rcVideoCanvas.h),
                D2D1::Point2F(m_rcVideoCanvas.x + st.points[pi].nx * m_rcVideoCanvas.w,
                              m_rcVideoCanvas.y + st.points[pi].ny * m_rcVideoCanvas.h),
                m_brush.Get(), st.thickness);
        }
    }

    if (m_scopeMode > 0) {
        if (m_scopeStamp == 0 && m_gradedFrame.valid()) updateVideoScopes(m_gradedFrame);
        renderVideoScopesOverlay();
    }
    if (m_showAudioSpectrum) renderAudioSpectrumOverlay();
    if (m_photoTool == PhotoTool::Clone && m_cloneSrcNx >= 0.0f) {
        float sx = m_rcVideoCanvas.x + m_cloneSrcNx * m_rcVideoCanvas.w;
        float sy = m_rcVideoCanvas.y + m_cloneSrcNy * m_rcVideoCanvas.h;
        m_brush->SetColor(kColAccent);
        m_rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(sx, sy), 9.0f, 9.0f), m_brush.Get(), 1.5f);
        m_rt->DrawLine(D2D1::Point2F(sx - 12, sy), D2D1::Point2F(sx + 12, sy), m_brush.Get(), 1.2f);
        m_rt->DrawLine(D2D1::Point2F(sx, sy - 12), D2D1::Point2F(sx, sy + 12), m_brush.Get(), 1.2f);
    }

    m_rt->PopAxisAlignedClip();

    m_brush->SetColor(kColBorder);
    m_rt->DrawRectangle(m_rcVideoCanvas.d2d(), m_brush.Get(), 1.0f);

    float barY = m_rcPreviewPanel.y + m_rcPreviewPanel.h - 42.0f;
    float midX = m_rcPreviewPanel.x + m_rcPreviewPanel.w * 0.5f;

    std::wstring tc = utf8ToWide(formatTimecode(m_engine.playhead()) + " / " + formatTimecode(m_engine.totalDuration()));
    drawText(tc, { m_rcPreviewPanel.x + 16, barY, 195, 30 }, m_fontMono, kColAccent);

    auto addTrBtn = [&](int id, const wchar_t* sym, float x, float w, bool accent = false) {
        UiRect r{ x, barY, w, 30 };
        bool hov = r.contains(m_hoverX, m_hoverY);
        drawRoundRect(r, 6.0f, accent ? kColAccent : (hov ? kColCardHover : kColCard));
        drawText(sym, r, m_fontBold, accent ? kColDarkText : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ id, r, 0 });
    };
    addTrBtn(BTN_JUMP_START, L"|<", midX - 88, 34);
    addTrBtn(BTN_STEP_BACK,  L"<",  midX - 48, 30);
    addTrBtn(BTN_PLAY_PAUSE, m_playing ? L"||" : L">", midX - 12, 38, true);
    addTrBtn(BTN_STEP_FWD,   L">|", midX + 32, 34);
}

void MainWindow::rebuildInspectorSliders() {
    m_sliders.clear();
    Clip* c = m_engine.selectedClip();
    if (!c) return;

    float x = m_rcInspectorPanel.x + 14.0f;
    float w = m_rcInspectorPanel.w - 28.0f;
    float y = m_rcInspectorPanel.y + 132.0f - m_inspScrollY;

    auto addSld = [&](int id, const wchar_t* label, float minV, float maxV, float val, const wchar_t* suffix) {
        SliderDef s;
        s.id = id;
        s.label = label;
        s.minVal = minV;
        s.maxVal = maxV;
        s.value = val;
        s.suffix = suffix;
        s.trackRect = { x, y + 16.0f, w, 12.0f };
        m_sliders.push_back(s);
        y += 33.0f;
    };

    if (c->type == MediaType::Text) {
        y += 148.0f;
        addSld(SLD_FONTSIZE,     tr(L"Font Size",     L"حجم الخط"),      18.0f, 140.0f, c->text.fontSize, L"px");
        addSld(SLD_TXT_STROKE,   tr(L"Text Stroke",   L"حدود النص"),      0.0f,   5.0f, c->text.strokeWidth, L"px");
        addSld(SLD_TXT_TRACKING, tr(L"Text Tracking", L"تباعد الحروف"),   0.0f,   8.0f, c->text.tracking, L"");
        addSld(SLD_TXT_CURVE,    tr(L"Text Curve",    L"انحناء النص"),   -1.0f,   1.0f, c->text.curve, L"");
        addSld(SLD_POS_X,    tr(L"Position X", L"أفقي X"),   -1.0f, 1.0f,   c->transform.posX, L"");
        addSld(SLD_POS_Y,    tr(L"Position Y", L"عمودي Y"),  -1.0f, 1.0f,   c->transform.posY, L"");
        addSld(SLD_SCALE,    tr(L"Scale",      L"الحجم"),     0.2f, 2.5f,   c->transform.scale, L"x");
        addSld(SLD_OPACITY,  tr(L"Opacity",    L"الشفافية"),  0.0f, 1.0f,   c->color.opacity, L"");
        if (c->transitionIn != TransitionType::None) {
            addSld(SLD_TRANS_DUR, tr(L"Trans. Dur", L"مدة الانتقال"), 0.15f, 2.5f, static_cast<float>(c->transitionDuration), L"s");
        }
        return;
    }

    if (m_inspTab == InspectorTab::Audio || c->type == MediaType::Audio) {
        y += 142.0f; // room for Ducking, Vocal Iso, Voice FX, Enhance, Sync, Silence Trim, Restore & Spectrum
        addSld(SLD_VOLUME,       tr(L"Volume",     L"الصوت"),          0.0f, 2.0f,  c->volume, L"x");
        addSld(SLD_FADE_IN,      tr(L"Fade In",    L"تلاشي دخول"),     0.0f, 3.0f,  c->audioFadeIn, L"s");
        addSld(SLD_FADE_OUT,     tr(L"Fade Out",   L"تلاشي خروج"),     0.0f, 3.0f,  c->audioFadeOut, L"s");
        addSld(SLD_AUDIO_PAN,    tr(L"Stereo Pan", L"توازن ستيريو"),  -1.0f, 1.0f,  c->audioDsp.pan, L"");
        addSld(SLD_AUDIO_PITCH,  tr(L"Pitch/Voice",L"طبقة الصوت"),     0.6f, 1.6f,  c->audioDsp.pitch, L"x");
        addSld(SLD_AUDIO_BASS,   tr(L"EQ Bass",    L"معادل الجهير"),   0.0f, 1.0f,  c->audioDsp.bassBoost, L"");
        addSld(SLD_AUDIO_MID,    tr(L"EQ Mid",     L"معادل الوسط"),   -1.0f, 1.0f,  c->audioDsp.midEq, L"");
        addSld(SLD_AUDIO_TREBLE, tr(L"Treble EQ",  L"معادل الحدة"),   -1.0f, 1.0f,  c->audioDsp.treble, L"");
        addSld(SLD_AUDIO_ECHO,   tr(L"Echo/Reverb",L"صدى صوتي"),       0.0f, 1.0f,  c->audioDsp.echo, L"");
        addSld(SLD_AUDIO_NOISE,  tr(L"Noise Red.", L"عزل الضوضاء"),    0.0f, 1.0f,  c->audioDsp.noiseReduction, L"");
        addSld(SLD_AUDIO_COMP,    tr(L"Compressor",   L"ضغط الصوت"),      0.0f, 1.0f,  c->audioDsp.compressor, L"");
        addSld(SLD_AUDIO_LIMITER, tr(L"Limiter Ceil", L"منع التشوه"),     0.2f, 1.0f,  c->audioDsp.limiter, L"");
        addSld(SLD_AUDIO_REVERB,  tr(L"Room Reverb",  L"الصدى المكاني"),  0.0f, 1.0f,  c->audioDsp.reverb, L"");
        addSld(SLD_AUDIO_DEESSER, tr(L"De-Esser",     L"تخفيف الصفير"),   0.0f, 1.0f,  c->audioDsp.deEsser, L"");
        addSld(SLD_AUDIO_WIDTH,   tr(L"Stereo Width", L"توسيع الاستيريو"),0.0f, 2.0f,  c->audioDsp.stereoWidth, L"x");
    } else if (m_inspTab == InspectorTab::Basic) {
        y += 94.0f; // room for Quick Tools + Speed Pills + Reverse/Mask row
        addSld(SLD_SCALE,    tr(L"Scale",      L"الحجم"),     0.2f, 2.5f,   c->transform.scale, L"x");
        addSld(SLD_POS_X,    tr(L"Position X", L"أفقي X"),   -1.0f, 1.0f,   c->transform.posX, L"");
        addSld(SLD_POS_Y,    tr(L"Position Y", L"عمودي Y"),  -1.0f, 1.0f,   c->transform.posY, L"");
        addSld(SLD_ROTATION, tr(L"Rotation",   L"الدوران"),-180.0f, 180.0f, c->transform.rotationDeg, L"°");
        addSld(SLD_OPACITY,  tr(L"Opacity",    L"الشفافية"),  0.0f, 1.0f,   c->color.opacity, L"");
        addSld(SLD_CROP_X,   tr(L"Crop Width", L"قص أفقي"),   0.0f, 0.45f,  c->transform.cropX, L"");
        addSld(SLD_CROP_Y,       tr(L"Crop Height",  L"قص عمودي"),     0.0f, 0.45f, c->transform.cropY, L"");
        addSld(SLD_MASK_FEATHER, tr(L"Mask Feather", L"تنعيم القناع"), 0.0f, 0.80f, c->maskFeather, L"");
        addSld(SLD_SKEW_X,   tr(L"Persp. X",   L"منظور X"),  -25.0f, 25.0f, c->transform.skewX, L"°");
        addSld(SLD_SKEW_Y,   tr(L"Persp. Y",   L"منظور Y"),  -25.0f, 25.0f, c->transform.skewY, L"°");
        addSld(SLD_SPEED_START, tr(L"Ramp Start", L"سرعة البداية"), 0.25f, 4.0f, c->speedStart, L"x");
        addSld(SLD_SPEED_MID,   tr(L"Ramp Mid",   L"سرعة الوسط"),   0.25f, 4.0f, c->speedMid,   L"x");
        addSld(SLD_SPEED_END,   tr(L"Ramp End",   L"سرعة النهاية"), 0.25f, 4.0f, c->speedEnd,   L"x");
    } else if (m_inspTab == InspectorTab::Color) {
        y += 224.0f; // room for LUT, WB, Scopes, Brushes, Batch & 8-Band HSL Selector
        addSld(SLD_BRIGHTNESS, tr(L"Brightness", L"السطوع"),     -0.5f, 0.5f, c->color.brightness, L"");
        addSld(SLD_CONTRAST,   tr(L"Contrast",   L"التباين"),     0.4f, 1.8f, c->color.contrast, L"");
        addSld(SLD_EXPOSURE,   tr(L"Exposure",   L"التعريض"),    -1.0f, 1.0f, c->color.exposure, L"");
        addSld(SLD_SATURATION, tr(L"Saturation", L"التشبع"),      0.0f, 2.0f, c->color.saturation, L"");
        addSld(SLD_VIBRANCE,   tr(L"Vibrance",   L"الحيوية"),    -1.0f, 1.0f, c->color.vibrance, L"");
        addSld(SLD_TEMP,       tr(L"Temperature",L"الحرارة"),    -0.6f, 0.6f, c->color.temperature, L"");
        addSld(SLD_TINT,       tr(L"Tint",       L"التوازن"),    -0.6f, 0.6f, c->color.tint, L"");
        addSld(SLD_HIGHLIGHTS, tr(L"Highlights", L"الإضاءات"),   -1.0f, 1.0f, c->color.highlights, L"");
        addSld(SLD_SHADOWS,    tr(L"Shadows",    L"الظلال"),     -1.0f, 1.0f, c->color.shadows, L"");
        addSld(SLD_SHARPNESS,  tr(L"Sharpness",  L"الحدة"),       0.0f, 1.0f, c->color.sharpness, L"");
        addSld(SLD_WHITES,       tr(L"Whites",      L"الأبيض"),      -1.0f, 1.0f,  c->color.whites, L"");
        addSld(SLD_BLACKS,       tr(L"Blacks",      L"الأسود"),      -1.0f, 1.0f,  c->color.blacks, L"");
        addSld(SLD_CLARITY,      tr(L"Clarity",     L"الوضوح"),       0.0f, 1.0f,  c->color.clarity, L"");
        addSld(SLD_BLUR,         tr(L"Blur",        L"التمويه"),      0.0f, 1.0f,  c->color.blur, L"");
        addSld(SLD_HSL_HUE,      tr(L"HSL Hue",       L"تدوير الألوان"), -180.0f, 180.0f, c->color.hslHue, L"°");
        addSld(SLD_HSL_RED,      tr(L"HSL Red Sat",   L"HSL أحمر"),        -1.0f,   1.0f, c->color.hslRedSat, L"");
        addSld(SLD_HSL_GREEN,    tr(L"HSL Green Sat", L"HSL أخضر"),        -1.0f,   1.0f, c->color.hslGreenSat, L"");
        addSld(SLD_HSL_BLUE,     tr(L"HSL Blue Sat",  L"HSL أزرق"),        -1.0f,   1.0f, c->color.hslBlueSat, L"");
        addSld(SLD_WHEEL_SHADOW, tr(L"Wheel Shadow",  L"عجلة الظلال"),     -1.0f,   1.0f, c->color.wheelShadow, L"");
        addSld(SLD_WHEEL_MID,    tr(L"Wheel Midtone", L"عجلة الوسط"),      -1.0f,   1.0f, c->color.wheelMid, L"");
        addSld(SLD_WHEEL_HIGH,   tr(L"Wheel High",    L"عجلة الإضاءة"),    -1.0f,   1.0f, c->color.wheelHigh, L"");
        addSld(SLD_CURVE_R,      tr(L"RGB Curve R",   L"منحنى الأحمر"),     -0.5f,   0.5f, c->color.curveR, L"");
        addSld(SLD_CURVE_G,      tr(L"RGB Curve G",   L"منحنى الأخضر"),     -0.5f,   0.5f, c->color.curveG, L"");
        addSld(SLD_CURVE_B,      tr(L"RGB Curve B",   L"منحنى الأزرق"),     -0.5f,   0.5f, c->color.curveB, L"");
        addSld(SLD_HSL_TARGET,   tr(L"HSL Target Hue",L"لون HSL محدد"),      0.0f, 360.0f, c->color.hslTargetHue, L"°");
        addSld(SLD_HSL_SECSHIFT, tr(L"HSL Sec. Shift",L"تغيير اللون المحدد"),-180.0f,180.0f, c->color.hslSecShift, L"°");
        addSld(SLD_HSL_SECSAT,   tr(L"HSL Sec. Sat",  L"تشبع اللون المحدد"), -1.0f,   1.0f, c->color.hslSecSat, L"");
        addSld(SLD_SPLIT_SH_HUE, tr(L"Split Shadow°", L"صبغة الظلال°"),      0.0f, 360.0f, c->color.splitShadowHue, L"°");
        addSld(SLD_SPLIT_HI_HUE, tr(L"Split High°",   L"صبغة الإضاءة°"),     0.0f, 360.0f, c->color.splitHighHue, L"°");
        addSld(SLD_SPLIT_AMT,    tr(L"Split Toning",  L"قوة التلوين الثنائي"),0.0f,   1.0f, c->color.splitAmount, L"");
        addSld(SLD_PHOTO_DENOISE,tr(L"Photo Denoise", L"تقليل التشويش"),      0.0f,   1.0f, c->color.photoDenoise, L"");
        addSld(SLD_GRADIENT_OVL, tr(L"Gradient Tool", L"تدرج لوني"),          0.0f,   1.0f, c->color.gradientOverlay, L"");
        addSld(SLD_RAW_FILMIC,   tr(L"RAW Filmic",    L"نطاق RAW سينمائي"),   0.0f,   1.0f, c->color.rawFilmicTone, L"");
        addSld(SLD_HSL_BAND_SAT, tr(L"8-Band HSL Sat",L"تشبع النطاق اللوني"),  -1.0f,   1.0f, c->color.hslBandSat[m_hslBandIdx % 8], L"");
        addSld(SLD_HSL_BAND_LUM, tr(L"8-Band HSL Lum",L"إضاءة النطاق اللوني"), -1.0f,   1.0f, c->color.hslBandLum[m_hslBandIdx % 8], L"");
        addSld(SLD_CURVE_SH_PT,  tr(L"Curve Shadows", L"نقطة منحنى الظلال"),   -0.4f,   0.4f, c->color.curveShadowPt, L"");
        addSld(SLD_CURVE_HI_PT,  tr(L"Curve Highs",   L"نقطة منحنى الإضاءة"),  -0.4f,   0.4f, c->color.curveHighPt, L"");
        addSld(SLD_BRUSH_SIZE,   tr(L"Brush Size",    L"حجم الفرشاة"),          0.02f,  0.14f, m_brushRadius, L"");
        addSld(SLD_BRUSH_PRESS,  tr(L"Brush Press.",  L"ضغط الفرشاة/التشكيل"),  0.25f,  1.60f, m_brushPressure, L"x");
        addSld(SLD_PERSP_WARP_H, tr(L"Keystone H",    L"تشويه منظور أفقي"),   -0.45f,  0.45f, c->color.perspWarpH, L"");
        addSld(SLD_PERSP_WARP_V, tr(L"Keystone V",    L"تشويه منظور عمودي"),  -0.45f,  0.45f, c->color.perspWarpV, L"");
        addSld(SLD_SKIN_SMOOTH,  tr(L"Skin Smooth", L"تنعيم البشرة"), 0.0f, 1.0f,  c->color.skinSmooth, L"");
        addSld(SLD_TEETH_WHITEN,   tr(L"Teeth White",   L"تبييض الأسنان"), 0.0f, 1.0f,  c->color.teethWhiten, L"");
        addSld(SLD_FACE_RETOUCH,   tr(L"Face Retouch",  L"تحسين الوجه"),   0.0f, 1.0f,  c->color.faceRetouch, L"");
        addSld(SLD_FACE_SLIM,      tr(L"Face Reshape",  L"تعديل الملامح"),-0.4f, 0.4f,  c->color.faceSlim, L"");
        addSld(SLD_BODY_SLIM,      tr(L"Body Reshape",  L"تعديل الجسم"),  -0.4f, 0.4f,  c->color.bodySlim, L"");
        addSld(SLD_ENHANCE_HD,     tr(L"HD Enhance",    L"تحسين الجودة"),  0.0f, 1.0f,  c->color.enhanceHD, L"");
        addSld(SLD_PORTRAIT_BOKEH, tr(L"Portrait Blur", L"بورتريه عزل"),   0.0f, 1.0f,  c->color.portraitBokeh, L"");
        addSld(SLD_CANVAS_EXPAND,  tr(L"Canvas Expand", L"توسيع الصورة"),  0.0f, 0.35f, c->color.canvasExpand, L"");
    } else if (m_inspTab == InspectorTab::FX) {
        y += 308.0f; // room for Chroma + NLE + Blend + Trackers + Scene Cut + Stab + 3D Cam Track
        addSld(SLD_CHROMA_SIM,    tr(L"Key Similarity", L"تشابه اللون"), 0.05f, 0.70f, c->chromaKey.similarity, L"");
        addSld(SLD_CHROMA_SMOOTH, tr(L"Key Smoothness", L"نعومة الحواف"), 0.01f, 0.35f, c->chromaKey.smoothness, L"");
        addSld(SLD_FX_INTENSITY,  tr(L"FX Intensity",   L"قوة المؤثر"),   0.0f, 1.0f,  c->effect.intensity, L"");
        addSld(SLD_TRANS_DUR,     tr(L"Trans. In",      L"مدة الدخول"),   0.15f, 2.5f, static_cast<float>(c->transitionDuration), L"s");
        addSld(SLD_FADE_OUT_VIS,  tr(L"Fade Out",       L"تلاشي خروج"),   0.0f,  2.5f, static_cast<float>(c->fadeOutDuration), L"s");
        addSld(SLD_LENS_DISTORT,  tr(L"Lens Correct",   L"تصحيح العدسة"),-0.5f,  0.5f, c->color.lensDistort, L"");
        addSld(SLD_ROLL_SHUTTER,  tr(L"Roll Shutter",   L"إصلاح الحركة"),-0.25f, 0.25f, c->color.rollingShutter, L"");
    }
}

void MainWindow::renderInspectorPanel() {
    m_brush->SetColor(kColPanel);
    m_rt->FillRectangle(m_rcInspectorPanel.d2d(), m_brush.Get());

    m_brush->SetColor(kColBorder);
    m_rt->DrawLine(D2D1::Point2F(m_rcInspectorPanel.x, m_rcInspectorPanel.y),
                   D2D1::Point2F(m_rcInspectorPanel.x, m_rcInspectorPanel.y + m_rcInspectorPanel.h),
                   m_brush.Get(), 1.0f);

    Clip* c = m_engine.selectedClip();
    drawText(tr(L"Inspector", L"الخصائص"), { m_rcInspectorPanel.x + 14, m_rcInspectorPanel.y + 6, 120, 24 }, m_fontBold, kColTextMain);

    if (!c) {
        m_sliders.clear();
        UiRect infoRc{ m_rcInspectorPanel.x + 14, m_rcInspectorPanel.y + 42, m_rcInspectorPanel.w - 28, 140 };
        drawRoundRect(infoRc, 8.0f, kColCard, true, kColBorder, 1.0f);
        drawText(tr(L"No Clip Selected\n\nClick any clip on Timeline\nto edit Basic, Color, Audio,\nChroma Key, Mask & FX.",
                    L"لم يتم تحديد مقطع\n\nاضغط على أي مقطع بالتايملاين\nلتعديل الألوان والصوت والقص\nوالكروما والأقنعة والمؤثرات."),
                 infoRc, m_fontSmall, kColTextMuted, DWRITE_TEXT_ALIGNMENT_CENTER);
        return;
    }

    UiRect nameRc{ m_rcInspectorPanel.x + 14, m_rcInspectorPanel.y + 34, m_rcInspectorPanel.w - 92, 24 };
    drawRoundRect(nameRc, 5.0f, kColCard);
    drawText(utf8ToWide(c->name), { nameRc.x + 8, nameRc.y, nameRc.w - 14, nameRc.h }, m_fontSmall, kColAccent);

    UiRect resetRc{ m_rcInspectorPanel.x + m_rcInspectorPanel.w - 72, m_rcInspectorPanel.y + 34, 58, 24 };
    drawRoundRect(resetRc, 5.0f, resetRc.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColBorder, 1.0f);
    drawText(tr(L"Reset", L"إعادة"), resetRc, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_buttons.push_back({ BTN_RESET_COLOR, resetRc, 0 });

    // 4 Inspector Sub-Tabs: Basic | Color | Audio | FX
    const wchar_t* subLbls[4] = { tr(L"Basic", L"أساسي"), tr(L"Color", L"ألوان"), tr(L"Audio", L"صوت"), tr(L"FX", L"مؤثر") };
    const int subIds[4] = { BTN_INSP_BASIC, BTN_INSP_COLOR, BTN_INSP_AUDIO, BTN_INSP_FX };
    float subW = (m_rcInspectorPanel.w - 28.0f) / 4.0f;
    for (int i = 0; i < 4; ++i) {
        UiRect sr{ m_rcInspectorPanel.x + 14.0f + i * subW, m_rcInspectorPanel.y + 64.0f, subW - 3.0f, 24.0f };
        bool act = (static_cast<int>(m_inspTab) == i);
        drawRoundRect(sr, 5.0f, act ? kColAccent : kColCard);
        drawText(subLbls[i], sr, m_fontSmall, act ? kColDarkText : kColTextMuted, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ subIds[i], sr, 0 });
    }

    // Keyframe toggle button
    bool hasKf = m_engine.hasKeyframeNearPlayhead(c->id);
    UiRect kfBtn{ m_rcInspectorPanel.x + 14, m_rcInspectorPanel.y + 94, m_rcInspectorPanel.w - 28, 24 };
    drawRoundRect(kfBtn, 6.0f, hasKf ? kColGold : (kfBtn.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard),
                  true, kColGold, 1.1f);
    wchar_t kfLabel[64];
    std::swprintf(kfLabel, 64, hasKf ? L"◆ Remove Keyframe (%zu)" : L"◇ + Keyframe (%zu)", c->keyframes.size());
    drawText(kfLabel, kfBtn, m_fontSmall, hasKf ? kColDarkText : kColGold, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_buttons.push_back({ BTN_TOGGLE_KEYFRAME, kfBtn, 0 });

    float qx = m_rcInspectorPanel.x + 14.0f;
    float fullW = m_rcInspectorPanel.w - 28.0f;

    if (c->type == MediaType::Text) {
        UiRect editBtn{ qx, m_rcInspectorPanel.y + 126, 108, 26 };
        drawRoundRect(editBtn, 6.0f, kColAccent);
        drawText(tr(L"Edit Text...", L"تعديل النص..."), editBtn, m_fontSmall, kColDarkText, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_EDIT_TEXT, editBtn, 0 });

        const uint32_t palette[5] = { 0xFFFFFFFF, 0xFF00E5D1, 0xFFFFD84D, 0xFFFF5E6C, 0xFF141416 };
        for (int i = 0; i < 5; ++i) {
            UiRect sw{ editBtn.x + 116 + i * 25.0f, editBtn.y + 3, 20, 20 };
            bool selCol = (c->text.colorARGB == palette[i]);
            drawRoundRect(sw, 10.0f, argbToD2D(palette[i]), true, selCol ? kColAccent : kColBorder, selCol ? 2.0f : 1.0f);
            m_buttons.push_back({ BTN_TEXT_COLOR, sw, palette[i] });
        }
        UiRect animBtn{ qx, m_rcInspectorPanel.y + 158, fullW, 25 };
        drawRoundRect(animBtn, 6.0f, animBtn.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColPurple, 1.0f);
        std::wstring animLbl = std::wstring(tr(L"Text Anim: ", L"حركة النص: ")) + utf8ToWide(textAnimName(c->text.anim));
        drawText(animLbl, animBtn, m_fontSmall, kColPurple, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CYCLE_TEXT_ANIM, animBtn, 0 });
        UiRect fontBtn{ qx, m_rcInspectorPanel.y + 188, fullW, 24 };
        drawRoundRect(fontBtn, 6.0f, fontBtn.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColAccent, 1.0f);
        drawText(std::wstring(tr(L"Font: ", L"الخط: ")) + utf8ToWide(c->text.fontName), fontBtn, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CYCLE_FONT, fontBtn, 0 });
        float hwT = (fullW - 6.0f) * 0.5f;
        UiRect fxTBtn{ qx, m_rcInspectorPanel.y + 216, hwT, 23 };
        drawRoundRect(fxTBtn, 5.0f, c->text.effect != TextEffectType::None ? kColPurple : kColCard, true, kColBorder, 1.0f);
        drawText(std::wstring(tr(L"FX: ", L"مؤثر: ")) + utf8ToWide(textEffectName(c->text.effect)), fxTBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CYCLE_TXTEFFECT, fxTBtn, 0 });

        UiRect bgTBtn{ qx + hwT + 6.0f, m_rcInspectorPanel.y + 216, hwT, 23 };
        drawRoundRect(bgTBtn, 5.0f, c->text.hasBackground ? kColAccentDim : kColCard, true, kColBorder, 1.0f);
        drawText(c->text.hasBackground ? tr(L"BG: ON", L"خلفية: مفعل") : tr(L"Text BG", L"خلفية النص"), bgTBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_TOGGLE_TXTBG, bgTBtn, 0 });

        UiRect shTBtn{ qx, m_rcInspectorPanel.y + 244, hwT, 23 };
        drawRoundRect(shTBtn, 5.0f, c->text.shadow ? kColAccentDim : kColCard, true, kColBorder, 1.0f);
        drawText(c->text.shadow ? tr(L"Shadow: ON", L"ظل: مفعل") : tr(L"Shadow: OFF", L"ظل النص"), shTBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_TOGGLE_TXTSHADOW, shTBtn, 0 });

        UiRect spTBtn{ qx + hwT + 6.0f, m_rcInspectorPanel.y + 244, hwT, 23 };
        drawRoundRect(spTBtn, 5.0f, kColCard, true, kColGold, 1.0f);
        drawText(tr(L"Speak (TTS)", L"تحويل لصوت"), spTBtn, m_fontSmall, kColGold, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_SPEAK_TEXTCLIP, spTBtn, 0 });
    } else if (m_inspTab == InspectorTab::Basic && c->type != MediaType::Audio) {
        float qw = (fullW - 16.0f) / 5.0f;
        struct QBtn { int id; const wchar_t* lbl; bool on; };
        QBtn qbtns[5] = {
            { BTN_FLIP_H,       tr(L"Flip H", L"قلب H"), c->transform.flipH },
            { BTN_FLIP_V,       tr(L"Flip V", L"قلب V"), c->transform.flipV },
            { BTN_ROT_90,       L"90°",                  false },
            { BTN_FILL_SCREEN,  tr(L"Fill",   L"ملء"),   false },
            { BTN_AUTO_ENHANCE, tr(L"Auto",   L"تحسين"), false }
        };
        for (int i = 0; i < 5; ++i) {
            UiRect r{ qx + i * (qw + 4.0f), m_rcInspectorPanel.y + 126.0f, qw, 24.0f };
            drawRoundRect(r, 5.0f, qbtns[i].on ? kColAccentDim : (r.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard),
                          true, qbtns[i].on ? kColAccent : kColBorder, 1.0f);
            drawText(qbtns[i].lbl, r, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
            m_buttons.push_back({ qbtns[i].id, r, 0 });
        }

        float sw = (fullW - 12.0f) / 4.0f;
        const wchar_t* spLabels[4] = { L"0.5x", L"1.0x", L"1.5x", L"2.0x" };
        const int spVals[4] = { 50, 100, 150, 200 };
        for (int i = 0; i < 4; ++i) {
            UiRect r{ qx + i * (sw + 4.0f), m_rcInspectorPanel.y + 156.0f, sw, 23.0f };
            bool active = std::abs(c->speed - spVals[i] / 100.0) < 0.05;
            drawRoundRect(r, 5.0f, active ? kColAccent : (r.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard),
                          true, active ? kColAccent : kColBorder, 1.0f);
            drawText(spLabels[i], r, m_fontSmall, active ? kColDarkText : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
            m_buttons.push_back({ BTN_SPEED_PRESET, r, static_cast<uint64_t>(spVals[i]) });
        }

        // Reverse + Mask Cycler row
        float hw = (fullW - 6.0f) * 0.5f;
        UiRect revBtn{ qx, m_rcInspectorPanel.y + 185.0f, hw, 24.0f };
        drawRoundRect(revBtn, 5.0f, c->reversed ? kColAccentDim : kColCard, true, c->reversed ? kColAccent : kColBorder, 1.0f);
        drawText(c->reversed ? tr(L"Reverse: ON", L"عكس: مفعل") : tr(L"Reverse: OFF", L"عكس الفيديو"), revBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_TOGGLE_REVERSE, revBtn, 0 });

        UiRect mskBtn{ qx + hw + 6.0f, m_rcInspectorPanel.y + 185.0f, hw, 24.0f };
        drawRoundRect(mskBtn, 5.0f, c->mask != MaskType::None ? kColPurple : kColCard, true, kColBorder, 1.0f);
        std::wstring mLbl = std::wstring(tr(L"Mask: ", L"قناع: ")) + utf8ToWide(maskName(c->mask));
        drawText(mLbl, mskBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CYCLE_MASK, mskBtn, 0 });
    } else if (m_inspTab == InspectorTab::Color && c->type != MediaType::Audio) {
        float hw = (fullW - 6.0f) * 0.5f;
        // Row 1: Import LUT | Export LUT
        UiRect lutBtn{ qx, m_rcInspectorPanel.y + 126.0f, hw, 24.0f };
        drawRoundRect(lutBtn, 5.0f, !c->color.lutPath.empty() ? kColAccentDim : kColCard, true, kColAccent, 1.0f);
        drawText(c->color.lutPath.empty() ? tr(L"Import LUT", L"استيراد LUT") : tr(L"LUT: Active", L"LUT: مفعل"),
                 lutBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_LOAD_LUT, lutBtn, 0 });

        UiRect expLutBtn{ qx + hw + 6.0f, m_rcInspectorPanel.y + 126.0f, hw, 24.0f };
        drawRoundRect(expLutBtn, 5.0f, kColCard, true, kColGold, 1.0f);
        drawText(tr(L"Export .cube", L"تصدير LUT"), expLutBtn, m_fontSmall, kColGold, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_EXPORT_LUT, expLutBtn, 0 });

        // Row 2: Auto White Balance | False Color IRE
        UiRect awbBtn{ qx, m_rcInspectorPanel.y + 154.0f, hw, 23.0f };
        drawRoundRect(awbBtn, 5.0f, kColCard, true, kColAccent, 1.0f);
        drawText(tr(L"Auto WB", L"توازن أبيض"), awbBtn, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_AUTO_WB, awbBtn, 0 });

        UiRect fcBtn{ qx + hw + 6.0f, m_rcInspectorPanel.y + 154.0f, hw, 23.0f };
        drawRoundRect(fcBtn, 5.0f, c->color.falseColor ? kColDanger : kColCard, true, c->color.falseColor ? kColDanger : kColBorder, 1.0f);
        drawText(c->color.falseColor ? tr(L"FalseCol: ON", L"كشف تعريض:ON") : tr(L"False Color", L"كشف التعريض"),
                 fcBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_FALSE_COLOR, fcBtn, 0 });

        // Row 3: Live Scopes (Hist/Wave/Vec/All) | RGB Curve Preset
        const wchar_t* scLblEn[5] = { L"Scopes: OFF", L"Histogram", L"Waveform", L"Vectorscope", L"All Scopes" };
        const wchar_t* scLblAr[5] = { L"المراقب: إيقاف", L"هستوغرام", L"موجة إضاءة", L"فيكتورسكوب", L"كل المراقبات" };
        int sm = std::clamp(m_scopeMode, 0, 4);
        UiRect scBtn{ qx, m_rcInspectorPanel.y + 182.0f, hw, 23.0f };
        drawRoundRect(scBtn, 5.0f, sm > 0 ? kColGreen : kColCard, true, sm > 0 ? kColGreen : kColBorder, 1.0f);
        drawText(tr(scLblEn[sm], scLblAr[sm]), scBtn, m_fontSmall, sm > 0 ? kColDarkText : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CYCLE_SCOPES, scBtn, 0 });

        UiRect crvBtn{ qx + hw + 6.0f, m_rcInspectorPanel.y + 182.0f, hw, 23.0f };
        drawRoundRect(crvBtn, 5.0f, c->color.curve != CurvePreset::Linear ? kColPurple : kColCard, true, kColBorder, 1.0f);
        drawText(std::wstring(tr(L"Curve: ", L"منحنى: ")) + utf8ToWide(curvePresetName(c->color.curve)), crvBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CYCLE_CURVE, crvBtn, 0 });

        // Row 4: Red Eye | BG Remove
        UiRect reBtn{ qx, m_rcInspectorPanel.y + 210.0f, hw, 23.0f };
        drawRoundRect(reBtn, 5.0f, c->color.redEyeRemove ? kColAccentDim : kColCard, true, kColBorder, 1.0f);
        drawText(tr(L"Red Eye", L"إزالة احمرار"), reBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_TOGGLE_REDEYE, reBtn, 0 });

        UiRect abgBtn{ qx + hw + 6.0f, m_rcInspectorPanel.y + 210.0f, hw, 23.0f };
        drawRoundRect(abgBtn, 5.0f, c->color.autoBgRemove ? kColGreen : kColCard, true, kColBorder, 1.0f);
        drawText(c->color.autoBgRemove ? tr(L"BG Cut: ON", L"إزالة خلفية: ON") : tr(L"BG Remove", L"إزالة الخلفية"), abgBtn, m_fontSmall, c->color.autoBgRemove ? kColDarkText : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_TOGGLE_AUTOBG, abgBtn, 0 });

        // Row 5: Studio BG
        UiRect sbgBtn{ qx, m_rcInspectorPanel.y + 238.0f, fullW, 22.0f };
        drawRoundRect(sbgBtn, 5.0f, c->studioBg != StudioBgPreset::None ? kColPurple : kColCard, true, kColBorder, 1.0f);
        drawText(std::wstring(tr(L"Studio BG: ", L"خلفية: ")) + utf8ToWide(studioBgName(c->studioBg)), sbgBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CYCLE_STUDIOBG, sbgBtn, 0 });

        // Row 6 (Batch 15): Photo Retouch Brush Selector + Clear Brushes
        UiRect brBtn{ qx, m_rcInspectorPanel.y + 264.0f, fullW - 46.0f, 22.0f };
        drawRoundRect(brBtn, 5.0f, m_photoTool != PhotoTool::None ? kColAccentDim : kColCard, true, kColAccent, 1.0f);
        drawText(std::wstring(tr(L"Brush: ", L"فرشاة: ")) + utf8ToWide(photoToolName(m_photoTool)), brBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CYCLE_PHOTO_TOOL, brBtn, 0 });

        UiRect clBtn{ qx + fullW - 42.0f, m_rcInspectorPanel.y + 264.0f, 42.0f, 22.0f };
        drawRoundRect(clBtn, 5.0f, kColCard, true, kColDanger, 1.0f);
        drawText(tr(L"Clr", L"مسح"), clBtn, m_fontSmall, kColDanger, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CLEAR_BRUSHES, clBtn, 0 });

        // Row 7 (Batch 15): Clipping Mask | Batch Edit | Batch Export
        float twP = (fullW - 8.0f) / 3.0f;
        UiRect cmBtn{ qx, m_rcInspectorPanel.y + 290.0f, twP, 22.0f };
        drawRoundRect(cmBtn, 4.0f, c->clippingMask ? kColPurple : kColCard, true, kColPurple, 1.0f);
        drawText(tr(L"ClipMask", L"قص قناع"), cmBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CLIPPING_MASK, cmBtn, 0 });

        UiRect beBtn{ qx + twP + 4.0f, m_rcInspectorPanel.y + 290.0f, twP, 22.0f };
        drawRoundRect(beBtn, 4.0f, kColCard, true, kColGold, 1.0f);
        drawText(tr(L"BatchEd", L"تعميم"), beBtn, m_fontSmall, kColGold, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_BATCH_EDIT, beBtn, 0 });

        UiRect bxBtn{ qx + (twP + 4.0f) * 2.0f, m_rcInspectorPanel.y + 290.0f, twP, 22.0f };
        drawRoundRect(bxBtn, 4.0f, kColCard, true, kColGreen, 1.0f);
        drawText(tr(L"BatchPNG", L"تصدير الكل"), bxBtn, m_fontSmall, kColGreen, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_BATCH_EXPORT, bxBtn, 0 });

        // Row 8 (V32): 8-Band HSL Color Band Selector
        const wchar_t* hslNamesEn[8] = { L"Red", L"Orange", L"Yellow", L"Green", L"Aqua", L"Blue", L"Purple", L"Magenta" };
        const wchar_t* hslNamesAr[8] = { L"أحمر", L"برتقالي", L"أصفر", L"أخضر", L"سماوي", L"أزرق", L"بنفسجي", L"أرجواني" };
        UiRect hbBtn{ qx, m_rcInspectorPanel.y + 316.0f, fullW, 22.0f };
        drawRoundRect(hbBtn, 5.0f, kColCard, true, kColAccent, 1.0f);
        drawText(std::wstring(tr(L"8-Band HSL Target: ", L"نطاق HSL (8 ألوان): ")) + tr(hslNamesEn[m_hslBandIdx % 8], hslNamesAr[m_hslBandIdx % 8]),
                 hbBtn, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CYCLE_HSL_BAND, hbBtn, 0 });
    } else if (m_inspTab == InspectorTab::Audio || c->type == MediaType::Audio) {
        UiRect duckBtn{ qx, m_rcInspectorPanel.y + 126.0f, fullW, 25.0f };
        drawRoundRect(duckBtn, 6.0f, c->audioDsp.autoDucking ? kColAccentDim : kColCard, true, kColBorder, 1.0f);
        drawText(c->audioDsp.autoDucking ? tr(L"Audio Ducking: ON", L"خفض الموسيقى عند الكلام: مفعل")
                                         : tr(L"Audio Ducking: OFF", L"خفض الموسيقى عند الكلام"),
                 duckBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_TOGGLE_DUCKING, duckBtn, 0 });
        float hw = (fullW - 6.0f) * 0.5f;
        UiRect vocBtn{ qx, m_rcInspectorPanel.y + 156.0f, hw, 23.0f };
        drawRoundRect(vocBtn, 5.0f, c->audioDsp.vocalIsolate ? kColAccentDim : kColCard, true, kColBorder, 1.0f);
        drawText(tr(L"Vocal Iso", L"عزل الصوت"), vocBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_TOGGLE_VOCAL, vocBtn, 0 });

        UiRect vcBtn{ qx + hw + 6.0f, m_rcInspectorPanel.y + 156.0f, hw, 23.0f };
        drawRoundRect(vcBtn, 5.0f, kColCard, true, kColPurple, 1.0f);
        drawText(tr(L"Voice FX", L"تغيير الصوت"), vcBtn, m_fontSmall, kColPurple, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_VOICE_PRESET, vcBtn, 0 });
        UiRect enhBtn{ qx, m_rcInspectorPanel.y + 184.0f, hw, 23.0f };
        drawRoundRect(enhBtn, 5.0f, kColCard, true, kColAccent, 1.0f);
        drawText(tr(L"Audio Enhance", L"تحسين الصوت"), enhBtn, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_AUDIO_ENHANCE, enhBtn, 0 });

        UiRect synBtn{ qx + hw + 6.0f, m_rcInspectorPanel.y + 184.0f, hw, 23.0f };
        drawRoundRect(synBtn, 5.0f, kColCard, true, kColGold, 1.0f);
        drawText(tr(L"Audio Sync", L"مزامنة الصوت"), synBtn, m_fontSmall, kColGold, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_AUDIO_SYNC, synBtn, 0 });

        UiRect silBtn{ qx, m_rcInspectorPanel.y + 212.0f, hw, 23.0f };
        drawRoundRect(silBtn, 5.0f, kColCard, true, kColAccentDim, 1.0f);
        drawText(tr(L"Trim Silence", L"حذف الصمت"), silBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_REMOVE_SILENCE, silBtn, 0 });

        UiRect resBtn{ qx + hw + 6.0f, m_rcInspectorPanel.y + 212.0f, hw, 23.0f };
        drawRoundRect(resBtn, 5.0f, kColCard, true, kColPurple, 1.0f);
        drawText(tr(L"Restore Audio", L"ترميم الصوت"), resBtn, m_fontSmall, kColPurple, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_AUDIO_RESTORE, resBtn, 0 });

        UiRect spcBtn{ qx, m_rcInspectorPanel.y + 240.0f, fullW, 22.0f };
        drawRoundRect(spcBtn, 5.0f, m_showAudioSpectrum ? kColGreen : kColCard, true, m_showAudioSpectrum ? kColGreen : kColBorder, 1.0f);
        drawText(m_showAudioSpectrum ? tr(L"Audio Spectrum: ON", L"طيف الصوت الحي: مفعل") : tr(L"Audio Spectrum: OFF", L"عرض طيف الصوت"),
                 spcBtn, m_fontSmall, m_showAudioSpectrum ? kColDarkText : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_TOGGLE_SPECTRUM, spcBtn, 0 });
    } else if (m_inspTab == InspectorTab::FX) {
        UiRect ckBtn{ qx, m_rcInspectorPanel.y + 126.0f, fullW - 58.0f, 25.0f };
        drawRoundRect(ckBtn, 6.0f, c->chromaKey.enabled ? kColGreen : kColCard, true, c->chromaKey.enabled ? kColGreen : kColBorder, 1.0f);
        drawText(c->chromaKey.enabled ? tr(L"Chroma Key: ON", L"الكروما: مفعل") : tr(L"Chroma Key: OFF", L"إزالة الكروما"),
                 ckBtn, m_fontSmall, c->chromaKey.enabled ? kColDarkText : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_TOGGLE_CHROMA, ckBtn, 0 });

        UiRect grnSw{ ckBtn.x + ckBtn.w + 6, ckBtn.y + 2, 22, 20 };
        UiRect bluSw{ grnSw.x + 26, ckBtn.y + 2, 22, 20 };
        bool isGreen = (c->chromaKey.keyColorRGB == 0x00B140);
        drawRoundRect(grnSw, 4.0f, D2D1::ColorF(0.0f, 0.70f, 0.25f, 1.0f), true, isGreen ? kColAccent : kColBorder, isGreen ? 2.0f : 1.0f);
        drawRoundRect(bluSw, 4.0f, D2D1::ColorF(0.0f, 0.28f, 0.72f, 1.0f), true, !isGreen ? kColAccent : kColBorder, !isGreen ? 2.0f : 1.0f);
        m_buttons.push_back({ BTN_CHROMA_COLOR, grnSw, 0x00B140 });
        m_buttons.push_back({ BTN_CHROMA_COLOR, bluSw, 0x0047AB });

        float hw = (fullW - 6.0f) * 0.5f;
        UiRect cpBtn{ qx, m_rcInspectorPanel.y + 158.0f, hw, 24.0f };
        UiRect psBtn{ qx + hw + 6.0f, m_rcInspectorPanel.y + 158.0f, hw, 24.0f };
        drawRoundRect(cpBtn, 5.0f, kColCard, true, kColBorder, 1.0f);
        drawText(tr(L"Copy FX", L"نسخ التأثيرات"), cpBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        drawRoundRect(psBtn, 5.0f, m_hasCopiedFx ? kColAccentDim : kColCard, true, kColBorder, 1.0f);
        drawText(tr(L"Paste FX", L"لصق التأثيرات"), psBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_COPY_FX, cpBtn, 0 });
        m_buttons.push_back({ BTN_PASTE_FX, psBtn, 0 });

        float tw = (fullW - 8.0f) / 3.0f;
        UiRect rmpBtn{ qx, m_rcInspectorPanel.y + 188.0f, tw, 23.0f };
        drawRoundRect(rmpBtn, 5.0f, c->speedRamp != SpeedRampType::None ? kColAccentDim : kColCard, true, kColBorder, 1.0f);
        drawText(tr(L"Ramp", L"تسريع"), rmpBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CYCLE_RAMP, rmpBtn, 0 });

        UiRect frmBtn{ qx + tw + 4.0f, m_rcInspectorPanel.y + 188.0f, tw, 23.0f };
        drawRoundRect(frmBtn, 5.0f, c->frame != FrameStyle::None ? kColGold : kColCard, true, kColBorder, 1.0f);
        drawText(tr(L"Frame", L"إطار"), frmBtn, m_fontSmall, c->frame != FrameStyle::None ? kColDarkText : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CYCLE_FRAME, frmBtn, 0 });

        UiRect bgBtn{ qx + (tw + 4.0f) * 2.0f, m_rcInspectorPanel.y + 188.0f, tw, 23.0f };
        drawRoundRect(bgBtn, 5.0f, c->bgBlur ? kColPurple : kColCard, true, kColBorder, 1.0f);
        drawText(tr(L"BG Blur", L"خلفية"), bgBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_TOGGLE_BGBLUR, bgBtn, 0 });

        // Batch 1: Stabilization + Optical Flow buttons
        float hw2 = (fullW - 6.0f) * 0.5f;
        UiRect stBtn{ qx, m_rcInspectorPanel.y + 216.0f, hw2, 23.0f };
        drawRoundRect(stBtn, 5.0f, c->stabilization != StabilizationLevel::Off ? kColAccentDim : kColCard, true, kColBorder, 1.0f);
        drawText(std::wstring(tr(L"Stab: ", L"تثبيت: ")) + utf8ToWide(stabLevelName(c->stabilization)), stBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CYCLE_STAB, stBtn, 0 });

        UiRect ofBtn{ qx + hw2 + 6.0f, m_rcInspectorPanel.y + 216.0f, hw2, 23.0f };
        drawRoundRect(ofBtn, 5.0f, c->opticalFlow ? kColAccentDim : kColCard, true, kColBorder, 1.0f);
        drawText(c->opticalFlow ? tr(L"OptFlow: ON", L"تدفق بصري: مفعل") : tr(L"Optical Flow", L"تدفق بصري"), ofBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_TOGGLE_OPTFLOW, ofBtn, 0 });
        float tw3 = (fullW - 8.0f) / 3.0f;
        UiRect jcBtn{ qx, m_rcInspectorPanel.y + 244.0f, tw3, 22.0f };
        UiRect lcBtn{ qx + tw3 + 4.0f, m_rcInspectorPanel.y + 244.0f, tw3, 22.0f };
        UiRect slBtn{ qx + (tw3 + 4.0f) * 2.0f, m_rcInspectorPanel.y + 244.0f, tw3, 22.0f };
        drawRoundRect(jcBtn, 4.0f, kColCard, true, kColAccentDim, 1.0f);
        drawText(L"J-Cut", jcBtn, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
        drawRoundRect(lcBtn, 4.0f, kColCard, true, kColAccentDim, 1.0f);
        drawText(L"L-Cut", lcBtn, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
        drawRoundRect(slBtn, 4.0f, kColCard, true, kColGold, 1.0f);
        drawText(tr(L"Slip +0.5s", L"Slip +"), slBtn, m_fontSmall, kColGold, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_NLE_JCUT, jcBtn, 0 });
        m_buttons.push_back({ BTN_NLE_LCUT, lcBtn, 0 });
        m_buttons.push_back({ BTN_NLE_SLIP, slBtn, 0 });

        UiRect cp2Btn{ qx, m_rcInspectorPanel.y + 270.0f, tw3, 22.0f };
        UiRect nsBtn{ qx + tw3 + 4.0f, m_rcInspectorPanel.y + 270.0f, tw3, 22.0f };
        UiRect mcBtn{ qx + (tw3 + 4.0f) * 2.0f, m_rcInspectorPanel.y + 270.0f, tw3, 22.0f };
        drawRoundRect(cp2Btn, 4.0f, kColCard, true, kColPurple, 1.0f);
        drawText(tr(L"Compound", L"مركب"), cp2Btn, m_fontSmall, kColPurple, DWRITE_TEXT_ALIGNMENT_CENTER);
        drawRoundRect(nsBtn, 4.0f, kColCard, true, kColPurple, 1.0f);
        drawText(tr(L"Nest", L"مشروع"), nsBtn, m_fontSmall, kColPurple, DWRITE_TEXT_ALIGNMENT_CENTER);
        drawRoundRect(mcBtn, 4.0f, kColCard, true, kColGreen, 1.0f);
        drawText(tr(L"MultiCam", L"كاميرات"), mcBtn, m_fontSmall, kColGreen, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_NLE_COMPOUND, cp2Btn, 0 });
        m_buttons.push_back({ BTN_NLE_NEST, nsBtn, 0 });
        m_buttons.push_back({ BTN_NLE_MULTICAM, mcBtn, 0 });

        UiRect blBtn{ qx, m_rcInspectorPanel.y + 296.0f, fullW, 22.0f };
        drawRoundRect(blBtn, 5.0f, c->blendMode != BlendMode::Normal ? kColAccentDim : kColCard, true, kColAccent, 1.0f);
        drawText(std::wstring(tr(L"Blend Mode: ", L"وضع الدمج: ")) + utf8ToWide(blendModeName(c->blendMode)),
                 blBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CYCLE_BLEND, blBtn, 0 });

        // V31: Real Object Track | Planar Track | Roto Keyframe
        UiRect otBtn{ qx, m_rcInspectorPanel.y + 322.0f, tw3, 22.0f };
        UiRect ptBtn{ qx + tw3 + 4.0f, m_rcInspectorPanel.y + 322.0f, tw3, 22.0f };
        UiRect rtBtn{ qx + (tw3 + 4.0f) * 2.0f, m_rcInspectorPanel.y + 322.0f, tw3, 22.0f };
        drawRoundRect(otBtn, 4.0f, kColCard, true, kColAccent, 1.0f);
        drawText(tr(L"ObjTrack", L"تتبع عنصر"), otBtn, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
        drawRoundRect(ptBtn, 4.0f, kColCard, true, kColGold, 1.0f);
        drawText(tr(L"Planar", L"تتبع سطح"), ptBtn, m_fontSmall, kColGold, DWRITE_TEXT_ALIGNMENT_CENTER);
        drawRoundRect(rtBtn, 4.0f, kColCard, true, kColPurple, 1.0f);
        drawText(tr(L"Roto KF", L"روتو إطار"), rtBtn, m_fontSmall, kColPurple, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_OBJ_TRACK, otBtn, 0 });
        m_buttons.push_back({ BTN_PLANAR_TRACK, ptBtn, 0 });
        m_buttons.push_back({ BTN_ROTO_KEYFRAME, rtBtn, 0 });

        // V31: Keyframe Easing Curve + Disk Proxy Generator
        float hwE = (fullW - 6.0f) * 0.5f;
        UiRect eaBtn{ qx, m_rcInspectorPanel.y + 348.0f, hwE, 22.0f };
        drawRoundRect(eaBtn, 4.0f, kColCard, true, kColGreen, 1.0f);
        drawText(std::wstring(tr(L"Ease: ", L"منحنى: ")) + utf8ToWide(keyframeEasingName(c->keyframeEasing)),
                 eaBtn, m_fontSmall, kColGreen, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CYCLE_EASING, eaBtn, 0 });

        UiRect pxBtn{ qx + hwE + 6.0f, m_rcInspectorPanel.y + 348.0f, hwE, 22.0f };
        drawRoundRect(pxBtn, 4.0f, !c->proxyPath.empty() ? kColGreen : kColCard, true, kColAccentDim, 1.0f);
        drawText(!c->proxyPath.empty() ? tr(L"Proxy ✓", L"بروكسي ✓") : tr(L"Gen Proxy", L"توليد بروكسي"),
                 pxBtn, m_fontSmall, !c->proxyPath.empty() ? kColDarkText : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_GEN_PROXIES, pxBtn, 0 });

        // V32: Real Scene Cut Detection + Optical Trajectory Stabilization
        UiRect sdBtn{ qx, m_rcInspectorPanel.y + 374.0f, hwE, 22.0f };
        drawRoundRect(sdBtn, 4.0f, kColCard, true, kColGold, 1.0f);
        drawText(tr(L"Scene Cut", L"كشف المشاهد"), sdBtn, m_fontSmall, kColGold, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_SCENE_DETECT, sdBtn, 0 });

        UiRect osBtn{ qx + hwE + 6.0f, m_rcInspectorPanel.y + 374.0f, hwE, 22.0f };
        drawRoundRect(osBtn, 4.0f, kColCard, true, kColAccent, 1.0f);
        drawText(tr(L"Opt. Stab", L"تثبيت بصري"), osBtn, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_OPTICAL_STAB, osBtn, 0 });

        UiRect c3Btn{ qx, m_rcInspectorPanel.y + 400.0f, fullW, 22.0f };
        drawRoundRect(c3Btn, 4.0f, kColCard, true, kColPurple, 1.0f);
        drawText(tr(L"3D Camera Track (6-DOF Parallax)", L"تتبع الكاميرا ثلاثي الأبعاد (3D Track)"), c3Btn, m_fontSmall, kColPurple, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_CAM3D_TRACK, c3Btn, 0 });
    }

    rebuildInspectorSliders();
    float clipTop = m_rcInspectorPanel.y + ((c->type == MediaType::Text) ? 270.0f : (m_inspTab == InspectorTab::FX ? 298.0f : 216.0f));
    for (const auto& s : m_sliders) {
        if (s.trackRect.y - 16.0f < clipTop) continue;
        if (s.trackRect.y + 12.0f > m_rcInspectorPanel.y + m_rcInspectorPanel.h - 4.0f) break;

        wchar_t valBuf[32];
        std::swprintf(valBuf, 32, L"%.2f%s", s.value, s.suffix.c_str());
        if (m_arabicUi) {
            drawText(s.label, { s.trackRect.x + s.trackRect.w * 0.36f, s.trackRect.y - 16.0f, s.trackRect.w * 0.64f, 16.0f },
                     m_fontSmall, kColTextMuted, DWRITE_TEXT_ALIGNMENT_LEADING);
            drawText(valBuf,  { s.trackRect.x, s.trackRect.y - 16.0f, s.trackRect.w * 0.35f, 16.0f },
                     m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_TRAILING);
        } else {
            drawText(s.label, { s.trackRect.x, s.trackRect.y - 16.0f, s.trackRect.w * 0.64f, 16.0f },
                     m_fontSmall, kColTextMuted, DWRITE_TEXT_ALIGNMENT_LEADING);
            drawText(valBuf,  { s.trackRect.x + s.trackRect.w * 0.65f, s.trackRect.y - 16.0f, s.trackRect.w * 0.35f, 16.0f },
                     m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_TRAILING);
        }

        float t = std::clamp((s.value - s.minVal) / std::max(0.0001f, s.maxVal - s.minVal), 0.0f, 1.0f);
        float barY = s.trackRect.y + s.trackRect.h * 0.5f - 2.0f;
        drawRoundRect({ s.trackRect.x, barY, s.trackRect.w, 4.0f }, 2.0f, kColCard);
        drawRoundRect({ s.trackRect.x, barY, s.trackRect.w * t, 4.0f }, 2.0f, kColAccent);

        float knobX = s.trackRect.x + s.trackRect.w * t;
        m_brush->SetColor(kColTextMain);
        m_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(knobX, barY + 2.0f), 5.0f, 5.0f), m_brush.Get());
    }
}

float MainWindow::timeToTimelineX(double sec) const {
    return m_rcTimelineRuler.x + static_cast<float>((sec - m_scrollSec) * m_pixelsPerSec);
}

double MainWindow::timelineXToTime(float x) const {
    double sec = m_scrollSec + static_cast<double>(x - m_rcTimelineRuler.x) / m_pixelsPerSec;
    return std::max(0.0, sec);
}

int MainWindow::timelineYToTrack(float y) const {
    int idx = static_cast<int>(std::floor((y - m_rcTimelineTracks.y - 6.0f + m_trackScrollY) / kTrackRowH));
    return std::clamp(idx, 0, static_cast<int>(m_engine.project().tracks.size()) - 1);
}

UiRect MainWindow::getClipRect(const Clip& c) const {
    float x1 = timeToTimelineX(c.timelineStart);
    float x2 = timeToTimelineX(c.timelineEnd());
    float y  = m_rcTimelineTracks.y + 6.0f + c.trackIndex * kTrackRowH + 4.0f - m_trackScrollY;
    return { x1, y, std::max(8.0f, x2 - x1), kTrackRowH - 8.0f };
}

void MainWindow::renderTimelinePanel() {
    m_brush->SetColor(kColPanel);
    m_rt->FillRectangle(m_rcTimelinePanel.d2d(), m_brush.Get());

    m_brush->SetColor(kColBorder);
    m_rt->DrawLine(D2D1::Point2F(0, m_rcTimelinePanel.y), D2D1::Point2F(m_width, m_rcTimelinePanel.y), m_brush.Get(), 1.0f);

    float bx = 12.0f;
    float by = m_rcTimelineToolbar.y + 5.0f;
    auto addTlBtn = [&](int id, const wchar_t* label, float w, bool active = false, bool danger = false) {
        UiRect r{ bx, by, w, 26.0f };
        bool hov = r.contains(m_hoverX, m_hoverY);
        D2D1_COLOR_F fill = active ? kColAccentDim : (hov ? kColCardHover : kColCard);
        drawRoundRect(r, 5.0f, fill, true, active ? kColAccent : kColBorder, 1.0f);
        drawText(label, r, m_fontSmall, danger ? kColDanger : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ id, r, 0 });
        bx += w + 6.0f;
    };

    addTlBtn(BTN_UNDO,          tr(L"Undo",          L"تراجع"),     52);
    addTlBtn(BTN_REDO,          tr(L"Redo",          L"إعادة"),     52);
    addTlBtn(BTN_SPLIT,         tr(L"Split",         L"تقطيع"),     50);
    addTlBtn(BTN_MERGE_NEXT,    tr(L"Merge",         L"دمج"),       46);
    addTlBtn(BTN_TRIM_LEFT,     tr(L"[ Trim",        L"[ قص قبل"),  64);
    addTlBtn(BTN_TRIM_RIGHT,    tr(L"Trim ]",        L"قص بعد ]"),  64);
    addTlBtn(BTN_FREEZE_FRAME,  tr(L"Freeze",        L"تجميد"),     62);
    addTlBtn(BTN_EXTRACT_AUDIO, tr(L"Extract Audio", L"فصل الصوت"), 88);
    addTlBtn(BTN_DUPLICATE,     tr(L"Duplicate",     L"تكرار"),     64);
    addTlBtn(BTN_DELETE,        tr(L"Delete",        L"حذف"),       46, false, true);
    addTlBtn(BTN_RIPPLE_DEL,    tr(L"Ripple",        L"حذف+سحب"),   62, false, true);
    addTlBtn(BTN_SNAP,          m_snapEnabled ? tr(L"Snap: ON", L"مغناطيس") : tr(L"Snap: OFF", L"حر"), 68, m_snapEnabled);
    addTlBtn(BTN_DETECT_BEATS,  tr(L"Beats", L"إيقاع"), 56);
    addTlBtn(BTN_ADD_TRACK_T,   L"+T", 34, false);
    addTlBtn(BTN_ADD_TRACK_V,   L"+V", 34, false);
    addTlBtn(BTN_ADD_TRACK_A,   L"+A", 34, false);

    UiRect zIn{ m_width - 42.0f, by, 30.0f, 26.0f };
    UiRect zOut{ m_width - 78.0f, by, 30.0f, 26.0f };
    drawRoundRect(zOut, 5.0f, zOut.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColBorder, 1.0f);
    drawText(L"-", zOut, m_fontBold, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
    drawRoundRect(zIn, 5.0f, zIn.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColBorder, 1.0f);
    drawText(L"+", zIn, m_fontBold, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_buttons.push_back({ BTN_ZOOM_OUT, zOut, 0 });
    m_buttons.push_back({ BTN_ZOOM_IN,  zIn,  0 });

    m_brush->SetColor(kColBg);
    m_rt->FillRectangle(m_rcTimelineRuler.d2d(), m_brush.Get());

    double stepSec = (m_pixelsPerSec < 10.0) ? 15.0 : ((m_pixelsPerSec < 20.0) ? 10.0 : ((m_pixelsPerSec < 40.0) ? 5.0 : (m_pixelsPerSec < 80.0 ? 2.0 : 1.0)));
    double firstTick = std::floor(m_scrollSec / stepSec) * stepSec;
    m_brush->SetColor(kColBorder);
    for (double t = firstTick; ; t += stepSec) {
        float tx = timeToTimelineX(t);
        if (tx > m_width) break;
        if (tx >= m_rcTimelineRuler.x) {
            m_rt->DrawLine(D2D1::Point2F(tx, m_rcTimelineRuler.y + 14),
                           D2D1::Point2F(tx, m_rcTimelineRuler.y + m_rcTimelineRuler.h), m_brush.Get(), 1.0f);
            wchar_t tbuf[16];
            int totalS = static_cast<int>(t);
            std::swprintf(tbuf, 16, L"%02d:%02d", totalS / 60, totalS % 60);
            drawText(tbuf, { tx + 3, m_rcTimelineRuler.y, 44, 16 }, m_fontSmall, kColTextMuted);
        }
    }

    m_rt->PushAxisAlignedClip(m_rcTimelineTracks.d2d(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    const auto& tracks = m_engine.project().tracks;
    for (size_t i = 0; i < tracks.size(); ++i) {
        float ty = m_rcTimelineTracks.y + 6.0f + i * kTrackRowH - m_trackScrollY;
        if (ty + kTrackRowH < m_rcTimelineTracks.y || ty > m_rcTimelineTracks.y + m_rcTimelineTracks.h) continue;

        UiRect rowBg{ kTrackHeaderW, ty, m_width - kTrackHeaderW, kTrackRowH - 2.0f };
        m_brush->SetColor((i % 2 == 0) ? kColBg : (m_lightMode ? D2D1::ColorF(0.955f, 0.965f, 0.982f, 1.0f) : D2D1::ColorF(0.095f, 0.095f, 0.108f, 1.0f)));
        m_rt->FillRectangle(rowBg.d2d(), m_brush.Get());

        UiRect hdr{ 4, ty + 2, kTrackHeaderW - 8, kTrackRowH - 6 };
        drawRoundRect(hdr, 5.0f, kColCard);
        drawText(utf8ToWide(tracks[i].label), { hdr.x + 6, hdr.y, 26, hdr.h }, m_fontBold, kColTextMain);

        UiRect tog{ hdr.x + 34, hdr.y + 9, 24, 22 };
        bool isAudio = (tracks[i].type == TrackType::Audio);
        bool off = isAudio ? tracks[i].muted : tracks[i].hidden;
        drawRoundRect(tog, 4.0f, off ? kColDanger : kColBg);
        drawText(isAudio ? L"M" : L"V", tog, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ isAudio ? BTN_TRACK_MUTE : BTN_TRACK_HIDE, tog, static_cast<uint64_t>(i) });
    }
    m_rt->PopAxisAlignedClip();

    UiRect clipArea{ kTrackHeaderW, m_rcTimelineTracks.y, m_width - kTrackHeaderW, m_rcTimelineTracks.h };
    m_rt->PushAxisAlignedClip(clipArea.d2d(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    for (const auto& c : m_engine.project().clips) {
        UiRect cr = getClipRect(c);
        if (cr.x + cr.w < kTrackHeaderW || cr.x > m_width) continue;

        D2D1_COLOR_F col = kColClipVideo;
        if (c.type == MediaType::Image) col = kColClipImage;
        else if (c.type == MediaType::Text) col = kColClipText;
        else if (c.type == MediaType::Audio) col = kColClipAudio;

        bool selected = (c.id == m_engine.selectedClipId());
        drawRoundRect(cr, 5.0f, col, true, selected ? kColAccent : D2D1::ColorF(1, 1, 1, 0.16f), selected ? 2.2f : 1.0f);

        if ((c.type == MediaType::Audio || c.type == MediaType::Video) && !c.sourcePath.empty() && cr.w > 24.0f) {
            auto peaks = m_audio.getWaveformPeaks(c.sourcePath);
            if (!peaks.empty()) {
                m_brush->SetColor(c.type == MediaType::Audio
                    ? D2D1::ColorF(0.0f, 0.92f, 0.78f, 0.55f)
                    : D2D1::ColorF(0.75f, 0.92f, 1.0f, 0.28f));
                float waveBase = cr.y + cr.h - 4.0f;
                float waveMaxH = (c.type == MediaType::Audio) ? (cr.h - 14.0f) : (cr.h * 0.45f);
                int numBars = std::min(static_cast<int>(peaks.size()), std::max(8, static_cast<int>(cr.w / 3.0f)));
                float stepX = (cr.w - 10.0f) / numBars;
                for (int b = 0; b < numBars; ++b) {
                    size_t pIdx = static_cast<size_t>(b) * peaks.size() / numBars;
                    float bh = std::max(2.0f, peaks[pIdx] * waveMaxH);
                    float bxBar = cr.x + 5.0f + b * stepX;
                    m_rt->DrawLine(D2D1::Point2F(bxBar, waveBase), D2D1::Point2F(bxBar, waveBase - bh), m_brush.Get(), 1.5f);
                }
            }
        }

        float badgeX = cr.x + 5.0f;
        if (c.transitionIn != TransitionType::None && cr.w > 30.0f) {
            UiRect trBadge{ badgeX, cr.y + 4, 22, 14 };
            drawRoundRect(trBadge, 3.0f, kColAccent);
            drawText(L"TR", trBadge, m_fontSmall, kColDarkText, DWRITE_TEXT_ALIGNMENT_CENTER);
            badgeX += 25.0f;
        }
        if (c.chromaKey.enabled && cr.w > 54.0f) {
            UiRect ckBadge{ badgeX, cr.y + 4, 22, 14 };
            drawRoundRect(ckBadge, 3.0f, kColGreen);
            drawText(L"CK", ckBadge, m_fontSmall, kColDarkText, DWRITE_TEXT_ALIGNMENT_CENTER);
            badgeX += 25.0f;
        }
        if (c.effect.type != EffectType::None && cr.w > 78.0f) {
            UiRect fxBadge{ badgeX, cr.y + 4, 22, 14 };
            drawRoundRect(fxBadge, 3.0f, kColPurple);
            drawText(L"FX", fxBadge, m_fontSmall, kColDarkText, DWRITE_TEXT_ALIGNMENT_CENTER);
            badgeX += 25.0f;
        }

        if (!c.keyframes.empty()) {
            m_brush->SetColor(kColGold);
            for (const auto& kf : c.keyframes) {
                float kx = timeToTimelineX(c.timelineStart + kf.localTime);
                float ky = cr.y + cr.h - 9.0f;
                if (kx >= cr.x + 4 && kx <= cr.x + cr.w - 4) {
                    m_rt->FillRectangle(D2D1::RectF(kx - 3.5f, ky - 3.5f, kx + 3.5f, ky + 3.5f), m_brush.Get());
                }
            }
        }

        if (selected && cr.w > 22.0f) {
            m_brush->SetColor(kColAccent);
            m_rt->FillRectangle(D2D1::RectF(cr.x + 1, cr.y + 4, cr.x + 5, cr.y + cr.h - 4), m_brush.Get());
            m_rt->FillRectangle(D2D1::RectF(cr.x + cr.w - 5, cr.y + 4, cr.x + cr.w - 1, cr.y + cr.h - 4), m_brush.Get());
        }
        if (cr.w > 34.0f) {
            drawText(utf8ToWide(c.name), { badgeX + 3.0f, cr.y + 1, std::max(10.0f, cr.x + cr.w - badgeX - 10.0f), 18 },
                     m_fontSmall, kColTextMain);
        }
    }

    float px = timeToTimelineX(m_engine.playhead());
    if (px >= kTrackHeaderW && px <= m_width) {
        m_brush->SetColor(kColAccent);
        m_rt->DrawLine(D2D1::Point2F(px, m_rcTimelineTracks.y),
                       D2D1::Point2F(px, m_rcTimelinePanel.y + m_rcTimelinePanel.h), m_brush.Get(), 2.0f);
    }

    m_rt->PopAxisAlignedClip();

    if (!m_beatMarkers.empty()) {
        m_brush->SetColor(kColGold);
        for (double bt : m_beatMarkers) {
            float btx = timeToTimelineX(bt);
            if (btx >= kTrackHeaderW && btx <= m_width) {
                m_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(btx, m_rcTimelineRuler.y + 20.0f), 3.0f, 3.0f), m_brush.Get());
            }
        }
    }
    if (px >= kTrackHeaderW && px <= m_width) {
        drawRoundRect({ px - 6.0f, m_rcTimelineRuler.y + 2.0f, 12.0f, 16.0f }, 3.0f, kColAccent);
    }

    // Draggable Horizontal Timeline Scrollbar (supports 00:00 .. 03:00+ navigation!)
    double totalSec = timelineTotalRangeSec();
    double visibleSec = std::max(1.0, static_cast<double>(m_rcTimelineRuler.w) / m_pixelsPerSec);
    double maxScroll = std::max(0.0, totalSec - visibleSec);
    m_scrollSec = std::clamp(m_scrollSec, 0.0, maxScroll);

    drawRoundRect(m_rcTimelineScrollH, 5.0f, kColBg, true, kColBorder, 1.0f);
    float thumbRatio = static_cast<float>(std::clamp(visibleSec / totalSec, 0.06, 1.0));
    float thumbW = std::max(36.0f, m_rcTimelineScrollH.w * thumbRatio);
    float scrollFrac = (maxScroll > 0.001) ? static_cast<float>(m_scrollSec / maxScroll) : 0.0f;
    float thumbX = m_rcTimelineScrollH.x + scrollFrac * (m_rcTimelineScrollH.w - thumbW);
    UiRect thumbRc{ thumbX, m_rcTimelineScrollH.y + 1.0f, thumbW, m_rcTimelineScrollH.h - 2.0f };
    drawRoundRect(thumbRc, 4.0f, (m_dragMode == DragMode::ScrollTimelineH || thumbRc.contains(m_hoverX, m_hoverY)) ? kColAccent : kColAccentDim);
}

void MainWindow::renderAllToolsModal() {
    m_brush->SetColor(D2D1::ColorF(0, 0, 0, 0.82f));
    m_rt->FillRectangle(D2D1::RectF(0, 0, m_width, m_height), m_brush.Get());

    float mw = std::min(1140.0f, m_width - 48.0f);
    float mh = std::min(670.0f, m_height - 56.0f);
    UiRect modal{ (m_width - mw) * 0.5f, (m_height - mh) * 0.5f, mw, mh };
    drawRoundRect(modal, 14.0f, kColPanel, true, kColAccent, 1.5f);

    drawText(tr(L"NovaCut Studio — All 160+ Pro Tools (Scroll or click Page 1/2)",
                L"مركز جميع الأدوات الاحترافية — 160+ أداة (مرّر بالماوس أو بدّل بين الصفحتين)"),
             { modal.x + 18, modal.y + 12, mw - 275, 28 }, m_fontTitle, kColAccent);

    UiRect p1Btn{ modal.x + mw - 246, modal.y + 12, 74, 26 };
    UiRect p2Btn{ modal.x + mw - 166, modal.y + 12, 74, 26 };
    UiRect closeBtn{ modal.x + mw - 86, modal.y + 12, 70, 26 };
    drawRoundRect(p1Btn, 6.0f, m_allToolsPage == 0 ? kColAccent : kColCard, true, kColAccent, 1.0f);
    drawText(tr(L"Page 1", L"صفحة 1"), p1Btn, m_fontSmall, m_allToolsPage == 0 ? kColDarkText : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
    drawRoundRect(p2Btn, 6.0f, m_allToolsPage == 1 ? kColAccent : kColCard, true, kColAccent, 1.0f);
    drawText(tr(L"Page 2", L"صفحة 2"), p2Btn, m_fontSmall, m_allToolsPage == 1 ? kColDarkText : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
    drawRoundRect(closeBtn, 6.0f, kColDanger);
    drawText(tr(L"Close", L"إغلاق"), closeBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_buttons.push_back({ BTN_TOOLS_PAGE, p1Btn, 0 });
    m_buttons.push_back({ BTN_TOOLS_PAGE, p2Btn, 1 });
    m_buttons.push_back({ BTN_ALL_TOOLS, closeBtn, 0 });

    float qy = modal.y + 46.0f;
    float qw = (mw - 36.0f - 7.0f * 6.0f) / 8.0f;
    auto addStripBtn = [&](int idx, int id, const wchar_t* lbl, D2D1_COLOR_F bg, D2D1_COLOR_F fg, D2D1_COLOR_F brd) {
        UiRect r{ modal.x + 18.0f + idx * (qw + 6.0f), qy, qw, 26.0f };
        bool hov = r.contains(m_hoverX, m_hoverY);
        drawRoundRect(r, 6.0f, hov ? kColCardHover : bg, true, hov ? kColAccent : brd, 1.0f);
        drawText(lbl, r, m_fontSmall, fg, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ id, r, 0 });
    };
    addStripBtn(0, BTN_RESTORE_VERSION, tr(L"Restore Ver.", L"استعادة نسخة"), kColCard, kColTextMain, kColBorder);
    addStripBtn(1, BTN_SAVE_VERSION,   tr(L"Save Version",  L"حفظ نسخة"),      kColCard, kColGold,     kColGold);
    addStripBtn(2, BTN_CUSTOM_PRESET,  m_hasCustomPreset ? tr(L"Apply Preset", L"تطبيق القالب") : tr(L"Save Preset", L"حفظ قالب"), kColCard, kColPurple, kColPurple);
    addStripBtn(3, BTN_SEARCH_ASSETS,  tr(L"Search FX",     L"بحث مؤثرات"),    kColCard, kColAccent,   kColBorder);
    addStripBtn(4, BTN_TOGGLE_FAV,     tr(L"★ Favorite",    L"★ المفضلة"),     kColCard, kColGold,     kColBorder);
    addStripBtn(5, BTN_CLOUD_SYNC,     tr(L"Cloud Sync",    L"مزامنة سحابية"), kColCard, kColTextMain, kColBorder);
    addStripBtn(6, BTN_ADD_MARKER,     tr(L"Add Marker",    L"إضافة علامة"),   kColCard, kColGold,     kColBorder);
    addStripBtn(7, BTN_TOGGLE_OFFLINE, m_offlineMode ? tr(L"Offline: ON", L"بدون إنترنت: مفعل") : tr(L"Online AI", L"الوضع السحابي"),
                m_offlineMode ? kColGreen : kColCard, m_offlineMode ? kColDarkText : kColTextMain, kColGreen);

    struct CatCol { const wchar_t* title; int jumpId; const wchar_t* en[12]; const wchar_t* ar[12]; };
    CatCol colsP1[4] = {
        { tr(L"Cinema NLE & Timeline", L"المونتاج السينمائي والخط الزمني"), 1,
          { L"J-Cut / L-Cut Audio Overlap", L"Slip Edit / Slide Edit", L"Compound Clips / Nested Project",
            L"Multi-Camera Switch", L"Ripple Delete & Split", L"Proxy Mode (Fast / Proxy / HQ)",
            L"Time Remapping & Speed Ramp", L"Optical Flow & Stabilization", L"Auto Reframe & Motion Track",
            L"Lens Distortion Correction", L"Rolling Shutter Correction", L"Scene & Beat Detection" },
          { L"تداخل الصوت قبل وبعد (J-Cut / L-Cut)", L"تعديل محتوى المقطع (Slip / Slide)", L"دمج المقاطع والمشاريع المتداخلة",
            L"التبديل بين عدة كاميرات (MultiCam)", L"قص المقطع وحذف الفراغ تلقائياً", L"وضع الأداء السريع (Proxy Mode)",
            L"التحكم المتقدم بالسرعة (Speed Ramp)", L"تثبيت الكاميرا والتدفق البصري", L"إعادة التأطير وتتبع الحركة",
            L"تصحيح تشوه العدسة (Lens Correct)", L"إصلاح تشوه الحركة السريعة", L"اكتشاف المشاهد والنبضات تلقائياً" }
        },
        { tr(L"Pro Color & Live Scopes", L"التلوين السينمائي والمراقبات الحية"), 0,
          { L"Auto White Balance", L"Temperature, Tint & Exposure", L"Highlights, Shadows, Whites & Blacks",
            L"Saturation, Vibrance & Hue", L"RGB Curves (R/G/B + Presets)", L"3-Way Color Wheels",
            L"HSL Secondary Color Qualifier", L"Split Toning (Shadows & Highlights)", L"3D LUT Import & Export (.cube)",
            L"Live Histogram & Waveform", L"Live Vectorscope (Cb/Cr)", L"False Color Exposure Overlay" },
          { L"توازن اللون الأبيض التلقائي", L"حرارة اللون والصبغة والتعريض", L"الإضاءات والظلال والأبيض والأسود",
            L"التشبع وحيوية ودرجة الألوان", L"منحنيات الألوان (RGB Curves)", L"عجلات الألوان الثلاثية",
            L"تعديل لون محدد (HSL Secondary)", L"تلوين الظلال والإضاءات الثنائي", L"استيراد وتصدير فلاتر LUT (.cube)",
            L"مراقب الإضاءة والهستوغرام الحي", L"مراقب الألوان الحي (Vectorscope)", L"كشف مستويات التعريض (False Color)" }
        },
        { tr(L"VFX, Compositing & 3D", L"المؤثرات البصرية والدمج وثلاثي الأبعاد"), 6,
          { L"Blend Modes (Screen / Add / Overlay)", L"Green Screen Chroma Key", L"Rotoscoping & Auto BG Remove",
            L"Light Leaks & Lens Flare", L"Soft Glow & Neon Effect", L"Particle Generator & Sparkles",
            L"Smoke & Fire Effects", L"Rain & Snow Effects", L"Liquid Wave & Displacement Map",
            L"Fractal Noise & Chromatic Ab.", L"Depth Map Estimator", L"3D Objects & 3D Extruded Text" },
          { L"أوضاع دمج الطبقات (Blend Modes)", L"إزالة الخلفية الخضراء (الكروما)", L"فصل الأشخاص وإزالة الخلفية",
            L"تسربات الضوء وتوهج العدسات", L"التوهج الناعم وتأثير النيون", L"توليد الجسيمات والشرارات",
            L"مؤثرات الدخان والنار الإجرائية", L"مؤثرات المطر وتساقط الثلج", L"تأثير السوائل وتشويه الصورة",
            L"الضوضاء الإجرائية والانحراف اللوني", L"خريطة العمق البصرية (Depth Map)", L"العناصر والنصوص ثلاثية الأبعاد" }
        },
        { tr(L"Pro Audio DAW & DSP", L"الهندسة الصوتية والمكساج الاحترافي"), 2,
          { L"Audio Ducking & Voice Isolation", L"Dynamic Compressor & Limiter", L"3-Band Equalizer (Bass/Mid/Treble)",
            L"Room Reverb (Multi-Tap)", L"Echo & Noise Reduction", L"De-Esser (Sibilance Control)",
            L"Pitch Shift & Time Stretch", L"Stereo Width & Pan Control", L"Live 16-Band Audio Spectrum",
            L"Auto Silence Removal", L"1-Click Audio Restoration", L"Multi-Track Mixing & Voiceover" },
          { L"خفض الموسيقى وعزل الصوت البشري", L"ضغط الصوت ومحدد منع التشوه", L"معادل الترددات ثلاثي النطاقات",
            L"الصدى المكاني للغرف (Reverb)", L"التكرار الصوتي وإزالة الضوضاء", L"تخفيف حروف الصفير (De-Esser)",
            L"تغيير طبقة الصوت ومدته", L"توسيع الاستيريو وتوزيع اليمين واليسار", L"عرض طيف الصوت الحي (16 نطاق)",
            L"حذف الصمت تلقائياً من المقطع", L"ترميم وتحسين الصوت بضغطة واحدة", L"مزج عدة مسارات وتسجيل صوتي" }
        }
    };
    CatCol colsP2[4] = {
        { tr(L"Photo & Retouch Studio", L"استوديو الصور وفرش التعديل"), 10,
          { L"Healing Brush (Canvas Paint)", L"Clone Stamp Brush", L"Content-Aware Fill Brush",
            L"Liquify Warp Brush", L"Dodge & Burn Brushes", L"Blur & Sharpen Brushes",
            L"Selection Brush & Magnetic Lasso", L"Clipping Mask & Layer Masks", L"Photo Noise Reduction",
            L"Gradient Tool & Perspective", L"RAW Photo Import (DNG/CR2/ARW)", L"Batch Edit & Batch PNG Export" },
          { L"فرشاة إزالة العيوب (Healing)", L"فرشاة الاستنساخ (Clone Stamp)", L"الملء الذكي لإزالة العناصر",
            L"فرشاة التشكيل (Liquify)", L"فرشاة التفتيح والتغميق (Dodge/Burn)", L"فرشاة الضبابية والحدة الموضعية",
            L"فرشاة التحديد والتحديد المغناطيسي", L"قناع القص وأقنعة الطبقات", L"تقليل التشويش في الصور",
            L"أداة التدرجات وتصحيح المنظور", L"تحرير الصور الخام (RAW / DNG)", L"التعديل والتصدير الجماعي للصور" }
        },
        { tr(L"Text, Captions & Stickers", L"النصوص والترجمة والملصقات"), 9,
          { L"Rich Text & Curved Text", L"3D Extruded & Neon Text", L"Text Stroke, Shadow & Tracking",
            L"Text Animations (Typewriter/Pop)", L"Auto & Manual Captions", L"SRT Subtitle Import",
            L"Text-to-Speech (SAPI Voice)", L"12 Animated Stickers", L"25 Cinema Filters (9 Categories)",
            L"19 Video Transitions", L"Freehand Canvas Drawing", L"Studio Backgrounds & Frames" },
          { L"كتابة النصوص والنص المنحني", L"النص ثلاثي الأبعاد والنيون", L"حدود النص والظل وتباعد الحروف",
            L"تحريك النصوص (آلة كاتبة / نبض)", L"الترجمة التلقائية واليدوية", L"استيراد ملفات الترجمة (SRT)",
            L"تحويل النص إلى صوت بشري", L"مكتبة الملصقات والشارات المتحركة", L"25 فلتر لوني في 9 تصنيفات",
            L"19 انتقال فيديو سينمائي", L"الرسم الحر المباشر على الشاشة", L"خلفيات وإطارات الاستوديو" }
        },
        { tr(L"Broadcast Export & Queue", L"التصدير الاحترافي وقائمة الرندر"), 11,
          { L"H.264 & H.265 / HEVC GPU", L"AV1 / ProRes / VP9 Profiles", L"Animated GIF Export (Native)",
            L"PNG Image Sequence Export", L"Alpha Transparent PNG Export", L"480P / 720P / 1080P / 2K / 4K",
            L"24 / 30 / 60 FPS (CFR / VFR)", L"Custom Bitrate (3M to 64M)", L"Audio 48kHz/44.1kHz (16/24-bit)",
            L"Color Space (Rec709 / 2020 / P3)", L"HDR10 & Smart HDR Export", L"Render Queue & Batch Render" },
          { L"ترميز الفيديو السريع H.264 و H.265", L"إعدادات AV1 و ProRes و VP9", L"تصدير صور متحركة (GIF) مباشرة",
            L"تصدير إطارات متسلسلة (PNG Seq)", L"تصدير بخلفية شفافة (Alpha PNG)", L"دقة التصدير من 480P حتى 4K UHD",
            L"معدل الإطارات الثابت والمتغير", L"تحديد معدل البت حتى 64 ميجابت", L"معدل عينات الصوت وعمق البت",
            L"مساحة الألوان (Rec.709 / 2020 / P3)", L"تصدير النطاق العالي (HDR / Smart)", L"قائمة مهام التصدير والتصيير الجماعي" }
        },
        { tr(L"Studio Workflow & AI", L"سير العمل والأداء والذكاء الاصطناعي"), 12,
          { L"Auto-Save Every 45 Seconds", L"1-Click Project Draft Recovery", L"Version History Snapshots",
            L"Custom Color/FX Presets", L"Offline Mode (100% Local)", L"Optional Cloud Sync Backup",
            L"Search FX (Ctrl+F) & Favorites", L"Timeline Markers", L"Touch Pinch Zoom & Drag-Drop",
            L"Before / After Live Compare", L"AI Cloud Tools (Online Opt-In)", L"Keyboard Shortcuts Guide (F1)" },
          { L"حفظ تلقائي للمشروع كل 45 ثانية", L"استعادة المسودة بضغطة واحدة", L"سجل التعديلات والنسخ المتسلسلة",
            L"حفظ وتطبيق الإعدادات المخصصة", L"العمل الكامل دون إنترنت (أوفلاين)", L"نسخ احتياطي ومزامنة سحابية",
            L"البحث في المؤثرات والمفضلة", L"علامات التايملاين للمشاهد والنبضات", L"التكبير باللمس والسحب والإفلات",
            L"مقارنة قبل وبعد التعديل مباشرة", L"أدوات الذكاء الاصطناعي عبر الإنترنت", L"دليل اختصارات لوحة المفاتيح (F1)" }
        }
    };

    const CatCol* cols = (m_allToolsPage == 0) ? colsP1 : colsP2;
    float colW = (mw - 36.0f - 30.0f) / 4.0f;
    float startY = modal.y + 82.0f;
    float colH = mh - 98.0f;
    for (int ci = 0; ci < 4; ++ci) {
        // Mirror column order naturally in Arabic RTL (1 on right -> 4 on left)
        int drawIdx = m_arabicUi ? (3 - ci) : ci;
        UiRect cr{ modal.x + 18.0f + drawIdx * (colW + 10.0f), startY, colW, colH };
        drawRoundRect(cr, 9.0f, kColBg, true, kColBorder, 1.0f);
        UiRect hdr{ cr.x + 6, cr.y + 6, cr.w - 12, 28 };
        drawRoundRect(hdr, 6.0f, kColCard, true, kColAccentDim, 1.0f);
        drawText(cols[ci].title, hdr, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_TOOL_JUMP, hdr, static_cast<uint64_t>(cols[ci].jumpId) });

        float iy = cr.y + 40.0f;
        float rowH = std::max(27.0f, (colH - 48.0f) / 12.0f);
        for (int li = 0; li < 12; ++li) {
            UiRect ir{ cr.x + 8, iy + li * rowH, cr.w - 16, rowH - 4.5f };
            bool hov = ir.contains(m_hoverX, m_hoverY);
            drawRoundRect(ir, 5.0f, hov ? kColCardHover : kColCard, true, hov ? kColAccent : kColBorder, hov ? 1.2f : 0.7f);
            // Subtle accent bar on hover for crisp tactile feedback
            if (hov) {
                UiRect bar{ m_arabicUi ? (ir.x + ir.w - 3.5f) : ir.x, ir.y + 3.0f, 3.5f, ir.h - 6.0f };
                drawRoundRect(bar, 1.5f, kColAccent);
            }
            const wchar_t* label = m_arabicUi ? cols[ci].ar[li] : cols[ci].en[li];
            drawText(label, { ir.x + 8, ir.y, ir.w - 16, ir.h }, m_fontSmall, hov ? kColAccent : kColTextMain, DWRITE_TEXT_ALIGNMENT_LEADING);
            int code = (m_allToolsPage == 0 ? 100 : 300) + ci * 20 + li;
            m_buttons.push_back({ BTN_TOOL_JUMP, ir, static_cast<uint64_t>(code) });
        }
    }
}

void MainWindow::renderAssetHubModal() {
    m_brush->SetColor(D2D1::ColorF(0, 0, 0, 0.80f));
    m_rt->FillRectangle(D2D1::RectF(0, 0, m_width, m_height), m_brush.Get());

    float mw = std::min(1140.0f, m_width - 48.0f);
    float mh = std::min(660.0f, m_height - 52.0f);
    UiRect modal{ (m_width - mw) * 0.5f, (m_height - mh) * 0.5f, mw, mh };
    drawRoundRect(modal, 14.0f, kColPanel, true, kColAccent, 1.5f);

    drawText(tr(L"Built-In Asset Library — 1-Click Direct Insert",
                L"مكتبة الإدراج الداخلية — اضغط على أي عنصر للإدراج الفوري"),
             { modal.x + 18, modal.y + 12, mw - 110, 28 }, m_fontTitle, kColAccent);

    UiRect closeBtn{ modal.x + mw - 88, modal.y + 12, 70, 26 };
    drawRoundRect(closeBtn, 6.0f, kColDanger);
    drawText(tr(L"Close", L"إغلاق"), closeBtn, m_fontSmall, D2D1::ColorF(1,1,1,1), DWRITE_TEXT_ALIGNMENT_CENTER);
    m_buttons.push_back({ BTN_OPEN_ASSETHUB, closeBtn, 0 });

    // 3 Unified Main Tabs
    float ty = modal.y + 46.0f;
    float tw = (mw - 36.0f - 16.0f) / 3.0f;
    const wchar_t* tabLblEn[3] = { L"Stickers & Badges (1040)", L"Sound Effects & Loops (120)", L"Cinema Color Filters (60)" };
    const wchar_t* tabLblAr[3] = { L"الملصقات والستكرات (1040)", L"الأصوات والمؤثرات (120)", L"الفلاتر السينمائية (60)" };
    for (int t = 0; t < 3; ++t) {
        int di = m_arabicUi ? (2 - t) : t;
        UiRect trc{ modal.x + 18.0f + di * (tw + 8.0f), ty, tw, 30.0f };
        bool act = (m_hubTab == t);
        drawRoundRect(trc, 7.0f, act ? kColAccent : kColCard, true, act ? kColAccent : kColBorder, 1.0f);
        drawText(tr(tabLblEn[t], tabLblAr[t]), trc, m_fontBold, act ? kColDarkText : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_HUB_TAB, trc, static_cast<uint64_t>(t) });
    }

    // Category Filter Pills
    float cy = ty + 38.0f;
    const wchar_t* stCatsAr[11] = { L"الكل", L"يوتيوب", L"عربي ترند", L"إيموجي", L"ألعاب", L"سينما", L"نيون", L"عروض", L"أسهم", L"طبيعة", L"موسيقى" };
    const wchar_t* stCatsEn[11] = { L"All", L"Social", L"Arabic", L"Emoji", L"Gaming", L"Cinema", L"Neon", L"Promo", L"Arrows", L"Nature", L"Music" };
    const wchar_t* auCatsAr[6]  = { L"الكل", L"ووش وانتقالات", L"يوتيوب ونقرات", L"سينما وبوم", L"ألعاب ونجاح", L"موسيقى ولوبات" };
    const wchar_t* auCatsEn[6]  = { L"All", L"Whoosh", L"UI & Pop", L"Cinema", L"Gaming", L"Music Loops" };
    const wchar_t* flCatsAr[5]  = { L"الكل", L"سينمائي", L"ريترو وفيلم", L"فلوق وحياة", L"ليلي وداكن" };
    const wchar_t* flCatsEn[5]  = { L"All", L"Cinema", L"Retro", L"Vlog", L"Moody" };

    int nCats = (m_hubTab == 0) ? 11 : ((m_hubTab == 1) ? 6 : 5);
    float cw = (mw - 36.0f - (nCats - 1) * 5.0f) / static_cast<float>(nCats);
    for (int ci = 0; ci < nCats; ++ci) {
        int di = m_arabicUi ? (nCats - 1 - ci) : ci;
        UiRect pr{ modal.x + 18.0f + di * (cw + 5.0f), cy, cw, 24.0f };
        bool act = (m_hubCat == ci);
        drawRoundRect(pr, 12.0f, act ? kColAccentDim : kColCard, true, act ? kColAccent : kColBorder, 1.0f);
        const wchar_t* lbl = (m_hubTab == 0) ? tr(stCatsEn[ci], stCatsAr[ci])
                           : ((m_hubTab == 1) ? tr(auCatsEn[ci], auCatsAr[ci]) : tr(flCatsEn[ci], flCatsAr[ci]));
        drawText(lbl, pr, m_fontSmall, act ? kColDarkText : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_HUB_CAT, pr, static_cast<uint64_t>(ci) });
    }

    // Build filtered item indices
    std::vector<int> items;
    if (m_hubTab == 0) {
        for (int i = 0; i < 1040; ++i) {
            if (m_hubCat == 0 || (i / 104) + 1 == m_hubCat) items.push_back(i);
        }
    } else if (m_hubTab == 1) {
        for (int i = 0; i < 120; ++i) {
            int cIdx = (i < 24) ? 1 : ((i < 48) ? 2 : ((i < 72) ? 3 : ((i < 96) ? 4 : 5)));
            if (m_hubCat == 0 || cIdx == m_hubCat) items.push_back(i);
        }
    } else {
        for (int i = 0; i < 60; ++i) {
            int cIdx = (i % 4) + 1;
            if (m_hubCat == 0 || cIdx == m_hubCat) items.push_back(i);
        }
    }

    const int cols = (m_hubTab == 0) ? 6 : 4;
    const int rows = 4;
    const int perPage = cols * rows;
    int totalPages = std::max(1, static_cast<int>((items.size() + perPage - 1) / perPage));
    if (m_hubPage >= totalPages) m_hubPage = totalPages - 1;

    float gx = modal.x + 18.0f;
    float gy = cy + 32.0f;
    float gw = mw - 36.0f;
    float gh = mh - (gy - modal.y) - 48.0f;
    float gap = 8.0f;
    float cardW = (gw - (cols - 1) * gap) / static_cast<float>(cols);
    float cardH = (gh - (rows - 1) * gap) / static_cast<float>(rows);

    static const wchar_t* kStW[10][13] = {
        {L"SUBSCRIBE", L"LIKE", L"SHARE", L"FOLLOW +", L"LIVE ●", L"NEW!", L"TRENDING", L"LINK BIO", L"BELL", L"COMMENT", L"TIKTOK", L"REELS HD", L"SHORTS"},
        {L"حصري", L"مفاجأة", L"لا يفوتك!", L"شاهد للنهاية", L"قصة حقيقية", L"تجربة", L"نصيحة ذهبية", L"مقارنة", L"الأفضل 2026", L"مراجعة", L"هل تعلم؟", L"الجزء الأول", L"تحدي جديد"},
        {L"WOW!", L"LOL", L"FIRE", L"LOVE", L"COOL", L"100%", L"CLAP", L"STAR", L"OMG", L"GENIUS", L"PARTY", L"OK", L"CROWN"},
        {L"GG EZ", L"VICTORY", L"HEADSHOT", L"LEVEL UP", L"BOSS FIGHT", L"1HP CLUTCH", L"PRO PLAYER", L"GAME OVER", L"COMBO x10", L"STREAM", L"AFK", L"NOOB", L"ULTRA KILL"},
        {L"REC ● 00:01", L"4K UHD", L"60 FPS", L"CINEMA", L"VLOG DAY 1", L"THE END", L"ACTION!", L"BTS", L"HDR10+", L"TRAVEL", L"FLASHBACK", L"SLOW MO", L"INTRO ▶"},
        {L"CYBER", L"GLOW BOX", L"SPARKLE", L"NEON RING", L"HEXAGON", L"DIAMOND", L"LIGHTNING", L"STARBURST", L"HEART", L"WAVE", L"TARGET", L"CROWN", L"PORTAL"},
        {L"50% OFF", L"FREE!", L"HOT DEAL", L"LIMITED", L"BUY NOW", L"VIP PRO", L"BEST SELLER", L"NEW IN", L"FLASH SALE", L"100% REAL", L"SAVE BIG", L"BONUS +", L"TOP PICK"},
        {L"ARROW ➔", L"LEFT ⬅", L"UP ⬆", L"DOWN ⬇", L"CURVE ↪", L"LOOK HERE", L"CHECK ✔", L"CROSS ✖", L"ALERT ⚠", L"PIN", L"ZOOM", L"STOP", L"NEXT ⏭"},
        {L"SUN", L"MOON", L"CLOUD", L"RAIN", L"SNOW", L"STORM", L"FIREWORK", L"ROSE", L"PALM", L"MOUNTAIN", L"OCEAN", L"RAINBOW", L"LEAF"},
        {L"BEAT", L"HEADPHONES", L"MIC ON", L"PODCAST", L"VOLUME", L"AI BRAIN", L"CODE </>", L"ROCKET", L"BATTERY", L"WIFI", L"CAMERA", L"LAPTOP", L"EQUALIZER"}
    };
    static const D2D1_COLOR_F kCatCols[10] = {
        {0.92f,0.18f,0.28f,1}, {0.92f,0.52f,0.08f,1}, {0.92f,0.28f,0.60f,1}, {0.55f,0.28f,0.94f,1}, {0.06f,0.65f,0.92f,1},
        {0.00f,0.84f,0.78f,1}, {0.88f,0.20f,0.20f,1}, {0.10f,0.72f,0.45f,1}, {0.12f,0.56f,0.90f,1}, {0.38f,0.34f,0.92f,1}
    };
    static const wchar_t* kAuNames[16] = {
        L"ووش سريع / Fast Whoosh", L"سووش عميق / Deep Swoosh", L"سحب كاميرا / Whip Pan", L"غليتش / Glitch Sweep",
        L"فرقعة بوب / Bubble Pop", L"جرس اشتراك / Sub Bell", L"رنة إعجاب / Like Ding", L"نقرة ماوس / Click",
        L"انفجار سينمائي / Sub Boom", L"برام تريلر / Trailer Braam", L"تصاعد تشويق / Riser", L"نبض قلب / Heartbeat",
        L"جمع عملة / Coin", L"ترقية مستوى / Level Up", L"لوب لو-فاي / Lo-Fi Loop", L"إيقاع تراب / Trap 808"
    };
    static const wchar_t* kFlNames[14] = {
        L"Teal & Orange سينما", L"هوليوود Blockbuster", L"نيون Cyberpunk", L"ذهبي صحراوي Dune",
        L"كوداك Portra 400", L"فوجي Velvia مشبع", L"أبيض وأسود Noir", L"لايم منعش Fresh",
        L"شريط VHS قديم", L"منتصف ليل طوكيو", L"الساعة الذهبية", L"أزرق شمالي بارد",
        L"فيلم مات ناعم", L"وثائقي نقي Crisp"
    };

    int startIdx = m_hubPage * perPage;
    for (int k = 0; k < perPage && (startIdx + k) < static_cast<int>(items.size()); ++k) {
        int id = items[startIdx + k];
        int r = k / cols, c = k % cols;
        int dc = m_arabicUi ? (cols - 1 - c) : c;
        UiRect card{ gx + dc * (cardW + gap), gy + r * (cardH + gap), cardW, cardH };
        bool hov = card.contains(m_hoverX, m_hoverY);
        drawRoundRect(card, 9.0f, hov ? kColCardHover : kColCard, true, hov ? kColAccent : kColBorder, hov ? 1.4f : 0.9f);

        if (m_hubTab == 0) {
            int catIdx = (id / 104) % 10;
            int styleIdx = ((id % 104) / 13) % 8;
            int itemIdx = id % 13;
            D2D1_COLOR_F bc = kCatCols[catIdx];
            UiRect badge{ card.x + 10, card.y + 12, card.w - 20, card.h - 46 };
            float rad = (styleIdx == 0) ? (badge.h * 0.45f) : ((styleIdx == 2) ? 18.0f : 7.0f);
            drawRoundRect(badge, rad, styleIdx == 1 ? kColBg : bc, true, styleIdx == 1 ? bc : D2D1::ColorF(1,1,1,0.5f), styleIdx == 1 ? 2.0f : 1.0f);
            drawText(kStW[catIdx][itemIdx], badge, m_fontBold, styleIdx == 1 ? bc : D2D1::ColorF(1,1,1,1), DWRITE_TEXT_ALIGNMENT_CENTER);
            wchar_t sub[32]; std::swprintf(sub, 32, L"#%d  •  %s", id + 1, tr(L"Insert", L"إدراج"));
            drawText(sub, { card.x + 6, card.y + card.h - 26, card.w - 12, 20 }, m_fontSmall, hov ? kColAccent : kColTextMuted, DWRITE_TEXT_ALIGNMENT_CENTER);
            m_buttons.push_back({ BTN_HUB_INSERT_STICKER, card, static_cast<uint64_t>(id) });
        } else if (m_hubTab == 1) {
            wchar_t title[96];
            std::swprintf(title, 96, L"♪ #%d  %s", id + 1, kAuNames[id % 16]);
            drawText(title, { card.x + 10, card.y + 10, card.w - 20, 24 }, m_fontBold, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
            // Mini waveform bars
            for (int b = 0; b < 14; ++b) {
                float bh = 8.0f + static_cast<float>((id * 19 + b * 29) % 24);
                UiRect br{ card.x + 16 + b * ((card.w - 32) / 14.0f), card.y + card.h - 32 - bh * 0.5f, 4.0f, bh };
                drawRoundRect(br, 2.0f, kColAccent);
            }
            drawText(tr(L"+ Insert to A1 (WAV)", L"+ إدراج مباشر في المسار الصوتي"), { card.x + 8, card.y + card.h - 24, card.w - 16, 18 }, m_fontSmall, hov ? kColAccent : kColTextMuted, DWRITE_TEXT_ALIGNMENT_CENTER);
            m_buttons.push_back({ BTN_HUB_INSERT_AUDIO, card, static_cast<uint64_t>(id) });
        } else {
            UiRect sw1{ card.x + 10, card.y + 10, (card.w - 24) * 0.5f, card.h - 50 };
            UiRect sw2{ sw1.x + sw1.w + 4, card.y + 10, (card.w - 24) * 0.5f, card.h - 50 };
            drawRoundRect(sw1, 6.0f, kCatCols[id % 10]);
            drawRoundRect(sw2, 6.0f, kCatCols[(id + 4) % 10]);
            wchar_t fn[96]; std::swprintf(fn, 96, L"#%d  %s", id + 1, kFlNames[id % 14]);
            drawText(fn, { card.x + 8, card.y + card.h - 34, card.w - 16, 26 }, m_fontSmall, hov ? kColAccent : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
            m_buttons.push_back({ BTN_HUB_APPLY_FILTER, card, static_cast<uint64_t>(id) });
        }
    }

    // Footer Pagination Bar
    float py = modal.y + mh - 38.0f;
    UiRect prevBtn{ modal.x + mw * 0.5f - 165, py, 90, 26 };
    UiRect nextBtn{ modal.x + mw * 0.5f + 75,  py, 90, 26 };
    drawRoundRect(prevBtn, 6.0f, kColCard, true, kColBorder, 1.0f);
    drawText(tr(L"◀ Prev", L"السابق ◀"), prevBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
    drawRoundRect(nextBtn, 6.0f, kColCard, true, kColBorder, 1.0f);
    drawText(tr(L"Next ▶", L"▶ التالي"), nextBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_buttons.push_back({ BTN_HUB_PAGE, prevBtn, 0 });
    m_buttons.push_back({ BTN_HUB_PAGE, nextBtn, 1 });

    wchar_t pgBuf[64];
    std::swprintf(pgBuf, 64, tr(L"Page %d / %d", L"صفحة %d / %d"), m_hubPage + 1, totalPages);
    drawText(pgBuf, { modal.x + mw * 0.5f - 70, py, 140, 26 }, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);
}

void MainWindow::renderExportSettingsModal() {
    m_brush->SetColor(D2D1::ColorF(0, 0, 0, 0.78f));
    m_rt->FillRectangle(D2D1::RectF(0, 0, m_width, m_height), m_brush.Get());

    float mw = std::min(640.0f, m_width - 36.0f);
    float mh = std::min(564.0f, m_height - 28.0f);
    UiRect modal{ (m_width - mw) * 0.5f, (m_height - mh) * 0.5f, mw, mh };
    drawRoundRect(modal, 14.0f, kColPanel, true, kColAccent, 1.5f);

    // Header Title + Optional Clear Queue button on opposite side
    drawText(tr(L"Export Video Settings", L"إعدادات تصدير الفيديو الاحترافية"),
             { modal.x + 22, modal.y + 12, mw - 44, 28 }, m_fontTitle, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
    if (!m_renderQueue.empty()) {
        UiRect clrQ{ m_arabicUi ? (modal.x + 20.0f) : (modal.x + mw - 122.0f), modal.y + 13.0f, 102.0f, 24.0f };
        drawRoundRect(clrQ, 6.0f, kColCard, true, kColDanger, 1.0f);
        drawText(tr(L"Clear Queue", L"مسح القائمة"), clrQ, m_fontSmall, kColDanger, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_EXP_CLEAR_QUEUE, clrQ, 0 });
    }

    float lx = modal.x + 22.0f;
    float rw = mw - 44.0f;
    float y  = modal.y + 48.0f;
    // Scale row spacing smoothly if screen height is compact (< 620px)
    float rowStep = (mh < 540.0f) ? 50.0f : 56.0f;
    float btnH    = (mh < 540.0f) ? 24.0f : 27.0f;

    auto drawRow = [&](const wchar_t* title, const wchar_t* const* opts, int count, int selIdx, int btnId) {
        drawText(title, { lx, y, rw, 18 }, m_fontSmall, kColTextMuted, DWRITE_TEXT_ALIGNMENT_LEADING);
        float by = y + 20.0f;
        float bw = (rw - (count - 1) * 6.0f) / static_cast<float>(count);
        for (int i = 0; i < count; ++i) {
            UiRect r{ lx + i * (bw + 6.0f), by, bw, btnH };
            bool act = (selIdx == i);
            bool hov = r.contains(m_hoverX, m_hoverY);
            drawRoundRect(r, 6.0f, act ? kColAccent : (hov ? kColCardHover : kColCard),
                          true, act ? kColAccent : (hov ? kColAccentDim : kColBorder), 1.0f);
            drawText(opts[i], r, m_fontSmall, act ? kColDarkText : kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
            m_buttons.push_back({ btnId, r, static_cast<uint64_t>(i) });
        }
        y += rowStep;
    };

    const wchar_t* codecOpts[8] = { L"H.264", L"H.265", L"AV1", L"ProRes", L"VP9", L"GIF", L"PNG Seq", L"Alpha" };
    drawRow(tr(L"Codec & Output Format", L"الترميز وصيغة الإخراج"), codecOpts, 8, m_exportCodec % 8, BTN_EXP_SET_CODEC);

    const wchar_t* resOpts[5] = { L"480P", L"720P HD", L"1080P FHD", L"2K QHD", L"4K UHD" };
    drawRow(tr(L"Resolution", L"دقة الفيديو"), resOpts, 5, m_resPreset % 5, BTN_EXP_SET_RES);

    // Frame Rate row + non-overlapping CFR/VFR toggle placed opposite the label side!
    float fpsLabelY = y;
    const wchar_t* fpsOpts[3] = { L"24 FPS", L"30 FPS", L"60 FPS" };
    drawRow(tr(L"Frame Rate", L"معدل الإطارات"), fpsOpts, 3, m_fpsPreset % 3, BTN_EXP_SET_FPS);
    {
        float vfrW = 128.0f;
        float vfrX = m_arabicUi ? lx : (lx + rw - vfrW);
        UiRect vfrBtn{ vfrX, fpsLabelY - 2.0f, vfrW, 20.0f };
        drawRoundRect(vfrBtn, 5.0f, m_exportVfr ? kColPurple : kColCard, true, m_exportVfr ? kColPurple : kColBorder, 1.0f);
        drawText(m_exportVfr ? tr(L"Mode: VFR", L"متغير (VFR)") : tr(L"Mode: CFR", L"ثابت (CFR)"),
                 vfrBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
        m_buttons.push_back({ BTN_EXP_SET_VFR, vfrBtn, 0 });
    }

    const wchar_t* brOpts[5] = { L"3 Mbps", L"8 Mbps", L"16 Mbps", L"32 Mbps", L"64 Mbps" };
    drawRow(tr(L"Custom Bitrate", L"معدل البت المخصص"), brOpts, 5, m_bitrateIdx % 5, BTN_EXP_SET_BITRATE);

    const wchar_t* audOpts[3] = { L"48kHz / 16-bit", L"48kHz / 24-bit", L"44.1kHz / 16-bit" };
    drawRow(tr(L"Audio Sample Rate & Bit Depth", L"معدل عينات الصوت وعمق البت"), audOpts, 3, m_exportAudioCfg % 3, BTN_EXP_SET_AUDIOCFG);

    const wchar_t* csOpts[4] = { L"Rec.709", L"sRGB", L"Rec.2020 HDR", L"DCI-P3" };
    drawRow(tr(L"Color Space", L"مساحة الألوان"), csOpts, 4, m_exportColorSp % 4, BTN_EXP_SET_COLORSP);

    const wchar_t* hdrOpts[3] = { tr(L"Standard SDR", L"عادي SDR"), L"HDR10", L"Smart HDR" };
    drawRow(tr(L"Dynamic Range (HDR)", L"النطاق الديناميكي (HDR)"), hdrOpts, 3, m_hdrMode % 3, BTN_EXP_SET_HDR);

    // Sleek Summary Pill (always cleanly above footer buttons)
    const int brMbps[5] = { 3, 8, 16, 32, 64 };
    double estMB = std::max(0.5, m_engine.totalDuration()) * brMbps[m_bitrateIdx % 5] / 8.0;
    wchar_t szBuf[140];
    std::swprintf(szBuf, 140,
                  tr(L"Est. Size: ~%.1f MB   •   Duration: %.1fs   •   Queue: %zu job(s)",
                     L"الحجم التقريبي: ~%.1f ميجا   •   المدة: %.1f ثانية   •   قائمة الانتظار: %zu"),
                  estMB, m_engine.totalDuration(), m_renderQueue.size());
    UiRect infoPill{ lx, modal.y + mh - 86.0f, rw, 26.0f };
    drawRoundRect(infoPill, 6.0f, kColBg, true, kColBorder, 1.0f);
    drawText(szBuf, infoPill, m_fontSmall, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);

    // 4 Mathematically Spaced Footer Action Buttons (ZERO overlap at any width!)
    float fy   = modal.y + mh - 50.0f;
    float fh   = 34.0f;
    float gap  = 8.0f;
    float wCan = rw * 0.16f;
    float wAdd = rw * 0.24f;
    float wRun = rw * 0.25f;
    float wGo  = rw - wCan - wAdd - wRun - gap * 3.0f;

    UiRect cancelBtn{ lx, fy, wCan, fh };
    UiRect addQBtn  { cancelBtn.x + wCan + gap, fy, wAdd, fh };
    UiRect runQBtn  { addQBtn.x   + wAdd + gap, fy, wRun, fh };
    UiRect goBtn    { runQBtn.x   + wRun + gap, fy, wGo,  fh };

    drawRoundRect(cancelBtn, 7.0f, cancelBtn.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColBorder, 1.0f);
    drawText(tr(L"Cancel", L"إلغاء"), cancelBtn, m_fontBold, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_buttons.push_back({ BTN_EXP_CANCEL, cancelBtn, 0 });

    drawRoundRect(addQBtn, 7.0f, addQBtn.contains(m_hoverX, m_hoverY) ? kColCardHover : kColCard, true, kColGold, 1.0f);
    drawText(tr(L"+ Add Queue", L"+ أضف للقائمة"), addQBtn, m_fontSmall, kColGold, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_buttons.push_back({ BTN_EXP_ADD_QUEUE, addQBtn, 0 });

    drawRoundRect(runQBtn, 7.0f, !m_renderQueue.empty() ? kColPurple : kColCard, true, kColPurple, 1.0f);
    drawText(tr(L"Batch Render", L"تصيير القائمة"), runQBtn, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_buttons.push_back({ BTN_EXP_RUN_QUEUE, runQBtn, 0 });

    drawRoundRect(goBtn, 7.0f, goBtn.contains(m_hoverX, m_hoverY) ? D2D1::ColorF(0.15f, 0.96f, 0.88f, 1.0f) : kColAccent);
    drawText(tr(L"Export Now", L"تصدير الآن"), goBtn, m_fontBold, kColDarkText, DWRITE_TEXT_ALIGNMENT_CENTER);
    m_buttons.push_back({ BTN_EXP_CONFIRM, goBtn, 0 });
}

void MainWindow::renderExportOverlay() {
    m_brush->SetColor(D2D1::ColorF(0, 0, 0, 0.68f));
    m_rt->FillRectangle(D2D1::RectF(0, 0, m_width, m_height), m_brush.Get());

    UiRect modal{ (m_width - 420.0f) * 0.5f, (m_height - 170.0f) * 0.5f, 420.0f, 170.0f };
    drawRoundRect(modal, 12.0f, kColPanel, true, kColAccent, 1.5f);

    drawText(tr(L"Exporting Project...", L"جاري تصدير المشروع..."), { modal.x + 20, modal.y + 20, modal.w - 40, 28 },
             m_fontTitle, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);

    float pct = m_exportProgress.load();
    UiRect barBg{ modal.x + 28, modal.y + 74, modal.w - 56, 16 };
    drawRoundRect(barBg, 8.0f, kColCard);
    drawRoundRect({ barBg.x, barBg.y, barBg.w * std::clamp(pct, 0.0f, 1.0f), barBg.h }, 8.0f, kColAccent);

    wchar_t pctBuf[32];
    std::swprintf(pctBuf, 32, L"%d%%", static_cast<int>(pct * 100.0f));
    drawText(pctBuf, { modal.x + 20, modal.y + 102, modal.w - 40, 24 }, m_fontMono, kColAccent, DWRITE_TEXT_ALIGNMENT_CENTER);

    std::wstring st;
    {
        std::lock_guard<std::mutex> lk(m_exportMutex);
        st = utf8ToWide(m_exportStatus);
    }
    drawText(st, { modal.x + 20, modal.y + 130, modal.w - 40, 22 }, m_fontSmall, kColTextMuted, DWRITE_TEXT_ALIGNMENT_CENTER);
}

void MainWindow::renderHintBar() {
    m_brush->SetColor(kColPanel);
    m_rt->FillRectangle(m_rcHintBar.d2d(), m_brush.Get());
    m_brush->SetColor(kColBorder);
    m_rt->DrawLine(D2D1::Point2F(0, m_rcHintBar.y), D2D1::Point2F(m_width, m_rcHintBar.y), m_brush.Get(), 1.0f);

    const wchar_t* hint = tr(
        L"Tips:  Space = Play/Pause   •   Ctrl+B = Split   •   [ Trim / Trim ] = Quick Cut   •   K = Keyframe   •   Drag video anywhere to import",
        L"تلميح سريع:  مسافة (Space) تشغيل/إيقاف   •   Ctrl+B تقطيع   •   اسحب حواف المقطع للقص   •   K كيفريم   •   اسحب أي فيديو هنا مباشرة");
    drawText(hint, { 12, m_rcHintBar.y, m_width - 24, m_rcHintBar.h }, m_fontSmall, kColTextMuted, DWRITE_TEXT_ALIGNMENT_CENTER);
}

void MainWindow::renderToast() {
    std::wstring msg;
    {
        std::lock_guard<std::mutex> lk(m_toastMutex);
        if (m_toastText.empty() || GetTickCount() > m_toastExpireTick) return;
        msg = m_toastText;
    }
    UiRect r{ (m_width - 560.0f) * 0.5f, 58.0f, 560.0f, 34.0f };
    drawRoundRect(r, 8.0f, kColPanel, true, kColAccent, 1.4f);
    drawText(msg, r, m_fontSmall, kColTextMain, DWRITE_TEXT_ALIGNMENT_CENTER);
}

void MainWindow::showToast(const std::wstring& msg) {
    {
        std::lock_guard<std::mutex> lk(m_toastMutex);
        m_toastText = msg;
        m_toastExpireTick = GetTickCount() + 4000;
    }
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::setPlaying(bool play) {
    if (m_playing == play) return;
    m_playing = play;
    if (m_playing) {
        m_lastTickSec = timeGetTime() / 1000.0;
        m_audio.startPlayback(m_engine.project(), m_engine.playhead());
    } else {
        m_audio.stopPlayback();
    }
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::onMouseDown(float x, float y) {
    if (m_exporting.load()) return;

    for (auto it = m_buttons.rbegin(); it != m_buttons.rend(); ++it) {
        if (it->rect.contains(x, y)) {
            switch (it->id) {
                case BTN_NEW:             actionNewProject(); return;
                case BTN_OPEN:            actionOpenProject(); return;
                case BTN_SAVE:            actionSaveProject(); return;
                case BTN_EXPORT:          m_showExportModal = true; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_NLE_JCUT:
                    if (m_engine.selectedClipId() != 0 && m_engine.applyJCut(m_engine.selectedClipId(), 1.0) != 0) {
                        m_audio.updateProjectSnapshot(m_engine.project());
                        showToast(tr(L"Applied J-Cut (Audio leads video by 1s)!", L"تم تطبيق J-Cut (دخول الصوت قبل الصورة)!"));
                    }
                    return;
                case BTN_NLE_LCUT:
                    if (m_engine.selectedClipId() != 0 && m_engine.applyLCut(m_engine.selectedClipId(), 1.0) != 0) {
                        m_audio.updateProjectSnapshot(m_engine.project());
                        showToast(tr(L"Applied L-Cut (Audio trails video by 1s)!", L"تم تطبيق L-Cut (استمرار الصوت بعد الصورة)!"));
                    }
                    return;
                case BTN_NLE_SLIP:
                    if (m_engine.selectedClipId() != 0 && m_engine.slipClip(m_engine.selectedClipId(), 0.5)) {
                        showToast(tr(L"Slip Edit: shifted clip content +0.5s!", L"تم تحرير Slip (إزاحة محتوى المقطع 0.5 ثانية)!"));
                    }
                    return;
                case BTN_NLE_COMPOUND:
                    if (Clip* s = m_engine.selectedClip(); s && m_engine.createCompoundOnTrack(s->trackIndex)) {
                        showToast(tr(L"Created Compound Clip on track!", L"تم دمج المسار في مقطع مركب (Compound Clip)!"));
                    } else showToast(tr(L"Need 2+ clips on the same track for Compound Clip.", L"يجب وجود مقطعين على نفس المسار للدمج المركب."));
                    return;
                case BTN_NLE_NEST: {
                    wchar_t fb[MAX_PATH] = {};
                    OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = m_hwnd;
                    ofn.lpstrFilter = L"CapCutVS Project (*.ccvs)\0*.ccvs\0"; ofn.lpstrFile = fb; ofn.nMaxFile = MAX_PATH;
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    if (GetOpenFileNameW(&ofn)) {
                        Project nested; std::string err;
                        if (ProjectIO::loadFromFile(wideToUtf8(fb), nested, err) && m_engine.importNestedSequence(nested, m_engine.playhead())) {
                            showToast(tr(L"Imported Nested Sequence at playhead!", L"تم إدراج المشروع المتداخل (Nested Sequence)!"));
                        }
                    }
                    return;
                }
                case BTN_NLE_MULTICAM:
                    if (m_engine.multiCamSwitchAtPlayhead()) {
                        showToast(tr(L"Multi-Cam switched between V1 & V2 at playhead!", L"تم التبديل بين الكاميرا V1 و V2 عند المؤشر!"));
                    } else showToast(tr(L"Place Cam A on V1 & Cam B on V2, then click MultiCam at playhead.", L"ضع الكاميرا 1 على V1 والكاميرا 2 على V2 للتبديل."));
                    return;
                case BTN_EXP_CANCEL:      m_showExportModal = false; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_EXP_SET_RES: {
                    m_resPreset = static_cast<int>(it->param) % 5;
                    int heights[5] = { 480, 720, 1080, 1440, 2160 };
                    int h = heights[m_resPreset];
                    if (m_aspectMode == 0) { m_engine.project().canvasWidth = h * 16 / 9; m_engine.project().canvasHeight = h; }
                    else if (m_aspectMode == 1) { m_engine.project().canvasWidth = h * 9 / 16; m_engine.project().canvasHeight = h; }
                    else { m_engine.project().canvasWidth = h; m_engine.project().canvasHeight = h; }
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                }
                case BTN_EXP_SET_FPS: {
                    m_fpsPreset = static_cast<int>(it->param) % 3;
                    const double fv[3] = { 24.0, 30.0, 60.0 };
                    m_engine.project().fps = fv[m_fpsPreset];
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                }
                case BTN_EXP_SET_BITRATE:  m_bitrateIdx = static_cast<int>(it->param) % 5; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_EXP_SET_CODEC:    m_exportCodec = static_cast<int>(it->param) % 8; m_exportMov = (m_exportCodec == 3); InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_EXP_SET_VFR:      m_exportVfr = !m_exportVfr; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_EXP_SET_AUDIOCFG: m_exportAudioCfg = static_cast<int>(it->param) % 3; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_EXP_SET_COLORSP:  m_exportColorSp = static_cast<int>(it->param) % 4; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_EXP_ADD_QUEUE:    actionAddToRenderQueue(); return;
                case BTN_EXP_RUN_QUEUE:    m_showExportModal = false; actionRunRenderQueue(); return;
                case BTN_EXP_CLEAR_QUEUE:
                    m_renderQueue.clear();
                    showToast(tr(L"Render Queue cleared.", L"تم مسح قائمة مهام التصدير."));
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_CYCLE_EASING:
                    if (Clip* sc = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sc->keyframeEasing = static_cast<KeyframeEasing>((static_cast<int>(sc->keyframeEasing) + 1) % 5);
                        showToast(std::wstring(tr(L"Keyframe Motion Curve: ", L"منحنى حركة الكي فريم: ")) + utf8ToWide(keyframeEasingName(sc->keyframeEasing)));
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_OBJ_TRACK:     actionRunObjectTrack(false); return;
                case BTN_PLANAR_TRACK:  actionRunObjectTrack(true);  return;
                case BTN_ROTO_KEYFRAME:
                    if (Clip* sc = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sc->color.autoBgRemove = true;
                        m_engine.addOrUpdateKeyframeAtPlayhead(sc->id);
                        m_engine.setPlayhead(m_engine.playhead() + 1.0 / std::max(1.0, m_engine.project().fps));
                        showToast(tr(L"Rotoscoping frame keyframed & stepped +1 frame!", L"تم تثبيت مفتاح روتوسكوبينج والانتقال للإطار التالي!"));
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_RESTORE_VERSION: actionRestoreVersionHistory(); return;
                case BTN_GEN_PROXIES:     actionGenerateDiskProxies(); return;
                case BTN_SCENE_DETECT:    actionDetectSceneCuts(); return;
                case BTN_OPTICAL_STAB:    actionRunOpticalStabilization(); return;
                case BTN_CYCLE_HSL_BAND:
                    m_hslBandIdx = (m_hslBandIdx + 1) % 8;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_CAM3D_TRACK:
                    if (Clip* sel = m_engine.selectedClip()) {
                        if (sel->type == MediaType::Video && !sel->sourcePath.empty()) {
                            uint64_t cid = sel->id; std::string src = sel->sourcePath;
                            double sIn = sel->sourceIn, dur = sel->duration;
                            runAiTaskAsync(tr(L"Solving 6-DOF 3D Camera Parallax...", L"جاري حساب تتبع الكاميرا ثلاثي الأبعاد في الخلفية..."), [this, cid, src, sIn, dur]() {
                                std::vector<Keyframe> kfs;
                                bool ok = m_media.trackCamera3DPath(src, sIn, dur, 8, kfs);
                                postUiTask([this, cid, ok, kfs]() {
                                    if (ok) {
                                        if (Clip* c = m_engine.findClip(cid)) { m_engine.pushUndoSnapshot(); c->keyframes = kfs; }
                                        showToast(tr(L"3D Camera Tracking applied (pan/tilt/zoom/roll/perspective)!", L"تم تطبيق تتبع الكاميرا ثلاثي الأبعاد بنجاح!"));
                                    }
                                });
                            });
                        } else showToast(tr(L"Select a video clip first.", L"اختر مقطع فيديو أولاً."));
                    }
                    return;
                case BTN_OPEN_ASSETHUB:
                    m_showAssetHubModal = !m_showAssetHubModal;
                    m_showAllToolsModal = false;
                    m_showExportModal = false;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_HUB_TAB:
                    m_hubTab = static_cast<int>(it->param) % 3;
                    m_hubCat = 0;
                    m_hubPage = 0;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_HUB_CAT:
                    m_hubCat = static_cast<int>(it->param);
                    m_hubPage = 0;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_HUB_PAGE:
                    m_hubPage = std::max(0, m_hubPage + (it->param == 1 ? 1 : -1));
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_HUB_INSERT_STICKER: {
                    int sid = static_cast<int>(it->param); // 0..519
                    int catIdx = (sid / 104) % 10;
                    int styleIdx = ((sid % 104) / 13) % 8;
                    int itemIdx = sid % 13;
                    static const char* kStTexts[10][13] = {
                        {"SUBSCRIBE", "LIKE 👍", "SHARE ↗", "FOLLOW +", "LIVE ●", "NEW!", "TRENDING 🔥", "LINK IN BIO", "BELL 🔔", "COMMENT 💬", "TIKTOK VIRAL", "REELS HD", "SHORTS ⚡"},
                        {"حصري 🔥", "مفاجأة ✨", "لا يفوتك!", "شاهد للنهاية", "قصة حقيقية", "تجربة", "نصيحة ذهبية", "مقارنة شاملة", "الأفضل 2026", "مراجعة سريعة", "هل تعلم؟", "الجزء الأول", "تحدي جديد"},
                        {"WOW! 🤯", "LOL 😂", "FIRE 🔥", "LOVE ❤️", "COOL 😎", "100% 💯", "CLAP 👏", "STAR ⭐", "OMG 😱", "GENIUS 🧠", "PARTY 🎉", "OK 👌", "CROWN 👑"},
                        {"GG EZ 🎮", "VICTORY 🏆", "HEADSHOT 🎯", "LEVEL UP ⬆", "BOSS FIGHT", "1HP CLUTCH", "PRO PLAYER", "GAME OVER", "COMBO x10", "STREAM LIVE", "AFK 💤", "NOOB 😂", "ULTRA KILL"},
                        {"REC ● 00:01", "4K ULTRA HD", "60 FPS ⚡", "CINEMA 🎬", "VLOG DAY 1", "THE END", "ACTION!", "BEHIND SCENES", "HDR10+ ✦", "TRAVEL ✈️", "FLASHBACK", "SLOW MO", "INTRO ▶"},
                        {"CYBER ◈", "GLOW BOX", "SPARKLE ✦", "NEON RING", "HEXAGON ⬡", "DIAMOND ◆", "LIGHTNING ⚡", "STARBURST", "HEART ♥", "WAVE 〰", "TARGET ◎", "CROWN ♛", "PORTAL ◉"},
                        {"50% OFF", "FREE! 🎁", "HOT DEAL 🔥", "LIMITED ⏳", "BUY NOW 🛒", "VIP PRO 💎", "BEST SELLER", "NEW ARRIVAL", "FLASH SALE", "100% REAL ✔", "SAVE BIG 💰", "BONUS + 🎀", "TOP PICK 🏅"},
                        {"ARROW ➔", "LEFT ⬅", "UP ⬆", "DOWN ⬇", "CURVE ↪", "LOOK HERE 👇", "CHECK ✔", "CROSS ✖", "ALERT ⚠", "PIN 📍", "ZOOM 🔍", "STOP ✋", "NEXT ⏭"},
                        {"SUN ☀️", "MOON 🌙", "CLOUD ☁️", "RAIN 🌧️", "SNOW ❄️", "STORM ⛈️", "FIREWORK 🎆", "ROSE 🌹", "PALM 🌴", "MOUNTAIN 🏔️", "OCEAN 🌊", "RAINBOW 🌈", "LEAF 🍃"},
                        {"BEAT 🎵", "HEADPHONES 🎧", "MIC ON 🎙️", "PODCAST 📻", "VOLUME 🔊", "AI BRAIN 🤖", "CODE </>", "ROCKET 🚀", "BATTERY 🔋", "WIFI 📶", "CAMERA 📷", "LAPTOP 💻", "EQUALIZER"}
                    };
                    static const uint32_t kCatBg[10] = {
                        0xF0E5253A, 0xF0D97706, 0xF0EC4899, 0xF07C3AED, 0xF00EA5E9,
                        0xF0111827, 0xF0DC2626, 0xF0059669, 0xF00284C7, 0xF04F46E5
                    };
                    uint64_t cid = m_engine.addTextClip(kStTexts[catIdx][itemIdx], m_engine.playhead(), 3.2);
                    if (Clip* tc = m_engine.findClip(cid)) {
                        tc->text.isStickerBadge = true;
                        tc->text.fontSize = (styleIdx == 2) ? 46.0f : 42.0f;
                        tc->text.bgARGB = (styleIdx == 1) ? 0xEE0B101B : kCatBg[catIdx];
                        tc->text.colorARGB = (styleIdx == 1) ? 0xFF00E5D1 : 0xFFFFFFFF;
                        tc->text.effect = (styleIdx == 1) ? TextEffectType::NeonGlow : ((styleIdx == 3) ? TextEffectType::Extrude3D : TextEffectType::None);
                        tc->text.anim = TextAnimType::BouncePop;
                        tc->transform.posY = -0.18f;
                    }
                    m_showAssetHubModal = false;
                    showToast(tr(L"Inserted Sticker directly onto Timeline!", L"تم إدراج الملصق مباشرةً على الخط الزمني!"));
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                }
                case BTN_HUB_INSERT_AUDIO: {
                    int aid = static_cast<int>(it->param) % 120;
                    std::filesystem::path sDir = std::filesystem::current_path() / "NovaCut_Sounds";
                    std::error_code ec;
                    std::filesystem::create_directories(sDir, ec);
                    std::string wavPath = (sDir / ("novacut_sfx_" + std::to_string(aid + 1) + ".wav")).string();
                    if (AudioEngine::generateHubSoundWav(aid, wavPath)) {
                        importFilePath(wavPath);
                        m_showAssetHubModal = false;
                        showToast(tr(L"Inserted Sound Effect onto Track A1!", L"تم إدراج المؤثر الصوتي مباشرةً على مسار الصوت!"));
                    }
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                }
                case BTN_HUB_APPLY_FILTER: {
                    int fid = static_cast<int>(it->param) % 60;
                    Clip* c = m_engine.selectedClip();
                    if (!c) {
                        for (auto& cl : m_engine.project().clips) {
                            if (cl.type == MediaType::Video || cl.type == MediaType::Image) { m_engine.selectClip(cl.id); c = &cl; break; }
                        }
                    }
                    if (c) {
                        m_engine.pushUndoSnapshot();
                        // Apply curated color grade profile for each of the 42 cinema filters
                        c->color.contrast    = 1.05f + (fid % 5) * 0.06f;
                        c->color.temperature = ((fid % 3 == 0) ? 0.22f : ((fid % 3 == 1) ? -0.20f : 0.06f));
                        c->color.tint        = ((fid % 4 == 2) ? 0.14f : ((fid % 4 == 3) ? -0.10f : 0.0f));
                        c->color.saturation  = (fid == 6 || fid == 23) ? 0.0f : (0.92f + (fid % 6) * 0.09f);
                        c->color.splitShadowHue = static_cast<float>((fid * 47) % 360);
                        c->color.splitHighHue   = static_cast<float>((fid * 47 + 165) % 360);
                        c->color.splitAmount    = (fid == 6 || fid == 23) ? 0.0f : 0.28f;
                        m_showAssetHubModal = false;
                        m_inspTab = InspectorTab::Color;
                        showToast(tr(L"Applied Cinema Filter directly to Clip!", L"تم تطبيق الفلتر السينمائي مباشرةً على المقطع!"));
                    } else {
                        showToast(tr(L"Add a video or image clip to Timeline first.", L"أضف فيديو أو صورة للتايملاين أولاً لتطبيق الفلتر."));
                    }
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                }
                case BTN_CLEAR_LUT:
                    if (Clip* sc = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sc->color.lutPath.clear();
                        showToast(tr(L"Cleared active 3D LUT.", L"تمت إزالة ملف الـ LUT."));
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_EXP_SET_FMT:     m_exportMov = (it->param != 0); InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_EXP_SET_HDR:     m_hdrMode = static_cast<int>(it->param) % 3; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_EXP_CONFIRM:     m_showExportModal = false; actionStartExport(); return;
                case BTN_RES_CYCLE:       actionCycleResolution(); return;
                case BTN_ASPECT_CYCLE:    actionCycleAspectRatio(); return;
                case BTN_TAB_MEDIA:       m_leftTab = LeftTab::Media; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_TAB_TEXT:        m_leftTab = LeftTab::Text; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_TAB_FILTERS:     m_leftTab = LeftTab::Filters; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_TAB_EFFECTS:     m_leftTab = LeftTab::Effects; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_TAB_TRANSITIONS: m_leftTab = LeftTab::Transitions; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_TAB_STICKERS:    m_leftTab = LeftTab::Stickers; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_TAB_AUDIO:       m_leftTab = LeftTab::Audio; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_TAB_AI:          m_leftTab = LeftTab::AI; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_AI_APIKEY: {
                    std::wstring k = utf8ToWide(m_aiConfig.geminiApiKey);
                    if (promptTextDialog(tr(L"Enter Gemini API Key (for Online Script & Translate)", L"أدخل مفتاح Gemini API (للكتابة والترجمة عبر الإنترنت)"), k)) {
                        m_aiConfig.geminiApiKey = wideToUtf8(k);
                        AICloudBridge::saveConfig(m_aiConfig);
                        showToast(tr(L"Saved AI API Key!", L"تم حفظ مفتاح الذكاء الاصطناعي!"));
                    }
                    return;
                }
                case BTN_AI_LOCAL_MODEL: {
                    std::wstring mod = utf8ToWide(m_aiConfig.localModel);
                    if (promptTextDialog(tr(L"Local Model (auto = active LM Studio model, or type model name)", L"اسم النموذج المحلي (اتركه auto لاستخدام النموذج المحمل في LM Studio تلقائياً)"), mod) && !mod.empty()) {
                        m_aiConfig.localModel = wideToUtf8(mod);
                        showToast(tr(L"Local AI Model set to: ", L"تم ضبط النموذج المحلي على: ") + mod);
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                }
                case BTN_AI_TEST_LOCAL: {
                    auto cfg = m_aiConfig;
                    runAiTaskAsync(tr(L"Checking LM Studio (127.0.0.1:1234)...", L"جاري فحص اتصال LM Studio..."), [this, cfg]() {
                        std::string mods;
                        bool ok = AICloudBridge::checkLocalServerOnline(cfg, mods);
                        postUiTask([this, ok, mods]() {
                            if (ok) {
                                showToast(tr(L"LM Studio Connected ✓ ", L"تم الاتصال بـ LM Studio بنجاح ✓ ") + utf8ToWide(mods));
                            } else {
                                showToast(tr(L"Start Local Server in LM Studio (port 1234) — Built-in Offline AI active!",
                                             L"شغّل Local Server في LM Studio (منفذ 1234) — والذكاء المدمج يعمل تلقائياً!"));
                            }
                        });
                    });
                    return;
                }
                case BTN_AI_TOOL: {
                    int aiCheck = static_cast<int>(it->param);
                    if (m_offlineMode && (aiCheck == 0 || aiCheck == 1 || aiCheck == 5)) {
                        showToast(tr(L"Offline Mode is ON! Click 'Offline' in top bar to enable Online AI.", L"وضع العمل دون إنترنت مفعل! اضغط زر (أوفلاين) بالأعلى لتفعيل الذكاء الاصطناعي السحابي."));
                        return;
                    }
                    int idx = static_cast<int>(it->param);
                    if (idx == 0) { // AI Image Generator (Online — Async)
                        std::wstring pr = L"cinematic futuristic neon city sunset 4k";
                        if (promptTextDialog(tr(L"AI Image Generator (Requires Internet)", L"توليد صورة بالذكاء الاصطناعي (يتطلب إنترنت) — اكتب الوصف"), pr) && !pr.empty()) {
                            std::string outImg = "ai_image_" + std::to_string(m_aiGenCounter++) + ".png";
                            std::string promptUtf8 = wideToUtf8(pr);
                            runAiTaskAsync(tr(L"Generating AI Image in background...", L"جاري توليد الصورة في الخلفية (يمكنك متابعة العمل)..."), [this, promptUtf8, outImg]() {
                                std::string err;
                                bool ok = AICloudBridge::generateImageOnline(promptUtf8, outImg, err, m_aiConfig.geminiApiKey);
                                postUiTask([this, ok, outImg, err]() {
                                    if (ok) { importFilePath(outImg); showToast(tr(L"AI Image generated & added!", L"تم توليد الصورة وإضافتها للتايملاين!")); }
                                    else showToast(utf8ToWide(err));
                                });
                            });
                        }
                    } else if (idx == 1) { // AI Video Generator (Async)
                        std::wstring pr = L"epic mountain drone sunrise cinema";
                        if (promptTextDialog(tr(L"AI Video Generator (Requires Internet) — Topic", L"توليد فيديو AI (يتطلب إنترنت) — اكتب موضوع الفيديو"), pr) && !pr.empty()) {
                            std::string promptUtf8 = wideToUtf8(pr);
                            int baseCnt = m_aiGenCounter; m_aiGenCounter += 3;
                            runAiTaskAsync(tr(L"Generating 3 AI scenes in background...", L"جاري توليد 3 مشاهد فيديو في الخلفية..."), [this, promptUtf8, baseCnt]() {
                                const char* angles[3] = { " wide establishing shot", " dramatic close up", " epic finale lighting" };
                                std::vector<std::string> imgs;
                                for (int s = 0; s < 3; ++s) {
                                    std::string outImg = "ai_scene_" + std::to_string(baseCnt + s) + ".png";
                                    std::string err;
                                    if (AICloudBridge::generateImageOnline(promptUtf8 + angles[s], outImg, err, m_aiConfig.geminiApiKey)) imgs.push_back(outImg);
                                }
                                postUiTask([this, imgs]() {
                                    int added = 0;
                                    for (size_t s = 0; s < imgs.size(); ++s) {
                                        MediaAsset a;
                                        if (m_media.probeFile(imgs[s], a)) {
                                            a.durationSec = 3.5;
                                            uint64_t aid = m_engine.addAsset(a);
                                            uint64_t cid = m_engine.addClipFromAsset(aid, m_engine.defaultTrackForMedia(MediaType::Image), s * 3.5);
                                            if (Clip* cl = m_engine.findClip(cid)) { cl->transitionIn = TransitionType::ZoomIn; cl->transitionDuration = 0.8; }
                                            ++added;
                                        }
                                    }
                                    showToast(added > 0 ? tr(L"AI Video scenes generated on V1!", L"تم توليد مشاهد الفيديو AI بنجاح!")
                                                        : tr(L"Internet connection required for AI Video.", L"يتطلب اتصالاً بالإنترنت لتوليد الفيديو."));
                                });
                            });
                        }
                    } else if (idx == 2) { // AI Script Writer (LM Studio Local 2B — Async)
                        std::wstring topic = tr(L"Top 3 AI Video Editing Tips", L"أفضل 3 نصائح لمونتاج فيديو سينمائي");
                        if (promptTextDialog(tr(L"AI Script Writer (LM Studio / Local 2B)", L"كتابة سكربت بالذكاء الاصطناعي (LM Studio محلي) — اكتب الموضوع"), topic) && !topic.empty()) {
                            auto cfg = m_aiConfig;
                            std::string tUtf8 = wideToUtf8(topic);
                            runAiTaskAsync(tr(L"Writing script with LM Studio AI...", L"جاري كتابة السكربت عبر LM Studio في الخلفية..."), [this, cfg, tUtf8]() {
                                std::string note;
                                auto lines = AICloudBridge::generateScriptLinesLocal(cfg, tUtf8, note);
                                postUiTask([this, lines, note]() {
                                    for (size_t i = 0; i < lines.size(); ++i) {
                                        uint64_t cid = m_engine.addTextClip(lines[i], i * 3.0, 2.8);
                                        if (Clip* tc = m_engine.findClip(cid)) { tc->text.anim = TextAnimType::Typewriter; tc->transform.posY = 0.65f; tc->text.fontSize = 38.0f; }
                                    }
                                    showToast(utf8ToWide(note));
                                });
                            });
                        }
                    } else if (idx == 3) { // AI Translation (LM Studio Local 2B — Async, No API key required!)
                        std::vector<std::pair<uint64_t, std::string>> texts;
                        for (const auto& c : m_engine.project().clips) {
                            if (c.type == MediaType::Text && !c.text.content.empty()) texts.push_back({ c.id, c.text.content });
                        }
                        if (texts.empty()) {
                            showToast(tr(L"Add Text/Captions on T1 first.", L"أضف نصوصاً على مسار T1 أولاً."));
                        } else {
                            auto cfg = m_aiConfig;
                            std::string targetLang = m_arabicUi ? "Arabic" : "English";
                            runAiTaskAsync(tr(L"Translating captions via LM Studio...", L"جاري ترجمة النصوص عبر LM Studio في الخلفية..."), [this, cfg, texts, targetLang]() {
                                std::vector<std::pair<uint64_t, std::string>> out;
                                for (const auto& kv : texts) {
                                    out.push_back({ kv.first, AICloudBridge::translateTextLocal(cfg, kv.second, targetLang) });
                                }
                                postUiTask([this, out]() {
                                    for (const auto& kv : out) {
                                        if (Clip* cl = m_engine.findClip(kv.first)) cl->text.content = kv.second;
                                    }
                                    showToast(tr(L"Translated all captions via Local AI!", L"تمت ترجمة جميع النصوص بنجاح!"));
                                });
                            });
                        }
                    } else if (idx == 4) { // AI Voiceover (TTS — Async so UI NEVER freezes!)
                        std::wstring txt = tr(L"Hello from NovaCut Studio AI Voice", L"مرحباً بكم في استوديو المونتاج الاحترافي");
                        if (promptTextDialog(tr(L"AI Voice Generator (Enter script)", L"توليد تعليق صوتي AI (اكتب النص)"), txt) && !txt.empty()) {
                            std::string wav = "ai_voice_" + std::to_string(m_voiceRecCounter++) + ".wav";
                            std::string txtUtf8 = wideToUtf8(txt);
                            runAiTaskAsync(tr(L"Synthesizing AI Voice in background...", L"جاري توليد التعليق الصوتي في الخلفية..."), [this, txtUtf8, wav]() {
                                bool ok = AudioEngine::synthesizeTextToSpeechWav(txtUtf8, wav);
                                postUiTask([this, ok, wav]() {
                                    if (ok) { importFilePath(wav); showToast(tr(L"AI Voice added to A1!", L"تم توليد الصوت وإضافته للتايملاين!")); }
                                    else showToast(tr(L"TTS voice generation failed.", L"فشل توليد الصوت."));
                                });
                            });
                        }
                    } else if (idx == 5) { // AI Avatar (Async)
                        std::wstring desc = L"friendly studio presenter portrait green background";
                        if (promptTextDialog(tr(L"AI Avatar (Requires Internet) — Describe character", L"شخصية افتراضية (يتطلب إنترنت) — صف الشخصية"), desc) && !desc.empty()) {
                            std::string outImg = "ai_avatar_" + std::to_string(m_aiGenCounter++) + ".png";
                            std::string descUtf8 = wideToUtf8(desc);
                            runAiTaskAsync(tr(L"Generating AI Avatar in background...", L"جاري إنشاء الشخصية الافتراضية في الخلفية..."), [this, descUtf8, outImg]() {
                                std::string err;
                                bool ok = AICloudBridge::generateImageOnline(descUtf8, outImg, err, m_aiConfig.geminiApiKey);
                                postUiTask([this, ok, outImg, err]() {
                                    if (ok) {
                                        MediaAsset a;
                                        if (m_media.probeFile(outImg, a)) {
                                            uint64_t aid = m_engine.addAsset(a);
                                            uint64_t cid = m_engine.addClipFromAsset(aid, 1, m_engine.playhead());
                                            if (Clip* av = m_engine.findClip(cid)) { av->color.autoBgRemove = true; av->transform.scale = 0.45f; av->transform.posX = 0.55f; av->transform.posY = 0.42f; }
                                            showToast(tr(L"AI Avatar placed on V2!", L"تم إنشاء الشخصية الافتراضية على مسار V2!"));
                                        }
                                    } else showToast(utf8ToWide(err));
                                });
                            });
                        }
                    } else if (idx == 6) { // V34: Real Neural Wav2Lip Lip Sync (or Audio-Amplitude Driven Sync)
                        if (Clip* sel = m_engine.selectedClip()) {
                            std::string audPath;
                            for (const auto& c : m_engine.project().clips) {
                                if (c.type == MediaType::Audio && !c.sourcePath.empty()) { audPath = c.sourcePath; break; }
                            }
                            uint64_t cid = sel->id; std::string src = sel->sourcePath; double dur = sel->duration;
                            float baseScale = sel->transform.scale, px = sel->transform.posX, py = sel->transform.posY, rot = sel->transform.rotationDeg, op = sel->color.opacity;
                            std::string outMp4 = "CapCutVS_AI/lipsync_" + std::to_string(m_aiGenCounter++) + ".mp4";
                            runAiTaskAsync(tr(L"Running AI Lip Sync (Wav2Lip / Audio Envelope)...", L"جاري تشغيل مزامنة الشفاه (Wav2Lip / تحليل الصوت)..."), [this, cid, src, audPath, outMp4, dur, baseScale, px, py, rot, op]() {
                                std::string log;
                                bool neuralOk = (!audPath.empty() && AICloudBridge::runNeuralPluginTask("lipsync", src, outMp4, audPath, log));
                                std::vector<float> peaks = audPath.empty() ? std::vector<float>{} : m_audio.getWaveformPeaks(audPath);
                                postUiTask([this, cid, neuralOk, outMp4, peaks, dur, baseScale, px, py, rot, op]() {
                                    if (neuralOk) {
                                        importFilePath(outMp4);
                                        showToast(tr(L"Wav2Lip Neural Lip Sync video added!", L"تم توليد فيديو مزامنة الشفاه الحقيقية (Wav2Lip) وإضافته!"));
                                    } else if (Clip* c = m_engine.findClip(cid)) {
                                        m_engine.pushUndoSnapshot();
                                        c->keyframes.clear();
                                        int steps = std::max(4, std::min(32, static_cast<int>(dur * 4.0)));
                                        for (int k = 0; k < steps; ++k) {
                                            double t = (dur * k) / std::max(1, steps - 1);
                                            float amp = 0.0f;
                                            if (!peaks.empty()) amp = peaks[std::min(peaks.size() - 1, static_cast<size_t>(k * peaks.size() / steps))];
                                            c->keyframes.push_back({ t, px, py, baseScale * (1.0f + amp * 0.08f), rot, op });
                                        }
                                        showToast(tr(L"Audio-Envelope Lip Motion applied! (Drop Wav2Lip in CapCutVS_AI/ for neural lips)", L"تمت المزامنة مع ذروة الصوت! (وللمزامنة العصبية الكاملة شغّل setup_plugins.bat)"));
                                    }
                                });
                            });
                        } else showToast(tr(L"Select an Avatar/Video clip first.", L"حدد مقطع الشخصية أو الفيديو أولاً."));
                    } else if (idx == 7) { actionAutoCaptions(); }
                    else if (idx == 8) { // V34: Real Neural BiRefNet/rembg Background Segmentation (with C++ Multi-Cue fallback)
                        if (Clip* s = m_engine.selectedClip()) {
                            uint64_t cid = s->id; std::string src = s->sourcePath; double sIn = s->sourceIn; MediaType mtype = s->type;
                            std::string outPng = "CapCutVS_AI/bg_cut_" + std::to_string(m_aiGenCounter++) + ".png";
                            runAiTaskAsync(tr(L"Running Neural Background Segmentation (BiRefNet/rembg)...", L"جاري فصل الخلفية بالذكاء الاصطناعي (BiRefNet/rembg)..."), [this, cid, src, sIn, mtype, outPng]() {
                                std::string inImg = src;
                                if (mtype == MediaType::Video) {
                                    FrameBGRA f;
                                    if (m_media.decodeFrame(src, MediaType::Video, sIn, f) && f.valid()) {
                                        inImg = "CapCutVS_AI/tmp_seg_in.png";
                                        m_media.saveFrameToPng(f, inImg);
                                    }
                                }
                                std::string log;
                                bool ok = AICloudBridge::runNeuralPluginTask("subject_hair_select", inImg, outPng, "", log);
                                postUiTask([this, cid, ok, outPng]() {
                                    if (ok) {
                                        importFilePath(outPng);
                                        showToast(tr(L"BiRefNet/rembg Neural Cutout added to Timeline!", L"تم فصل الخلفية بشبكة BiRefNet/rembg العصبية وإضافتها!"));
                                    } else if (Clip* c = m_engine.findClip(cid)) {
                                        c->color.autoBgRemove = !c->color.autoBgRemove;
                                        showToast(tr(L"Applied Multi-Cue C++ Segmentation (Run setup_plugins.bat for BiRefNet)", L"تم تطبيق فصل الخلفية المدمج (شغّل setup_plugins.bat لتفعيل BiRefNet العصبي)"));
                                    }
                                });
                            });
                        }
                    }
                    else if (idx == 9) { if (Clip* s = m_engine.selectedClip()) { s->color.aiRelight = (s->color.aiRelight > 0.1f) ? 0.0f : 0.75f; showToast(tr(L"Toggled AI Studio Relight!", L"تم تبديل تعديل الإضاءة الذكي!")); } }
                    else if (idx == 10){ // V40: Real-ESRGAN 2x/4x Neural Super-Resolution (with C++ HD Enhance fallback)
                        if (Clip* s = m_engine.selectedClip()) {
                            uint64_t cid = s->id; std::string src = s->sourcePath; double sIn = s->sourceIn; MediaType mtype = s->type;
                            std::string outPng = "CapCutVS_AI/esrgan_" + std::to_string(m_aiGenCounter++) + ".png";
                            runAiTaskAsync(tr(L"Running Real-ESRGAN Super-Resolution...", L"جاري تحسين الدقة عبر Real-ESRGAN في الخلفية..."), [this, cid, src, sIn, mtype, outPng]() {
                                std::string inImg = src;
                                if (mtype == MediaType::Video) {
                                    FrameBGRA f;
                                    if (m_media.decodeFrame(src, MediaType::Video, sIn, f) && f.valid()) {
                                        inImg = "CapCutVS_AI/tmp_esr_in.png";
                                        m_media.saveFrameToPng(f, inImg);
                                    }
                                }
                                std::string log;
                                bool ok = AICloudBridge::runNeuralPluginTask("realesrgan_upscale", inImg, outPng, "", log);
                                postUiTask([this, cid, ok, outPng]() {
                                    if (ok) { importFilePath(outPng); showToast(tr(L"Real-ESRGAN Upscaled image added to Timeline!", L"تم تحسين الدقة عبر Real-ESRGAN وإضافة النتيجة!")); }
                                    else if (Clip* c = m_engine.findClip(cid)) {
                                        c->color.enhanceHD = (c->color.enhanceHD > 0.1f) ? 0.0f : 0.70f;
                                        showToast(tr(L"Applied C++ HD Enhance (Run setup_plugins.bat for Real-ESRGAN)", L"تم تطبيق تحسين الجودة المدمج (شغّل setup_plugins.bat لتفعيل Real-ESRGAN)"));
                                    }
                                });
                            });
                        }
                    }
                    else if (idx == 11){ // Auto Reframe 9:16
                        m_aspectMode = 1; computeLayout();
                        if (Clip* s = m_engine.selectedClip()) { s->transform.posX = 0; s->transform.posY = 0; s->transform.scale = 1.78f; }
                        showToast(tr(L"Auto Reframed to 9:16 Vertical!", L"تم ضبط الإطار تلقائياً لمقاس 9:16!"));
                    } else if (idx == 12){ // V34: Real Optical Block-Matching Motion Tracking (NO hardcoded keyframes!)
                        actionRunObjectTrack(false);
                    } else if (idx == 13){ // V34: Real AI Object Removal (LaMa / OpenCV Inpainting on painted brush dots!)
                        if (Clip* s = m_engine.selectedClip()) {
                            if (s->color.brushDots.empty()) {
                                m_photoTool = PhotoTool::ContentFill;
                                m_drawMode = false;
                                showToast(tr(L"Paint over the unwanted object on canvas, then click Remove Object again for AI Inpainting!",
                                             L"ارسم بالفرشاة فوق العنصر المراد إزالته على الشاشة، ثم اضغط إزالة العناصر مرة أخرى للملء الذكي (AI Inpainting)!"));
                            } else {
                                std::string src = s->sourcePath; double sIn = s->sourceIn; MediaType mtype = s->type;
                                std::ostringstream oss;
                                for (const auto& d : s->color.brushDots) oss << d.nx << "," << d.ny << "," << d.radius << ";";
                                std::string maskArg = oss.str();
                                std::string outPng = "CapCutVS_AI/inpaint_" + std::to_string(m_aiGenCounter++) + ".png";
                                runAiTaskAsync(tr(L"Running AI Inpainting (LaMa / OpenCV)...", L"جاري إزالة العنصر وملء الخلفية (AI Inpainting)..."), [this, src, sIn, mtype, maskArg, outPng]() {
                                    std::string inImg = src;
                                    if (mtype == MediaType::Video) {
                                        FrameBGRA f;
                                        if (m_media.decodeFrame(src, MediaType::Video, sIn, f) && f.valid()) {
                                            inImg = "CapCutVS_AI/tmp_inp_in.png";
                                            m_media.saveFrameToPng(f, inImg);
                                        }
                                    }
                                    std::string log;
                                    bool ok = AICloudBridge::runNeuralPluginTask("inpaint", inImg, outPng, maskArg, log);
                                    postUiTask([this, ok, outPng]() {
                                        if (ok) { importFilePath(outPng); showToast(tr(L"AI Inpainting complete! Added cleaned image.", L"اكتمل الملء الذكي (AI Inpainting) وتمت إضافة النتيجة!")); }
                                        else showToast(tr(L"Real-time C++ Inpainting active on canvas (Run setup_plugins.bat for LaMa/OpenCV)", L"الملء الذكي المدمج مطبق مباشرة على الشاشة (شغّل setup_plugins.bat لتفعيل LaMa/OpenCV)"));
                                    });
                                });
                            }
                        } else showToast(tr(L"Select an image or video clip first.", L"اختر صورة أو مقطع فيديو أولاً."));
                    } else if (idx == 14){ // V34: Real Face-Detected AI Retouch (OpenCV Face ROI + C++ fallback)
                        if (Clip* s = m_engine.selectedClip()) {
                            uint64_t cid = s->id; std::string src = s->sourcePath; double sIn = s->sourceIn; MediaType mtype = s->type;
                            std::string outPng = "CapCutVS_AI/retouch_" + std::to_string(m_aiGenCounter++) + ".png";
                            runAiTaskAsync(tr(L"Running Face-Detected AI Retouch...", L"جاري اكتشاف الوجه وتحسين البشرة والملامح..."), [this, cid, src, sIn, mtype, outPng]() {
                                std::string inImg = src;
                                if (mtype == MediaType::Video) {
                                    FrameBGRA f;
                                    if (m_media.decodeFrame(src, MediaType::Video, sIn, f) && f.valid()) {
                                        inImg = "CapCutVS_AI/tmp_ret_in.png";
                                        m_media.saveFrameToPng(f, inImg);
                                    }
                                }
                                std::string log;
                                bool ok = AICloudBridge::runNeuralPluginTask("face_landmarks", inImg, outPng, "", log);
                                postUiTask([this, cid, ok, outPng]() {
                                    if (ok) { importFilePath(outPng); showToast(tr(L"Face-Detected AI Retouch added!", L"تم اكتشاف الوجه وتطبيق التحسين الموضعي وإضافته!")); }
                                    else if (Clip* c = m_engine.findClip(cid)) {
                                        c->color.faceRetouch = 0.65f; c->color.skinSmooth = 0.55f; c->color.teethWhiten = 0.40f;
                                        showToast(tr(L"Portrait Retouch applied (Run setup_plugins.bat for OpenCV Face Detection)", L"تم تطبيق تحسين البشرة (شغّل setup_plugins.bat لتفعيل اكتشاف الوجه بـ OpenCV)"));
                                    }
                                });
                            });
                        }
                    } else if (idx == 15) { // Smart Color Mood (Async)
                        if (Clip* s = m_engine.selectedClip()) {
                            uint64_t cid = s->id;
                            ColorAdjust curCol = s->color;
                            std::wstring mood = tr(L"Warm Sunset Cinema", L"غروب سينمائي دافئ");
                            if (promptTextDialog(tr(L"AI Smart Color Grade (Describe Mood/Vibe)", L"التلوين الذكي بالذكاء الاصطناعي — اكتب طابع الألوان (غروب سينمائي، ليلي نيون، وثائقي بارد)"), mood) && !mood.empty()) {
                                auto cfg = m_aiConfig; std::string mUtf8 = wideToUtf8(mood);
                                runAiTaskAsync(tr(L"AI Color Grading in background...", L"جاري تحليل وتطبيق التلوين الذكي في الخلفية..."), [this, cfg, mUtf8, cid, curCol]() mutable {
                                    std::string sum;
                                    AICloudBridge::applySmartColorMood(cfg, mUtf8, curCol, sum);
                                    postUiTask([this, cid, curCol, sum]() {
                                        if (Clip* cl = m_engine.findClip(cid)) {
                                            m_engine.pushUndoSnapshot();
                                            cl->color = curCol;
                                            m_inspTab = InspectorTab::Color;
                                            showToast(utf8ToWide(sum));
                                        }
                                    });
                                });
                            }
                        } else showToast(tr(L"Select a video or image clip first.", L"اختر مقطع فيديو أو صورة أولاً."));
                    } else if (idx == 16) { // Smart Audio Master (Async)
                        if (Clip* s = m_engine.selectedClip()) {
                            uint64_t cid = s->id;
                            AudioDspSettings curDsp = s->audioDsp;
                            std::wstring goal = tr(L"Clear Podcast Studio Voice", L"بودكاست واضح ونقي");
                            if (promptTextDialog(tr(L"AI Audio Mastering (Podcast, Cinema, Clean Voice)", L"المكساج الصوتي الذكي — اكتب الهدف (بودكاست واضح، صوت سينمائي، إزالة ضوضاء)"), goal) && !goal.empty()) {
                                auto cfg = m_aiConfig; std::string gUtf8 = wideToUtf8(goal);
                                runAiTaskAsync(tr(L"AI Audio Mastering in background...", L"جاري ضبط المكساج الصوتي الذكي في الخلفية..."), [this, cfg, gUtf8, cid, curDsp]() mutable {
                                    std::string sum;
                                    AICloudBridge::applySmartAudioGoal(cfg, gUtf8, curDsp, sum);
                                    postUiTask([this, cid, curDsp, sum]() {
                                        if (Clip* cl = m_engine.findClip(cid)) {
                                            m_engine.pushUndoSnapshot();
                                            cl->audioDsp = curDsp;
                                            m_audio.updateProjectSnapshot(m_engine.project());
                                            m_inspTab = InspectorTab::Audio;
                                            showToast(utf8ToWide(sum));
                                        }
                                    });
                                });
                            }
                        } else showToast(tr(L"Select an audio or video clip first.", L"اختر مقطع صوت أو فيديو أولاً."));
                    } else if (idx == 17) { // Viral Pack (Async)
                        std::wstring topic = tr(L"Cinematic Travel Vlog", L"فيديو سينمائي احترافي");
                        if (promptTextDialog(tr(L"AI Viral Pack — Enter Video Topic", L"توليد العناوين والهاشتاغات — اكتب موضوع الفيديو"), topic) && !topic.empty()) {
                            auto cfg = m_aiConfig; std::string tUtf8 = wideToUtf8(topic);
                            runAiTaskAsync(tr(L"Generating Viral Pack via LM Studio...", L"جاري توليد العناوين والهاشتاغات في الخلفية..."), [this, cfg, tUtf8]() {
                                std::string hook = AICloudBridge::generateViralPack(cfg, tUtf8, "CapCutVS_AI_Pack.txt");
                                postUiTask([this, hook]() {
                                    m_engine.addTextClip(hook, m_engine.playhead(), 3.5);
                                    showToast(tr(L"Added Viral Hook & saved pack to CapCutVS_AI_Pack.txt!", L"تمت إضافة الجملة الجاذبة وحفظ العناوين والهاشتاغات في CapCutVS_AI_Pack.txt!"));
                                });
                            });
                        }
                    } else if (idx == 18) { // AI Director (Async)
                        auto cfg = m_aiConfig;
                        int cnt = static_cast<int>(m_engine.project().clips.size());
                        double dur = m_engine.totalDuration();
                        runAiTaskAsync(tr(L"AI Director analyzing timeline...", L"المخرج الذكي يحلل مشروعك في الخلفية..."), [this, cfg, cnt, dur]() {
                            AICloudBridge::generateDirectorAdvice(cfg, cnt, dur, "CapCutVS_AI_Director.txt");
                            postUiTask([this]() {
                                showToast(tr(L"AI Director tips saved to CapCutVS_AI_Director.txt!", L"تم حفظ نصائح المخرج الذكي في CapCutVS_AI_Director.txt!"));
                            });
                        });
                    } else if (idx == 19) { // Combined Script + Voiceover (Async)
                        std::wstring topic = tr(L"Welcome to my channel", L"مرحباً بكم في هذا الفيديو الاحترافي");
                        if (promptTextDialog(tr(L"AI Script + Voiceover (LM Studio) — Topic", L"كتابة سكربت + توليد تعليق صوتي تلقائي — اكتب الموضوع"), topic) && !topic.empty()) {
                            auto cfg = m_aiConfig; std::string tUtf8 = wideToUtf8(topic);
                            std::string wav = "ai_voice_2b_" + std::to_string(m_aiGenCounter++) + ".wav";
                            runAiTaskAsync(tr(L"Generating Script + Voiceover in background...", L"جاري توليد السكربت والتعليق الصوتي في الخلفية..."), [this, cfg, tUtf8, wav]() {
                                std::string note;
                                auto lines = AICloudBridge::generateScriptLinesLocal(cfg, tUtf8, note);
                                std::string fullSpoken;
                                for (const auto& ln : lines) fullSpoken += ln + ". ";
                                bool hasWav = AudioEngine::synthesizeTextToSpeechWav(fullSpoken, wav);
                                postUiTask([this, lines, hasWav, wav]() {
                                    double t = m_engine.playhead();
                                    for (const auto& ln : lines) { m_engine.addTextClip(ln, t, 3.0); t += 3.0; }
                                    if (hasWav) importFilePath(wav);
                                    showToast(tr(L"Generated AI Script + Voiceover Track!", L"تم توليد السكربت مع التعليق الصوتي وإضافتهما للتايملاين!"));
                                });
                            });
                        }
                    }
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                }
                case BTN_LANG_TOGGLE:     m_arabicUi = !m_arabicUi; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_FREEZE_FRAME:    actionFreezeFrame(); return;
                case BTN_EXTRACT_AUDIO:   actionExtractAudio(); return;
                case BTN_CYCLE_TEXT_ANIM: actionCycleTextAnim(); return;
                case BTN_FLIP_H:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->transform.flipH = !sel->transform.flipH;
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_FLIP_V:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->transform.flipV = !sel->transform.flipV;
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_ROT_90:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->transform.rotationDeg += 90.0f;
                        if (sel->transform.rotationDeg >= 360.0f) sel->transform.rotationDeg -= 360.0f;
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_FILL_SCREEN:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->transform.posX = 0.0f;
                        sel->transform.posY = 0.0f;
                        sel->transform.scale = (sel->transform.scale < 1.5f) ? 1.78f : 1.0f;
                        showToast(tr(L"Toggled Fill Screen / Fit!", L"تم تبديل ملء الشاشة / الاحتواء!"));
                    }
                    return;
                case BTN_AUTO_ENHANCE:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->color.brightness = 0.04f;
                        sel->color.contrast = 1.16f;
                        sel->color.saturation = 1.24f;
                        sel->color.preset = FilterPreset::Vivid;
                        showToast(tr(L"Magic Auto-Enhance applied!", L"تم تطبيق التحسين اللوني التلقائي!"));
                    }
                    return;
                case BTN_SPEED_PRESET:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->speed = static_cast<double>(it->param) / 100.0;
                        m_audio.updateProjectSnapshot(m_engine.project());
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_TRIM_LEFT:
                    if (m_engine.selectedClipId() != 0 && m_engine.trimBeforePlayhead(m_engine.selectedClipId())) {
                        showToast(tr(L"Trimmed clip start to playhead!", L"تم قص ما قبل المؤشر!"));
                    } else {
                        showToast(tr(L"Move playhead inside the selected clip first.", L"ضع المؤشر داخل المقطع المحدد أولًا."));
                    }
                    return;
                case BTN_THEME_TOGGLE:  m_lightMode = !m_lightMode; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_BEFORE_AFTER:  m_beforeAfter = !m_beforeAfter; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_VOICE_REC:     actionToggleVoiceRec(); return;
                case BTN_EXTRACT_PNG:   actionExtractPngFrame(); return;
                case BTN_FPS_CYCLE: {
                    m_fpsPreset = (m_fpsPreset + 1) % 3;
                    const double fpsVals[3] = { 24.0, 30.0, 60.0 };
                    m_engine.project().fps = fpsVals[m_fpsPreset];
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                }
                case BTN_FULLSCREEN:    actionToggleFullScreen(); return;
                case BTN_INSP_BASIC:    m_inspTab = InspectorTab::Basic; m_inspScrollY = 0; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_INSP_COLOR:    m_inspTab = InspectorTab::Color; m_inspScrollY = 0; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_INSP_AUDIO:    m_inspTab = InspectorTab::Audio; m_inspScrollY = 0; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_INSP_FX:       m_inspTab = InspectorTab::FX; m_inspScrollY = 0; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_TOGGLE_REVERSE:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->reversed = !sel->reversed;
                        m_audio.updateProjectSnapshot(m_engine.project());
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_CYCLE_MASK:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->mask = static_cast<MaskType>((static_cast<int>(sel->mask) + 1) % 8);
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_LOAD_LUT:       actionLoadCubeLut(); return;
                case BTN_EXPORT_LUT:     actionExportCubeLut(); return;
                case BTN_AUTO_WB:        actionAutoWhiteBalance(); return;
                case BTN_FALSE_COLOR:
                    if (Clip* sc = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sc->color.falseColor = !sc->color.falseColor;
                        showToast(sc->color.falseColor ? tr(L"False Color IRE Exposure Overlay: ON", L"كشف مستويات التعريض (False Color): مفعل")
                                                       : tr(L"False Color Overlay: OFF", L"تم إيقاف كشف التعريض"));
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_CYCLE_SCOPES:
                    m_scopeMode = (m_scopeMode + 1) % 5;
                    if (m_scopeMode > 0 && m_gradedFrame.valid()) updateVideoScopes(m_gradedFrame);
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_CYCLE_BLEND:
                    if (Clip* sc = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sc->blendMode = static_cast<BlendMode>((static_cast<int>(sc->blendMode) + 1) % 10);
                        showToast(std::wstring(tr(L"Compositing Blend Mode: ", L"وضع دمج الطبقات: ")) + utf8ToWide(blendModeName(sc->blendMode)));
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_REMOVE_SILENCE:
                    if (Clip* sc = m_engine.selectedClip()) {
                        double tIn = 0.0, tOut = sc->duration;
                        if (m_audio.detectNonSilentRange(sc->sourcePath, sc->duration, tIn, tOut)) {
                            m_engine.pushUndoSnapshot();
                            sc->sourceIn += tIn;
                            sc->duration = std::max(0.2, tOut - tIn);
                            m_audio.updateProjectSnapshot(m_engine.project());
                            showToast(tr(L"Silence trimmed automatically!", L"تم قص الصمت تلقائياً من المقطع!"));
                            InvalidateRect(m_hwnd, nullptr, FALSE);
                        } else {
                            showToast(tr(L"No leading/trailing silence detected (or audio still loading).", L"لم يتم رصد صمت قابل للقص في أطراف المقطع."));
                        }
                    }
                    return;
                case BTN_AUDIO_RESTORE:
                    if (Clip* sc = m_engine.selectedClip()) {
                        uint64_t cid = sc->id; std::string src = sc->sourcePath;
                        std::string outWav = "CapCutVS_AI/restored_" + std::to_string(m_aiGenCounter++) + ".wav";
                        runAiTaskAsync(tr(L"Running Neural Audio Denoise & Restoration...", L"جاري ترميم الصوت وإزالة الضوضاء الطيفية في الخلفية..."), [this, cid, src, outWav]() {
                            std::string log;
                            bool ok = (!src.empty() && AICloudBridge::runNeuralPluginTask("denoise", src, outWav, "", log));
                            postUiTask([this, cid, ok, outWav]() {
                                if (ok) {
                                    importFilePath(outWav);
                                    showToast(tr(L"Neural Audio Restoration (noisereduce) added to Timeline!", L"تم ترميم الصوت وإزالة الضوضاء بالشبكة العصبية وإضافته!"));
                                } else if (Clip* c = m_engine.findClip(cid)) {
                                    m_engine.pushUndoSnapshot();
                                    c->audioDsp.noiseReduction = 0.65f; c->audioDsp.deEsser = 0.55f;
                                    c->audioDsp.compressor = 0.45f; c->audioDsp.limiter = 0.92f; c->audioDsp.vocalIsolate = true;
                                    m_audio.updateProjectSnapshot(m_engine.project());
                                    showToast(tr(L"Audio Restoration DSP applied (Run setup_plugins.bat for neural denoise)", L"تم تطبيق ترميم الصوت (شغّل setup_plugins.bat لإزالة الضوضاء الطيفية العصبية)"));
                                }
                            });
                        });
                    }
                    return;
                case BTN_TOGGLE_SPECTRUM:
                    m_showAudioSpectrum = !m_showAudioSpectrum;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_CYCLE_PHOTO_TOOL:
                    m_photoTool = static_cast<PhotoTool>((static_cast<int>(m_photoTool) + 1) % 11);
                    if (m_photoTool != PhotoTool::None) m_drawMode = false;
                    showToast(std::wstring(tr(L"Photo Tool: ", L"أداة تعديل الصور: ")) + utf8ToWide(photoToolName(m_photoTool)));
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_CLEAR_BRUSHES:
                    if (Clip* sc = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sc->color.brushDots.clear();
                        showToast(tr(L"Cleared all photo retouch brush strokes.", L"تم مسح ضربات فرشاة التعديل."));
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_CLIPPING_MASK:
                    if (Clip* sc = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sc->clippingMask = !sc->clippingMask;
                        showToast(sc->clippingMask ? tr(L"Clipping Mask: ON (clipped to underlying layer)", L"قناع القص: مفعل على الطبقة السفلية")
                                                   : tr(L"Clipping Mask: OFF", L"قناع القص: متوقف"));
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_BATCH_EDIT:   actionBatchEdit(); return;
                case BTN_BATCH_EXPORT: actionBatchExport(); return;
                case BTN_RECOVER_DRAFT: actionRecoverDraft(); return;
                case BTN_SAVE_VERSION:  actionSaveVersionSnapshot(); return;
                case BTN_CUSTOM_PRESET: actionCustomPreset(m_hasCustomPreset); return;
                case BTN_TOGGLE_OFFLINE:
                    m_offlineMode = !m_offlineMode;
                    showToast(m_offlineMode ? tr(L"Offline Mode: ON (100% local, zero internet used)", L"وضع العمل دون إنترنت: مفعل (عمل محلي بالكامل 100%)")
                                            : tr(L"Online AI Ready (Cloud AI tools enabled)", L"الوضع السحابي جاهز (أدوات الذكاء الاصطناعي عبر الإنترنت مفعلة)"));
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_PREVIEW_QUAL:
                    m_previewQuality = (m_previewQuality + 1) % 3;
                    showToast(m_previewQuality == 0 ? tr(L"Preview Quality: 1/4 Ultra-Fast", L"جودة المعاينة: ربع (أسرع أداء)")
                            : (m_previewQuality == 1 ? tr(L"Preview Quality: 1/2 Proxy Mode", L"جودة المعاينة: وضع البروكسي المتوازن")
                                                     : tr(L"Preview Quality: Full HQ", L"جودة المعاينة: كاملة عالية الدقة")));
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_SEARCH_ASSETS: actionSearchAssets(); return;
                case BTN_TOGGLE_FAV:
                    if (Clip* sc = m_engine.selectedClip()) {
                        int fxId = static_cast<int>(sc->effect.type);
                        auto itF = std::find(m_favEffects.begin(), m_favEffects.end(), fxId);
                        if (itF == m_favEffects.end()) { m_favEffects.push_back(fxId); showToast(tr(L"Added effect to Favorites ★", L"تمت إضافة المؤثر للمفضلة ★")); }
                        else { m_favEffects.erase(itF); showToast(tr(L"Removed effect from Favorites", L"تمت إزالة المؤثر من المفضلة")); }
                    }
                    return;
                case BTN_CLOUD_SYNC: actionCloudSyncBackup(); return;
                case BTN_TOOLS_PAGE:
                    m_allToolsPage = static_cast<int>(it->param) % 2;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_ADD_MARKER:
                    m_engine.project().markers.push_back(m_engine.playhead());
                    showToast(tr(L"Added Timeline Marker (M)!", L"تمت إضافة علامة على الخط الزمني!"));
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_SHOW_SHORTCUTS:
                    showToast(tr(L"Shortcuts: Space=Play | S=Split | M=Marker | Ctrl+Z/Y=Undo/Redo | Ctrl+F=Search | Del=Delete", L"الاختصارات: Space=تشغيل | S=قص | M=علامة | Ctrl+Z/Y=تراجع | Ctrl+F=بحث | Del=حذف"));
                    return;
                case BTN_COPY_FX:        actionCopyEffects(); return;
                case BTN_PASTE_FX:       actionPasteEffects(); return;
                case BTN_DETECT_BEATS:   actionDetectBeats(); return;
                case BTN_ALL_TOOLS:      m_showAllToolsModal = !m_showAllToolsModal; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_DRAW_MODE:
                    m_drawMode = !m_drawMode;
                    showToast(m_drawMode ? tr(L"Drawing Mode ON! Drag on Preview Canvas to draw.", L"وضع الرسم مفعل! اسحب بالماوس على شاشة المعاينة للرسم.")
                                         : tr(L"Drawing Mode OFF.", L"تم إيقاف وضع الرسم."));
                    return;
                case BTN_CLEAR_DRAW:
                    m_engine.project().drawings.clear();
                    showToast(tr(L"Cleared all drawings.", L"تم مسح الرسومات."));
                    return;
                case BTN_SFX_ADD: {
                    const char* names[9] = { "sfx_whoosh.wav", "sfx_pop.wav", "sfx_click.wav", "sfx_beep.wav", "sfx_riser.wav", "sfx_drop.wav", "sfx_ding.wav", "sfx_laser.wav", "sfx_clap.wav" };
                    int idx = static_cast<int>(it->param) % 9;
                    if (AudioEngine::generateSfxWav(idx, names[idx])) {
                        importFilePath(names[idx]);
                        showToast(tr(L"Added Sound Effect to Timeline!", L"تمت إضافة المؤثر الصوتي إلى التايملاين!"));
                    }
                    return;
                }
                case BTN_TEMPLATE_ADD: {
                    int ti = static_cast<int>(it->param) % 3;
                    if (ti == 0) {
                        uint64_t t1 = m_engine.addTextClip("WELCOME TO MY CHANNEL", 0.0, 4.0);
                        if (Clip* c = m_engine.findClip(t1)) { c->text.anim = TextAnimType::BouncePop; c->text.colorARGB = 0xFF00E5D1; }
                        uint64_t t2 = m_engine.addTextClip("SUBSCRIBE", 1.5, 3.0);
                        if (Clip* c = m_engine.findClip(t2)) { c->text.isStickerBadge = true; c->text.bgARGB = 0xFFE5253A; c->transform.posY = 0.65f; c->text.anim = TextAnimType::Pulse; }
                    } else if (ti == 1) {
                        m_aspectMode = 1; computeLayout();
                        uint64_t t1 = m_engine.addTextClip("WAIT FOR IT...", 0.0, 3.0);
                        if (Clip* c = m_engine.findClip(t1)) { c->text.anim = TextAnimType::Typewriter; c->text.colorARGB = 0xFFFFD84D; }
                    } else {
                        uint64_t t1 = m_engine.addTextClip("A CINEMATIC FILM", 0.0, 4.5);
                        if (Clip* c = m_engine.findClip(t1)) { c->text.anim = TextAnimType::FadeRise; c->text.fontSize = 46.0f; }
                        if (Clip* sel = m_engine.selectedClip()) { sel->mask = MaskType::CinemaBars; sel->color.preset = FilterPreset::Cinematic; }
                    }
                    showToast(tr(L"Applied Starter Template!", L"تم تطبيق القالب الجاهز!"));
                    return;
                }
                case BTN_CYCLE_RAMP:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->speedRamp = static_cast<SpeedRampType>((static_cast<int>(sel->speedRamp) + 1) % 5);
                        showToast(std::wstring(tr(L"Speed Ramp: ", L"تسريع تدريجي: ")) + utf8ToWide(speedRampName(sel->speedRamp)));
                    }
                    return;
                case BTN_CYCLE_FRAME:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->frame = static_cast<FrameStyle>((static_cast<int>(sel->frame) + 1) % 7);
                        showToast(std::wstring(tr(L"Frame: ", L"إطار: ")) + utf8ToWide(frameStyleName(sel->frame)));
                    }
                    return;
                case BTN_CYCLE_CURVE:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->color.curve = static_cast<CurvePreset>((static_cast<int>(sel->color.curve) + 1) % 5);
                        showToast(std::wstring(tr(L"Tone Curve: ", L"منحنى الألوان: ")) + utf8ToWide(curvePresetName(sel->color.curve)));
                    }
                    return;
                case BTN_TOGGLE_BGBLUR:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->bgBlur = !sel->bgBlur;
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_TOGGLE_REDEYE:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->color.redEyeRemove = !sel->color.redEyeRemove;
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_TOGGLE_VOCAL:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->audioDsp.vocalIsolate = !sel->audioDsp.vocalIsolate;
                        m_audio.updateProjectSnapshot(m_engine.project());
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_VOICE_PRESET:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        if (sel->audioDsp.pitch < 0.85f) { sel->audioDsp.pitch = 1.42f; sel->audioDsp.echo = 0.0f; showToast(L"Voice: Chipmunk"); }
                        else if (sel->audioDsp.pitch > 1.25f) { sel->audioDsp.pitch = 1.0f; sel->audioDsp.echo = 0.65f; showToast(L"Voice: Hall Echo"); }
                        else if (sel->audioDsp.echo > 0.3f) { sel->audioDsp.pitch = 1.0f; sel->audioDsp.echo = 0.0f; showToast(L"Voice: Normal"); }
                        else { sel->audioDsp.pitch = 0.72f; sel->audioDsp.bassBoost = 0.6f; showToast(L"Voice: Deep Robot"); }
                        m_audio.updateProjectSnapshot(m_engine.project());
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_TOOL_JUMP: {
                    m_showAllToolsModal = false;
                    int code = static_cast<int>(it->param);

                    // Auto-select first clip on timeline if none is selected so Inspector tools appear immediately!
                    auto ensureClipSelected = [&]() -> Clip* {
                        if (Clip* s = m_engine.selectedClip()) return s;
                        if (!m_engine.project().clips.empty()) {
                            m_engine.selectClip(m_engine.project().clips.front().id);
                            return m_engine.selectedClip();
                        }
                        showToast(tr(L"Import a video or photo first, then use this tool!", L"استورد فيديو أو صورة أولًا ثم استخدم هذه الأداة!"));
                        return nullptr;
                    };

                    if (code < 100) {
                        // Column header clicked
                        if (code == 0) { ensureClipSelected(); m_inspTab = InspectorTab::Color; m_inspScrollY = 0; }
                        else if (code == 1) { ensureClipSelected(); m_inspTab = InspectorTab::Basic; m_inspScrollY = 0; }
                        else if (code == 2) { ensureClipSelected(); m_inspTab = InspectorTab::Audio; m_inspScrollY = 0; }
                        else if (code == 9) { m_leftTab = LeftTab::Text; }
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                        return;
                    }

                    if (code >= 300) {
                        int p2c = (code - 300) / 20;
                        int p2i = (code - 300) % 20;
                        if (p2c == 0) {
                            ensureClipSelected(); m_inspTab = InspectorTab::Color; m_inspScrollY = 0;
                            if (p2i <= 6) { m_photoTool = static_cast<PhotoTool>(std::min(10, p2i + 1)); showToast(std::wstring(tr(L"Activated Brush: ", L"تم تفعيل الفرشاة: ")) + utf8ToWide(photoToolName(m_photoTool))); }
                            else if (p2i == 10) { actionImportMedia(false); }
                            else if (p2i == 11) { actionBatchExport(); }
                        } else if (p2c == 1) {
                            m_leftTab = (p2i == 8) ? LeftTab::Filters : (p2i == 9 ? LeftTab::Transitions : (p2i == 7 ? LeftTab::Stickers : LeftTab::Text));
                        } else if (p2c == 2) {
                            m_showExportModal = true;
                        } else {
                            if (p2i == 1) actionRecoverDraft();
                            else if (p2i == 2) actionSaveVersionSnapshot();
                            else if (p2i == 5) actionCloudSyncBackup();
                            else if (p2i == 6) actionSearchAssets();
                            else if (p2i == 7) { m_engine.project().markers.push_back(m_engine.playhead()); showToast(tr(L"Marker added!", L"تمت إضافة علامة!")); }
                            else if (p2i == 10) { m_leftTab = LeftTab::AI; }
                            else if (p2i == 11) { showToast(tr(L"Shortcuts: Space=Play | S=Split | M=Marker | Ctrl+Z/Y=Undo/Redo | Ctrl+F=Search | Del=Delete", L"الاختصارات: Space=تشغيل | S=قص | M=علامة | Ctrl+Z/Y=تراجع | Ctrl+F=بحث | Del=حذف")); }
                        }
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                        return;
                    }
                    if (code == 6)  { ensureClipSelected(); m_leftTab = LeftTab::Effects; m_inspTab = InspectorTab::FX; InvalidateRect(m_hwnd, nullptr, FALSE); return; }
                    if (code == 10) { ensureClipSelected(); m_inspTab = InspectorTab::Color; InvalidateRect(m_hwnd, nullptr, FALSE); return; }
                    if (code == 11) { m_showExportModal = true; InvalidateRect(m_hwnd, nullptr, FALSE); return; }
                    if (code == 12) { m_leftTab = LeftTab::AI; InvalidateRect(m_hwnd, nullptr, FALSE); return; }
                    int ci = (code - 100) / 20;
                    int li = (code - 100) % 20;

                    if (ci == 0) { // 1. Cinema NLE & Timeline
                        ensureClipSelected();
                        m_inspTab = InspectorTab::FX; m_inspScrollY = 0;
                        if (li == 4) actionSplitSelected();
                        else if (li == 5) { m_previewQuality = (m_previewQuality + 1) % 3; showToast(tr(L"Switched Proxy Mode!", L"تم تبديل وضع الأداء السريع (Proxy)!")); }
                        else if (li == 11) actionDetectBeats();
                        else showToast(tr(L"Opened Cinema NLE Tools in FX Inspector", L"تم فتح أدوات المونتاج السينمائي في تبويب المؤثرات (FX)"));
                    } else if (ci == 1) { // 2. Pro Color & Live Scopes
                        Clip* c = ensureClipSelected();
                        m_inspTab = InspectorTab::Color; m_inspScrollY = 0;
                        if (li == 0) actionAutoWhiteBalance();
                        else if (li == 8) actionLoadCubeLut();
                        else if (li == 9) { m_scopeMode = (m_scopeMode == 1 ? 2 : 1); if (m_gradedFrame.valid()) updateVideoScopes(m_gradedFrame); showToast(tr(L"Live Histogram / Waveform ON!", L"تم تفعيل مراقب الإضاءة والهستوغرام الحي!")); }
                        else if (li == 10) { m_scopeMode = 3; if (m_gradedFrame.valid()) updateVideoScopes(m_gradedFrame); showToast(tr(L"Live Vectorscope ON!", L"تم تفعيل مراقب الألوان الحي (Vectorscope)!")); }
                        else if (li == 11 && c) { m_engine.pushUndoSnapshot(); c->color.falseColor = !c->color.falseColor; showToast(tr(L"Toggled False Color Exposure!", L"تم تبديل كشف مستويات التعريض (False Color)!")); }
                        else { m_inspScrollY = static_cast<float>(li * 65); showToast(tr(L"Opened Pro Color Grading in Color Tab", L"تم فتح أدوات التلوين السينمائي في تبويب الألوان")); }
                    } else if (ci == 2) { // 3. VFX, Compositing & 3D
                        ensureClipSelected();
                        m_leftTab = LeftTab::Effects; m_inspTab = InspectorTab::FX; m_leftScrollY = 0;
                        showToast(tr(L"Opened 27 VFX, Compositing & 3D Tools!", L"تم فتح المؤثرات البصرية الـ 27 والدمج والـ 3D!"));
                    } else if (ci == 3) { // 4. Pro Audio DAW & DSP
                        Clip* c = ensureClipSelected();
                        m_inspTab = InspectorTab::Audio; m_inspScrollY = 0;
                        if (li == 8) { m_showAudioSpectrum = !m_showAudioSpectrum; showToast(tr(L"Toggled Live 16-Band Audio Spectrum!", L"تم تبديل طيف الصوت الحي (16 نطاق)!")); }
                        else if (li == 10 && c) {
                            m_engine.pushUndoSnapshot();
                            c->audioDsp.noiseReduction = 0.65f; c->audioDsp.deEsser = 0.55f;
                            c->audioDsp.compressor = 0.45f; c->audioDsp.limiter = 0.92f; c->audioDsp.vocalIsolate = true;
                            m_audio.updateProjectSnapshot(m_engine.project());
                            showToast(tr(L"Audio Restoration Applied!", L"تم تطبيق ترميم الصوت الاحترافي بضغطة واحدة!"));
                        } else if (li == 11) { actionToggleVoiceRec(); }
                        else showToast(tr(L"Opened Pro Audio DAW Controls in Audio Tab", L"تم فتح أدوات الهندسة الصوتية في تبويب الصوت"));
                    }
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                }
                case BTN_TOGGLE_DUCKING:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->audioDsp.autoDucking = !sel->audioDsp.autoDucking;
                        m_audio.updateProjectSnapshot(m_engine.project());
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_TRIM_RIGHT:
                    if (m_engine.selectedClipId() != 0 && m_engine.trimAfterPlayhead(m_engine.selectedClipId())) {
                        showToast(tr(L"Trimmed clip end to playhead!", L"تم قص ما بعد المؤشر!"));
                    } else {
                        showToast(tr(L"Move playhead inside the selected clip first.", L"ضع المؤشر داخل المقطع المحدد أولًا."));
                    }
                    return;
                case BTN_STICKER_PRESET: {
                    const char* stTexts[12] = { "SUBSCRIBE", "LIKE", "LIVE", "VIP", "FIRE", "100%", "REC", "NEW!", "TOP", "SALE", "PRO", "LOVE" };
                    const uint32_t stBgs[12] = { 0xFFE5253A, 0xFF1877F2, 0xFFE5253A, 0xFFD49B00, 0xFFFF5722, 0xFF00B894, 0xFFD63031, 0xFF6C5CE7, 0xFFE17055, 0xFF00CEC9, 0xFF0984E3, 0xFFE84393 };
                    int idx = static_cast<int>(it->param) % 12;
                    uint64_t cid = m_engine.addTextClip(stTexts[idx], m_engine.playhead(), 3.5);
                    if (Clip* sc = m_engine.findClip(cid)) {
                        sc->text.isStickerBadge = true;
                        sc->text.bgARGB = stBgs[idx];
                        sc->text.colorARGB = 0xFFFFFFFF;
                        sc->text.fontSize = 42.0f;
                        sc->text.anim = TextAnimType::BouncePop;
                        sc->transform.posY = -0.55f;
                    }
                    showToast(tr(L"Added animated sticker on T1!", L"تمت إضافة ملصق متحرك على مسار T1!"));
                    return;
                }
                case BTN_IMPORT_MEDIA:    actionImportMedia(false); return;
                case BTN_IMPORT_AUDIO:    actionImportMedia(true); return;
                case BTN_IMPORT_SRT:      actionImportSrt(); return;
                case BTN_AUTO_CAPTIONS:   actionAutoCaptions(); return;
                case BTN_ASSET_ADD: {
                    const MediaAsset* a = m_engine.findAsset(it->param);
                    if (a) {
                        int tr = m_engine.defaultTrackForMedia(a->type);
                        m_engine.addClipFromAsset(a->id, tr, m_engine.playhead());
                        showToast(L"Added to Timeline: " + utf8ToWide(a->name));
                    }
                    return;
                }
                case BTN_TEXT_PRESET: {
                    const char* samples[4] = { "NovaCut Title", "CINEMATIC STORY", "CYBERPUNK 2026", "Subtitle Caption" };
                    int idx = static_cast<int>(it->param) % 4;
                    uint64_t cid = m_engine.addTextClip(samples[idx], m_engine.playhead(), 4.0);
                    if (Clip* tc = m_engine.findClip(cid)) {
                        if (idx == 2) tc->text.colorARGB = 0xFF00E5D1;
                        if (idx == 3) { tc->transform.posY = 0.72f; tc->text.fontSize = 36.0f; }
                    }
                    showToast(L"Added Text Clip on T1");
                    return;
                }
                case BTN_FILTER_CAT: m_filterCategory = static_cast<int>(it->param); m_leftScrollY = 0; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_FILTER_PRESET: {
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->color.preset = static_cast<FilterPreset>(it->param);
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    } else showToast(L"Select a video or image clip first.");
                    return;
                }
                case BTN_FX_CAT: m_fxCategory = static_cast<int>(it->param); m_leftScrollY = 0; InvalidateRect(m_hwnd, nullptr, FALSE); return;
                case BTN_EFFECT_PRESET: {
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->effect.type = static_cast<EffectType>(it->param);
                        showToast(L"Effect applied: " + utf8ToWide(effectName(sel->effect.type)));
                    } else showToast(L"Select a video or image clip first.");
                    return;
                }
                case BTN_TRANS_PRESET: {
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->transitionIn = static_cast<TransitionType>(it->param);
                        showToast(L"Transition applied: " + utf8ToWide(transitionName(sel->transitionIn)));
                    } else showToast(L"Select a clip on the timeline first.");
                    return;
                }
                case BTN_TOGGLE_CHROMA: {
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->chromaKey.enabled = !sel->chromaKey.enabled;
                        showToast(sel->chromaKey.enabled ? L"Chroma Key Enabled! Put clip on V2 over a V1 background." : L"Chroma Key Disabled.");
                    }
                    return;
                }
                case BTN_CHROMA_COLOR: {
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->chromaKey.keyColorRGB = static_cast<uint32_t>(it->param);
                        sel->chromaKey.enabled = true;
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                }
                case BTN_PLAY_PAUSE:  setPlaying(!m_playing); return;
                case BTN_JUMP_START:
                    setPlaying(false);
                    m_engine.setPlayhead(0.0);
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_STEP_BACK:
                    setPlaying(false);
                    m_engine.setPlayhead(std::max(0.0, m_engine.playhead() - 1.0 / 30.0));
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_STEP_FWD:
                    setPlaying(false);
                    m_engine.setPlayhead(m_engine.playhead() + 1.0 / 30.0);
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_UNDO:            if (m_engine.undo()) showToast(L"Undo"); return;
                case BTN_REDO:            if (m_engine.redo()) showToast(L"Redo"); return;
                case BTN_SPLIT:           actionSplitSelected(); return;
                case BTN_DUPLICATE:       actionDuplicateSelected(); return;
                case BTN_DELETE:          actionDeleteSelected(); return;
                case BTN_MERGE_NEXT:
                    if (m_engine.selectedClipId() != 0 && m_engine.mergeWithNextClip(m_engine.selectedClipId())) {
                        showToast(tr(L"Merged clip with next clip on track!", L"تم دمج المقطع مع المقطع التالي بنجاح!"));
                    } else {
                        showToast(tr(L"Select a clip that has another clip after it on the same track.", L"حدد مقطعًا يوجد بعده مقطع آخر على نفس المسار للدمج."));
                    }
                    return;
                case BTN_CYCLE_STAB:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->stabilization = static_cast<StabilizationLevel>((static_cast<int>(sel->stabilization) + 1) % 4);
                        showToast(std::wstring(tr(L"Stabilization: ", L"تثبيت الاهتزاز: ")) + utf8ToWide(stabLevelName(sel->stabilization)));
                    }
                    return;
                case BTN_TOGGLE_AUTOBG:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->color.autoBgRemove = !sel->color.autoBgRemove;
                        showToast(sel->color.autoBgRemove ? tr(L"Auto Background Remover ON!", L"تم تفعيل إزالة الخلفية التلقائية!") : tr(L"Background Remover OFF", L"تم إيقاف إزالة الخلفية"));
                    }
                    return;
                case BTN_CYCLE_STUDIOBG:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->studioBg = static_cast<StudioBgPreset>((static_cast<int>(sel->studioBg) + 1) % 5);
                        showToast(std::wstring(tr(L"Studio Background: ", L"خلفية الاستوديو: ")) + utf8ToWide(studioBgName(sel->studioBg)));
                    }
                    return;
                case BTN_TTS_GENERATE: {
                    std::wstring ttsText = L"Welcome to CapCutVS Pro Studio";
                    if (promptTextDialog(tr(L"Text to Speech (Enter text to speak)", L"تحويل النص إلى صوت (اكتب النص)"), ttsText) && !ttsText.empty()) {
                        std::string wavPath = "tts_voice_" + std::to_string(m_voiceRecCounter++) + ".wav";
                        if (AudioEngine::synthesizeTextToSpeechWav(wideToUtf8(ttsText), wavPath)) {
                            importFilePath(wavPath);
                            showToast(tr(L"Generated TTS Voiceover on Track A1!", L"تم تحويل النص إلى صوت وإضافته للتايملاين!"));
                        } else {
                            showToast(tr(L"TTS failed. Ensure Windows Speech is enabled.", L"تعذر توليد الصوت."));
                        }
                    }
                    return;
                }
                case BTN_MUSIC_ADD: {
                    const char* mNames[4] = { "music_lofi.wav", "music_cyber.wav", "music_cinema.wav", "music_upbeat.wav" };
                    int mi = static_cast<int>(it->param) % 4;
                    if (AudioEngine::generateMusicLoopWav(mi, mNames[mi])) {
                        importFilePath(mNames[mi]);
                        showToast(tr(L"Added Music Loop to Timeline!", L"تمت إضافة الموسيقى إلى التايملاين!"));
                    }
                    return;
                }
                case BTN_AUDIO_ENHANCE:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->audioDsp.noiseReduction = 0.35f;
                        sel->audioDsp.compressor = 0.45f;
                        sel->audioDsp.bassBoost = 0.25f;
                        sel->audioDsp.treble = 0.30f;
                        sel->audioDsp.vocalIsolate = true;
                        m_audio.updateProjectSnapshot(m_engine.project());
                        showToast(tr(L"Studio Audio Enhance applied!", L"تم تطبيق تحسين الصوت الاحترافي!"));
                    }
                    return;
                case BTN_AUDIO_SYNC:
                    if (Clip* sel = m_engine.selectedClip()) {
                        double targetStart = m_engine.playhead();
                        for (const auto& c : m_engine.project().clips) {
                            if (c.type == MediaType::Video && c.id != sel->id) { targetStart = c.timelineStart; break; }
                        }
                        m_engine.pushUndoSnapshot();
                        sel->timelineStart = targetStart;
                        m_audio.updateProjectSnapshot(m_engine.project());
                        showToast(tr(L"Synced audio clip to video start!", L"تمت مزامنة الصوت مع بداية الفيديو!"));
                    }
                    return;
                case BTN_MANUAL_CAPTION: {
                    std::wstring cap = L"اكتب الترجمة هنا";
                    if (promptTextDialog(tr(L"Add Manual Caption", L"كتابة ترجمة يدوية"), cap) && !cap.empty()) {
                        uint64_t cid = m_engine.addTextClip(wideToUtf8(cap), m_engine.playhead(), 3.5);
                        if (Clip* tc = m_engine.findClip(cid)) {
                            tc->transform.posY = 0.74f; tc->text.fontSize = 38.0f; tc->text.hasBackground = true;
                        }
                        showToast(tr(L"Added Manual Caption on T1!", L"تمت إضافة الترجمة اليدوية على مسار T1!"));
                    }
                    return;
                }
                case BTN_TOGGLE_TXTBG:
                    if (Clip* s = m_engine.selectedClip(); s && s->type == MediaType::Text) { s->text.hasBackground = !s->text.hasBackground; InvalidateRect(m_hwnd, nullptr, FALSE); }
                    return;
                case BTN_TOGGLE_TXTSHADOW:
                    if (Clip* s = m_engine.selectedClip(); s && s->type == MediaType::Text) { s->text.shadow = !s->text.shadow; InvalidateRect(m_hwnd, nullptr, FALSE); }
                    return;
                case BTN_CYCLE_TXTEFFECT:
                    if (Clip* s = m_engine.selectedClip(); s && s->type == MediaType::Text) {
                        s->text.effect = static_cast<TextEffectType>((static_cast<int>(s->text.effect) + 1) % 5);
                        showToast(std::wstring(tr(L"Text Effect: ", L"مؤثر النص: ")) + utf8ToWide(textEffectName(s->text.effect)));
                    }
                    return;
                case BTN_SPEAK_TEXTCLIP:
                    if (Clip* s = m_engine.selectedClip(); s && s->type == MediaType::Text && !s->text.content.empty()) {
                        std::string wav = "tts_clip_" + std::to_string(m_voiceRecCounter++) + ".wav";
                        if (AudioEngine::synthesizeTextToSpeechWav(s->text.content, wav)) {
                            importFilePath(wav);
                            showToast(tr(L"Converted Text Clip to Speech on A1!", L"تم تحويل النص إلى صوت وإضافته للتايملاين!"));
                        }
                    }
                    return;
                case BTN_CYCLE_FONT:
                    if (Clip* sel = m_engine.selectedClip(); sel && sel->type == MediaType::Text) {
                        const char* fonts[6] = { "Segoe UI", "Arial Black", "Georgia", "Consolas", "Impact", "Tahoma" };
                        int cur = 0;
                        for (int fi = 0; fi < 6; ++fi) if (sel->text.fontName == fonts[fi]) cur = fi;
                        m_engine.pushUndoSnapshot();
                        sel->text.fontName = fonts[(cur + 1) % 6];
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_TOGGLE_OPTFLOW:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->opticalFlow = !sel->opticalFlow;
                        showToast(sel->opticalFlow ? tr(L"Optical Flow Slow-Mo Smoothing ON!", L"تم تفعيل التدفق البصري لتنعيم الحركة البطيئة!")
                                                   : tr(L"Optical Flow OFF", L"تم إيقاف التدفق البصري"));
                    }
                    return;
                case BTN_RIPPLE_DEL:
                    if (m_engine.selectedClipId() != 0 && m_engine.rippleDeleteClip(m_engine.selectedClipId())) {
                        m_previewCache.clear();
                        showToast(tr(L"Ripple Deleted clip & closed gap!", L"تم الحذف وسحب الفراغ تلقائيًا!"));
                    }
                    return;
                case BTN_TOGGLE_KEYFRAME: actionToggleKeyframe(); return;
                case BTN_SNAP:
                    m_snapEnabled = !m_snapEnabled;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_ZOOM_IN:
                    m_pixelsPerSec = std::min(300.0, m_pixelsPerSec * 1.25);
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_ADD_TRACK_T: {
                    int idx = m_engine.addTrack(TrackType::Text);
                    showToast(L"Added Track " + utf8ToWide(m_engine.project().tracks[idx].label));
                    return;
                }
                case BTN_ADD_TRACK_V: {
                    int idx = m_engine.addTrack(TrackType::Video);
                    showToast(L"Added Track " + utf8ToWide(m_engine.project().tracks[idx].label));
                    return;
                }
                case BTN_ADD_TRACK_A: {
                    int idx = m_engine.addTrack(TrackType::Audio);
                    showToast(L"Added Track " + utf8ToWide(m_engine.project().tracks[idx].label));
                    return;
                }
                case BTN_ZOOM_OUT:
                    m_pixelsPerSec = std::max(5.0, m_pixelsPerSec / 1.25);
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                case BTN_TRACK_HIDE:
                    if (it->param < m_engine.project().tracks.size()) {
                        m_engine.project().tracks[it->param].hidden = !m_engine.project().tracks[it->param].hidden;
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_TRACK_MUTE:
                    if (it->param < m_engine.project().tracks.size()) {
                        m_engine.project().tracks[it->param].muted = !m_engine.project().tracks[it->param].muted;
                        m_audio.updateProjectSnapshot(m_engine.project());
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_RESET_COLOR:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->transform = Transform2D{};
                        sel->color = ColorAdjust{};
                        sel->chromaKey = ChromaKeySettings{};
                        sel->effect = EffectSettings{};
                        sel->speed = 1.0;
                        sel->volume = 1.0f;
                        sel->transitionIn = TransitionType::None;
                        sel->keyframes.clear();
                        m_audio.updateProjectSnapshot(m_engine.project());
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
                case BTN_EDIT_TEXT: actionEditSelectedText(); return;
                case BTN_TEXT_COLOR:
                    if (Clip* sel = m_engine.selectedClip()) {
                        m_engine.pushUndoSnapshot();
                        sel->text.colorARGB = static_cast<uint32_t>(it->param);
                        InvalidateRect(m_hwnd, nullptr, FALSE);
                    }
                    return;
            }
        }
    }

    if (m_photoTool != PhotoTool::None && m_rcVideoCanvas.contains(x, y)) {
        if (Clip* sel = m_engine.selectedClip()) {
            if (sel->type == MediaType::Image || sel->type == MediaType::Video) {
                m_engine.pushUndoSnapshot();
                m_dragMode = DragMode::PhotoBrush;
                float nx = std::clamp((x - m_rcVideoCanvas.x) / std::max(1.0f, m_rcVideoCanvas.w), 0.0f, 1.0f);
                float ny = std::clamp((y - m_rcVideoCanvas.y) / std::max(1.0f, m_rcVideoCanvas.h), 0.0f, 1.0f);
                if (m_photoTool == PhotoTool::Clone && (GetKeyState(VK_MENU) & 0x8000) != 0) {
                    m_cloneSrcNx = nx; m_cloneSrcNy = ny;
                    showToast(tr(L"Clone Stamp source point set! Now paint to clone.", L"تم تحديد نقطة مصدر الاستنساخ! ارسم الآن للاستنساخ."));
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                    return;
                }
                m_lastBrushNx = nx; m_lastBrushNy = ny;
                sel->color.brushDots.push_back({ m_photoTool, nx, ny, 0.0f, 0.0f, m_brushRadius, m_cloneSrcNx, m_cloneSrcNy, m_brushPressure });
                InvalidateRect(m_hwnd, nullptr, FALSE);
                return;
            }
        }
    }
    if (!m_drawMode && m_rcVideoCanvas.contains(x, y)) {
        if (Clip* sel = m_engine.selectedClip()) {
            if (sel->type != MediaType::Audio) {
                m_engine.pushUndoSnapshot();
                m_dragMode = DragMode::MoveCanvasClip;
                m_dragStartX = x;
                m_dragStartY = y;
                m_dragOrigPosX = sel->transform.posX;
                m_dragOrigPosY = sel->transform.posY;
                return;
            }
        }
    }
    if (m_drawMode && m_rcVideoCanvas.contains(x, y)) {
        m_dragMode = DragMode::FreehandDraw;
        DrawStroke st;
        st.colorARGB = 0xFF00E5D1;
        st.thickness = 4.0f;
        st.points.push_back({ (x - m_rcVideoCanvas.x) / std::max(1.0f, m_rcVideoCanvas.w),
                              (y - m_rcVideoCanvas.y) / std::max(1.0f, m_rcVideoCanvas.h) });
        m_engine.project().drawings.push_back(std::move(st));
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    for (const auto& s : m_sliders) {
        UiRect hit{ s.trackRect.x - 6, s.trackRect.y - 6, s.trackRect.w + 12, s.trackRect.h + 12 };
        if (hit.contains(x, y)) {
            m_engine.pushUndoSnapshot();
            m_dragMode = DragMode::Slider;
            m_activeSliderId = s.id;
            float t = std::clamp((x - s.trackRect.x) / std::max(1.0f, s.trackRect.w), 0.0f, 1.0f);
            applySliderValue(s.id, s.minVal + t * (s.maxVal - s.minVal));
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
    }

    if (m_rcTimelineScrollH.contains(x, y)) {
        m_dragMode = DragMode::ScrollTimelineH;
        double totalSec = timelineTotalRangeSec();
        double visibleSec = std::max(1.0, static_cast<double>(m_rcTimelineRuler.w) / m_pixelsPerSec);
        double maxScroll = std::max(0.0, totalSec - visibleSec);
        float frac = std::clamp((x - m_rcTimelineScrollH.x) / std::max(1.0f, m_rcTimelineScrollH.w), 0.0f, 1.0f);
        m_scrollSec = frac * maxScroll;
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    if (m_rcTimelineRuler.contains(x, y)) {
        setPlaying(false);
        m_dragMode = DragMode::ScrubPlayhead;
        m_engine.setPlayhead(timelineXToTime(x));
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    if (m_rcTimelineTracks.contains(x, y) && x >= kTrackHeaderW) {
        const auto& clips = m_engine.project().clips;
        for (auto it = clips.rbegin(); it != clips.rend(); ++it) {
            UiRect cr = getClipRect(*it);
            if (cr.contains(x, y)) {
                m_engine.selectClip(it->id);
                m_engine.pushUndoSnapshot();
                m_dragClipId = it->id;
                m_dragStartX = x;
                m_dragOrigStart = it->timelineStart;
                m_dragOrigEnd = it->timelineEnd();

                if (x <= cr.x + 8.0f && cr.w > 20.0f) m_dragMode = DragMode::TrimClipLeft;
                else if (x >= cr.x + cr.w - 8.0f && cr.w > 20.0f) m_dragMode = DragMode::TrimClipRight;
                else m_dragMode = DragMode::MoveClip;

                InvalidateRect(m_hwnd, nullptr, FALSE);
                return;
            }
        }
        m_engine.selectClip(0);
        setPlaying(false);
        m_dragMode = DragMode::ScrubPlayhead;
        m_engine.setPlayhead(timelineXToTime(x));
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void MainWindow::onMouseMove(float x, float y) {
    m_hoverX = x;
    m_hoverY = y;

    if (m_dragMode == DragMode::MoveCanvasClip) {
        if (Clip* sel = m_engine.selectedClip()) {
            float dx = (x - m_dragStartX) / std::max(1.0f, m_rcVideoCanvas.w * 0.5f);
            float dy = (y - m_dragStartY) / std::max(1.0f, m_rcVideoCanvas.h * 0.5f);
            sel->transform.posX = std::clamp(m_dragOrigPosX + dx, -1.5f, 1.5f);
            sel->transform.posY = std::clamp(m_dragOrigPosY + dy, -1.5f, 1.5f);
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return;
    }
    if (m_dragMode == DragMode::PhotoBrush && m_photoTool != PhotoTool::None) {
        if (Clip* sel = m_engine.selectedClip()) {
            float nx = std::clamp((x - m_rcVideoCanvas.x) / std::max(1.0f, m_rcVideoCanvas.w), 0.0f, 1.0f);
            float ny = std::clamp((y - m_rcVideoCanvas.y) / std::max(1.0f, m_rcVideoCanvas.h), 0.0f, 1.0f);
            float ddx = nx - m_lastBrushNx, ddy = ny - m_lastBrushNy;
            if (ddx * ddx + ddy * ddy > 0.00025f && sel->color.brushDots.size() < 400) {
                sel->color.brushDots.push_back({ m_photoTool, nx, ny, ddx, ddy, m_brushRadius, m_cloneSrcNx, m_cloneSrcNy, m_brushPressure });
                m_lastBrushNx = nx; m_lastBrushNy = ny;
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
        }
        return;
    }
    if (m_dragMode == DragMode::FreehandDraw && !m_engine.project().drawings.empty()) {
        float nx = std::clamp((x - m_rcVideoCanvas.x) / std::max(1.0f, m_rcVideoCanvas.w), 0.0f, 1.0f);
        float ny = std::clamp((y - m_rcVideoCanvas.y) / std::max(1.0f, m_rcVideoCanvas.h), 0.0f, 1.0f);
        m_engine.project().drawings.back().points.push_back({ nx, ny });
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    if (m_dragMode == DragMode::ScrollTimelineH) {
        double totalSec = timelineTotalRangeSec();
        double visibleSec = std::max(1.0, static_cast<double>(m_rcTimelineRuler.w) / m_pixelsPerSec);
        double maxScroll = std::max(0.0, totalSec - visibleSec);
        float frac = std::clamp((x - m_rcTimelineScrollH.x) / std::max(1.0f, m_rcTimelineScrollH.w), 0.0f, 1.0f);
        m_scrollSec = frac * maxScroll;
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    if (m_dragMode == DragMode::ScrubPlayhead) {
        m_engine.setPlayhead(timelineXToTime(x));
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    if (m_dragMode == DragMode::Slider) {
        for (const auto& s : m_sliders) {
            if (s.id == m_activeSliderId) {
                float t = std::clamp((x - s.trackRect.x) / std::max(1.0f, s.trackRect.w), 0.0f, 1.0f);
                applySliderValue(s.id, s.minVal + t * (s.maxVal - s.minVal));
                break;
            }
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    if (m_dragMode == DragMode::MoveClip) {
        double dt = static_cast<double>(x - m_dragStartX) / m_pixelsPerSec;
        int tr = timelineYToTrack(y);
        m_engine.moveClip(m_dragClipId, tr, m_dragOrigStart + dt, m_snapEnabled);
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    if (m_dragMode == DragMode::TrimClipLeft) {
        double dt = static_cast<double>(x - m_dragStartX) / m_pixelsPerSec;
        m_engine.trimClipStart(m_dragClipId, m_dragOrigStart + dt);
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    if (m_dragMode == DragMode::TrimClipRight) {
        double dt = static_cast<double>(x - m_dragStartX) / m_pixelsPerSec;
        m_engine.trimClipEnd(m_dragClipId, m_dragOrigEnd + dt);
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    int hoveredIdx = -1;
    for (int i = static_cast<int>(m_buttons.size()) - 1; i >= 0; --i) {
        if (m_buttons[i].rect.contains(x, y)) {
            hoveredIdx = i;
            break;
        }
    }
    if (hoveredIdx != m_lastHoveredBtn) {
        m_lastHoveredBtn = hoveredIdx;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void MainWindow::onMouseUp(float /*x*/, float /*y*/) {
    if (m_dragMode != DragMode::None && m_playing) {
        m_audio.updateProjectSnapshot(m_engine.project());
    }
    m_dragMode = DragMode::None;
    m_dragClipId = 0;
    m_activeSliderId = 0;
}

void MainWindow::onMouseWheel(float x, float y, short delta, bool ctrlDown) {
    if (m_showAllToolsModal) {
        m_allToolsPage = (delta < 0) ? 1 : 0;
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    if (m_showAssetHubModal) {
        m_hubPage = std::max(0, m_hubPage + (delta < 0 ? 1 : -1));
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    // Touchpad / Touch Pinch-to-Zoom on Preview Canvas scales active clip smoothly
    if (ctrlDown && m_rcVideoCanvas.contains(x, y)) {
        if (Clip* sel = m_engine.selectedClip()) {
            float factor = (delta > 0) ? 1.08f : 0.92f;
            sel->transform.scale = std::clamp(sel->transform.scale * factor, 0.2f, 2.5f);
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
    }
    if (m_rcLeftPanel.contains(x, y)) {
        m_leftScrollY = std::clamp(m_leftScrollY + (delta > 0 ? -48.0f : 48.0f), 0.0f, 1100.0f);
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    if (m_rcInspectorPanel.contains(x, y)) {
        m_inspScrollY = std::clamp(m_inspScrollY + (delta > 0 ? -44.0f : 44.0f), 0.0f, 1850.0f);
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    if (m_rcTimelinePanel.contains(x, y)) {
        if (ctrlDown) {
            double factor = (delta > 0) ? 1.18 : (1.0 / 1.18);
            m_pixelsPerSec = std::clamp(m_pixelsPerSec * factor, 5.0, 300.0);
        } else if ((GetKeyState(VK_SHIFT) & 0x8000) != 0 || x < kTrackHeaderW) {
            float maxVScroll = std::max(0.0f, static_cast<float>(m_engine.project().tracks.size()) * kTrackRowH - m_rcTimelineTracks.h + 16.0f);
            m_trackScrollY = std::clamp(m_trackScrollY + (delta > 0 ? -36.0f : 36.0f), 0.0f, maxVScroll);
        } else {
            double totalSec = timelineTotalRangeSec();
            double visibleSec = std::max(1.0, static_cast<double>(m_rcTimelineRuler.w) / m_pixelsPerSec);
            double maxScroll = std::max(0.0, totalSec - visibleSec);
            double shift = (delta > 0 ? -2.5 : 2.5) * (60.0 / m_pixelsPerSec);
            m_scrollSec = std::clamp(m_scrollSec + shift, 0.0, maxScroll);
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void MainWindow::onKeyDown(WPARAM vk) {
    if (m_exporting.load()) return;
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    if (vk == VK_ESCAPE) {
        if (m_showAllToolsModal || m_showExportModal || m_showAssetHubModal) {
            m_showAllToolsModal = false;
            m_showExportModal = false;
            m_showAssetHubModal = false;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
    }
    if (ctrl && vk == 'F') { actionSearchAssets(); return; }
    if (vk == VK_F1) {
        showToast(tr(L"Shortcuts: Space=Play | Ctrl+B=Split | Ctrl+Z/Y=Undo/Redo | Ctrl+F=Search | Esc=Close",
                     L"الاختصارات: مسافة=تشغيل | Ctrl+B=قص | Ctrl+Z/Y=تراجع | Ctrl+F=بحث | Esc=إغلاق"));
        return;
    }
    if (vk == VK_SPACE) setPlaying(!m_playing);
    else if (vk == VK_DELETE || vk == VK_BACK) actionDeleteSelected();
    else if (ctrl && vk == 'Z') { if (m_engine.undo()) showToast(L"Undo"); }
    else if (ctrl && vk == 'Y') { if (m_engine.redo()) showToast(L"Redo"); }
    else if (ctrl && vk == 'B') actionSplitSelected();
    else if (ctrl && vk == 'D') actionDuplicateSelected();
    else if (ctrl && vk == 'S') actionSaveProject();
    else if (vk == 'K') actionToggleKeyframe();
    else if (vk == VK_F11) actionToggleFullScreen();
    else if (!ctrl && vk == 'M') { if (Clip* s = m_engine.selectedClip()) { s->muted = !s->muted; m_audio.updateProjectSnapshot(m_engine.project()); showToast(s->muted ? tr(L"Clip Muted", L"تم كتم المقطع") : tr(L"Clip Unmuted", L"تم إلغاء الكتم")); } }
    else if (!ctrl && vk == 'B') { m_beforeAfter = !m_beforeAfter; InvalidateRect(m_hwnd, nullptr, FALSE); }
    else if (vk == VK_LEFT) {
        setPlaying(false);
        m_engine.setPlayhead(std::max(0.0, m_engine.playhead() - 1.0 / 30.0));
        InvalidateRect(m_hwnd, nullptr, FALSE);
    } else if (vk == VK_RIGHT) {
        setPlaying(false);
        m_engine.setPlayhead(m_engine.playhead() + 1.0 / 30.0);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void MainWindow::onDropFiles(HDROP hDrop) {
    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; ++i) {
        UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
        std::wstring wPath(len + 1, L'\0');
        DragQueryFileW(hDrop, i, wPath.data(), len + 1);
        wPath.resize(len);
        importFilePath(wideToUtf8(wPath));
    }
    DragFinish(hDrop);
}

void MainWindow::onTimer() {
    bool needsRedraw = false;
    {
        std::vector<std::function<void()>> tasks;
        {
            std::lock_guard<std::mutex> lk(m_uiTaskMutex);
            tasks.swap(m_pendingUiTasks);
        }
        for (auto& fn : tasks) {
            fn();
            needsRedraw = true;
        }
    }
    DWORD nowAutoTick = GetTickCount();
    if (m_lastAutoSaveTick == 0) m_lastAutoSaveTick = nowAutoTick;
    if (nowAutoTick - m_lastAutoSaveTick >= 45000 && !m_engine.project().clips.empty()) {
        m_lastAutoSaveTick = nowAutoTick;
        actionAutoSaveDraft(true);
    }
    if (m_playing) {
        DWORD tickNow = GetTickCount();
        if (tickNow - m_lastPlayRedrawTick >= 30) { // Ultra-light 33 FPS video sync -> zero CPU/GPU overload!
            m_lastPlayRedrawTick = tickNow;
            double now = timeGetTime() / 1000.0;
            double dt = std::clamp(now - m_lastTickSec, 0.0, 0.12);
            m_lastTickSec = now;
            double nextT = m_engine.playhead() + dt;
            double total = m_engine.totalDuration();
            if (total > 0.1 && nextT >= total) {
                m_engine.setPlayhead(0.0);
                setPlaying(false);
            } else {
                m_engine.setPlayhead(nextT);
                double visibleSec = std::max(1.0, static_cast<double>(m_rcTimelineRuler.w) / m_pixelsPerSec);
                if (nextT > m_scrollSec + visibleSec * 0.92) {
                    m_scrollSec = nextT - visibleSec * 0.15;
                }
            }
            needsRedraw = true;
        }
    } else {
        auto active = m_engine.activeVisualClipsAt(m_engine.playhead());
        for (const Clip* c : active) {
            if (c->type == MediaType::Video) {
                double decSec = -999.0;
                double srcT = c->sourceTimeAt(m_engine.playhead());
                if (m_media.tryGetLatestFramePtr((!c->proxyPath.empty() && m_previewQuality < 2) ? c->proxyPath : c->sourcePath, c->type, srcT, decSec)) {
                    auto it = m_previewCache.find(c->id);
                    if (it == m_previewCache.end() || std::abs(decSec - it->second.decodedSec) > 0.005) {
                        needsRedraw = true;
                        break;
                    }
                }
            }
        }
    }

    if (m_exporting.load()) needsRedraw = true;
    if (!m_engine.project().clips.empty() && GetTickCount() - m_lastAutoSaveTick > 60000) {
        m_lastAutoSaveTick = GetTickCount();
    m_aiConfig = AICloudBridge::loadConfig();
        std::string dummyErr;
        ProjectIO::saveToFile("autosave.ccvs", m_engine.project(), dummyErr);
    }
    {
        std::lock_guard<std::mutex> lk(m_toastMutex);
        if (!m_toastText.empty() && GetTickCount() > m_toastExpireTick) {
            m_toastText.clear();
            needsRedraw = true;
        }
    }
    if (needsRedraw) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void MainWindow::applySliderValue(int sliderId, float val) {
    Clip* c = m_engine.selectedClip();
    if (!c) return;
    switch (sliderId) {
        case SLD_SCALE:         c->transform.scale = val; break;
        case SLD_POS_X:         c->transform.posX = val; break;
        case SLD_POS_Y:         c->transform.posY = val; break;
        case SLD_ROTATION:      c->transform.rotationDeg = val; break;
        case SLD_OPACITY:       c->color.opacity = val; break;
        case SLD_SPEED:         c->speed = val; break;
        case SLD_VOLUME:        c->volume = val; m_audio.updateProjectSnapshot(m_engine.project()); break;
        case SLD_FADE_IN:       c->audioFadeIn = val; m_audio.updateProjectSnapshot(m_engine.project()); break;
        case SLD_FADE_OUT:      c->audioFadeOut = val; m_audio.updateProjectSnapshot(m_engine.project()); break;
        case SLD_TRANS_DUR:     c->transitionDuration = val; break;
        case SLD_FX_INTENSITY:  c->effect.intensity = val; break;
        case SLD_CHROMA_SIM:    c->chromaKey.similarity = val; break;
        case SLD_CHROMA_SMOOTH: c->chromaKey.smoothness = val; break;
        case SLD_BRIGHTNESS:    c->color.brightness = val; break;
        case SLD_CONTRAST:      c->color.contrast = val; break;
        case SLD_SATURATION:    c->color.saturation = val; break;
        case SLD_TEMP:          c->color.temperature = val; break;
        case SLD_FONTSIZE:      c->text.fontSize = val; break;
        case SLD_CROP_X:        c->transform.cropX = val; break;
        case SLD_CROP_Y:        c->transform.cropY = val; break;
        case SLD_EXPOSURE:      c->color.exposure = val; break;
        case SLD_VIBRANCE:      c->color.vibrance = val; break;
        case SLD_TINT:          c->color.tint = val; break;
        case SLD_HIGHLIGHTS:    c->color.highlights = val; break;
        case SLD_SHADOWS:       c->color.shadows = val; break;
        case SLD_SHARPNESS:     c->color.sharpness = val; break;
        case SLD_BLUR:          c->color.blur = val; break;
        case SLD_AUDIO_PAN:     c->audioDsp.pan = val; m_audio.updateProjectSnapshot(m_engine.project()); break;
        case SLD_AUDIO_PITCH:   c->audioDsp.pitch = val; m_audio.updateProjectSnapshot(m_engine.project()); break;
        case SLD_AUDIO_BASS:    c->audioDsp.bassBoost = val; m_audio.updateProjectSnapshot(m_engine.project()); break;
        case SLD_AUDIO_TREBLE:  c->audioDsp.treble = val; m_audio.updateProjectSnapshot(m_engine.project()); break;
        case SLD_AUDIO_ECHO:    c->audioDsp.echo = val; m_audio.updateProjectSnapshot(m_engine.project()); break;
        case SLD_AUDIO_NOISE:   c->audioDsp.noiseReduction = val; m_audio.updateProjectSnapshot(m_engine.project()); break;
        case SLD_AUDIO_COMP:    c->audioDsp.compressor = val; m_audio.updateProjectSnapshot(m_engine.project()); break;
        case SLD_SKEW_X:        c->transform.skewX = val; break;
        case SLD_SKEW_Y:        c->transform.skewY = val; break;
        case SLD_WHITES:        c->color.whites = val; break;
        case SLD_BLACKS:        c->color.blacks = val; break;
        case SLD_CLARITY:       c->color.clarity = val; break;
        case SLD_HSL_HUE:       c->color.hslHue = val; break;
        case SLD_SKIN_SMOOTH:   c->color.skinSmooth = val; break;
        case SLD_TEETH_WHITEN:  c->color.teethWhiten = val; break;
        case SLD_FADE_OUT_VIS:  c->fadeOutDuration = val; break;
        case SLD_MASK_FEATHER:  c->maskFeather = val; break;
        case SLD_HSL_RED:       c->color.hslRedSat = val; break;
        case SLD_HSL_GREEN:     c->color.hslGreenSat = val; break;
        case SLD_HSL_BLUE:      c->color.hslBlueSat = val; break;
        case SLD_WHEEL_SHADOW:  c->color.wheelShadow = val; break;
        case SLD_WHEEL_MID:     c->color.wheelMid = val; break;
        case SLD_WHEEL_HIGH:     c->color.wheelHigh = val; break;
        case SLD_FACE_RETOUCH:   c->color.faceRetouch = val; break;
        case SLD_FACE_SLIM:      c->color.faceSlim = val; break;
        case SLD_BODY_SLIM:      c->color.bodySlim = val; break;
        case SLD_ENHANCE_HD:     c->color.enhanceHD = val; break;
        case SLD_PORTRAIT_BOKEH: c->color.portraitBokeh = val; break;
        case SLD_CANVAS_EXPAND:  c->color.canvasExpand = val; break;
        case SLD_AUDIO_MID:      c->audioDsp.midEq = val; m_audio.updateProjectSnapshot(m_engine.project()); break;
        case SLD_TXT_STROKE:     c->text.strokeWidth = val; break;
        case SLD_TXT_TRACKING:   c->text.tracking = val; break;
        case SLD_TXT_CURVE:      c->text.curve = val; break;
        case SLD_LENS_DISTORT:   c->color.lensDistort = val; break;
        case SLD_ROLL_SHUTTER:   c->color.rollingShutter = val; break;
        case SLD_CURVE_R:        c->color.curveR = val; break;
        case SLD_CURVE_G:        c->color.curveG = val; break;
        case SLD_CURVE_B:        c->color.curveB = val; break;
        case SLD_HSL_TARGET:     c->color.hslTargetHue = val; break;
        case SLD_HSL_SECSHIFT:   c->color.hslSecShift = val; break;
        case SLD_HSL_SECSAT:     c->color.hslSecSat = val; break;
        case SLD_SPLIT_SH_HUE:   c->color.splitShadowHue = val; break;
        case SLD_SPLIT_HI_HUE:   c->color.splitHighHue = val; break;
        case SLD_SPLIT_AMT:      c->color.splitAmount = val; break;
        case SLD_AUDIO_REVERB:   c->audioDsp.reverb = val; m_audio.updateProjectSnapshot(m_engine.project()); break;
        case SLD_AUDIO_DEESSER:  c->audioDsp.deEsser = val; m_audio.updateProjectSnapshot(m_engine.project()); break;
        case SLD_AUDIO_WIDTH:    c->audioDsp.stereoWidth = val; m_audio.updateProjectSnapshot(m_engine.project()); break;
        case SLD_AUDIO_LIMITER:  c->audioDsp.limiter = val; m_audio.updateProjectSnapshot(m_engine.project()); break;
        case SLD_PHOTO_DENOISE:  c->color.photoDenoise = val; break;
        case SLD_GRADIENT_OVL:   c->color.gradientOverlay = val; break;
        case SLD_RAW_FILMIC:     c->color.rawFilmicTone = val; break;
        case SLD_HSL_BAND_SAT:   c->color.hslBandSat[m_hslBandIdx % 8] = val; break;
        case SLD_HSL_BAND_LUM:   c->color.hslBandLum[m_hslBandIdx % 8] = val; break;
        case SLD_CURVE_SH_PT:    c->color.curveShadowPt = val; break;
        case SLD_CURVE_HI_PT:    c->color.curveHighPt = val; break;
        case SLD_BRUSH_SIZE:     m_brushRadius = val; break;
        case SLD_BRUSH_PRESS:    m_brushPressure = val; break;
        case SLD_SPEED_START:    c->speedStart = val; break;
        case SLD_SPEED_MID:      c->speedMid   = val; break;
        case SLD_SPEED_END:      c->speedEnd   = val; break;
        case SLD_PERSP_WARP_H:   c->color.perspWarpH = val; break;
        case SLD_PERSP_WARP_V:   c->color.perspWarpV = val; break;
        default: break;
    }
    if (m_engine.hasKeyframeNearPlayhead(c->id)) {
        double localT = m_engine.playhead() - c->timelineStart;
        for (auto& k : c->keyframes) {
            if (std::abs(k.localTime - localT) <= 0.12) {
                k.posX = c->transform.posX;
                k.posY = c->transform.posY;
                k.scale = c->transform.scale;
                k.rotationDeg = c->transform.rotationDeg;
                k.opacity = c->color.opacity;
                break;
            }
        }
    }
}

void MainWindow::actionCycleResolution() {
    m_resPreset = (m_resPreset + 1) % 5;
    int heights[5] = { 480, 720, 1080, 1440, 2160 };
    int h = heights[m_resPreset];
    if (m_aspectMode == 0) { m_engine.project().canvasWidth = h * 16 / 9; m_engine.project().canvasHeight = h; }
    else if (m_aspectMode == 1) { m_engine.project().canvasWidth = h * 9 / 16; m_engine.project().canvasHeight = h; }
    else { m_engine.project().canvasWidth = h; m_engine.project().canvasHeight = h; }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::actionCycleAspectRatio() {
    m_aspectMode = (m_aspectMode + 1) % 3;
    int heights[5] = { 480, 720, 1080, 1440, 2160 };
    int h = heights[m_resPreset % 5];
    if (m_aspectMode == 0) { m_engine.project().canvasWidth = h * 16 / 9; m_engine.project().canvasHeight = h; }
    else if (m_aspectMode == 1) { m_engine.project().canvasWidth = h * 9 / 16; m_engine.project().canvasHeight = h; }
    else { m_engine.project().canvasWidth = h; m_engine.project().canvasHeight = h; }
    computeLayout();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::actionFreezeFrame() {
    uint64_t id = m_engine.selectedClipId();
    if (id != 0 && m_engine.freezeFrameAtPlayhead(id, 3.0) != 0) {
        showToast(tr(L"Created 3s Freeze Frame on V2!", L"تم تجميد الإطار لمدة 3 ثوانٍ على مسار V2!"));
    } else {
        showToast(tr(L"Select a video clip under the playhead to Freeze.", L"حدد مقطع فيديو تحت المؤشر لتجميد الإطار."));
    }
}

void MainWindow::actionExtractAudio() {
    uint64_t id = m_engine.selectedClipId();
    if (id != 0 && m_engine.extractAudioFromClip(id) != 0) {
        m_audio.updateProjectSnapshot(m_engine.project());
        showToast(tr(L"Extracted Audio to Track A1!", L"تم فصل الصوت بنجاح إلى مسار A1!"));
    } else {
        showToast(tr(L"Select a video clip first to Extract Audio.", L"حدد مقطع فيديو أولًا لفصل الصوت."));
    }
}

void MainWindow::actionCycleTextAnim() {
    Clip* c = m_engine.selectedClip();
    if (!c || c->type != MediaType::Text) return;
    m_engine.pushUndoSnapshot();
    c->text.anim = static_cast<TextAnimType>((static_cast<int>(c->text.anim) + 1) % 5);
    showToast(std::wstring(tr(L"Text Animation: ", L"حركة النص: ")) + utf8ToWide(textAnimName(c->text.anim)));
}

void MainWindow::actionToggleFullScreen() {
    m_fullScreenPreview = !m_fullScreenPreview;
    computeLayout();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::actionToggleVoiceRec() {
    if (m_audio.isRecording()) {
        std::string wavName = "voiceover_" + std::to_string(m_voiceRecCounter++) + ".wav";
        if (m_audio.stopRecordingToWav(wavName)) {
            importFilePath(wavName);
            showToast(tr(L"Voice Over saved & added to Timeline!", L"تم حفظ التسجيل الصوتي وإضافته للتايملاين!"));
        } else {
            showToast(tr(L"No microphone audio captured.", L"لم يتم التقاط صوت من الميكروفون."));
        }
    } else {
        if (m_audio.startRecording()) {
            showToast(tr(L"Recording Microphone... Click Stop when done.", L"جاري التسجيل من الميكروفون... اضغط إيقاف عند الانتهاء."));
        } else {
            showToast(tr(L"Failed to open microphone device.", L"تعذر فتح الميكروفون."));
        }
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::actionExtractPngFrame() {
    auto active = m_engine.activeVisualClipsAt(m_engine.playhead());
    const Clip* target = nullptr;
    for (auto it = active.rbegin(); it != active.rend(); ++it) {
        if ((*it)->type == MediaType::Video || (*it)->type == MediaType::Image) { target = *it; break; }
    }
    if (!target) {
        showToast(tr(L"Move playhead over a video or image to extract PNG.", L"ضع المؤشر فوق فيديو أو صورة لاستخراج صورة PNG."));
        return;
    }
    wchar_t fileBuf[MAX_PATH] = L"CapCutVS_Frame.png";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"PNG Image (*.png) *.png ";
    ofn.lpstrDefExt = L"png";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&ofn)) return;

    FrameBGRA raw, graded;
    if (m_media.decodeFrame(target->sourcePath, target->type, target->sourceTimeAt(m_engine.playhead()), raw)) {
        MediaEngine::applyPipeline(raw, target->color, target->chromaKey, target->effect, graded,
                                   target->mask, target->transform.cropX, target->transform.cropY);
        if (m_media.saveFrameToPng(graded, wideToUtf8(fileBuf))) {
            showToast(tr(L"Saved frame as PNG!", L"تم حفظ الإطار كصورة PNG بنجاح!"));
            return;
        }
    }
    showToast(tr(L"Failed to save PNG frame.", L"تعذر حفظ الصورة."));
}

void MainWindow::actionLoadCubeLut() {
    Clip* sel = m_engine.selectedClip();
    if (!sel) return;
    wchar_t fileBuf[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"3D LUT Files (*.cube) *.cube All Files (*.*) *.* ";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;
    m_engine.pushUndoSnapshot();
    sel->color.lutPath = wideToUtf8(fileBuf);
    showToast(tr(L"Applied 3D LUT (.cube)!", L"تم تطبيق فلتر 3D LUT بنجاح!"));
}

void MainWindow::actionCopyEffects() {
    Clip* sel = m_engine.selectedClip();
    if (!sel) return;
    m_copiedFx = *sel;
    m_hasCopiedFx = true;
    showToast(tr(L"Copied Color, FX, Chroma Key & Audio settings!", L"تم نسخ جميع التأثيرات والألوان والصوت!"));
}

void MainWindow::actionPasteEffects() {
    Clip* sel = m_engine.selectedClip();
    if (!sel || !m_hasCopiedFx) return;
    m_engine.pushUndoSnapshot();
    sel->color = m_copiedFx.color;
    sel->chromaKey = m_copiedFx.chromaKey;
    sel->effect = m_copiedFx.effect;
    sel->mask = m_copiedFx.mask;
    sel->transitionIn = m_copiedFx.transitionIn;
    sel->transitionDuration = m_copiedFx.transitionDuration;
    sel->audioDsp = m_copiedFx.audioDsp;
    m_audio.updateProjectSnapshot(m_engine.project());
    showToast(tr(L"Pasted all effects onto clip!", L"تم لصق جميع التأثيرات على المقطع!"));
}

void MainWindow::actionDetectBeats() {
    const Clip* target = m_engine.selectedClip();
    if (!target || (target->type != MediaType::Audio && target->type != MediaType::Video)) {
        for (const auto& c : m_engine.project().clips) {
            if (c.type == MediaType::Audio || c.type == MediaType::Video) { target = &c; break; }
        }
    }
    if (!target) {
        showToast(tr(L"Add an audio or video clip first.", L"أضف صوتًا أو فيديو أولًا لكشف الإيقاع."));
        return;
    }
    m_beatMarkers = m_audio.detectBeats(target->sourcePath, target->timelineStart, target->duration);
    wchar_t msg[96];
    std::swprintf(msg, 96, tr(L"Detected %zu audio beats on Timeline ruler!", L"تم اكتشاف %zu نقطة إيقاع على التايملاين!"), m_beatMarkers.size());
    showToast(msg);
}

void MainWindow::actionImportSrt() {
    wchar_t fileBuf[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"SubRip Subtitles (*.srt)\0*.srt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;

    std::ifstream ifs(std::filesystem::path(fileBuf), std::ios::binary);
    if (!ifs) {
        showToast(L"Failed to read SRT file.");
        return;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    auto entries = parseSrtText(ss.str());
    if (entries.empty()) {
        showToast(L"No valid subtitle entries found in SRT.");
        return;
    }

    for (const auto& e : entries) {
        uint64_t cid = m_engine.addTextClip(e.text, e.startSec, std::max(0.3, e.endSec - e.startSec));
        if (Clip* tc = m_engine.findClip(cid)) {
            tc->transform.posY = 0.74f;
            tc->text.fontSize = 38.0f;
            tc->text.colorARGB = 0xFFFFD84D;
        }
    }
    wchar_t msg[96];
    std::swprintf(msg, 96, L"Imported %zu timed captions onto Track T1!", entries.size());
    showToast(msg);
}

void MainWindow::actionAutoCaptions() {
    const Clip* target = m_engine.selectedClip();
    if (!target || (target->type != MediaType::Video && target->type != MediaType::Audio)) {
        for (const auto& c : m_engine.project().clips) {
            if (c.type == MediaType::Video || c.type == MediaType::Audio) {
                target = &c;
                break;
            }
        }
    }
    if (!target) {
        showToast(L"Add a video or audio clip to the Timeline first for Auto Captions.");
        return;
    }

    std::string srcPath = target->sourcePath;
    double tlStart = target->timelineStart;
    double dur = target->duration;
    runAiTaskAsync(tr(L"Transcribing speech (Whisper / Windows STT)...", L"جاري تفريغ الصوت لنص (Whisper / SAPI) في الخلفية..."), [this, srcPath, tlStart, dur]() {
        std::vector<SubtitleEntry> segs;
        std::string engineNote;
        bool okStt = AICloudBridge::transcribeAudioWhisperLocal(srcPath, tlStart, segs, engineNote);
        if (!okStt || segs.empty()) {
            segs = m_audio.detectSpeechSegments(srcPath, tlStart, dur);
            engineNote = "Auto Speech Segmentation";
        }
        postUiTask([this, segs, engineNote]() {
            if (segs.empty()) {
                showToast(tr(L"No speech detected in clip.", L"لم يتم رصد كلام في المقطع."));
                return;
            }
            for (const auto& s : segs) {
                uint64_t cid = m_engine.addTextClip(s.text, s.startSec, std::max(0.4, s.endSec - s.startSec));
                if (Clip* tc = m_engine.findClip(cid)) {
                    tc->transform.posY = 0.74f;
                    tc->text.fontSize = 38.0f;
                    tc->text.colorARGB = 0xFFFFFFFF;
                }
            }
            showToast(tr(L"Captions generated via: ", L"تم توليد الترجمة عبر: ") + utf8ToWide(engineNote));
        });
    });
    return;
}

void MainWindow::actionNewProject() {
    setPlaying(false);
    m_engine.resetProject("Untitled Project");
    m_thumbBitmaps.clear();
    m_previewCache.clear();
    m_media.clearCache();
    m_audio.clearCache();
    m_currentProjectPath.clear();
    showToast(L"Created New Project");
}

void MainWindow::actionOpenProject() {
    setPlaying(false);
    wchar_t fileBuf[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"CapCutVS Project (*.ccvs)\0*.ccvs\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;

    std::string path = wideToUtf8(fileBuf);
    Project loaded;
    std::string err;
    if (!ProjectIO::loadFromFile(path, loaded, err)) {
        showToast(L"Open failed: " + utf8ToWide(err));
        return;
    }
    m_engine.project() = std::move(loaded);
    m_engine.selectClip(0);
    m_engine.setPlayhead(0.0);
    m_currentProjectPath = path;
    m_thumbBitmaps.clear();
    m_previewCache.clear();

    for (const auto& a : m_engine.project().assets) {
        FrameBGRA thumb;
        if (m_media.extractThumbnail(a.filePath, a.type, 160, 90, thumb)) {
            uploadFrameToBitmap(thumb, m_thumbBitmaps[a.id]);
        }
        if (a.type == MediaType::Video || a.type == MediaType::Audio) {
            m_audio.preloadAudioAsync(a.filePath);
        }
    }
    showToast(L"Opened project: " + utf8ToWide(m_engine.project().name));
}

void MainWindow::actionSaveProject() {
    std::string path = m_currentProjectPath;
    if (path.empty()) {
        wchar_t fileBuf[MAX_PATH] = L"project.ccvs";
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = m_hwnd;
        ofn.lpstrFilter = L"CapCutVS Project (*.ccvs)\0*.ccvs\0";
        ofn.lpstrDefExt = L"ccvs";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_OVERWRITEPROMPT;
        if (!GetSaveFileNameW(&ofn)) return;
        path = wideToUtf8(fileBuf);
    }

    std::string err;
    if (ProjectIO::saveToFile(path, m_engine.project(), err)) {
        m_currentProjectPath = path;
        m_engine.project().name = wideToUtf8(std::filesystem::u8path(path).stem().wstring());
        showToast(L"Project saved successfully!");
    } else {
        showToast(L"Save failed: " + utf8ToWide(err));
    }
}

void MainWindow::actionImportMedia(bool audioOnly) {
    std::vector<wchar_t> buf(32768, L'\0');
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = audioOnly
        ? L"Audio Files (*.mp3;*.wav;*.aac;*.m4a)\0*.mp3;*.wav;*.aac;*.m4a\0All Files (*.*)\0*.*\0"
        : L"Media Files (*.mp4;*.mov;*.mkv;*.avi;*.wmv;*.png;*.jpg;*.jpeg;*.bmp;*.mp3;*.wav)\0"
          L"*.mp4;*.mov;*.mkv;*.avi;*.wmv;*.png;*.jpg;*.jpeg;*.bmp;*.mp3;*.wav\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = buf.data();
    ofn.nMaxFile = static_cast<DWORD>(buf.size());
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    if (!GetOpenFileNameW(&ofn)) return;

    const wchar_t* p = buf.data();
    std::wstring dir = p;
    p += dir.size() + 1;
    if (*p == L'\0') {
        importFilePath(wideToUtf8(dir));
    } else {
        while (*p != L'\0') {
            std::wstring name = p;
            importFilePath(wideToUtf8(dir + L"\\" + name));
            p += name.size() + 1;
        }
    }
}

void MainWindow::importFilePath(const std::string& utf8Path) {
    MediaAsset asset;
    if (!m_media.probeFile(utf8Path, asset)) {
        showToast(L"Unsupported or unreadable media file.");
        return;
    }
    uint64_t aid = m_engine.addAsset(asset);
    FrameBGRA thumb;
    if (m_media.extractThumbnail(asset.filePath, asset.type, 160, 90, thumb)) {
        uploadFrameToBitmap(thumb, m_thumbBitmaps[aid]);
    }
    if (asset.type == MediaType::Video || asset.type == MediaType::Audio) {
        m_audio.preloadAudioAsync(asset.filePath);
    }
    if (asset.type == MediaType::Video) {
        m_media.requestFrameAsync(asset.filePath, asset.type, 0.0);
    }
    if (m_engine.project().clips.empty()) {
        m_engine.addClipFromAsset(aid, m_engine.defaultTrackForMedia(asset.type), 0.0);
        double availW = std::max(300.0, static_cast<double>(m_width - kTrackHeaderW) * 0.65);
        m_pixelsPerSec = std::clamp(availW / std::max(1.0, asset.durationSec), 20.0, 160.0);
        m_scrollSec = 0.0;
    }
    showToast(L"Imported: " + utf8ToWide(asset.name));
}

void MainWindow::actionSplitSelected() {
    uint64_t id = m_engine.selectedClipId();
    if (id == 0) { showToast(L"Select a clip to split at the playhead."); return; }
    if (m_engine.splitClipAt(id, m_engine.playhead()) != 0) showToast(L"Clip split at playhead.");
    else showToast(L"Move playhead inside the selected clip to split.");
}

void MainWindow::actionDeleteSelected() {
    uint64_t id = m_engine.selectedClipId();
    if (id != 0 && m_engine.deleteClip(id)) {
        m_previewCache.erase(id);
        showToast(L"Deleted selected clip.");
    }
}

void MainWindow::actionDuplicateSelected() {
    uint64_t id = m_engine.selectedClipId();
    if (id != 0 && m_engine.duplicateClip(id) != 0) showToast(L"Duplicated clip.");
}

void MainWindow::actionToggleKeyframe() {
    uint64_t id = m_engine.selectedClipId();
    if (id == 0) { showToast(L"Select a clip first to add a Keyframe."); return; }
    if (m_engine.hasKeyframeNearPlayhead(id)) {
        m_engine.removeKeyframeNearPlayhead(id);
        showToast(L"Removed Keyframe at playhead.");
    } else {
        m_engine.addOrUpdateKeyframeAtPlayhead(id);
        showToast(L"Added Keyframe! Move playhead & adjust Scale/Pos to animate.");
    }
}

bool MainWindow::promptTextDialog(const std::wstring& title, std::wstring& ioText) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = &TextPromptProc;
        wc.hInstance = m_hInst;
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = L"CapCutVSTextPrompt";
        RegisterClassW(&wc);
        registered = true;
    }

    TextPromptState st;
    st.result = ioText;

    RECT pr{};
    GetWindowRect(m_hwnd, &pr);
    int dlgW = 416, dlgH = 145;
    int x = pr.left + ((pr.right - pr.left) - dlgW) / 2;
    int y = pr.top  + ((pr.bottom - pr.top) - dlgH) / 2;

    EnableWindow(m_hwnd, FALSE);
    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"CapCutVSTextPrompt", title.c_str(),
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, dlgW, dlgH, m_hwnd, nullptr, m_hInst, &st);

    MSG msg{};
    while (IsWindow(hDlg) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) { SendMessageW(hDlg, WM_COMMAND, IDOK, 0); continue; }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { SendMessageW(hDlg, WM_COMMAND, IDCANCEL, 0); continue; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    EnableWindow(m_hwnd, TRUE);
    SetForegroundWindow(m_hwnd);
    if (st.accepted) { ioText = st.result; return true; }
    return false;
}

void MainWindow::actionEditSelectedText() {
    Clip* c = m_engine.selectedClip();
    if (!c || c->type != MediaType::Text) return;
    std::wstring current = utf8ToWide(c->text.content);
    if (promptTextDialog(L"Edit Text Overlay", current)) {
        m_engine.pushUndoSnapshot();
        c->text.content = wideToUtf8(current);
        c->name = c->text.content.empty() ? "Text" : c->text.content.substr(0, 24);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void MainWindow::actionStartExport() {
    if (m_exporting.load()) return;
    if (m_engine.project().clips.empty()) {
        showToast(L"Add at least one clip to the Timeline before exporting.");
        return;
    }

    wchar_t fileBuf[MAX_PATH] = {};
    if (m_exportCodec == 5) wcscpy_s(fileBuf, L"NovaCut_Export.gif");
    else if (m_exportCodec == 6 || m_exportCodec == 7) wcscpy_s(fileBuf, L"NovaCut_Sequence.png");
    else wcscpy_s(fileBuf, m_exportMov ? L"NovaCut_Export.mov" : L"NovaCut_Export.mp4");
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = m_exportMov ? L"MOV Video (*.mov)\0*.mov\0MP4 Video (*.mp4)\0*.mp4\0"
                                  : L"MP4 Video (*.mp4)\0*.mp4\0MOV Video (*.mov)\0*.mov\0";
    ofn.lpstrDefExt = m_exportMov ? L"mov" : L"mp4";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&ofn)) return;

    if (m_exportThread.joinable()) m_exportThread.join();

    Project snapshot = m_engine.project();
    ExportSettings settings;
    settings.outputPath = wideToUtf8(fileBuf);
    const int bitrates[5] = { 3, 8, 16, 32, 64 };
    settings.width = snapshot.canvasWidth;
    settings.height = snapshot.canvasHeight;
    settings.fps = static_cast<int>(snapshot.fps);
    settings.bitrateMbps = bitrates[m_bitrateIdx % 5];
    settings.formatMov = m_exportMov;
    settings.hdrMode = m_hdrMode;
    settings.codec = m_exportCodec;
    settings.variableFps = m_exportVfr;
    settings.audioSampleRate = (m_exportAudioCfg == 2) ? 44100 : 48000;
    settings.audioBitDepth = (m_exportAudioCfg == 1) ? 24 : 16;
    settings.colorSpace = m_exportColorSp;
    double totalDur = m_engine.totalDuration();

    setPlaying(false);
    m_exporting.store(true);
    m_exportProgress.store(0.0f);
    {
        std::lock_guard<std::mutex> lk(m_exportMutex);
        m_exportStatus = "Starting FFmpeg encoder...";
        m_exportError.clear();
    }

    m_exportThread = std::thread([this, snapshot = std::move(snapshot), settings, totalDur]() {
        std::string err;
        bool ok = FFmpegExporter::exportProject(
            snapshot, settings, totalDur,
            [this](float p, const std::string& st) {
                m_exportProgress.store(p);
                std::lock_guard<std::mutex> lk(m_exportMutex);
                m_exportStatus = st;
            },
            err);

        m_exporting.store(false);
        if (ok) showToast(L"Export complete: " + utf8ToWide(settings.outputPath));
        else showToast(L"Export failed: " + utf8ToWide(err));
    });
}


void MainWindow::actionExportCubeLut() {
    Clip* c = m_engine.selectedClip();
    if (!c) { showToast(tr(L"Select a video or image clip first.", L"اختر مقطع فيديو أو صورة أولاً.")); return; }
    wchar_t fileBuf[MAX_PATH] = L"NovaCut_Grade.cube";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"3D LUT (*.cube)\0*.cube\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"cube";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return;

    std::string outPath = wideToUtf8(fileBuf);
    if (MediaEngine::exportCubeLutFile(outPath, c->color, 17)) {
        showToast(tr(L"Exported 17x17x17 .cube 3D LUT successfully!", L"تم تصدير ملف 3D LUT (.cube) بنجاح!"));
    } else {
        showToast(tr(L"Failed to export .cube LUT.", L"فشل تصدير ملف LUT."));
    }
}

void MainWindow::actionAutoWhiteBalance() {
    Clip* c = m_engine.selectedClip();
    if (!c || !m_gradedFrame.valid()) {
        showToast(tr(L"Play or select a video/image clip first.", L"اختر مقطع فيديو أو صورة أولاً."));
        return;
    }
    float outTemp = 0.0f, outTint = 0.0f;
    MediaEngine::computeAutoWhiteBalance(m_gradedFrame, outTemp, outTint);
    m_engine.pushUndoSnapshot();
    c->color.temperature = std::clamp(c->color.temperature + outTemp, -0.6f, 0.6f);
    c->color.tint = std::clamp(c->color.tint + outTint, -0.6f, 0.6f);
    showToast(tr(L"Auto White Balance applied!", L"تم ضبط توازن اللون الأبيض تلقائياً!"));
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::updateVideoScopes(const FrameBGRA& frame) {
    m_histR.fill(0);
    m_histG.fill(0);
    m_histB.fill(0);
    m_histL.fill(0);
    m_waveBins.fill(0);
    m_vecBins.fill(0);
    if (!frame.valid()) return;

    const int w = frame.width;
    const int h = frame.height;
    const int stepY = std::max(1, h / 96);
    const int stepX = std::max(1, w / 128);
    for (int y = 0; y < h; y += stepY) {
        const uint8_t* row = frame.pixels.data() + static_cast<size_t>(y) * w * 4;
        for (int x = 0; x < w; x += stepX) {
            uint8_t b = row[x * 4 + 0];
            uint8_t g = row[x * 4 + 1];
            uint8_t r = row[x * 4 + 2];
            uint8_t l = static_cast<uint8_t>(std::clamp(static_cast<int>(0.299f * r + 0.587f * g + 0.114f * b), 0, 255));
            m_histR[r >> 2]++;
            m_histG[g >> 2]++;
            m_histB[b >> 2]++;
            m_histL[l >> 2]++;

            int wx = std::clamp((x * 64) / std::max(1, w), 0, 63);
            int wy = std::clamp(l >> 3, 0, 31);
            m_waveBins[static_cast<size_t>(wy) * 64 + wx]++;

            float cb = (-0.168736f * r - 0.331264f * g + 0.5f * b) / 128.0f;
            float cr = ( 0.5f * r - 0.418688f * g - 0.081312f * b) / 128.0f;
            int vx = std::clamp(static_cast<int>((cb * 0.5f + 0.5f) * 47.0f), 0, 47);
            int vy = std::clamp(static_cast<int>((-cr * 0.5f + 0.5f) * 47.0f), 0, 47);
            m_vecBins[static_cast<size_t>(vy) * 48 + vx]++;
        }
    }
    ++m_scopeStamp;
}

void MainWindow::renderVideoScopesOverlay() {
    if (m_scopeMode <= 0 || m_rcVideoCanvas.w < 160.0f || m_rcVideoCanvas.h < 120.0f) return;

    auto drawHistogramBox = [&](const UiRect& box) {
        drawRoundRect(box, 6.0f, D2D1::ColorF(0.05f, 0.06f, 0.09f, 0.86f), true, kColBorder, 1.0f);
        drawText(tr(L"HISTOGRAM (RGB)", L"هستوغرام الإضاءة"), { box.x + 6, box.y + 2, box.w - 12, 16 }, m_fontSmall, kColTextMuted);
        uint32_t maxV = 1;
        for (int i = 0; i < 64; ++i) maxV = std::max({ maxV, m_histR[i], m_histG[i], m_histB[i] });
        float padX = 6.0f, padTop = 20.0f, padBot = 6.0f;
        float gw = box.w - padX * 2.0f;
        float gh = box.h - padTop - padBot;
        float bw = gw / 64.0f;
        for (int i = 0; i < 64; ++i) {
            float x = box.x + padX + i * bw;
            float hr = (static_cast<float>(m_histR[i]) / maxV) * gh;
            float hg = (static_cast<float>(m_histG[i]) / maxV) * gh;
            float hb = (static_cast<float>(m_histB[i]) / maxV) * gh;
            m_brush->SetColor(D2D1::ColorF(1.0f, 0.25f, 0.25f, 0.55f));
            m_rt->FillRectangle(D2D1::RectF(x, box.y + padTop + gh - hr, x + bw, box.y + padTop + gh), m_brush.Get());
            m_brush->SetColor(D2D1::ColorF(0.25f, 1.0f, 0.35f, 0.50f));
            m_rt->FillRectangle(D2D1::RectF(x, box.y + padTop + gh - hg, x + bw, box.y + padTop + gh), m_brush.Get());
            m_brush->SetColor(D2D1::ColorF(0.25f, 0.55f, 1.0f, 0.50f));
            m_rt->FillRectangle(D2D1::RectF(x, box.y + padTop + gh - hb, x + bw, box.y + padTop + gh), m_brush.Get());
        }
    };

    auto drawWaveformBox = [&](const UiRect& box) {
        drawRoundRect(box, 6.0f, D2D1::ColorF(0.05f, 0.06f, 0.09f, 0.86f), true, kColBorder, 1.0f);
        drawText(tr(L"WAVEFORM (0-100 IRE)", L"مراقب الإضاءة (Waveform)"), { box.x + 6, box.y + 2, box.w - 12, 16 }, m_fontSmall, kColTextMuted);
        float padX = 6.0f, padTop = 20.0f, padBot = 6.0f;
        float gw = box.w - padX * 2.0f;
        float gh = box.h - padTop - padBot;
        // 25 / 50 / 75 IRE reference lines
        m_brush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f));
        for (int g = 1; g <= 3; ++g) {
            float gy = box.y + padTop + gh * (g * 0.25f);
            m_rt->DrawLine(D2D1::Point2F(box.x + padX, gy), D2D1::Point2F(box.x + padX + gw, gy), m_brush.Get(), 1.0f);
        }
        float cw = gw / 64.0f;
        float ch = gh / 32.0f;
        for (int wy = 0; wy < 32; ++wy) {
            for (int wx = 0; wx < 64; ++wx) {
                uint32_t cnt = m_waveBins[static_cast<size_t>(wy) * 64 + wx];
                if (cnt == 0) continue;
                float a = std::clamp(cnt * 0.28f, 0.20f, 0.95f);
                m_brush->SetColor(D2D1::ColorF(0.18f, 0.95f, 0.72f, a));
                float rx = box.x + padX + wx * cw;
                float ry = box.y + padTop + gh - (wy + 1) * ch;
                m_rt->FillRectangle(D2D1::RectF(rx, ry, rx + std::max(1.5f, cw), ry + std::max(1.5f, ch)), m_brush.Get());
            }
        }
    };

    auto drawVectorscopeBox = [&](const UiRect& box) {
        drawRoundRect(box, 6.0f, D2D1::ColorF(0.05f, 0.06f, 0.09f, 0.86f), true, kColBorder, 1.0f);
        drawText(tr(L"VECTORSCOPE (Cb/Cr)", L"مراقب الألوان (Vectorscope)"), { box.x + 6, box.y + 2, box.w - 12, 16 }, m_fontSmall, kColTextMuted);
        float cx = box.x + box.w * 0.5f;
        float cy = box.y + 18.0f + (box.h - 22.0f) * 0.5f;
        float rad = std::min(box.w - 16.0f, box.h - 26.0f) * 0.48f;
        m_brush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.18f));
        m_rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rad, rad), m_brush.Get(), 1.0f);
        m_rt->DrawLine(D2D1::Point2F(cx - rad, cy), D2D1::Point2F(cx + rad, cy), m_brush.Get(), 1.0f);
        m_rt->DrawLine(D2D1::Point2F(cx, cy - rad), D2D1::Point2F(cx, cy + rad), m_brush.Get(), 1.0f);
        // Cinema skin-tone line (approx 123 deg)
        m_brush->SetColor(D2D1::ColorF(1.0f, 0.65f, 0.45f, 0.45f));
        m_rt->DrawLine(D2D1::Point2F(cx, cy), D2D1::Point2F(cx - rad * 0.54f, cy - rad * 0.84f), m_brush.Get(), 1.0f);

        for (int vy = 0; vy < 48; ++vy) {
            for (int vx = 0; vx < 48; ++vx) {
                uint32_t cnt = m_vecBins[static_cast<size_t>(vy) * 48 + vx];
                if (cnt == 0) continue;
                float nx = (vx / 47.0f) * 2.0f - 1.0f;
                float ny = (vy / 47.0f) * 2.0f - 1.0f;
                float px = cx + nx * rad;
                float py = cy + ny * rad;
                float cr = -ny, cb = nx;
                float rC = std::clamp(0.55f + cr * 0.8f, 0.15f, 1.0f);
                float gC = std::clamp(0.55f - 0.35f * cr - 0.25f * cb, 0.15f, 1.0f);
                float bC = std::clamp(0.55f + cb * 0.8f, 0.15f, 1.0f);
                m_brush->SetColor(D2D1::ColorF(rC, gC, bC, std::clamp(cnt * 0.35f, 0.35f, 0.98f)));
                m_rt->FillRectangle(D2D1::RectF(px - 1.5f, py - 1.5f, px + 1.5f, py + 1.5f), m_brush.Get());
            }
        }
    };

    if (m_scopeMode == 4) {
        float gap = 6.0f;
        float sw = std::min(156.0f, (m_rcVideoCanvas.w - 32.0f) / 3.0f);
        float sh = std::min(112.0f, m_rcVideoCanvas.h * 0.36f);
        float y = m_rcVideoCanvas.y + m_rcVideoCanvas.h - sh - 8.0f;
        float x0 = m_rcVideoCanvas.x + m_rcVideoCanvas.w - (sw * 3.0f + gap * 2.0f) - 8.0f;
        drawHistogramBox({ x0, y, sw, sh });
        drawWaveformBox({ x0 + sw + gap, y, sw, sh });
        drawVectorscopeBox({ x0 + (sw + gap) * 2.0f, y, sw, sh });
    } else {
        float sw = std::min(210.0f, m_rcVideoCanvas.w * 0.42f);
        float sh = std::min(140.0f, m_rcVideoCanvas.h * 0.42f);
        UiRect box{ m_rcVideoCanvas.x + m_rcVideoCanvas.w - sw - 10.0f, m_rcVideoCanvas.y + m_rcVideoCanvas.h - sh - 10.0f, sw, sh };
        if (m_scopeMode == 1)      drawHistogramBox(box);
        else if (m_scopeMode == 2) drawWaveformBox(box);
        else if (m_scopeMode == 3) drawVectorscopeBox(box);
    }
}


void MainWindow::renderAudioSpectrumOverlay() {
    if (!m_showAudioSpectrum || m_rcVideoCanvas.w < 180.0f || m_rcVideoCanvas.h < 90.0f) return;
    std::array<float, 16> bands{};
    m_audio.getSpectrumBands(bands);

    float sw = std::min(220.0f, m_rcVideoCanvas.w * 0.40f);
    float sh = 74.0f;
    UiRect box{ m_rcVideoCanvas.x + 10.0f, m_rcVideoCanvas.y + m_rcVideoCanvas.h - sh - 10.0f, sw, sh };
    drawRoundRect(box, 6.0f, D2D1::ColorF(0.05f, 0.06f, 0.09f, 0.86f), true, kColBorder, 1.0f);
    drawText(tr(L"AUDIO SPECTRUM (16-BAND)", L"طيف الصوت الحي (16 نطاق)"), { box.x + 6, box.y + 2, box.w - 12, 16 }, m_fontSmall, kColTextMuted);

    float padX = 8.0f, padTop = 20.0f, padBot = 6.0f;
    float gw = box.w - padX * 2.0f;
    float gh = box.h - padTop - padBot;
    float bw = gw / 16.0f;
    for (int i = 0; i < 16; ++i) {
        float lvl = std::clamp(bands[i], 0.04f, 1.0f);
        float bh = lvl * gh;
        float bx = box.x + padX + i * bw + 1.0f;
        float by = box.y + padTop + gh - bh;
        D2D1_COLOR_F col = (lvl > 0.78f) ? kColDanger : ((lvl > 0.48f) ? kColGold : kColAccent);
        m_brush->SetColor(col);
        m_rt->FillRectangle(D2D1::RectF(bx, by, bx + std::max(2.0f, bw - 2.5f), box.y + padTop + gh), m_brush.Get());
    }
}


void MainWindow::actionBatchEdit() {
    Clip* src = m_engine.selectedClip();
    if (!src) { showToast(tr(L"Select a graded photo/clip first.", L"اختر صورة أو مقطعاً معدلاً أولاً.")); return; }
    m_engine.pushUndoSnapshot();
    int count = 0;
    for (auto& c : m_engine.project().clips) {
        if (c.id == src->id || (c.type != MediaType::Image && c.type != MediaType::Video)) continue;
        auto savedDots = c.color.brushDots;
        c.color = src->color;
        c.color.brushDots = std::move(savedDots);
        ++count;
    }
    showToast(tr(L"Batch Edit applied grade & retouch to all clips!", L"تم تعميم تعديلات الألوان والتصحيح على جميع الصور والمقاطع!"));
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::actionBatchExport() {
    std::filesystem::path outDir = std::filesystem::current_path() / "CapCutVS_BatchExport";
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    int exported = 0;
    for (const auto& c : m_engine.project().clips) {
        if (c.type != MediaType::Image && c.type != MediaType::Video) continue;
        double decSec = -999.0;
        auto framePtr = m_media.tryGetLatestFramePtr(c.sourcePath, c.type, c.sourceIn, decSec);
        if (!framePtr || !framePtr->valid()) continue;
        FrameBGRA outFrame;
        MediaEngine::applyPipeline(*framePtr, c.color, c.chromaKey, c.effect, outFrame,
                                   c.mask, c.transform.cropX, c.transform.cropY, c.maskFeather, nullptr);
        ++exported;
        std::string fname = (outDir / ("batch_photo_" + std::to_string(exported) + ".png")).string();
        m_media.saveFrameToPng(outFrame, fname);
    }
    if (exported > 0) {
        showToast(tr(L"Batch Exported PNGs to CapCutVS_BatchExport folder!", L"تم تصدير جميع الصور المعدلة إلى مجلد CapCutVS_BatchExport!"));
    } else {
        showToast(tr(L"No loaded photos/clips ready for Batch Export yet.", L"أضف صوراً أو مقاطع للتايملاين أولاً للتصدير الجماعي."));
    }
}


void MainWindow::actionAutoSaveDraft(bool silent) {
    std::filesystem::path dir = std::filesystem::current_path() / "CapCutVS_Drafts";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::string path = (dir / "autosave_draft.ccvs").string();
    std::string err;
    if (ProjectIO::saveToFile(path, m_engine.project(), err) && !silent) {
        showToast(tr(L"Draft Auto-Saved!", L"تم حفظ المسودة تلقائياً!"));
    }
}

void MainWindow::actionRecoverDraft() {
    std::filesystem::path p = std::filesystem::current_path() / "CapCutVS_Drafts" / "autosave_draft.ccvs";
    if (!std::filesystem::exists(p)) {
        showToast(tr(L"No Auto-Save draft found yet. Edit a project first.", L"لا توجد مسودة محفوظة تلقائياً بعد."));
        return;
    }
    Project loaded;
    std::string err;
    if (ProjectIO::loadFromFile(p.string(), loaded, err)) {
        m_engine.project() = std::move(loaded);
        m_audio.updateProjectSnapshot(m_engine.project());
        showToast(tr(L"Recovered last Auto-Saved project draft!", L"تمت استعادة آخر مسودة مشروع محفوظة تلقائياً!"));
        InvalidateRect(m_hwnd, nullptr, FALSE);
    } else {
        showToast(tr(L"Recovery failed: ", L"فشل الاستعادة: ") + utf8ToWide(err));
    }
}

void MainWindow::actionSaveVersionSnapshot() {
    std::filesystem::path dir = std::filesystem::current_path() / "CapCutVS_Drafts";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::string fname = "version_" + std::to_string(m_versionCounter++) + ".ccvs";
    std::string err;
    if (ProjectIO::saveToFile((dir / fname).string(), m_engine.project(), err)) {
        showToast(tr(L"Saved Version History snapshot: ", L"تم حفظ نسخة في سجل التعديلات: ") + utf8ToWide(fname));
    }
}

void MainWindow::actionCustomPreset(bool loadMode) {
    Clip* c = m_engine.selectedClip();
    if (!c) { showToast(tr(L"Select a clip first.", L"اختر مقطعاً أولاً.")); return; }
    if (!loadMode) {
        m_savedPresetColor = c->color;
        m_savedPresetFx = c->effect;
        m_hasCustomPreset = true;
        showToast(tr(L"Custom Preset Saved! Click 'LoadPre' on any clip to apply.", L"تم حفظ الإعدادات المخصصة! اضغط (تطبيق) على أي مقطع."));
    } else {
        m_engine.pushUndoSnapshot();
        c->color = m_savedPresetColor;
        c->effect = m_savedPresetFx;
        m_hasCustomPreset = false;
        showToast(tr(L"Custom Preset Applied! Click 'SavePre' to save a new one.", L"تم تطبيق الإعدادات المخصصة على المقطع!"));
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::actionCloudSyncBackup() {
    wchar_t fileBuf[MAX_PATH] = L"NovaCut_CloudSync.ccvs";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"CapCutVS Cloud Sync (*.ccvs)\0*.ccvs\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"ccvs";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return;
    std::string err;
    if (ProjectIO::saveToFile(wideToUtf8(fileBuf), m_engine.project(), err)) {
        showToast(tr(L"Project synced to cloud folder!", L"تمت مزامنة وحفظ نسخة المشروع في مجلد السحابة!"));
    }
}

void MainWindow::actionSearchAssets() {
    std::wstring q = utf8ToWide(m_assetSearchQuery);
    if (promptTextDialog(tr(L"Search Effects & Assets (leave empty to show all)", L"البحث عن المؤثرات والموارد (اتركه فارغاً لعرض الكل)"), q)) {
        m_assetSearchQuery = wideToUtf8(q);
        m_leftTab = LeftTab::Effects;
        m_fxCategory = 0;
        m_leftScrollY = 0.0f;
        showToast(m_assetSearchQuery.empty() ? tr(L"Search filter cleared.", L"تم مسح فلتر البحث.")
                                             : tr(L"Filtered FX by: ", L"تمت تصفية المؤثرات حسب: ") + q);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}


void MainWindow::actionAddToRenderQueue() {
    if (m_engine.project().clips.empty()) {
        showToast(tr(L"Add clips to Timeline before queuing.", L"أضف مقاطع للتايملاين قبل إضافتها لقائمة التصدير."));
        return;
    }
    const int bitrates[5] = { 3, 8, 16, 32, 64 };
    ExportSettings s;
    std::filesystem::path qDir = std::filesystem::current_path() / "CapCutVS_RenderQueue";
    std::error_code ec;
    std::filesystem::create_directories(qDir, ec);
    std::string ext = (m_exportCodec == 3) ? ".mov" : ".mp4";
    s.outputPath = (qDir / ("render_job_" + std::to_string(m_renderQueue.size() + 1) + ext)).string();
    s.width = m_engine.project().canvasWidth;
    s.height = m_engine.project().canvasHeight;
    s.fps = static_cast<int>(m_engine.project().fps);
    s.bitrateMbps = bitrates[m_bitrateIdx % 5];
    s.formatMov = m_exportMov;
    s.hdrMode = m_hdrMode;
    s.codec = m_exportCodec;
    s.variableFps = m_exportVfr;
    s.audioSampleRate = (m_exportAudioCfg == 2) ? 44100 : 48000;
    s.audioBitDepth = (m_exportAudioCfg == 1) ? 24 : 16;
    s.colorSpace = m_exportColorSp;
    m_renderQueue.push_back(std::move(s));
    showToast(tr(L"Added job to Render Queue!", L"تمت إضافة المهمة إلى قائمة التصدير!"));
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MainWindow::actionRunRenderQueue() {
    if (m_exporting.load()) return;
    if (m_renderQueue.empty()) {
        showToast(tr(L"Render Queue is empty. Click '+ Add Queue' first.", L"قائمة التصدير فارغة. اضغط (+ أضف للقائمة) أولاً."));
        return;
    }
    if (m_exportThread.joinable()) m_exportThread.join();
    auto jobs = m_renderQueue;
    m_renderQueue.clear();
    Project snapshot = m_engine.project();
    double totalDur = m_engine.totalDuration();
    m_exporting.store(true);
    m_exportProgress.store(0.0f);
    m_exportThread = std::thread([this, jobs = std::move(jobs), snapshot = std::move(snapshot), totalDur]() {
        size_t done = 0;
        for (const auto& job : jobs) {
            std::string err;
            FFmpegExporter::exportProject(snapshot, job, totalDur, [&](float p, const std::string& st) {
                float overall = (static_cast<float>(done) + p) / static_cast<float>(std::max<size_t>(1, jobs.size()));
                m_exportProgress.store(overall);
                std::lock_guard<std::mutex> lk(m_exportMutex);
                m_exportStatus = st;
            }, err);
            ++done;
        }
        m_exporting.store(false);
        showToast(tr(L"Batch Render Queue finished! Saved in CapCutVS_RenderQueue.", L"اكتمل تصيير جميع مهام قائمة التصدير داخل مجلد CapCutVS_RenderQueue!"));
        InvalidateRect(m_hwnd, nullptr, FALSE);
    });
}


void MainWindow::postUiTask(std::function<void()> fn) {
    std::lock_guard<std::mutex> lk(m_uiTaskMutex);
    m_pendingUiTasks.push_back(std::move(fn));
}

void MainWindow::runAiTaskAsync(const std::wstring& busyMsg, std::function<void()> bgWork) {
    if (m_aiBusy.exchange(true)) {
        showToast(tr(L"Another AI task is still running, please wait...", L"توجد مهمة ذكاء اصطناعي قيد التنفيذ حالياً، انتظر لحظة..."));
        return;
    }
    showToast(busyMsg);
    std::thread([this, work = std::move(bgWork)]() {
        work();
        m_aiBusy.store(false);
    }).detach();
}


void MainWindow::actionRestoreVersionHistory() {
    std::filesystem::path dir = std::filesystem::current_path() / "CapCutVS_Drafts";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    wchar_t fileBuf[MAX_PATH] = L"";
    std::wstring initDir = dir.wstring();
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrInitialDir = initDir.c_str();
    ofn.lpstrFilter = L"CapCutVS Version (*.ccvs)\0*.ccvs\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;

    Project loaded;
    std::string err;
    if (ProjectIO::loadFromFile(wideToUtf8(fileBuf), loaded, err)) {
        m_engine.pushUndoSnapshot();
        m_engine.project() = std::move(loaded);
        m_audio.updateProjectSnapshot(m_engine.project());
        showToast(tr(L"Restored project version snapshot!", L"تمت استعادة نسخة المشروع بنجاح!"));
        InvalidateRect(m_hwnd, nullptr, FALSE);
    } else {
        showToast(tr(L"Restore failed: ", L"فشل استعادة النسخة: ") + utf8ToWide(err));
    }
}

void MainWindow::actionGenerateDiskProxies() {
    Clip* sel = m_engine.selectedClip();
    if (!sel || sel->type != MediaType::Video || sel->sourcePath.empty()) {
        showToast(tr(L"Select a video clip first to generate a 360p Proxy.", L"اختر مقطع فيديو أولاً لتوليد ملف بروكسي خفيف 360p."));
        return;
    }
    uint64_t cid = sel->id;
    std::string src = sel->sourcePath;
    std::filesystem::path pDir = std::filesystem::current_path() / "CapCutVS_Proxies";
    std::error_code ec;
    std::filesystem::create_directories(pDir, ec);
    std::string outProxy = (pDir / ("proxy_" + std::to_string(cid) + ".mp4")).string();

    runAiTaskAsync(tr(L"Generating 640x360 Disk Proxy in background...", L"جاري توليد ملف بروكسي 640x360 خفيف في الخلفية..."), [this, cid, src, outProxy]() {
        std::string err;
        bool ok = FFmpegExporter::generateProxyFile(src, outProxy, err);
        postUiTask([this, cid, ok, outProxy, err]() {
            if (ok) {
                if (Clip* c = m_engine.findClip(cid)) c->proxyPath = outProxy;
                showToast(tr(L"Disk Proxy generated in CapCutVS_Proxies/!", L"تم توليد ملف البروكسي الخفيف في مجلد CapCutVS_Proxies!"));
            } else {
                showToast(tr(L"Proxy generation failed: ", L"فشل توليد البروكسي: ") + utf8ToWide(err));
            }
        });
    });
}

void MainWindow::actionRunObjectTrack(bool planarMode) {
    Clip* sel = m_engine.selectedClip();
    if (!sel || sel->type != MediaType::Video || sel->sourcePath.empty()) {
        showToast(tr(L"Select a video clip first to track.", L"اختر مقطع فيديو أولاً لتشغيل التتبع."));
        return;
    }
    uint64_t cid = sel->id;
    std::string src = sel->sourcePath;
    double sIn = sel->sourceIn;
    double dur = sel->duration;
    runAiTaskAsync(planarMode ? tr(L"Running 4-Point Planar Surface Tracker...", L"جاري تتبع السطح رباعي النقاط في الخلفية...")
                              : tr(L"Running Block-Matching Object Tracker...", L"جاري تتبع حركة العنصر تلقائياً في الخلفية..."),
        [this, cid, src, sIn, dur, planarMode]() {
            std::vector<Keyframe> kfs;
            bool ok = planarMode ? m_media.trackPlanarSurfacePath(src, sIn, dur, 8, kfs)
                                 : m_media.trackObjectPath(src, sIn, dur, 10, kfs);
            postUiTask([this, cid, ok, kfs, planarMode]() {
                if (ok) {
                    if (Clip* c = m_engine.findClip(cid)) {
                        m_engine.pushUndoSnapshot();
                        c->keyframes = kfs;
                    }
                    showToast(planarMode ? tr(L"Planar Tracking complete! Perspective keyframes applied.", L"اكتمل تتبع السطح وتم تطبيق مفاتيح المنظور!")
                                         : tr(L"Object Tracking complete! Motion keyframes applied.", L"اكتمل تتبع العنصر وتم تطبيق مسار الحركة!"));
                } else {
                    showToast(tr(L"Could not track frames on this clip.", L"تعذر تتبع الإطارات لهذا المقطع."));
                }
            });
        });
}


void MainWindow::actionDetectSceneCuts() {
    Clip* sel = m_engine.selectedClip();
    if (!sel || sel->type != MediaType::Video || sel->sourcePath.empty()) {
        showToast(tr(L"Select a video clip first for Scene Detection.", L"اختر مقطع فيديو أولاً لاكتشاف وتقطيع المشاهد."));
        return;
    }
    uint64_t cid = sel->id;
    std::string src = sel->sourcePath;
    double sIn = sel->sourceIn, dur = sel->duration, tlStart = sel->timelineStart;
    runAiTaskAsync(tr(L"Analyzing frames for Scene Cuts...", L"جاري تحليل الإطارات لاكتشاف تغير المشاهد..."), [this, cid, src, sIn, dur, tlStart]() {
        std::vector<double> cuts;
        bool ok = m_media.detectSceneCuts(src, sIn, dur, cuts);
        postUiTask([this, cid, ok, cuts, tlStart]() {
            if (!ok || cuts.empty()) {
                showToast(tr(L"No sharp scene cuts detected in this clip.", L"لم يتم رصد تغيرات مشهد حادة في هذا المقطع."));
                return;
            }
            m_engine.pushUndoSnapshot();
            for (double locT : cuts) {
                double absT = tlStart + locT;
                m_engine.project().markers.push_back(absT);
            }
            // Split from last cut backwards so earlier clip stays valid
            for (auto it = cuts.rbegin(); it != cuts.rend(); ++it) {
                m_engine.splitClipAt(cid, tlStart + *it);
            }
            showToast(tr(L"Scene Detection complete! Split clip & placed markers.", L"اكتمل كشف المشاهد وتم تقطيع المقطع ووضع العلامات تلقائياً!"));
        });
    });
}

void MainWindow::actionRunOpticalStabilization() {
    Clip* sel = m_engine.selectedClip();
    if (!sel || sel->type != MediaType::Video || sel->sourcePath.empty()) {
        showToast(tr(L"Select a video clip first to run Optical Stabilization.", L"اختر مقطع فيديو أولاً لتشغيل التثبيت البصري الحقيقي."));
        return;
    }
    uint64_t cid = sel->id;
    std::string src = sel->sourcePath;
    double sIn = sel->sourceIn, dur = sel->duration;
    runAiTaskAsync(tr(L"Computing Optical Shake Stabilization...", L"جاري حساب مسار اهتزاز الكاميرا وتثبيته بصرياً..."), [this, cid, src, sIn, dur]() {
        std::string outStab = "CapCutVS_AI/stabilized_" + std::to_string(cid) + ".mp4";
        std::string log;
        bool proOk = AICloudBridge::runNeuralPluginTask("stabilize_pro", src, outStab, "", log);
        std::vector<Keyframe> kfs;
        bool ok = proOk ? true : m_media.computeStabilizationKeyframes(src, sIn, dur, 12, kfs);
        postUiTask([this, cid, proOk, outStab, ok, kfs]() {
            if (proOk) {
                importFilePath(outStab);
                showToast(tr(L"Pro RANSAC Affine Stabilized video added to Timeline!", L"تم إنتاج الفيديو المثبّت احترافياً (RANSAC Affine) وإضافته للتايملاين!"));
            } else if (ok) {
                if (Clip* c = m_engine.findClip(cid)) {
                    m_engine.pushUndoSnapshot();
                    c->keyframes = kfs;
                    c->stabilization = StabilizationLevel::High;
                }
                showToast(tr(L"C++ Optical Stabilization applied (Run setup_plugins.bat for RANSAC Affine)", L"تم تطبيق التثبيت البصري المدمج (شغّل setup_plugins.bat لتفعيل RANSAC Affine)"));
            } else {
                showToast(tr(L"Could not stabilize this clip.", L"تعذر تحليل التثبيت لهذا المقطع."));
            }
        });
    });
}

} // namespace cc

#endif // _WIN32
