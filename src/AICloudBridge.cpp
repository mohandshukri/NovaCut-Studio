#include "AICloudBridge.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace cc {

namespace {

std::string urlEncode(const std::string& s) {
    std::ostringstream out;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out << static_cast<char>(c);
        } else if (c == ' ') {
            out << "%20";
        } else {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            out << buf;
        }
    }
    return out.str();
}

#ifdef _WIN32
std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), len);
    return w;
}

bool runHiddenPowerShell(const std::string& psScript, std::string& outStdout, DWORD timeoutMs = 20000) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;

    std::string fullCmd = "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -Command \"" + psScript + "\"";
    std::wstring wCmd = utf8ToWide(fullCmd);
    std::vector<wchar_t> buf(wCmd.begin(), wCmd.end());
    buf.push_back(L'\0');

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWrite);
    if (!ok) {
        CloseHandle(hRead);
        return false;
    }

    outStdout.clear();
    char rbuf[1024];
    DWORD nRead = 0;
    while (ReadFile(hRead, rbuf, sizeof(rbuf) - 1, &nRead, nullptr) && nRead > 0) {
        outStdout.append(rbuf, nRead);
    }

    WaitForSingleObject(pi.hProcess, timeoutMs);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode == 0;
}
#endif

} // namespace

AICloudConfig AICloudBridge::loadConfig() {
    AICloudConfig cfg;
    std::ifstream ifs("ai_config.txt");
    if (ifs) std::getline(ifs, cfg.geminiApiKey);
    while (!cfg.geminiApiKey.empty() && (cfg.geminiApiKey.back() == '\r' || cfg.geminiApiKey.back() == '\n' || cfg.geminiApiKey.back() == ' ')) {
        cfg.geminiApiKey.pop_back();
    }
    return cfg;
}

bool AICloudBridge::saveConfig(const AICloudConfig& cfg) {
    std::ofstream ofs("ai_config.txt", std::ios::trunc);
    if (!ofs) return false;
    ofs << cfg.geminiApiKey << "\n";
    return ofs.good();
}

bool AICloudBridge::checkInternetAvailable() {
#ifdef _WIN32
    std::string out;
    return runHiddenPowerShell("[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
                               "(Invoke-WebRequest -Uri 'https://www.google.com/generate_204' -UseBasicParsing -TimeoutSec 4).StatusCode",
                               out, 6000);
#else
    return false;
#endif
}

