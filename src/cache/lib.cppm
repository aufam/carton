module;

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>

export module carton:cache;
import :dependency;
import :compile_command;
import :fingerprint;

export struct Cache;
export struct ResolvedPackage;
export struct ResolvedDependency;

struct ResolvedPackage {
    std::string name;
    std::string version;

    // The manifest that produced this node.
    const void *carton = nullptr;

    // Direct dependencies of this package.
    std::vector<ResolvedDependency *> dependencies;
};

struct ResolvedDependency {
    std::string name;

    // Requested version from the parent manifest.
    std::string requested_version;

    // Actual package selected by the resolver.
    ResolvedPackage *package = nullptr;

    // Parent in the dependency tree.
    ResolvedPackage *parent = nullptr;
};

struct Cache {
    std::vector<Dependency *>                       dependencies;
    std::map<std::string, std::vector<std::string>> mods;
    std::map<std::string, std::string>              mod_paths;
    std::map<std::string, std::string>              mod_objs;
    std::unordered_map<std::string, std::string>    resolved_versions;
    std::unordered_map<std::string, std::string>    bmi_paths;
    std::string                                     directory;

    std::string common_flags;
    std::string module_compiler;
    bool        module_support = false;
    int         cppm_standard  = 0;

    std::unordered_map<std::string, std::unordered_map<std::string, Fingerprint>> fingerprint_map;

    std::unordered_map<std::string, uint64_t> hash_history;
};
