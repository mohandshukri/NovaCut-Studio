#include "ProjectIO.h"

#include <sstream>
#include <fstream>
#include <iomanip>
#include <map>
#include <variant>
#include <cctype>
#include <filesystem>

namespace cc {

namespace {

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

struct JsonValue {
    using Array  = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data;

    bool isObject() const { return std::holds_alternative<Object>(data); }
    bool isArray()  const { return std::holds_alternative<Array>(data); }
    bool isString() const { return std::holds_alternative<std::string>(data); }
    bool isNumber() const { return std::holds_alternative<double>(data); }
    bool isBool()   const { return std::holds_alternative<bool>(data); }

    const Object& asObject() const { return std::get<Object>(data); }
    const Array&  asArray()  const { return std::get<Array>(data); }

    std::string getString(const std::string& key, const std::string& def = "") const {
        if (!isObject()) return def;
        const auto& obj = asObject();
        auto it = obj.find(key);
        if (it != obj.end() && it->second.isString()) return std::get<std::string>(it->second.data);
        return def;
    }

    double getDouble(const std::string& key, double def = 0.0) const {
        if (!isObject()) return def;
        const auto& obj = asObject();
        auto it = obj.find(key);
        if (it != obj.end() && it->second.isNumber()) return std::get<double>(it->second.data);
        return def;
    }

    int getInt(const std::string& key, int def = 0) const {
        return static_cast<int>(getDouble(key, static_cast<double>(def)));
    }

    uint64_t getU64(const std::string& key, uint64_t def = 0) const {
        double v = getDouble(key, static_cast<double>(def));
        return v < 0.0 ? 0ULL : static_cast<uint64_t>(v);
    }

    bool getBool(const std::string& key, bool def = false) const {
        if (!isObject()) return def;
        const auto& obj = asObject();
        auto it = obj.find(key);
        if (it != obj.end() && it->second.isBool()) return std::get<bool>(it->second.data);
        return def;
    }

    const JsonValue* getField(const std::string& key) const {
        if (!isObject()) return nullptr;
        const auto& obj = asObject();
        auto it = obj.find(key);
        return (it != obj.end()) ? &it->second : nullptr;
    }
};

class JsonParser {
public:
    explicit JsonParser(const std::string& s) : m_s(s), m_pos(0) {}

    bool parse(JsonValue& out, std::string& err) {
        skipWs();
        if (!parseValue(out, err)) return false;
        skipWs();
        return true;
    }

private:
    const std::string& m_s;
    size_t m_pos;

    void skipWs() {
        while (m_pos < m_s.size() && std::isspace(static_cast<unsigned char>(m_s[m_pos]))) ++m_pos;
    }

    bool parseValue(JsonValue& out, std::string& err) {
        skipWs();
        if (m_pos >= m_s.size()) { err = "Unexpected end of JSON"; return false; }
        char c = m_s[m_pos];
        if (c == '{') return parseObject(out, err);
        if (c == '[') return parseArray(out, err);
        if (c == '"') {
            std::string str;
            if (!parseString(str, err)) return false;
            out.data = std::move(str);
            return true;
        }
        if (c == 't' && m_s.compare(m_pos, 4, "true") == 0) {
            m_pos += 4; out.data = true; return true;
        }
        if (c == 'f' && m_s.compare(m_pos, 5, "false") == 0) {
            m_pos += 5; out.data = false; return true;
        }
        if (c == 'n' && m_s.compare(m_pos, 4, "null") == 0) {
            m_pos += 4; out.data = nullptr; return true;
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            return parseNumber(out, err);
        }
        err = std::string("Unexpected character: ") + c;
        return false;
    }