bool AICloudBridge::generateImageOnline(const std::string& promptUtf8, const std::string& outImagePathUtf8, std::string& outError, const std::string& geminiApiKey) {
    if (promptUtf8.empty()) { outError = "Prompt is empty."; return false; }
#ifdef _WIN32
    std::string safeP = promptUtf8;
    std::replace(safeP.begin(), safeP.end(), '\'', ' ');
    std::replace(safeP.begin(), safeP.end(), '"', ' ');

    // Priority 1: Local Stable Diffusion WebUI / Forge (http://127.0.0.1:7860) — 100% Offline & Logo-Free!
    {
        std::string psSd =
            "$body = @{ prompt='" + safeP + "'; width=1024; height=576; steps=20 } | ConvertTo-Json -Compress;"
            "try { $r = Invoke-RestMethod -Method Post -Uri 'http://127.0.0.1:7860/sdapi/v1/txt2img' -Body ([System.Text.Encoding]::UTF8.GetBytes($body)) -ContentType 'application/json' -TimeoutSec 45;"
            "if ($r.images -and $r.images.Count -gt 0) { [IO.File]::WriteAllBytes('" + outImagePathUtf8 + "', [Convert]::FromBase64String($r.images[0])); exit 0 } else { exit 1 } }"
            "catch { exit 1 }";
        std::string out;
        if (runHiddenPowerShell(psSd, out, 50000) && std::filesystem::exists(std::filesystem::u8path(outImagePathUtf8)) &&
            std::filesystem::file_size(std::filesystem::u8path(outImagePathUtf8)) > 1024) {
            return true;
        }
    }

    // Priority 2: Local Diffusers AI Worker (CapCutVS_AI/ai_worker.py --task gen_image) — 100% Offline & Logo-Free!
    {
        std::string log;
        if (runNeuralPluginTask("gen_image", "none", outImagePathUtf8, safeP, log) &&
            std::filesystem::exists(std::filesystem::u8path(outImagePathUtf8)) &&
            std::filesystem::file_size(std::filesystem::u8path(outImagePathUtf8)) > 1024) {
            return true;
        }
    }

    // Priority 3: Official Google Gemini Imagen 3 API (when Gemini API Key is set) — Clean & Logo-Free!
    if (!geminiApiKey.empty()) {
        std::string psGem =
            "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12;"
            "$body = @{ instances=@(@{ prompt='" + safeP + "' }); parameters=@{ sampleCount=1; aspectRatio='16:9' } } | ConvertTo-Json -Depth 5 -Compress;"
            "try { $r = Invoke-RestMethod -Method Post -Uri 'https://generativelanguage.googleapis.com/v1beta/models/imagen-3.0-generate-002:predict?key=" + geminiApiKey + "' -Body ([System.Text.Encoding]::UTF8.GetBytes($body)) -ContentType 'application/json; charset=utf-8' -TimeoutSec 30;"
            "if ($r.predictions -and $r.predictions[0].bytesBase64Encoded) { [IO.File]::WriteAllBytes('" + outImagePathUtf8 + "', [Convert]::FromBase64String($r.predictions[0].bytesBase64Encoded)); exit 0 } else { exit 1 } }"
            "catch { exit 1 }";
        std::string out;
        if (runHiddenPowerShell(psGem, out, 35000) && std::filesystem::exists(std::filesystem::u8path(outImagePathUtf8)) &&
            std::filesystem::file_size(std::filesystem::u8path(outImagePathUtf8)) > 1024) {
            return true;
        }
    }

    outError = "For 100% logo-free AI images: enter your Gemini API Key (Imagen 3) OR run Local Stable Diffusion (127.0.0.1:7860) / diffusers.";
    return false;
#else
    (void)outImagePathUtf8; (void)geminiApiKey;
    outError = "AI image generation requires Windows runtime.";
    return false;
#endif
}

std::vector<std::string> AICloudBridge::generateScriptLines(const std::string& topicUtf8, const std::string& geminiApiKey, std::string& outNote) {
    std::vector<std::string> lines;
#ifdef _WIN32
    if (!geminiApiKey.empty()) {
        std::string safeTopic = topicUtf8;
        std::replace(safeTopic.begin(), safeTopic.end(), '\'', ' ');
        std::replace(safeTopic.begin(), safeTopic.end(), '"', ' ');
        std::string ps =
            "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
            "$body = @{ contents = @(@{ parts = @(@{ text = 'Write 4 short punchy video script title lines (1 per line, no numbering) about: " + safeTopic + "' }) }) } | ConvertTo-Json -Depth 6; "
            "$res = Invoke-RestMethod -Method Post -Uri 'https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + geminiApiKey + "' -ContentType 'application/json' -Body $body -TimeoutSec 20; "
            "$res.candidates[0].content.parts[0].text";
        std::string out;
        if (runHiddenPowerShell(ps, out, 25000) && !out.empty()) {
            std::istringstream iss(out);
            std::string ln;
            while (std::getline(iss, ln)) {
                if (!ln.empty() && ln.back() == '\r') ln.pop_back();
                if (ln.size() > 2) lines.push_back(ln);
                if (lines.size() >= 4) break;
            }
            if (!lines.empty()) {
                outNote = "Generated online via Gemini AI!";
                return lines;
            }
        }
    }
#else
    (void)geminiApiKey;
#endif
    // Smart structured fallback if offline or no API key set
    std::string t = topicUtf8.empty() ? "Your Story" : topicUtf8;
    lines.push_back("HOOK: " + t);
    lines.push_back("THE PROBLEM & DISCOVERY");
    lines.push_back("STEP-BY-STEP REVEAL");
    lines.push_back("SUBSCRIBE FOR PART 2!");
    outNote = "Generated structured script (Add Gemini API Key in AI tab for live cloud AI).";
    return lines;
}

