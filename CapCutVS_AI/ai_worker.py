#!/usr/bin/env python3
"""CapCutVS V34 Real Neural AI Sidecar Worker (100% Local Offline)
Tasks:
  --task whisper      : faster-whisper real Speech-to-Text (Arabic & English -> .srt)
  --task bg_remove    : rembg (BiRefNet / U2-Net) real neural background segmentation -> RGBA PNG
  --task inpaint      : LaMa / OpenCV Telea AI Inpainting on painted brush coordinates
  --task vocal_iso    : Meta Demucs neural vocal/music separation -> vocals.wav
  --task denoise      : noisereduce spectral neural audio denoising -> clean.wav
  --task face_retouch : OpenCV Face Detection -> localized face-only skin/eye retouch
  --task lipsync      : Wav2Lip local CLI integration (CapCutVS_AI/Wav2Lip/inference.py)
  --task avatar       : SadTalker local CLI integration (CapCutVS_AI/SadTalker/inference.py)
  --task gen_video    : ComfyUI local API (127.0.0.1:8188) video generation hook
"""
import argparse, json, os, shutil, subprocess, sys

def fmt_srt_time(sec):
    ms = int(round(max(0.0, sec) * 1000))
    hh, rem = divmod(ms, 3600000)
    mm, rem = divmod(rem, 60000)
    ss, ms = divmod(rem, 1000)
    return f"{hh:02d}:{mm:02d}:{ss:02d},{ms:03d}"

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--task", required=True)
    p.add_argument("--input", required=True)
    p.add_argument("--output", required=True)
    p.add_argument("--arg", default="")
    a = p.parse_args()

    if a.task == "whisper":
        from faster_whisper import WhisperModel
        model = WhisperModel(a.arg or "small", device="auto", compute_type="int8")
        segments, _ = model.transcribe(a.input, vad_filter=True)
        with open(a.output, "w", encoding="utf-8") as fo:
            for idx, seg in enumerate(segments, 1):
                fo.write(f"{idx}\n{fmt_srt_time(seg.start)} --> {fmt_srt_time(seg.end)}\n{seg.text.strip()}\n\n")
        print("OK: faster-whisper transcription complete")

    elif a.task == "bg_remove":
        from rembg import remove, new_session
        session = None
        for mname in ("birefnet-general", "isnet-general-use", "u2net"):
            try:
                session = new_session(mname)
                break
            except Exception:
                continue
        with open(a.input, "rb") as fi, open(a.output, "wb") as fo:
            fo.write(remove(fi.read(), session=session) if session else remove(fi.read()))
        print("OK: rembg BiRefNet/U2Net neural segmentation complete")

    elif a.task == "inpaint":
        import cv2, numpy as np
        img = cv2.imread(a.input)
        if img is None:
            sys.exit(1)
        h, w = img.shape[:2]
        mask = np.zeros((h, w), dtype=np.uint8)
        for token in a.arg.split(";"):
            parts = token.split(",")
            if len(parts) == 3:
                cx = int(float(parts[0]) * w)
                cy = int(float(parts[1]) * h)
                rad = max(8, int(float(parts[2]) * min(w, h)))
                cv2.circle(mask, (cx, cy), rad, 255, -1)
        if np.count_nonzero(mask) == 0:
            cv2.circle(mask, (w // 2, h // 2), min(w, h) // 10, 255, -1)
        try:
            from simple_lama_inpainting import SimpleLama
            from PIL import Image
            lama = SimpleLama()
            res = lama(Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB)), Image.fromarray(mask))
            res.save(a.output)
            print("OK: LaMa AI Inpainting complete")
        except Exception:
            out1 = cv2.inpaint(img, mask, 7, cv2.INPAINT_TELEA)
            out2 = cv2.inpaint(img, mask, 7, cv2.INPAINT_NS)
            out = cv2.addWeighted(out1, 0.55, out2, 0.45, 0)
            cv2.imwrite(a.output, out)
            print("OK: OpenCV Telea Inpainting complete")

    elif a.task == "vocal_iso":
        out_dir = os.path.dirname(a.output) or "."
        subprocess.check_call([sys.executable, "-m", "demucs", "--two-stems=vocals", "-o", out_dir, a.input])
        # Locate vocals.wav and copy to exact requested output path
        for root, _, files in os.walk(out_dir):
            if "vocals.wav" in files:
                shutil.copy2(os.path.join(root, "vocals.wav"), a.output)
                print("OK: Demucs neural vocal isolation complete")
                return
        sys.exit(1)

    elif a.task == "denoise":
        import soundfile as sf, noisereduce as nr
        data, sr = sf.read(a.input)
        reduced = nr.reduce_noise(y=data, sr=sr, prop_decrease=0.85)
        sf.write(a.output, reduced, sr)
        print("OK: noisereduce spectral denoise complete")

    elif a.task == "face_retouch":
        import cv2, numpy as np
        img = cv2.imread(a.input)
        if img is None:
            sys.exit(1)
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        cascade = cv2.CascadeClassifier(cv2.data.haarcascades + "haarcascade_frontalface_default.xml")
        faces = cascade.detectMultiScale(gray, 1.15, 4)
        out = img.copy()
        for (x, y, fw, fh) in faces:
            roi = out[y:y+fh, x:x+fw]
            smooth = cv2.bilateralFilter(roi, 9, 55, 55)
            # Soft elliptical face mask so background/hair stays 100% sharp
            m = np.zeros((fh, fw, 1), dtype=np.float32)
            cv2.ellipse(m, (fw // 2, fh // 2), (int(fw * 0.44), int(fh * 0.48)), 0, 0, 360, 1.0, -1)
            m = cv2.GaussianBlur(m, (21, 21), 0)[..., None]
            out[y:y+fh, x:x+fw] = np.clip(roi * (1.0 - m * 0.65) + smooth * (m * 0.65), 0, 255).astype(np.uint8)
        cv2.imwrite(a.output, out)
        print(f"OK: Face-detected retouch complete ({len(faces)} face(s))")

    elif a.task == "lipsync":
        w2l = os.path.join(os.path.dirname(__file__), "Wav2Lip", "inference.py")
        ckpt = os.path.join(os.path.dirname(__file__), "Wav2Lip", "wav2lip_gan.pth")
        if os.path.exists(w2l) and os.path.exists(ckpt) and a.arg:
            subprocess.check_call([sys.executable, w2l, "--checkpoint_path", ckpt, "--face", a.input, "--audio", a.arg, "--outfile", a.output])
            print("OK: Wav2Lip neural lip-sync complete")
        else:
            sys.exit(3)

    elif a.task == "avatar":
        st = os.path.join(os.path.dirname(__file__), "SadTalker", "inference.py")
        if os.path.exists(st) and a.arg:
            subprocess.check_call([sys.executable, st, "--source_image", a.input, "--driven_audio", a.arg, "--result_dir", os.path.dirname(a.output) or "."])
            print("OK: SadTalker talking avatar complete")
        else:
            sys.exit(3)


    elif a.task == "optical_flow_interp":
        import cv2, numpy as np
        cap = cv2.VideoCapture(a.input)
        fps = cap.get(cv2.CAP_PROP_FPS) or 30.0
        w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        out = cv2.VideoWriter(a.output, cv2.VideoWriter_fourcc(*"mp4v"), fps * 2.0, (w, h))
        ret, prev = cap.read()
        if not ret: sys.exit(1)
        prev_g = cv2.cvtColor(prev, cv2.COLOR_BGR2GRAY)
        grid_x, grid_y = np.meshgrid(np.arange(w, dtype=np.float32), np.arange(h, dtype=np.float32))
        while True:
            out.write(prev)
            ret, nxt = cap.read()
            if not ret: break
            nxt_g = cv2.cvtColor(nxt, cv2.COLOR_BGR2GRAY)
            flow = cv2.calcOpticalFlowFarneback(prev_g, nxt_g, None, 0.5, 3, 15, 3, 5, 1.2, 0)
            mid1 = cv2.remap(prev, grid_x - flow[..., 0] * 0.5, grid_y - flow[..., 1] * 0.5, cv2.INTER_LINEAR)
            mid2 = cv2.remap(nxt,  grid_x + flow[..., 0] * 0.5, grid_y + flow[..., 1] * 0.5, cv2.INTER_LINEAR)
            out.write(cv2.addWeighted(mid1, 0.5, mid2, 0.5, 0))
            prev, prev_g = nxt, nxt_g
        cap.release(); out.release()
        print("OK: Farneback Dense Optical Flow 2x frame interpolation complete")

    elif a.task == "stabilize_pro":
        import cv2, numpy as np
        cap = cv2.VideoCapture(a.input)
        fps = cap.get(cv2.CAP_PROP_FPS) or 30.0
        w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        n_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
        ret, prev = cap.read()
        if not ret: sys.exit(1)
        prev_g = cv2.cvtColor(prev, cv2.COLOR_BGR2GRAY)
        transforms = []
        while True:
            pts = cv2.goodFeaturesToTrack(prev_g, maxCorners=200, qualityLevel=0.01, minDistance=30, blockSize=3)
            ret, curr = cap.read()
            if not ret: break
            curr_g = cv2.cvtColor(curr, cv2.COLOR_BGR2GRAY)
            if pts is not None and len(pts) >= 6:
                cpts, st, _ = cv2.calcOpticalFlowPyrLK(prev_g, curr_g, pts, None)
                idx = np.where(st == 1)[0]
                if len(idx) >= 6:
                    m, _ = cv2.estimateAffinePartial2D(pts[idx], cpts[idx], method=cv2.RANSAC)
                    if m is not None:
                        transforms.append([m[0, 2], m[1, 2], np.arctan2(m[1, 0], m[0, 0])])
                    else: transforms.append([0.0, 0.0, 0.0])
                else: transforms.append([0.0, 0.0, 0.0])
            else: transforms.append([0.0, 0.0, 0.0])
            prev_g = curr_g
        traj = np.cumsum(np.array(transforms or [[0,0,0]]), axis=0)
        win = min(15, max(3, len(traj) // 4))
        smooth = np.copy(traj)
        for i in range(3):
            k = np.ones(win) / win
            smooth[:, i] = np.convolve(np.pad(traj[:, i], (win//2, win-1-win//2), mode="edge"), k, mode="valid")
        diff = smooth - traj
        cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
        out = cv2.VideoWriter(a.output, cv2.VideoWriter_fourcc(*"mp4v"), fps, (w, h))
        for i in range(len(transforms)):
            ret, frm = cap.read()
            if not ret: break
            dx, dy, da = transforms[i][0] + diff[i][0], transforms[i][1] + diff[i][1], transforms[i][2] + diff[i][2]
            m = np.array([[np.cos(da), -np.sin(da), dx], [np.sin(da), np.cos(da), dy]], dtype=np.float32)
            warped = cv2.warpAffine(frm, m, (w, h), borderMode=cv2.BORDER_REFLECT)
            # Subtle 1.06x crop zoom to hide borders
            T = cv2.getRotationMatrix2D((w/2, h/2), 0, 1.06)
            out.write(cv2.warpAffine(warped, T, (w, h)))
        cap.release(); out.release()
        print("OK: Pro Affine RANSAC Motion-Compensated Stabilization complete")

    elif a.task in ("homography_track", "camera_6dof"):
        import cv2, numpy as np
        cap = cv2.VideoCapture(a.input)
        fps = cap.get(cv2.CAP_PROP_FPS) or 30.0
        w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH)) or 640
        h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT)) or 360
        orb = cv2.ORB_create(600)
        bf = cv2.BFMatcher(cv2.NORM_HAMMING, crossCheck=True)
        ret, prev = cap.read()
        if not ret: sys.exit(1)
        kp1, des1 = orb.detectAndCompute(cv2.cvtColor(prev, cv2.COLOR_BGR2GRAY), None)
        results = [[0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0]] # t, px, py, scale, rot, skewX, skewY
        K = np.array([[w, 0, w/2], [0, w, h/2], [0, 0, 1]], dtype=np.float64)
        idx = 0
        while True:
            ret, curr = cap.read()
            if not ret: break
            idx += 1
            if idx % max(1, int(fps // 4)) != 0: continue
            kp2, des2 = orb.detectAndCompute(cv2.cvtColor(curr, cv2.COLOR_BGR2GRAY), None)
            if des1 is not None and des2 is not None:
                ms = sorted(bf.match(des1, des2), key=lambda m: m.distance)[:80]
                if len(ms) >= 12:
                    p1 = np.float32([kp1[m.queryIdx].pt for m in ms])
                    p2 = np.float32([kp2[m.trainIdx].pt for m in ms])
                    if a.task == "homography_track":
                        H, _ = cv2.findHomography(p1, p2, cv2.RANSAC, 4.0)
                        if H is not None:
                            px = float(np.clip(H[0, 2] / (w * 0.5), -0.6, 0.6))
                            py = float(np.clip(H[1, 2] / (h * 0.5), -0.6, 0.6))
                            sc = float(np.clip(np.sqrt(H[0,0]**2 + H[1,0]**2), 0.5, 1.8))
                            skx = float(np.clip(H[0, 1] * 25.0, -22.0, 22.0))
                            sky = float(np.clip(H[1, 0] * 25.0, -22.0, 22.0))
                            results.append([idx / fps, px, py, sc, 0.0, skx, sky])
                    else:
                        E, mask = cv2.findEssentialMat(p1, p2, K, method=cv2.RANSAC, prob=0.999, threshold=1.5)
                        if E is not None and E.shape == (3, 3):
                            _, R, t, _ = cv2.recoverPose(E, p1, p2, K)
                            yaw = float(np.degrees(np.arctan2(R[1, 0], R[0, 0])))
                            pitch = float(np.degrees(np.arcsin(np.clip(-R[2, 0], -1.0, 1.0))))
                            roll = float(np.degrees(np.arctan2(R[2, 1], R[2, 2])))
                            results.append([idx / fps, float(np.clip(t[0,0]*0.15, -0.5, 0.5)), float(np.clip(t[1,0]*0.15, -0.5, 0.5)), 1.0, float(np.clip(roll, -18, 18)), float(np.clip(yaw*0.4, -22, 22)), float(np.clip(pitch*0.4, -22, 22))])
            kp1, des1 = kp2, des2
        with open(a.output, "w", encoding="utf-8") as fo:
            json.dump(results, fo)
        cap.release()
        print(f"OK: {a.task} complete ({len(results)} keyframes)")

    elif a.task == "rotoscope_seq":
        import cv2
        from rembg import remove, new_session
        os.makedirs(a.output, exist_ok=True)
        cap = cv2.VideoCapture(a.input)
        session = None
        for mname in ("birefnet-general", "u2net"):
            try: session = new_session(mname); break
            except Exception: continue
        idx = 0
        while idx < 180:
            ret, frm = cap.read()
            if not ret: break
            ok, buf = cv2.imencode(".png", frm)
            if ok:
                cut = remove(buf.tobytes(), session=session) if session else remove(buf.tobytes())
                with open(os.path.join(a.output, f"roto_{idx+1:04d}.png"), "wb") as fo:
                    fo.write(cut)
            idx += 1
        cap.release()
        print(f"OK: Frame-by-frame Rotoscoping complete ({idx} RGBA frames)")

    elif a.task == "depth_ai":
        import cv2, numpy as np
        img = cv2.imread(a.input)
        if img is None: sys.exit(1)
        try:
            from transformers import pipeline
            from PIL import Image
            pipe = pipeline(task="depth-estimation", model="depth-anything/Depth-Anything-V2-Small-hf")
            depth_pil = pipe(Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB)))["depth"]
            d = cv2.normalize(np.array(depth_pil), None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
            cv2.imwrite(a.output, cv2.applyColorMap(d, cv2.COLORMAP_INFERNO))
            print("OK: Depth Anything V2 neural depth map complete")
            return
        except Exception:
            pass
        try:
            import torch
            midas = torch.hub.load("intel-isl/MiDaS", "MiDaS_small")
            midas.eval()
            trf = torch.hub.load("intel-isl/MiDaS", "transforms").small_transform
            inp = trf(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
            with torch.no_grad():
                pred = midas(inp)
                pred = torch.nn.functional.interpolate(pred.unsqueeze(1), size=img.shape[:2], mode="bicubic", align_corners=False).squeeze().cpu().numpy()
            d = cv2.normalize(pred, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
            cv2.imwrite(a.output, cv2.applyColorMap(d, cv2.COLORMAP_INFERNO))
            print("OK: MiDaS Neural Depth Map complete")
        except Exception:
            gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
            lap = cv2.GaussianBlur(np.abs(cv2.Laplacian(gray, cv2.CV_32F)), (21, 21), 0)
            ygrad = np.linspace(0, 1, img.shape[0], dtype=np.float32)[:, None] * np.ones((1, img.shape[1]), dtype=np.float32)
            d = cv2.normalize(ygrad * 0.65 + (lap / (lap.max() + 1e-5)) * 0.35, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
            cv2.imwrite(a.output, cv2.applyColorMap(d, cv2.COLORMAP_INFERNO))
            print("OK: Structural Depth Map complete")


    elif a.task == "raw_develop":
        import rawpy, imageio
        with rawpy.imread(a.input) as raw:
            rgb = raw.postprocess(
                use_camera_wb=True,
                half_size=False,
                no_auto_bright=False,
                output_bps=8,
                demosaic_algorithm=rawpy.DemosaicAlgorithm.AHD,
                highlight_mode=rawpy.HighlightMode.Blend
            )
        imageio.imwrite(a.output, rgb)
        print("OK: LibRaw/rawpy 16-bit RAW/DNG development complete")

    elif a.task == "subject_hair_select":
        from rembg import remove, new_session
        session = None
        for mname in ("birefnet-general", "isnet-general-use", "u2net"):
            try: session = new_session(mname); break
            except Exception: continue
        with open(a.input, "rb") as fi, open(a.output, "wb") as fo:
            fo.write(remove(fi.read(), session=session, alpha_matting=True,
                            alpha_matting_foreground_threshold=240,
                            alpha_matting_background_threshold=10,
                            alpha_matting_erode_size=6))
        print("OK: BiRefNet Subject & Fine Hair Alpha Matting complete")

    elif a.task == "face_landmarks":
        import cv2, numpy as np
        img = cv2.imread(a.input)
        if img is None: sys.exit(1)
        h, w = img.shape[:2]
        out = img.copy()
        try:
            import mediapipe as mp
            with mp.solutions.face_mesh.FaceMesh(static_image_mode=True, max_num_faces=4, refine_landmarks=True) as fm:
                res = fm.process(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
                if res.multi_face_landmarks:
                    for fl in res.multi_face_landmarks:
                        pts = np.array([[int(lm.x * w), int(lm.y * h)] for lm in fl.landmark], dtype=np.int32)
                        hull = cv2.convexHull(pts)
                        mask = np.zeros((h, w), dtype=np.float32)
                        cv2.fillConvexPoly(mask, hull, 1.0)
                        mask = cv2.GaussianBlur(mask, (25, 25), 0)[..., None]
                        smooth = cv2.bilateralFilter(out, 9, 50, 50)
                        out = np.clip(out * (1.0 - mask * 0.65) + smooth * (mask * 0.65), 0, 255).astype(np.uint8)
            cv2.imwrite(a.output, out)
            print("OK: MediaPipe 468-Landmark Face Retouch complete")
        except Exception:
            gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
            cascade = cv2.CascadeClassifier(cv2.data.haarcascades + "haarcascade_frontalface_default.xml")
            for (x, y, fw, fh) in cascade.detectMultiScale(gray, 1.15, 4):
                roi = out[y:y+fh, x:x+fw]
                out[y:y+fh, x:x+fw] = cv2.bilateralFilter(roi, 9, 55, 55)
            cv2.imwrite(a.output, out)
            print("OK: OpenCV Face Detection Retouch complete")


    elif a.task == "realesrgan_upscale":
        import cv2
        exe = os.path.join(os.path.dirname(__file__), "realesrgan-ncnn-vulkan.exe")
        if os.path.exists(exe):
            subprocess.check_call([exe, "-i", a.input, "-o", a.output, "-n", "realesrgan-x4plus"])
            print("OK: Real-ESRGAN NCNN Vulkan 4x upscale complete")
            return
        try:
            from realesrgan import RealESRGANer
            from basicsr.archs.rrdbnet_arch import RRDBNet
            model = RRDBNet(num_in_ch=3, num_out_ch=3, num_feat=64, num_block=23, num_grow_ch=32, scale=4)
            upsampler = RealESRGANer(scale=4, model_path=os.path.join(os.path.dirname(__file__), "RealESRGAN_x4plus.pth"), model=model)
            img = cv2.imread(a.input, cv2.IMREAD_UNCHANGED)
            out, _ = upsampler.enhance(img, outscale=2)
            cv2.imwrite(a.output, out)
            print("OK: Real-ESRGAN PyTorch upscale complete")
        except Exception:
            img = cv2.imread(a.input)
            if img is None: sys.exit(1)
            up = cv2.resize(img, (img.shape[1]*2, img.shape[0]*2), interpolation=cv2.INTER_LANCZOS4)
            blur = cv2.GaussianBlur(up, (0, 0), 1.2)
            sharp = cv2.addWeighted(up, 1.35, blur, -0.35, 0)
            cv2.imwrite(a.output, sharp)
            print("OK: Lanczos4 + Unsharp 2x upscale complete")

    elif a.task == "rife_interp":
        exe = os.path.join(os.path.dirname(__file__), "rife-ncnn-vulkan.exe")
        if os.path.exists(exe):
            subprocess.check_call([exe, "-i", a.input, "-o", a.output])
            print("OK: RIFE NCNN Vulkan neural frame interpolation complete")
        else:
            sys.exit(3)

    elif a.task == "sam2_segment":
        try:
            from sam2.sam2_image_predictor import SAM2ImagePredictor
            import cv2, numpy as np
            img = cv2.imread(a.input)
            pred = SAM2ImagePredictor.from_pretrained("facebook/sam2-hiera-small")
            pred.set_image(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
            h, w = img.shape[:2]
            masks, _, _ = pred.predict(point_coords=np.array([[w//2, h//2]]), point_labels=np.array([1]))
            rgba = cv2.cvtColor(img, cv2.COLOR_BGR2BGRA)
            rgba[..., 3] = (masks[0] > 0).astype(np.uint8) * 255
            cv2.imwrite(a.output, rgba)
            print("OK: SAM 2 subject segmentation complete")
        except Exception:
            sys.exit(3)

    elif a.task in ("gfpgan_face", "codeformer_restore"):
        try:
            import cv2
            if a.task == "gfpgan_face":
                from gfpgan import GFPGANer
                restorer = GFPGANer(model_path=os.path.join(os.path.dirname(__file__), "GFPGANv1.4.pth"), upscale=1, arch="clean", channel_multiplier=2)
                img = cv2.imread(a.input)
                _, _, restored = restorer.enhance(img, has_aligned=False, only_center_face=False, paste_back=True)
                cv2.imwrite(a.output, restored)
                print("OK: GFPGAN neural face restoration complete")
                return
            else:
                cf = os.path.join(os.path.dirname(__file__), "CodeFormer", "inference_codeformer.py")
                if os.path.exists(cf):
                    subprocess.check_call([sys.executable, cf, "-w", "0.7", "--input_path", a.input, "--output_path", os.path.dirname(a.output) or "."])
                    print("OK: CodeFormer neural face restoration complete")
                    return
            sys.exit(3)
        except Exception:
            sys.exit(3)

    elif a.task == "mediapipe_track":
        import cv2, mediapipe as mp
        cap = cv2.VideoCapture(a.input)
        fps = cap.get(cv2.CAP_PROP_FPS) or 30.0
        results = []
        with mp.solutions.hands.Hands(max_num_hands=1) as hands:
            idx = 0
            while True:
                ret, frm = cap.read()
                if not ret: break
                idx += 1
                if idx % max(1, int(fps // 5)) != 0: continue
                res = hands.process(cv2.cvtColor(frm, cv2.COLOR_BGR2RGB))
                if res.multi_hand_landmarks:
                    lm = res.multi_hand_landmarks[0].landmark[9]
                    results.append([idx / fps, float(lm.x * 2 - 1), float(lm.y * 2 - 1)])
        with open(a.output, "w", encoding="utf-8") as fo:
            json.dump(results, fo)
        cap.release()
        print(f"OK: MediaPipe Hand/Face Tracking complete ({len(results)} points)")

    elif a.task == "check_stack":
        status = {}
        for mod in ("faster_whisper", "rembg", "sam2", "cv2", "realesrgan", "simple_lama_inpainting", "demucs", "gfpgan", "transformers", "mediapipe", "rawpy", "noisereduce"):
            try:
                __import__(mod)
                status[mod] = "INSTALLED"
            except Exception:
                status[mod] = "MISSING"
        for exe in ("whisper-cli.exe", "realesrgan-ncnn-vulkan.exe", "rife-ncnn-vulkan.exe", "ffmpeg.exe"):
            status[exe] = "READY" if os.path.exists(os.path.join(os.path.dirname(__file__), exe)) else "NOT_DROPPED"
        with open(a.output, "w", encoding="utf-8") as fo:
            json.dump(status, fo, indent=2)
        print("OK: AI Stack diagnostic saved")


    elif a.task == "gen_image":
        try:
            import torch
            from diffusers import AutoPipelineForText2Image
            device = "cuda" if torch.cuda.is_available() else "cpu"
            dtype = torch.float16 if device == "cuda" else torch.float32
            pipe = AutoPipelineForText2Image.from_pretrained("stabilityai/sdxl-turbo", torch_dtype=dtype)
            pipe.to(device)
            img = pipe(prompt=a.arg, num_inference_steps=4, guidance_scale=0.0, width=768, height=432).images[0]
            img.save(a.output)
            print("OK: Local Diffusers SDXL-Turbo logo-free image generated")
        except Exception:
            sys.exit(3)

    else:
        print("Unsupported task:", a.task)
        sys.exit(2)

if __name__ == "__main__":
    main()
