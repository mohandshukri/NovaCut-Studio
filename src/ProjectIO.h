#pragma once

#include "Types.h"
#include <string>

namespace cc {

class ProjectIO {
public:
    static std::string serializeToJson(const Project& project);
    static bool deserializeFromJson(const std::string& jsonText, Project& outProject, std::string& outError);

    static bool saveToFile(const std::string& filePathUtf8, const Project& project, std::string& outError);
    static bool loadFromFile(const std::string& filePathUtf8, Project& outProject, std::string& outError);
};

} // namespace cc