std::string AICloudBridge::translateTextOnline(const std::string& textUtf8, const std::string& targetLang, const std::string& geminiApiKey) {
#ifdef _WIN32
    if (textUtf8.empty()) return textUtf8;
    if (!geminiApiKey.empty()) {
        std::string safe = textUtf8;
        std::replace(safe.begin(), safe.end(), '\'', ' ');
        std::replace(safe.begin(), safe.end(), '"', ' ');
        std::string ps =
            "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
            "$body = @{ contents = @(@{ parts = @(@{ text = 'Translate to " + targetLang + " (return ONLY the translation): " + safe + "' }) }) } | ConvertTo-Json -Depth 6; "
            "$res = Invoke-RestMethod -Method Post -Uri 'https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + geminiApiKey + "' -ContentType 'application/json' -Body $body -TimeoutSec 15; "
            "$res.candidates[0].content.parts[0].text";
        std::string out;
        if (runHiddenPowerShell(ps, out, 20000) && !out.empty()) {
            while (!out.empty() && (out.back() == '\r' || out.back() == '\n' || out.back() == ' ')) out.pop_back();
            if (!out.empty()) return out;
        }
    }
#else
    (void)targetLang; (void)geminiApiKey;
#endif
    return textUtf8;
}


bool AICloudBridge::checkLocalServerOnline(const AICloudConfig& cfg, std::string& outModelsSummary) {
    outModelsSummary.clear();
#ifdef _WIN32
    // 1. Try LM Studio OpenAI-compatible Local Server (http://127.0.0.1:1234/v1/models) first!
    {
        std::string ep = cfg.localEndpoint.empty() ? "http://127.0.0.1:1234" : cfg.localEndpoint;
        std::string psLm =
            "$OutputEncoding=[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;"
            "try { $r = Invoke-RestMethod -Uri '" + ep + "/v1/models' -TimeoutSec 2;"
            "if ($r.data) { 'LM Studio: ' + (($r.data | ForEach-Object { $_.id }) -join ', ') } else { 'LM Studio READY' } }"
            "catch { exit 1 }";
        std::string out;
        if (runHiddenPowerShell(psLm, out, 4000)) {
            while (!out.empty() && (out.back() == '\r' || out.back() == '\n' || out.back() == ' ')) out.pop_back();
            outModelsSummary = out.empty() ? "LM Studio Connected" : out;
            return true;
        }
    }
    // 2. Fallback: Try Ollama Local Server (http://127.0.0.1:11434/api/tags)
    {
        std::string psOl =
            "$OutputEncoding=[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;"
            "try { $r = Invoke-RestMethod -Uri 'http://127.0.0.1:11434/api/tags' -TimeoutSec 2;"
            "if ($r.models) { 'Ollama: ' + (($r.models | ForEach-Object { $_.name }) -join ', ') } else { 'Ollama READY' } }"
            "catch { exit 1 }";
        std::string out;
        if (runHiddenPowerShell(psOl, out, 4000)) {
            while (!out.empty() && (out.back() == '\r' || out.back() == '\n' || out.back() == ' ')) out.pop_back();
            outModelsSummary = out.empty() ? "Ollama Connected" : out;
            return true;
        }
    }
#else
    (void)cfg;
#endif
    return false;
}