    bool parseObject(JsonValue& out, std::string& err) {
        ++m_pos; // '{'
        JsonValue::Object obj;
        skipWs();
        if (m_pos < m_s.size() && m_s[m_pos] == '}') {
            ++m_pos;
            out.data = std::move(obj);
            return true;
        }
        while (m_pos < m_s.size()) {
            skipWs();
            if (m_pos >= m_s.size() || m_s[m_pos] != '"') { err = "Expected string key"; return false; }
            std::string key;
            if (!parseString(key, err)) return false;
            skipWs();
            if (m_pos >= m_s.size() || m_s[m_pos] != ':') { err = "Expected ':'"; return false; }
            ++m_pos;
            JsonValue val;
            if (!parseValue(val, err)) return false;
            obj[std::move(key)] = std::move(val);
            skipWs();
            if (m_pos < m_s.size() && m_s[m_pos] == ',') { ++m_pos; continue; }
            if (m_pos < m_s.size() && m_s[m_pos] == '}') { ++m_pos; out.data = std::move(obj); return true; }
            err = "Expected ',' or '}'";
            return false;
        }
        err = "Unterminated object";
        return false;
    }

    bool parseArray(JsonValue& out, std::string& err) {
        ++m_pos; // '['
        JsonValue::Array arr;
        skipWs();
        if (m_pos < m_s.size() && m_s[m_pos] == ']') {
            ++m_pos;
            out.data = std::move(arr);
            return true;
        }
        while (m_pos < m_s.size()) {
            JsonValue val;
            if (!parseValue(val, err)) return false;
            arr.push_back(std::move(val));
            skipWs();
            if (m_pos < m_s.size() && m_s[m_pos] == ',') { ++m_pos; continue; }
            if (m_pos < m_s.size() && m_s[m_pos] == ']') { ++m_pos; out.data = std::move(arr); return true; }
            err = "Expected ',' or ']'";
            return false;
        }
        err = "Unterminated array";
        return false;
    }

    bool parseString(std::string& out, std::string& err) {
        ++m_pos; // '"'
        out.clear();
        while (m_pos < m_s.size()) {
            char c = m_s[m_pos++];
            if (c == '"') return true;
            if (c == '\\') {
                if (m_pos >= m_s.size()) { err = "Invalid escape"; return false; }
                char e = m_s[m_pos++];
                switch (e) {
                    case '"':  out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/'); break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'u': {
                        if (m_pos + 4 > m_s.size()) { err = "Invalid \\u escape"; return false; }
                        unsigned int code = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = m_s[m_pos++];
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= (h - '0');
                            else if (h >= 'a' && h <= 'f') code |= (h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= (h - 'A' + 10);
                            else { err = "Invalid hex in \\u"; return false; }
                        }
                        if (code < 0x80) {
                            out.push_back(static_cast<char>(code));
                        } else if (code < 0x800) {
                            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        } else {
                            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        }
                        break;
                    }
                    default: out.push_back(e); break;
                }
            } else {
                out.push_back(c);
            }
        }
        err = "Unterminated string";
        return false;
    }

    bool parseNumber(JsonValue& out, std::string& err) {
        size_t start = m_pos;
        if (m_s[m_pos] == '-') ++m_pos;
        while (m_pos < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[m_pos]))) ++m_pos;
        if (m_pos < m_s.size() && m_s[m_pos] == '.') {
            ++m_pos;
            while (m_pos < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[m_pos]))) ++m_pos;
        }
        if (m_pos < m_s.size() && (m_s[m_pos] == 'e' || m_s[m_pos] == 'E')) {
            ++m_pos;
            if (m_pos < m_s.size() && (m_s[m_pos] == '+' || m_s[m_pos] == '-')) ++m_pos;
            while (m_pos < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[m_pos]))) ++m_pos;
        }
        try {
            out.data = std::stod(m_s.substr(start, m_pos - start));
            return true;
        } catch (...) {
            err = "Invalid number";
            return false;
        }
    }
};

} // namespace

