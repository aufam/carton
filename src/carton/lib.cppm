module;

#include <string>
#include <vector>
#include <unordered_map>

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

    static constexpr std::tuple __field_tags__{
        cpx::field<&Carton::package>      = "package",
        cpx::field<&Carton::registry>     = "registry     , skipmissing",
        cpx::field<&Carton::profiles>     = "profile      , skipmissing",
        cpx::field<&Carton::dependencies> = "dependencies , skipmissing",
        cpx::field<&Carton::lib>          = "lib          , skipmissing",
        cpx::field<&Carton::features>     = "features     , skipmissing",
    };

    Dependency bin;
    Carton    *pparent = nullptr;
    Cache     *cache   = nullptr;
    const Cli *cli     = nullptr;

    static int Update();
    static int Init(Package &args);

    void configure(const Profile &profile, const std::vector<std::string> &features = {}, bool from_registry = false);
    void build(const Profile &profile, std::vector<CompileCommand> &ccs, bool do_build);
    int  run();
    int  add(Dependency &dep) const;

private:
    void apply_package_placeholders();
    void resolve_remote_dep(const Profile &profile, Dependency &dep, bool from_registry = false);
    void collect_meta(const Profile &profile, Dependency &dep, bool is_bin = false);
};
