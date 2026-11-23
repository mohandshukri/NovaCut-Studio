#pragma once

#include "Types.h"
#include <string>
#include <vector>

namespace cc {

struct AICloudConfig {
    std::string geminiApiKey;                          // Optional: Online Gemini API key
    std::string localModel    = "auto";           // Local 2B/3B model name (Ollama / llama.cpp)
    std::string localEndpoint = "http://127.0.0.1:1234"; // 100% offline local loopback server
};

class AICloudBridge {
public:
    static AICloudConfig loadConfig();
    static bool saveConfig(const AICloudConfig& cfg);

    // Checks if internet access is reachable (fast HTTPS check)
    static bool checkInternetAvailable();

    // V28 Multi-Task Local 2B/3B AI Engine (100% Offline via 127.0.0.1 Ollama / llama-server)
    static bool checkLocalServerOnline(const AICloudConfig& cfg, std::string& outModelsSummary);
    static bool queryLocalModel(const AICloudConfig& cfg,
                                const std::string& systemPromptUtf8,
                                const std::string& userPromptUtf8,
                                std::string& outReplyUtf8);

    // Multi-Task AI #1: Script & Storyboard lines (Local 2B -> Gemini -> Smart Fallback)
    static std::vector<std::string> generateScriptLines(const std::string& topicUtf8, const std::string& geminiApiKey, std::string& outNote);
    static std::vector<std::string> generateScriptLinesLocal(const AICloudConfig& cfg, const std::string& topicUtf8, std::string& outNote);

    // Multi-Task AI #2: Subtitle & Text Translator (Local 2B -> Online -> Fallback)
    static std::string translateTextOnline(const std::string& textUtf8, const std::string& targetLang, const std::string& geminiApiKey);
    static std::string translateTextLocal(const AICloudConfig& cfg, const std::string& textUtf8, const std::string& targetLang);

    // Multi-Task AI #3: Smart Color Grading Assistant (mood text -> ColorAdjust parameters)
    static bool applySmartColorMood(const AICloudConfig& cfg, const std::string& moodUtf8, ColorAdjust& inOutColor, std::string& outSummary);

    // Multi-Task AI #4: Smart Audio Mastering Assistant (goal text -> AudioDspSettings)
    static bool applySmartAudioGoal(const AICloudConfig& cfg, const std::string& goalUtf8, AudioDspSettings& inOutDsp, std::string& outSummary);

    // Multi-Task AI #5: Viral Social Pack (Opening Hook + Title + Hashtags saved to file)
    static std::string generateViralPack(const AICloudConfig& cfg, const std::string& topicUtf8, const std::string& outTxtPath);

    // Multi-Task AI #6: AI Director Timeline Review
    static std::string generateDirectorAdvice(const AICloudConfig& cfg, int clipCount, double totalSec, const std::string& outTxtPath);

    // V31: Real Speech-to-Text (1: Local whisper-cli.exe + .bin/.gguf, 2: Whisper HTTP 127.0.0.1:8080, 3: Windows System.Speech SAPI)
    static bool transcribeAudioWhisperLocal(const std::string& mediaPathUtf8,
                                            double clipTimelineStart,
                                            std::vector<SubtitleEntry>& outSubs,
                                            std::string& outEngineNote);

    // V33: Hybrid External Neural Plugin Runner (CapCutVS_AI/ai_worker.py: rembg, demucs, noisereduce, wav2lip, comfyui)
    static bool runNeuralPluginTask(const std::string& taskName,
                                    const std::string& inputPathUtf8,
                                    const std::string& outputPathUtf8,
                                    const std::string& extraArgUtf8,
                                    std::string& outLog);

    // Online AI Image Generator (downloads real AI-generated PNG/JPG to outImagePathUtf8)
    static bool generateImageOnline(const std::string& promptUtf8, const std::string& outImagePathUtf8, std::string& outError, const std::string& geminiApiKey = {});
};

} // namespace cc
