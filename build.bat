@echo off
setlocal
echo ============================================================
echo   NovaCut Studio v1.0 — Independent Pro NLE Release — Automated Release x64 Build Script
echo ============================================================

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] Visual Studio Installer not found. Please open CapCutVS.sln in Visual Studio 2022.
    pause
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    set "MSBUILD=%%i"
)

if "%MSBUILD%"=="" (
    echo [ERROR] MSBuild.exe not found. Please build CapCutVS.sln inside Visual Studio 2022.
    pause
    exit /b 1
)

echo [*] Using MSBuild: "%MSBUILD%"
"%MSBUILD%" "%~dp0CapCutVS.sln" /p:Configuration=Release /p:Platform=x64 /m /nologo

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build failed! Check messages above.
    pause
    exit /b 1
)

echo.
echo [OK] Build succeeded! Executable created at:
echo      %~dp0bin\Release\CapCutVS.exe
echo.
if exist "E:\Rea\CapCut\Apps\6.4.0.26\ffmpeg.exe" (
    echo [OK] FFmpeg detected at E:\Rea\CapCut\Apps\6.4.0.26\ffmpeg.exe
) else (
    echo [NOTE] For MP4 export, place ffmpeg.exe next to CapCutVS.exe or in PATH.
)
echo.
pause
