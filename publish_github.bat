@echo off
title NovaCut Studio - GitHub Quick Publish Helper
echo ============================================================
echo   NovaCut Studio v1.3 - GitHub Repository Setup
echo ============================================================
git init
git add .
git commit -m "Release NovaCut Studio v1.3 - Native C++20 Video, Photo & Audio NLE"
git branch -M main
echo.
set /p REPO_URL=Paste your GitHub Repository URL (e.g. https://github.com/USERNAME/NovaCut-Studio.git): 
if "%REPO_URL%"=="" (
    echo [!] No URL entered. You can push later with:
    echo     git remote add origin YOUR_URL
    echo     git push -u origin main
    pause
    exit /b 0
)
git remote remove origin 2>nul
git remote add origin "%REPO_URL%"
git push -u origin main
echo.
echo [OK] Published to GitHub! GitHub Actions will automatically build the Release x64 binary.
pause