bool AICloudBridge::queryLocalModel(const AICloudConfig& cfg,
                                    const std::string& systemPromptUtf8,
                                    const std::string& userPromptUtf8,
                                    std::string& outReplyUtf8) {
    outReplyUtf8.clear();
#ifdef _WIN32
    std::filesystem::path tmpDir = std::filesystem::current_path() / "CapCutVS_AI";
    std::error_code ec;
    std::filesystem::create_directories(tmpDir, ec);
    std::filesystem::path sysTxt = tmpDir / "sys_in.txt";
    std::filesystem::path usrTxt = tmpDir / "usr_in.txt";
    {
        std::ofstream ofs1(sysTxt, std::ios::binary | std::ios::trunc);
        ofs1 << systemPromptUtf8;
        std::ofstream ofs2(usrTxt, std::ios::binary | std::ios::trunc);
        ofs2 << userPromptUtf8;
    }

    // 1. Try LM Studio OpenAI-compatible Chat Completions API (http://127.0.0.1:1234/v1/chat/completions)
    {
        std::string ep = cfg.localEndpoint.empty() ? "http://127.0.0.1:1234" : cfg.localEndpoint;
        std::string model = cfg.localModel.empty() ? "auto" : cfg.localModel;
        std::string psLm =
            "$OutputEncoding=[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;"
            "$s = Get-Content -Raw -Encoding UTF8 '" + sysTxt.string() + "';"
            "$u = Get-Content -Raw -Encoding UTF8 '" + usrTxt.string() + "';"
            "$m = '" + model + "';"
            "try {"
            "  if ($m -eq 'auto') { $ms = Invoke-RestMethod -Uri '" + ep + "/v1/models' -TimeoutSec 2; if ($ms.data -and $ms.data.Count -gt 0) { $m = $ms.data[0].id } }"
            "  $body = @{ model=$m; temperature=0.7; max_tokens=512; messages=@(@{role='system';content=$s},@{role='user';content=$u}) } | ConvertTo-Json -Depth 5 -Compress;"
            "  $r = Invoke-RestMethod -Method Post -Uri '" + ep + "/v1/chat/completions' -Body ([System.Text.Encoding]::UTF8.GetBytes($body)) -ContentType 'application/json; charset=utf-8' -TimeoutSec 18;"
            "  if ($r.choices -and $r.choices.Count -gt 0 -and $r.choices[0].message.content) { Write-Output $r.choices[0].message.content } else { exit 1 }"
            "} catch { exit 1 }";
        std::string out;
        if (runHiddenPowerShell(psLm, out, 45000) && !out.empty()) {
            outReplyUtf8 = out;
            return true;
        }
    }

    // 2. Fallback: Try Ollama Local API (http://127.0.0.1:11434/api/generate)
    {
        std::string model = (cfg.localModel.empty() || cfg.localModel == "auto") ? "gemma2:2b" : cfg.localModel;
        std::string psOl =
            "$OutputEncoding=[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;"
            "$s = Get-Content -Raw -Encoding UTF8 '" + sysTxt.string() + "';"
            "$u = Get-Content -Raw -Encoding UTF8 '" + usrTxt.string() + "';"
            "$body = @{ model='" + model + "'; prompt=($s + [Environment]::NewLine + $u); stream=$false } | ConvertTo-Json -Compress;"
            "try { $r = Invoke-RestMethod -Method Post -Uri 'http://127.0.0.1:11434/api/generate' -Body ([System.Text.Encoding]::UTF8.GetBytes($body)) -ContentType 'application/json; charset=utf-8' -TimeoutSec 18;"
            "if ($r.response) { Write-Output $r.response } else { exit 1 } }"
            "catch { exit 1 }";
        std::string out;
        if (runHiddenPowerShell(psOl, out, 40000) && !out.empty()) {
            outReplyUtf8 = out;
            return true;
        }
    }
#else
    (void)cfg; (void)systemPromptUtf8; (void)userPromptUtf8;
#endif
    return false;
}

std::vector<std::string> AICloudBridge::generateScriptLinesLocal(const AICloudConfig& cfg, const std::string& topicUtf8, std::string& outNote) {
    std::string reply;
    if (queryLocalModel(cfg,
            "You are a concise video script writer. Write exactly 4 short cinematic subtitle lines (one per line, no numbering, same language as the topic).",
            "Topic: " + topicUtf8, reply)) {
        std::vector<std::string> lines;
        std::istringstream iss(reply);
        std::string ln;
        while (std::getline(iss, ln)) {
            while (!ln.empty() && (ln.back() == '\r' || ln.back() == ' ')) ln.pop_back();
            if (ln.size() >= 3) lines.push_back(ln);
            if (lines.size() >= 5) break;
        }
        if (!lines.empty()) {
            outNote = "Local 2B AI (" + cfg.localModel + ")";
            return lines;
        }
    }
    return generateScriptLines(topicUtf8, cfg.geminiApiKey, outNote);
}

