# NovaCut Studio v1.3

![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11%20x64-00e5d1?style=flat-square)
![Language](https://img.shields.io/badge/C%2B%2B-C%2B%2B20-blue?style=flat-square)
![SDK](https://img.shields.io/badge/Dependencies-Zero%20External%20SDKs-brightgreen?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)

**NovaCut Studio** is an ultra-lightweight, native **C++20** desktop Video, Photo & Audio NLE Studio for Windows built exclusively on the **Windows SDK** (`Direct2D`, `DirectWrite`, `WIC`, `Windows Media Foundation`, `WinMM`) with **zero external C++ SDK dependencies**, **100% offline-first operation**, full **Arabic (RTL) & English** localization, and **Obsidian Dark / Studio Daylight** themes.

---

## Key Features | أبرز المميزات

### 1. Cinema NLE & Multi-Track Timeline
- Unlimited Video (`V1/V2+`), Text (`T1+`), and Audio (`A1+`) tracks with drag-and-drop import, ripple delete, snap, undo/redo, and timeline markers (`M`).
- Pro editing tools: **J-Cut / L-Cut**, **Slip / Slide Edit**, **Compound Clips**, **Nested Sequences (`.ccvs`)**, **Multi-Camera Switching**, **Scene Cut Detection**, **Custom Editable Speed Ramp (`Start / Mid / End`)**, **Keyframe Motion Curves (`Linear / EaseIn / EaseOut / Bezier / Bounce`)**, and **640×360 Disk Proxy Generator**.

### 2. Built-In Asset Library (`1,220` Instant-Insert Assets)
- **1,040 Stickers & Badges** across 10 popular creator categories and 8 visual styles (1-click direct insert).
- **120 Synthesized 48kHz Stereo WAV Sound Effects & Music Loops** (Whoosh, Pop, YouTube UI, Cinema Booms, Gaming, Lo-Fi/Trap/Synth Loops).
- **60 Cinema Color Filters**, **27 Procedural VFX** (Light Leaks, Smoke, Fire, Rain, Snow, Liquid, Fractal Noise, Chromatic Aberration, Depth Map, 3D Object), and **19 Transitions**.

### 3. Pro Color Grading & Live Video Scopes
- Real-time Direct2D **Histogram**, **Waveform (0–100 IRE)**, **Vectorscope (Cb/Cr + Skin-Tone Line)**, and **False Color IRE** exposure monitor.
- **Auto White Balance**, **8-Band HSL**, **Multi-Point RGB Curves**, **3-Way Color Wheels**, **Split Toning**, **RAW ACES Filmic Tone**, and **`.cube` 3D LUT Import & Export (`17×17×17`)**.

### 4. Pro Photo & Retouching Studio
- 10 interactive direct-on-canvas brushes: **Healing Brush**, **Clone Stamp (`Alt+Click` manual source)**, **Content-Aware Fill**, **Liquify (with Size & Pressure)**, **Dodge**, **Burn**, **Blur**, **Sharpen**, **Selection Brush**, and **Magnetic Lasso**.
- **10 Blend Modes**, **Projective Perspective Keystone Warp**, **Vector Polygon Masks**, **Bilateral Denoise**, **Batch Edit**, and **Batch PNG Export**.

### 5. Pro Audio DAW (100% Preview & Export Parity)
- **3-Band EQ (Bass/Mid/Treble)**, **Dynamic Compressor**, **Brickwall Limiter**, **2nd-Order Biquad 6.5kHz De-Esser**, **3-Tap Schroeder Room Reverb**, **250ms Echo**, **Stereo Width (M/S)**, **Audio Ducking**, **Silence Removal**, **Live 16-Band Goertzel Spectrum**, **Microphone Voiceover Recorder**, and **Windows SAPI TTS**.

### 6. Broadcast Export & Render Queue
- Native GPU-accelerated **H.264 / H.265 (HEVC) / AV1** via Windows Media Foundation, plus **Animated GIF**, **PNG Sequence**, **Alpha Transparent PNG**, **480P–4K UHD**, **CFR / VFR**, **3–64 Mbps Custom Bitrate**, **44.1/48kHz Audio (16/24-bit)**, **Rec.709 / sRGB / Rec.2020 HDR10 (PQ ST.2084) / SMPTE DCI-P3**, and **Multi-Job Render Queue**.
- Optional **ProRes 422 HQ / AV1 / VP9** broadcast transcoding when a full `ffmpeg.exe` is placed in `CapCutVS_AI/`.

### 7. Optional Offline Local AI & LM Studio Integration
- Connects out of the box to **LM Studio (`http://127.0.0.1:1234`)** or **Ollama (`127.0.0.1:11434`)** for offline script writing, subtitle translation, smart color mood grading, audio mastering, viral packs, and timeline director advice.
- Includes `setup_plugins.bat` and `CapCutVS_AI/ai_worker.py` to optionally plug in local **Whisper**, **BiRefNet/rembg**, **SAM 2**, **OpenCV**, **Real-ESRGAN**, **RIFE**, **LaMa Inpainting**, **Demucs**, **GFPGAN**, **CodeFormer**, **Depth Anything V2**, **MediaPipe**, **LibRaw (`rawpy`)**, and **Wav2Lip**.

---

## Quick Build (Visual Studio 2022) | طريقة البناء السريع

1. Install **Visual Studio 2022** with the **Desktop development with C++** workload (Windows 10/11 SDK).
2. Double-click **`build.bat`** (or open `CapCutVS.sln` and build `Release | x64`).
3. Run the compiled binary in `bin\Release\CapCutVS.exe`.
4. *(Optional)* Run **`setup_plugins.bat`** once to install optional offline Python neural AI plugins.

## Keyboard Shortcuts | اختصارات لوحة المفاتيح

| Shortcut | Action |
| :--- | :--- |
| `Space` | Play / Pause |
| `Ctrl + B` | Split clip at playhead |
| `Ctrl + Z` / `Ctrl + Y` | Undo / Redo |
| `Ctrl + D` | Duplicate selected clip |
| `Ctrl + F` | Search Effects |
| `Ctrl + S` | Save Project |
| `K` | Add / Update Keyframe |
| `M` | Mute / Unmute Clip |
| `B` | Toggle Before / After |
| `F1` / `F11` | Shortcuts / Fullscreen |
| `Esc` | Close active modal |
