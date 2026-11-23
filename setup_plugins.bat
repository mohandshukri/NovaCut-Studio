@echo off
title CapCutVS V40 - Complete 13-Model Local AI Stack Installer
echo ====================================================================
echo   CapCutVS V40 - 13-Model Offline AI Stack Installer
echo ====================================================================
echo Installing Python AI engines:
echo   1. faster-whisper         (Whisper Speech-to-Text)
echo   2. rembg                  (BiRefNet / U2-Net Background Removal)
echo   3. opencv-python          (Stabilization, Homography, 6DOF, Inpaint)
echo   4. simple-lama-inpainting (LaMa AI Object Removal)
echo   5. demucs                 (Meta Vocal/Music Isolation)
echo   6. noisereduce + soundfile(Spectral Audio Restoration)
echo   7. mediapipe              (468-Point Face Mesh & Hand Tracking)
echo   8. transformers + torch   (Depth Anything V2)
echo   9. rawpy + imageio        (LibRaw 16-bit RAW/DNG Decoder)
echo.
pause
pip install faster-whisper rembg opencv-python pillow numpy noisereduce soundfile demucs rawpy imageio mediapipe simple-lama-inpainting transformers
echo.
echo [Optional Standalone GPU Executables -> Drop directly into CapCutVS_AI\]:
echo   - whisper-cli.exe + ggml-small.bin   (Whisper.cpp native)
echo   - realesrgan-ncnn-vulkan.exe         (Real-ESRGAN 4x GPU)
echo   - rife-ncnn-vulkan.exe               (RIFE Neural Frame Interp)
echo   - ffmpeg.exe (full build)            (ProRes 422 HQ / AV1 / VP9)
echo   - GFPGANv1.4.pth / CodeFormer / SAM2 / Wav2Lip
echo.
echo Done! Restart CapCutVS V40 to activate all installed models automatically.
pause