std::string AICloudBridge::translateTextLocal(const AICloudConfig& cfg, const std::string& textUtf8, const std::string& targetLang) {
    std::string reply;
    if (queryLocalModel(cfg,
            "Translate the following video subtitle cleanly into " + targetLang + ". Output ONLY the translation with no extra commentary.",
            textUtf8, reply)) {
        while (!reply.empty() && (reply.back() == '\r' || reply.back() == '\n' || reply.back() == ' ')) reply.pop_back();
        if (!reply.empty()) return reply;
    }
    return translateTextOnline(textUtf8, targetLang, cfg.geminiApiKey);
}

bool AICloudBridge::applySmartColorMood(const AICloudConfig& cfg, const std::string& moodUtf8, ColorAdjust& c, std::string& outSummary) {
    std::string lo = moodUtf8;
    for (char& ch : lo) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    // Try Local 2B Model first for nuanced custom moods (returns 6 floats: exp cont temp tint sat split)
    std::string reply;
    if (queryLocalModel(cfg,
            "Return ONLY 6 space-separated numbers for video color grading matching the mood: exposure(-0.4..0.4) contrast(0.8..1.4) temperature(-0.4..0.4) tint(-0.3..0.3) saturation(0.5..1.5) splitAmount(0..0.7).",
            moodUtf8, reply)) {
        std::istringstream iss(reply);
        float e = 0, ct = 1, tp = 0, ti = 0, sa = 1, sp = 0;
        if (iss >> e >> ct >> tp >> ti >> sa >> sp) {
            c.exposure    = std::clamp(e, -0.5f, 0.5f);
            c.contrast    = std::clamp(ct, 0.6f, 1.6f);
            c.temperature = std::clamp(tp, -0.5f, 0.5f);
            c.tint        = std::clamp(ti, -0.4f, 0.4f);
            c.saturation  = std::clamp(sa, 0.2f, 1.8f);
            c.splitAmount = std::clamp(sp, 0.0f, 0.8f);
            outSummary = "Local 2B AI Color Grade applied (" + cfg.localModel + ")";
            return true;
        }
    }

    // Instant built-in offline semantic color grading engine (Arabic & English)
    if (lo.find("warm") != std::string::npos || lo.find("sunset") != std::string::npos || moodUtf8.find("دافئ") != std::string::npos || moodUtf8.find("غروب") != std::string::npos) {
        c.temperature = 0.28f; c.contrast = 1.15f; c.saturation = 1.18f; c.splitShadowHue = 220.0f; c.splitHighHue = 34.0f; c.splitAmount = 0.42f;
        outSummary = "Applied Warm Sunset Cinema Grade";
    } else if (lo.find("cyber") != std::string::npos || lo.find("neon") != std::string::npos || moodUtf8.find("نيون") != std::string::npos || moodUtf8.find("ليلي") != std::string::npos) {
        c.temperature = -0.22f; c.tint = 0.18f; c.contrast = 1.24f; c.saturation = 1.32f; c.splitShadowHue = 205.0f; c.splitHighHue = 315.0f; c.splitAmount = 0.55f;
        outSummary = "Applied Cyberpunk Neon Night Grade";
    } else if (lo.find("cold") != std::string::npos || lo.find("doc") != std::string::npos || moodUtf8.find("بارد") != std::string::npos || moodUtf8.find("وثائقي") != std::string::npos) {
        c.temperature = -0.25f; c.contrast = 1.12f; c.saturation = 0.85f; c.clarity = 0.28f;
        outSummary = "Applied Cool Documentary Grade";
    } else if (lo.find("noir") != std::string::npos || lo.find("bw") != std::string::npos || moodUtf8.find("أبيض وأسود") != std::string::npos) {
        c.saturation = 0.0f; c.contrast = 1.32f; c.highlights = 0.15f; c.shadows = -0.18f;
        outSummary = "Applied Cinema B&W Noir Grade";
    } else {
        c.contrast = 1.16f; c.vibrance = 0.25f; c.highlights = 0.10f; c.shadows = 0.08f; c.splitShadowHue = 210.0f; c.splitHighHue = 38.0f; c.splitAmount = 0.30f;
        outSummary = "Applied Balanced Cinema Film Grade";
    }
    return true;
}