std::string ProjectIO::serializeToJson(const Project& p) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(4);
    os << "{\n";
    os << "  \"version\": \"1.0\",\n";
    os << "  \"name\": \"" << jsonEscape(p.name) << "\",\n";
    os << "  \"canvasWidth\": " << p.canvasWidth << ",\n";
    os << "  \"canvasHeight\": " << p.canvasHeight << ",\n";
    os << "  \"fps\": " << p.fps << ",\n";
    os << "  \"nextAssetId\": " << p.nextAssetId << ",\n";
    os << "  \"nextClipId\": " << p.nextClipId << ",\n";

    os << "  \"assets\": [\n";
    for (size_t i = 0; i < p.assets.size(); ++i) {
        const auto& a = p.assets[i];
        os << "    {"
           << "\"id\": " << a.id
           << ", \"name\": \"" << jsonEscape(a.name) << "\""
           << ", \"filePath\": \"" << jsonEscape(a.filePath) << "\""
           << ", \"type\": " << static_cast<int>(a.type)
           << ", \"durationSec\": " << a.durationSec
           << ", \"width\": " << a.width
           << ", \"height\": " << a.height
           << ", \"fps\": " << a.fps
           << ", \"hasAudio\": " << (a.hasAudio ? "true" : "false")
           << "}" << (i + 1 < p.assets.size() ? "," : "") << "\n";
    }
    os << "  ],\n";

    os << "  \"tracks\": [\n";
    for (size_t i = 0; i < p.tracks.size(); ++i) {
        const auto& t = p.tracks[i];
        os << "    {"
           << "\"index\": " << t.index
           << ", \"label\": \"" << jsonEscape(t.label) << "\""
           << ", \"type\": " << static_cast<int>(t.type)
           << ", \"muted\": " << (t.muted ? "true" : "false")
           << ", \"hidden\": " << (t.hidden ? "true" : "false")
           << ", \"locked\": " << (t.locked ? "true" : "false")
           << "}" << (i + 1 < p.tracks.size() ? "," : "") << "\n";
    }
    os << "  ],\n";

    os << "  \"clips\": [\n";
    for (size_t i = 0; i < p.clips.size(); ++i) {
        const auto& c = p.clips[i];
        os << "    {\n";
        os << "      \"id\": " << c.id
           << ", \"assetId\": " << c.assetId
           << ", \"trackIndex\": " << c.trackIndex
           << ", \"type\": " << static_cast<int>(c.type)
           << ", \"name\": \"" << jsonEscape(c.name) << "\""
           << ", \"sourcePath\": \"" << jsonEscape(c.sourcePath) << "\",\n";
        os << "      \"timelineStart\": " << c.timelineStart
           << ", \"sourceIn\": " << c.sourceIn
           << ", \"duration\": " << c.duration
           << ", \"speed\": " << c.speed
           << ", \"volume\": " << c.volume
           << ", \"muted\": " << (c.muted ? "true" : "false") << ", \"freezeFrame\": " << (c.freezeFrame ? "true" : "false") << ", \"reversed\": " << (c.reversed ? "true" : "false") << ", \"mask\": " << static_cast<int>(c.mask) << ", \"audioFadeIn\": " << c.audioFadeIn << ", \"audioFadeOut\": " << c.audioFadeOut << ",\n";
        os << "      \"transform\": {"
           << "\"posX\": " << c.transform.posX
           << ", \"posY\": " << c.transform.posY
           << ", \"scale\": " << c.transform.scale
           << ", \"rotationDeg\": " << c.transform.rotationDeg << ", \"flipH\": " << (c.transform.flipH ? "true" : "false") << ", \"flipV\": " << (c.transform.flipV ? "true" : "false") << ", \"cropX\": " << c.transform.cropX << ", \"cropY\": " << c.transform.cropY << "},\n";
        os << "      \"color\": {"
           << "\"brightness\": " << c.color.brightness
           << ", \"contrast\": " << c.color.contrast
           << ", \"saturation\": " << c.color.saturation
           << ", \"temperature\": " << c.color.temperature
           << ", \"opacity\": " << c.color.opacity
           << ", \"exposure\": " << c.color.exposure << ", \"vibrance\": " << c.color.vibrance << ", \"tint\": " << c.color.tint << ", \"highlights\": " << c.color.highlights << ", \"shadows\": " << c.color.shadows << ", \"sharpness\": " << c.color.sharpness << ", \"blur\": " << c.color.blur << ", \"lutPath\": \"" << jsonEscape(c.color.lutPath) << "\", \"preset\": " << static_cast<int>(c.color.preset) << "},\n";
        os << "      \"text\": {"
           << "\"content\": \"" << jsonEscape(c.text.content) << "\""
           << ", \"fontName\": \"" << jsonEscape(c.text.fontName) << "\""
           << ", \"fontSize\": " << c.text.fontSize
           << ", \"colorARGB\": " << c.text.colorARGB
           << ", \"bgARGB\": " << c.text.bgARGB
           << ", \"bold\": " << (c.text.bold ? "true" : "false")
           << ", \"shadow\": " << (c.text.shadow ? "true" : "false") << ", \"anim\": " << static_cast<int>(c.text.anim) << ", \"isStickerBadge\": " << (c.text.isStickerBadge ? "true" : "false") << "},\n";
        os << "      \"transitionIn\": " << static_cast<int>(c.transitionIn)
           << ", \"transitionDuration\": " << c.transitionDuration << ",\n";
        os << "      \"keyframes\": [";
        for (size_t k = 0; k < c.keyframes.size(); ++k) {
            const auto& kf = c.keyframes[k];
            os << "{\"t\": " << kf.localTime << ", \"x\": " << kf.posX << ", \"y\": " << kf.posY
               << ", \"s\": " << kf.scale << ", \"r\": " << kf.rotationDeg << ", \"o\": " << kf.opacity << "}"
               << (k + 1 < c.keyframes.size() ? ", " : "");
        }
        os << "],\n";
        os << "      \"chromaKey\": {"
           << "\"enabled\": " << (c.chromaKey.enabled ? "true" : "false")
           << ", \"keyColorRGB\": " << c.chromaKey.keyColorRGB
           << ", \"similarity\": " << c.chromaKey.similarity
           << ", \"smoothness\": " << c.chromaKey.smoothness
           << ", \"spillSuppress\": " << c.chromaKey.spillSuppress << "},\n";
        os << "      \"effect\": {"
           << "\"type\": " << static_cast<int>(c.effect.type)
           << ", \"intensity\": " << c.effect.intensity << "}\n";
        os << "    }" << (i + 1 < p.clips.size() ? "," : "") << "\n";
    }
    os << "  ]\n";
    os << "}\n";
    return os.str();
}

