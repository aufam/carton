module;

#include <cpx/reflect.h>
#include <cpx/fmt.h>
#include <fmt/color.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include "macro.h"

export module carton;
export import :profile;
export import :dependency;
export import :package;
export import :compile_command;
export import :cache;
export import :cli;
export import :config;

export {
    struct Carton;

    namespace fs = std::filesystem;

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

    void print_status(const std::string_view title, std::string_view status) {
        fmt::print(stderr, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::green), "{:>12} ", title);
        fmt::println(stderr, "{}", status);
    }

    void print_progress(const std::string_view title, size_t current, size_t total) {
        const int   bar_width = 27;
        const float progress  = total == 0 ? 1.0f : static_cast<float>(current) / static_cast<float>(total);

        const int filled = static_cast<int>(bar_width * progress);

        fmt::print(stderr, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::cyan), "\r{:>12} ", title);
        fmt::print(stderr, "[");
        for (int i = 0; i < bar_width; ++i) {
            if (i < filled)
                fmt::print(stderr, "=");
            else if (i == filled && current < total)
                fmt::print(stderr, ">");
            else
                fmt::print(stderr, " ");
        }

        if (total == 100)
            fmt::print(stderr, "] {}%", current);
        else
            fmt::print(stderr, "] {}/{}", current, total);
        fflush(stderr);
    }

    void print_end_progress() {
        fmt::print(stderr, "\r\033[2K");
        fflush(stderr);
    }
}

struct Carton {
    using Registry     = std::unordered_map<std::string, Carton>;
    using Dependencies = std::unordered_map<std::string, Dependency>;
    using Features     = std::unordered_map<std::string, std::vector<std::string>>;

    Package      package;
    Registry     registry;
    Profiles     profiles;
    Dependencies dependencies;
    Dependency   lib;
    Features     features;
    bool         no_default_features;

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
