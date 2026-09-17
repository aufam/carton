module;

#include <string>
#include <vector>
#include <unordered_map>
#include <set>
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
import :target;
import :binary;
import cpx;

export struct Carton {
    using Registry     = std::unordered_map<std::string, Carton>;
    using Dependencies = std::unordered_map<std::string, Dependency>;
    using Features     = std::unordered_map<std::string, std::vector<std::string>>;
    using Binaries     = std::vector<Binary>;

    Package      package;
    Registry     registry;
    Profiles     profiles;
    Dependencies dependencies;
    Dependency   lib;
    Features     features;
    Binaries     bins;
    Binaries     examples;

    static constexpr std::tuple __field_tags__{
        cpx::field<&Carton::package>      = "package",
        cpx::field<&Carton::registry>     = "registry     , skipmissing",
        cpx::field<&Carton::profiles>     = "profile      , skipmissing",
        cpx::field<&Carton::dependencies> = "dependencies , skipmissing",
        cpx::field<&Carton::lib>          = "lib          , skipmissing",
        cpx::field<&Carton::bins>         = "bin          , skipmissing",
        cpx::field<&Carton::features>     = "features     , skipmissing",
    };

    Carton                                        *root = nullptr;
    std::shared_ptr<Cache>                         cache;
    std::vector<std::shared_ptr<Carton>>           locals;
    std::map<std::string, std::unique_ptr<Target>> targets;
    bool                                           configured = false;

    static int    Update();
    static int    Init(Package &args);
    static Carton New(const std::string &cache_dir);
    int           add(Dependency &dep);
    int           execute(Cli &cli);

private:
    int run(Cli::Run &);

    void apply_placeholders();

    auto
    get_requested_features(const std::vector<std::string> &features, bool default_features = true) -> std::vector<std::string>;

    auto configure_package(
        const Profile &,
        const std::string              &working_dir,
        const std::vector<std::string> &features         = {},
        bool                            default_features = true
    ) -> std::vector<Target *>;

    auto configure_extras(
        const Profile                  &profile,
        Target                         &main_target,
        const std::vector<Target *>    &required_targets,
        const std::set<std::string>    &nameset,
        const std::vector<std::string> &extra_features
    ) -> std::vector<Target *>;

    auto configure_bins(const Profile &) -> std::vector<Target *>;

    void resolve_package(const std::string &working_dir);

    auto resolve_dep(const std::string &name, Dependency &dep) -> std::pair<Carton *, std::string>;
};
