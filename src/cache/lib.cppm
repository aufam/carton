module;

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <cstdint>

export module carton:cache;
import :dependency;
import :compile_command;
import :fingerprint;

export struct Cache {
    /* modules related */
    std::map<std::string, std::vector<std::string>> mods;
    std::map<std::string, std::string>              mod_paths;
    std::map<std::string, std::string>              mod_objs;
    std::unordered_map<std::string, std::string>    bmi_paths;

    /// single version definition
    std::unordered_map<std::string, std::string> resolved_versions;

    /// cache directory
    std::string directory;

    /// common build flags
    std::string common_flags;

    /// c++ standard for all c++ modules
    int cppm_standard = 0;

    /// fingerprint maps with key is build directory
    std::unordered_map<std::string, std::unordered_map<std::string, Fingerprint>> fingerprint_map;

    /// hash map for given build
    std::unordered_map<std::string, uint64_t> hash_history;

    /// clangd compile commands to be collected
    std::vector<CompileCommand> compile_commands;
};
