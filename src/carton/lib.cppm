module;

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>

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

    Registry   *pregistry = nullptr;
    std::string working_dir;
    std::string build_dir;
    bool        resolved = false;

    std::map<std::string, std::unique_ptr<Library>> libraries;

    Dependency bin;

    Carton                *root = nullptr;
    std::shared_ptr<Cache> cache;
    std::string            cache_dir;

    static int    Update();
    static int    Init(Package &args);
    static Carton New(const std::string &cache_dir);
    int           add(Dependency &dep);
    int           execute(Cli &cli);

private:
    void configure(const Profile &profile, const std::vector<std::string> &features = {}, bool from_registry = false);
    void build(const Profile &profile, std::vector<CompileCommand> &ccs, bool do_build);
    int  run(const std::vector<std::string> &args);

    void apply_package_placeholders();
    void resolve_remote_dep(const Profile &profile, Dependency &dep, bool from_registry = false);
    void collect_meta(const Profile &profile, Dependency &dep, bool is_bin = false);

    // v2
    auto configure_v2(const std::vector<std::string> &features = {}, bool default_features = true) -> std::vector<Library *>;

    auto
    get_requested_features(const std::vector<std::string> &features, bool default_features = true) -> std::vector<std::string>;

    void resolve_package();
    void resolve_dep(const std::string &name, Dependency &dep);
};
