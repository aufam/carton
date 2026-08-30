module;

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>

export module carton:cache;
import :dependency;
import :compile_command;
import :library;

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
    std::string                                     directory;

    ResolvedPackage                                  root;
    std::vector<std::unique_ptr<ResolvedPackage>>    resolved_packages;
    std::vector<std::unique_ptr<ResolvedDependency>> resolved_dependencies;
};