bool ProjectIO::deserializeFromJson(const std::string& jsonText, Project& outProject, std::string& outError) {
    JsonParser parser(jsonText);
    JsonValue root;
    if (!parser.parse(root, outError) || !root.isObject()) {
        if (outError.empty()) outError = "Root JSON must be an object";
        return false;
    }

    Project p;
    p.name         = root.getString("name", "Untitled Project");
    p.canvasWidth  = root.getInt("canvasWidth", 1920);
    p.canvasHeight = root.getInt("canvasHeight", 1080);
    p.fps          = root.getDouble("fps", 30.0);
    p.nextAssetId  = root.getU64("nextAssetId", 1);
    p.nextClipId   = root.getU64("nextClipId", 1);

    if (const JsonValue* assets = root.getField("assets"); assets && assets->isArray()) {
        for (const auto& av : assets->asArray()) {
            if (!av.isObject()) continue;
            MediaAsset a;
            a.id          = av.getU64("id", 0);
            a.name        = av.getString("name");
            a.filePath    = av.getString("filePath");
            a.type        = static_cast<MediaType>(av.getInt("type", 0));
            a.durationSec = av.getDouble("durationSec", 5.0);
            a.width       = av.getInt("width", 1920);
            a.height      = av.getInt("height", 1080);
            a.fps         = av.getDouble("fps", 30.0);
            a.hasAudio    = av.getBool("hasAudio", true);
            p.assets.push_back(std::move(a));
        }
    }

    if (const JsonValue* tracks = root.getField("tracks"); tracks && tracks->isArray()) {
        for (const auto& tv : tracks->asArray()) {
            if (!tv.isObject()) continue;
            Track t;
            t.index  = tv.getInt("index", 0);
            t.label  = tv.getString("label", "V1");
            t.type   = static_cast<TrackType>(tv.getInt("type", 1));
            t.muted  = tv.getBool("muted", false);
            t.hidden = tv.getBool("hidden", false);
            t.locked = tv.getBool("locked", false);
            p.tracks.push_back(std::move(t));
        }
    }
    if (p.tracks.empty()) p.initDefaultTracks();

    if (const JsonValue* clips = root.getField("clips"); clips && clips->isArray()) {
        for (const auto& cv : clips->asArray()) {
            if (!cv.isObject()) continue;
            Clip c;
            c.id            = cv.getU64("id", 0);
            c.assetId       = cv.getU64("assetId", 0);
            c.trackIndex    = cv.getInt("trackIndex", 2);
            c.type          = static_cast<MediaType>(cv.getInt("type", 0));
            c.name          = cv.getString("name");
            c.sourcePath    = cv.getString("sourcePath");
            c.timelineStart = cv.getDouble("timelineStart", 0.0);
            c.sourceIn      = cv.getDouble("sourceIn", 0.0);
            c.duration      = cv.getDouble("duration", 5.0);
            c.speed         = cv.getDouble("speed", 1.0);
            c.volume        = static_cast<float>(cv.getDouble("volume", 1.0));
            c.muted         = cv.getBool("muted", false);
            c.freezeFrame   = cv.getBool("freezeFrame", false);
            c.reversed      = cv.getBool("reversed", false);
            c.mask          = static_cast<MaskType>(cv.getInt("mask", 0));
            c.audioFadeIn   = static_cast<float>(cv.getDouble("audioFadeIn", 0.0));
            c.audioFadeOut  = static_cast<float>(cv.getDouble("audioFadeOut", 0.0));

            if (const JsonValue* tr = cv.getField("transform"); tr && tr->isObject()) {
                c.transform.posX        = static_cast<float>(tr->getDouble("posX", 0.0));
                c.transform.posY        = static_cast<float>(tr->getDouble("posY", 0.0));
                c.transform.scale       = static_cast<float>(tr->getDouble("scale", 1.0));
                c.transform.rotationDeg = static_cast<float>(tr->getDouble("rotationDeg", 0.0));
                c.transform.flipH       = tr->getBool("flipH", false);
                c.transform.flipV       = tr->getBool("flipV", false);
                c.transform.cropX       = static_cast<float>(tr->getDouble("cropX", 0.0));
                c.transform.cropY       = static_cast<float>(tr->getDouble("cropY", 0.0));
            }
            if (const JsonValue* co = cv.getField("color"); co && co->isObject()) {
                c.color.brightness  = static_cast<float>(co->getDouble("brightness", 0.0));
                c.color.contrast    = static_cast<float>(co->getDouble("contrast", 1.0));
                c.color.saturation  = static_cast<float>(co->getDouble("saturation", 1.0));
                c.color.temperature = static_cast<float>(co->getDouble("temperature", 0.0));
                c.color.opacity     = static_cast<float>(co->getDouble("opacity", 1.0));
                c.color.exposure    = static_cast<float>(co->getDouble("exposure", 0.0));
                c.color.vibrance    = static_cast<float>(co->getDouble("vibrance", 0.0));
                c.color.tint        = static_cast<float>(co->getDouble("tint", 0.0));
                c.color.highlights  = static_cast<float>(co->getDouble("highlights", 0.0));
                c.color.shadows     = static_cast<float>(co->getDouble("shadows", 0.0));
                c.color.sharpness   = static_cast<float>(co->getDouble("sharpness", 0.0));
                c.color.blur        = static_cast<float>(co->getDouble("blur", 0.0));
                c.color.lutPath     = co->getString("lutPath");
                c.color.preset      = static_cast<FilterPreset>(co->getInt("preset", 0));
            }
            if (const JsonValue* tx = cv.getField("text"); tx && tx->isObject()) {
                c.text.content   = tx->getString("content", "CapCutVS Title");
                c.text.fontName  = tx->getString("fontName", "Segoe UI");
                c.text.fontSize  = static_cast<float>(tx->getDouble("fontSize", 52.0));
                c.text.colorARGB = static_cast<uint32_t>(tx->getU64("colorARGB", 0xFFFFFFFF));
                c.text.bgARGB    = static_cast<uint32_t>(tx->getU64("bgARGB", 0x00000000));
                c.text.bold      = tx->getBool("bold", true);
                c.text.shadow    = tx->getBool("shadow", true);
                c.text.anim      = static_cast<TextAnimType>(tx->getInt("anim", 0));
                c.text.isStickerBadge = tx->getBool("isStickerBadge", false);
            }
            c.transitionIn = static_cast<TransitionType>(cv.getInt("transitionIn", 0));
            c.transitionDuration = cv.getDouble("transitionDuration", 0.6);
            if (const JsonValue* kfs = cv.getField("keyframes"); kfs && kfs->isArray()) {
                for (const auto& kv : kfs->asArray()) {
                    if (!kv.isObject()) continue;
                    Keyframe kf;
                    kf.localTime   = kv.getDouble("t", 0.0);
                    kf.posX        = static_cast<float>(kv.getDouble("x", 0.0));
                    kf.posY        = static_cast<float>(kv.getDouble("y", 0.0));
                    kf.scale       = static_cast<float>(kv.getDouble("s", 1.0));
                    kf.rotationDeg = static_cast<float>(kv.getDouble("r", 0.0));
                    kf.opacity     = static_cast<float>(kv.getDouble("o", 1.0));
                    c.keyframes.push_back(kf);
                }
            }
            if (const JsonValue* ck = cv.getField("chromaKey"); ck && ck->isObject()) {
                c.chromaKey.enabled       = ck->getBool("enabled", false);
                c.chromaKey.keyColorRGB   = static_cast<uint32_t>(ck->getU64("keyColorRGB", 0x00B140));
                c.chromaKey.similarity    = static_cast<float>(ck->getDouble("similarity", 0.32));
                c.chromaKey.smoothness    = static_cast<float>(ck->getDouble("smoothness", 0.12));
                c.chromaKey.spillSuppress = static_cast<float>(ck->getDouble("spillSuppress", 0.50));
            }
            if (const JsonValue* fx = cv.getField("effect"); fx && fx->isObject()) {
                c.effect.type      = static_cast<EffectType>(fx->getInt("type", 0));
                c.effect.intensity = static_cast<float>(fx->getDouble("intensity", 0.65));
            }
            p.clips.push_back(std::move(c));
        }
    }

    outProject = std::move(p);
    return true;
}

bool ProjectIO::saveToFile(const std::string& filePathUtf8, const Project& project, std::string& outError) {
    try {
        std::filesystem::path fp = std::filesystem::u8path(filePathUtf8);
        std::ofstream ofs(fp, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            outError = "Failed to open project file for writing.";
            return false;
        }
        std::string json = serializeToJson(project);
        ofs.write(json.data(), static_cast<std::streamsize>(json.size()));
        return ofs.good();
    } catch (const std::exception& ex) {
        outError = ex.what();
        return false;
    }
}

bool ProjectIO::loadFromFile(const std::string& filePathUtf8, Project& outProject, std::string& outError) {
    try {
        std::filesystem::path fp = std::filesystem::u8path(filePathUtf8);
        std::ifstream ifs(fp, std::ios::binary);
        if (!ifs) {
            outError = "Failed to open project file for reading.";
            return false;
        }
        std::ostringstream ss;
        ss << ifs.rdbuf();
        return deserializeFromJson(ss.str(), outProject, outError);
    } catch (const std::exception& ex) {
        outError = ex.what();
        return false;
    }
}

} // namespace cc