bool AICloudBridge::applySmartAudioGoal(const AICloudConfig& cfg, const std::string& goalUtf8, AudioDspSettings& d, std::string& outSummary) {
    (void)cfg;
    std::string lo = goalUtf8;
    for (char& ch : lo) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    if (lo.find("podcast") != std::string::npos || lo.find("voice") != std::string::npos || goalUtf8.find("بودكاست") != std::string::npos || goalUtf8.find("صوت") != std::string::npos) {
        d.noiseReduction = 0.55f; d.deEsser = 0.50f; d.compressor = 0.55f; d.limiter = 0.92f; d.bassBoost = 0.22f; d.treble = 0.18f; d.vocalIsolate = true;
        outSummary = "Podcast Studio Vocal Chain applied";
    } else if (lo.find("cinema") != std::string::npos || lo.find("epic") != std::string::npos || goalUtf8.find("سينما") != std::string::npos) {
        d.stereoWidth = 1.45f; d.reverb = 0.28f; d.bassBoost = 0.35f; d.compressor = 0.40f; d.limiter = 0.94f;
        outSummary = "Cinema Wide Stereo & Reverb applied";
    } else {
        d.noiseReduction = 0.65f; d.deEsser = 0.45f; d.compressor = 0.45f; d.limiter = 0.92f;
        outSummary = "Clean Broadcast Audio Master applied";
    }
    return true;
}

std::string AICloudBridge::generateViralPack(const AICloudConfig& cfg, const std::string& topicUtf8, const std::string& outTxtPath) {
    std::string reply;
    if (!queryLocalModel(cfg,
            "Generate a short viral video pack in the same language as the topic: 1) Catchy 5-word Hook, 2) YouTube/TikTok Title, 3) 5 Hashtags.",
            topicUtf8, reply)) {
        reply = "Hook: " + topicUtf8 + "!\nTitle: " + topicUtf8 + " - Full Story\nHashtags: #CapCutVS #VideoEditing #Cinema #Shorts #Viral";
    }
    std::ofstream ofs(std::filesystem::u8path(outTxtPath), std::ios::binary | std::ios::trunc);
    if (ofs) ofs << reply;
    // Return first line as Hook for T1 clip
    std::istringstream iss(reply);
    std::string firstLine;
    std::getline(iss, firstLine);
    while (!firstLine.empty() && (firstLine.back() == '\r' || firstLine.back() == ' ')) firstLine.pop_back();
    return firstLine.empty() ? topicUtf8 : firstLine;
}

std::string AICloudBridge::generateDirectorAdvice(const AICloudConfig& cfg, int clipCount, double totalSec, const std::string& outTxtPath) {
    std::string prompt = "Project has " + std::to_string(clipCount) + " clips and duration " + std::to_string(static_cast<int>(totalSec)) + " seconds. Give 3 short pro editing tips (pacing, audio, color).";
    std::string reply;
    if (!queryLocalModel(cfg, "You are a concise film editor giving 3 actionable bullet tips.", prompt, reply)) {
        reply = "1. Add J-Cut/L-Cut transitions between clips for smooth flow.\n2. Enable Audio Ducking + Podcast EQ for clear dialogue.\n3. Check Live Waveform & False Color before 4K Export.";
    }
    std::ofstream ofs(std::filesystem::u8path(outTxtPath), std::ios::binary | std::ios::trunc);
    if (ofs) ofs << reply;
    return reply;
}


