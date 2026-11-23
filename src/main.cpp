#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include "MainWindow.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hrCom)) return -1;

    int exitCode = 0;
    {
        cc::MainWindow window;
        if (window.create(hInstance, nCmdShow)) {
            exitCode = window.runMessageLoop();
        } else {
            MessageBoxW(nullptr, L"Failed to initialize CapCutVS Direct2D window.",
                        L"CapCutVS Error", MB_ICONERROR | MB_OK);
            exitCode = -2;
        }
    }

    CoUninitialize();
    return exitCode;
}

#endif // _WIN32
