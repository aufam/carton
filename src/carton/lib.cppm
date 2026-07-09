module;

#include <string>
#include <vector>
#include <unordered_map>
#include "../macro.h"

export module carton:carton;
import :profile;
import :dependency;
import :package;
import :compile_command;
import :cache;
import :cli;
import :config;
import cpx;

export struct Carton {
    using Registry     = std::unordered_map<std::string, Carton>;
    using Dependencies = std::unordered_map<std::string, Dependency>;
    using Features     = std::unordered_map<std::string, std::vector<std::string>>;

    Package      package;
    Registry     registry;
    Profiles     profiles;
    Dependencies dependencies;
    Dependency   lib;
    Features     features;
    bool         no_default_features = false;

    Dependency bin;
    Carton    *pparent = nullptr;
    Cache     *cache   = nullptr;
    const Cli *cli     = nullptr;

    void configure(const Profile &profile, const std::vector<std::string> &features = {}, bool from_registry = false);
    void build(const Profile &profile, std::vector<CompileCommand> &ccs, bool do_build);
    int  run();

private:
    void apply_package_placeholders();
    void resolve_remote_dep(const Profile &profile, Dependency &dep, bool from_registry = false);
    void collect_meta(const Profile &profile, Dependency &dep, bool is_bin = false);
};

// clang-format off
CPX_REFLECT(
    (Carton, ),

    ((package            , "package"                         ))
    ((registry           , "registry           , skipmissing"))
    ((profiles           , "profile            , skipmissing"))
    ((dependencies       , "dependencies       , skipmissing"))
    ((lib                , "lib                , skipmissing"))
    ((features           , "features           , skipmissing"))
);
// clang-format on