bool AICloudBridge::transcribeAudioWhisperLocal(const std::string& mediaPathUtf8,
                                                double clipTimelineStart,
                                                std::vector<SubtitleEntry>& outSubs,
                                                std::string& outEngineNote) {
    outSubs.clear();
    outEngineNote.clear();
#ifdef _WIN32
    if (mediaPathUtf8.empty()) return false;
    std::filesystem::path aiDir = std::filesystem::current_path() / "CapCutVS_AI";
    std::error_code ec;
    std::filesystem::create_directories(aiDir, ec);
    std::filesystem::path srtOut = aiDir / "whisper_out.srt";
    std::filesystem::remove(srtOut, ec);

    // 0. Try CapCutVS_AI/ai_worker.py --task whisper (faster-whisper Arabic/English neural STT)
    {
        std::string wLog;
        if (runNeuralPluginTask("whisper", mediaPathUtf8, srtOut.string(), "small", wLog) && std::filesystem::exists(srtOut)) {
            std::ifstream ifs(srtOut);
            std::string line, textAcc;
            double sSec = 0.0, eSec = 0.0;
            auto parseSrtT = [](const std::string& ts) -> double {
                int hh = 0, mm = 0, ss = 0, ms = 0;
                if (ts.size() >= 12) {
                    hh = std::atoi(ts.substr(0, 2).c_str()); mm = std::atoi(ts.substr(3, 2).c_str());
                    ss = std::atoi(ts.substr(6, 2).c_str()); ms = std::atoi(ts.substr(9, 3).c_str());
                }
                return hh * 3600.0 + mm * 60.0 + ss + ms / 1000.0;
            };
            while (std::getline(ifs, line)) {
                while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
                if (line.find("-->") != std::string::npos) {
                    size_t pos = line.find("-->"); sSec = parseSrtT(line.substr(0, pos));
                    size_t p2 = pos + 3; while (p2 < line.size() && line[p2] == ' ') ++p2;
                    eSec = parseSrtT(line.substr(p2)); textAcc.clear();
                } else if (line.empty()) {
                    if (!textAcc.empty()) { outSubs.push_back({ clipTimelineStart + sSec, clipTimelineStart + std::max(sSec + 0.5, eSec), textAcc }); textAcc.clear(); }
                } else if (!std::all_of(line.begin(), line.end(), [](unsigned char ch){ return std::isdigit(ch) != 0; })) {
                    if (!textAcc.empty()) textAcc += " "; textAcc += line;
                }
            }
            if (!textAcc.empty()) outSubs.push_back({ clipTimelineStart + sSec, clipTimelineStart + std::max(sSec + 0.5, eSec), textAcc });
            if (!outSubs.empty()) { outEngineNote = "faster-whisper Local Neural STT"; return true; }
        }
    }

    // 1. Check for local whisper-cli.exe or whisper.exe + any .bin/.gguf model in CapCutVS_AI
    std::filesystem::path whisperExe;
    std::filesystem::path whisperModel;
    for (const char* cand : { "whisper-cli.exe", "whisper.exe", "main.exe" }) {
        if (std::filesystem::exists(aiDir / cand)) { whisperExe = aiDir / cand; break; }
    }
    if (std::filesystem::exists(aiDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(aiDir, ec)) {
            auto ext = entry.path().extension().string();
            if (ext == ".bin" || ext == ".gguf") { whisperModel = entry.path(); break; }
        }
    }

    if (!whisperExe.empty() && !whisperModel.empty()) {
        std::string ps =
            "$OutputEncoding=[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;"
            "& '" + whisperExe.string() + "' -m '" + whisperModel.string() + "' -f '" + mediaPathUtf8 + "' -l auto -osrt -of '" + (aiDir / "whisper_out").string() + "' 2>$null";
        std::string dummy;
        runHiddenPowerShell(ps, dummy, 60000);
        if (std::filesystem::exists(srtOut)) {
            std::ifstream ifs(srtOut);
            std::string line, textAcc;
            double sSec = 0.0, eSec = 0.0;
            auto parseSrtT = [](const std::string& ts) -> double {
                int hh = 0, mm = 0, ss = 0, ms = 0;
                if (ts.size() >= 12) {
                    hh = std::atoi(ts.substr(0, 2).c_str());
                    mm = std::atoi(ts.substr(3, 2).c_str());
                    ss = std::atoi(ts.substr(6, 2).c_str());
                    ms = std::atoi(ts.substr(9, 3).c_str());
                }
                return hh * 3600.0 + mm * 60.0 + ss + ms / 1000.0;
            };
            while (std::getline(ifs, line)) {
                while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
                if (line.find("-->") != std::string::npos) {
                    size_t pos = line.find("-->");
                    sSec = parseSrtT(line.substr(0, pos));
                    size_t p2 = pos + 3;
                    while (p2 < line.size() && line[p2] == ' ') ++p2;
                    eSec = parseSrtT(line.substr(p2));
                    textAcc.clear();
                } else if (line.empty()) {
                    if (!textAcc.empty()) {
                        outSubs.push_back({ clipTimelineStart + sSec, clipTimelineStart + std::max(sSec + 0.5, eSec), textAcc });
                        textAcc.clear();
                    }
                } else if (!std::all_of(line.begin(), line.end(), [](unsigned char ch){ return std::isdigit(ch) != 0; })) {
                    if (!textAcc.empty()) textAcc += " ";
                    textAcc += line;
                }
            }
            if (!textAcc.empty()) {
                outSubs.push_back({ clipTimelineStart + sSec, clipTimelineStart + std::max(sSec + 0.5, eSec), textAcc });
            }
            if (!outSubs.empty()) {
                outEngineNote = "Whisper Local STT (" + whisperModel.filename().string() + ")";
                return true;
            }
        }
    }

    // 2. Fallback: Real Windows Built-in Speech Recognition (System.Speech SAPI) for WAV files
    std::string ext = std::filesystem::u8path(mediaPathUtf8).extension().string();
    for (char& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (ext == ".wav") {
        std::string psSapi =
            "$OutputEncoding=[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;"
            "Add-Type -AssemblyName System.Speech;"
            "$rec = New-Object System.Speech.Recognition.SpeechRecognitionEngine;"
            "$rec.LoadGrammar((New-Object System.Speech.Recognition.DictationGrammar));"
            "$rec.SetInputToWaveFile('" + mediaPathUtf8 + "');"
            "$res = $rec.Recognize();"
            "if ($res -and $res.Text) { Write-Output $res.Text }";
        std::string out;
        if (runHiddenPowerShell(psSapi, out, 20000)) {
            while (!out.empty() && (out.back() == '\r' || out.back() == '\n' || out.back() == ' ')) out.pop_back();
            if (out.size() >= 2) {
                outSubs.push_back({ clipTimelineStart, clipTimelineStart + 4.0, out });
                outEngineNote = "Windows SAPI Speech-to-Text";
                return true;
            }
        }
    }
#else
    (void)mediaPathUtf8; (void)clipTimelineStart;
#endif
    return false;
}


bool AICloudBridge::runNeuralPluginTask(const std::string& taskName,
                                        const std::string& inputPathUtf8,
                                        const std::string& outputPathUtf8,
                                        const std::string& extraArgUtf8,
                                        std::string& outLog) {
    outLog.clear();
#ifdef _WIN32
    std::filesystem::path worker = std::filesystem::current_path() / "CapCutVS_AI" / "ai_worker.py";
    if (!std::filesystem::exists(worker)) {
        outLog = "CapCutVS_AI/ai_worker.py not found.";
        return false;
    }
    std::string ps =
        "$OutputEncoding=[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;"
        "python '" + worker.string() + "' --task '" + taskName + "' --input '" + inputPathUtf8 +
        "' --output '" + outputPathUtf8 + "' --arg '" + extraArgUtf8 + "' 2>&1";
    if (runHiddenPowerShell(ps, outLog, 120000)) {
        return std::filesystem::exists(std::filesystem::u8path(outputPathUtf8));
    }
#else
    (void)taskName; (void)inputPathUtf8; (void)outputPathUtf8; (void)extraArgUtf8;
#endif
    return false;
}

} // namespace cc
