module;

#include <cpx/reflect.h>
#include <cpx/fmt.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <variant>

export module carton;
export import :profile;
export import :dependency;
export import :package;
export import :compile_command;
export import :cache;
export import :cli;

export {
    struct Carton;

    auto convert_dep(std::variant<std::string, Dependency> & dep)->Dependency &;
    void push_unique(std::vector<std::string> & vec, const std::string &value, bool front = false);
    void push_unique(std::vector<std::string> & vec, const std::vector<std::string> &values, bool front = false);

    auto git_clone(const std::string &cache, const std::string &git, const std::string &tag) -> std::string;
    auto expand_path(const std::string &working_dir, std::vector<std::string> &sources) -> std::vector<std::string>;
    auto resolve_path(const std::string &cache, const std::string &path) -> std::string;

    auto sort_modules(
        const std::string                               &working_dir, //
        std::vector<std::string>                        &sources,
        std::map<std::string, std::vector<std::string>> &mods
    ) -> std::vector<std::string>;
    auto collect_module_deps(const std::string &working_dir, const std::string &source) -> std::vector<std::string>;

    template <typename... Args>
    auto f(fmt::format_string<Args...> fmt, Args && ...args) {
        return fmt::format(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto ferr(fmt::format_string<Args...> fmt, Args && ...args) {
        return std::runtime_error(fmt::format(fmt, std::forward<Args>(args)...));
    }
}

struct Carton {

    struct Profiles {
        Profile release = Profile::Release();
        Profile dev     = Profile::Dev();
    };

    std::unordered_map<std::string, Carton>                                registry;
    Profiles                                                               profiles;
    Package                                                                package;
    std::unordered_map<std::string, std::variant<std::string, Dependency>> dependencies;
    Dependency                                                             lib;
    std::unordered_map<std::string, std::vector<std::string>>              features;
    bool                                                                   no_default_features;

    Carton    *pparent = nullptr;
    Cache     *cache   = nullptr;
    const Cli *cli     = nullptr;

    void configure(const Profile &profile, const std::vector<std::string> &features = {});
    auto build(const Profile &profile, std::vector<CompileCommand> &ccs, bool do_build) -> std::pair<bool, Cache::Meta>;
    int  run(const Cache::Meta &m);

private:
    void apply_package_placeholders();
    void resolve_remote_dep(const Profile &profile, const std::string &name, Dependency &dep);
    auto collect_meta(const Profile &profile, Dependency &dep) -> Cache::Meta;
};

template <>
struct cpx::Reflect<Carton> //
    : Fields<
          Reflect<Carton>,
          &Carton::registry,
          &Carton::profiles,
          &Carton::package,
          &Carton::dependencies,
          &Carton::lib,
          &Carton::features,
          &Carton::no_default_features> {

    static constexpr TagInfo package             = "package";
    static constexpr TagInfo registry            = "registry,skipmissing";
    static constexpr TagInfo profiles            = "profiles,skipmissing";
    static constexpr TagInfo dependencies        = "dependencies,skipmissing";
    static constexpr TagInfo lib                 = "lib,skipmissing";
    static constexpr TagInfo features            = "features,skipmissing";
    static constexpr TagInfo no_default_features = "no-default-features,skipmissing";

    static constexpr tags_type tags() {
        return std::tie(package, registry, profiles, dependencies, lib, features, no_default_features);
    }
};
