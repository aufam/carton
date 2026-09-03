module;

#include <spdlog/spdlog.h>
#include <reproc++/run.hpp>
#include <filesystem>

module carton;

#ifdef _WIN32
constexpr char PATH_SEPARATOR = ';';
#else
constexpr char PATH_SEPARATOR = ':';
#endif

static std::string resolve_compiler(std::string &compiler) {
    {
        auto paths = std::vector<std::string>{compiler};
        expand_path(".", paths, false);
        compiler = paths.front();
    }

    fs::path p{compiler};

    auto is_executable = [](const fs::path &path) {
        std::error_code ec;
        auto            perms = fs::status(path, ec).permissions();
        if (ec || !fs::is_regular_file(path, ec))
            return false;

#ifdef _WIN32
        return true;
#else
        using fs::perms;
        return (perms & (perms::owner_exec | perms::group_exec | perms::others_exec)) != fs::perms::none;
#endif
    };

    // Already a path?
    if (p.has_parent_path() || p.is_absolute()) {
        std::error_code ec;
        auto            canonical = fs::weakly_canonical(p, ec);
        if (ec || !is_executable(canonical))
            throw std::runtime_error("Compiler not found or not executable: " + p.string());

        return canonical.string();
    }

    // Search PATH.
    const char *env = std::getenv("PATH");
    if (!env)
        throw std::runtime_error("PATH is not set");

    std::string path_env = env;
    size_t      begin    = 0;

    while (begin <= path_env.size()) {
        size_t end = path_env.find(PATH_SEPARATOR, begin);
        if (end == std::string::npos)
            end = path_env.size();

        fs::path candidate = fs::path(path_env.substr(begin, end - begin)) / p;

        if (is_executable(candidate)) {
            std::error_code ec;
            auto            canonical = fs::weakly_canonical(candidate, ec);
            if (!ec)
                return canonical.string();
        }

        begin = end + 1;
    }

    throw std::runtime_error("Compiler '" + std::string(compiler) + "' not found in PATH");
}

void Profiles::check_module_support() {
    dev._module_compiler     = resolve_compiler(dev.cxx);
    release._module_compiler = resolve_compiler(release.cxx);

    // TODO: check clang support module
    auto create_cmd = [](const std::string &cxx) {
        return std::vector<std::string>{"sh", "-c", f("{0} --version | grep clang > /dev/null", cxx)};
    };

    dev._module_support     = dev.modules == "auto" && reproc::run(create_cmd(dev._module_compiler)).first == 0;
    release._module_support = release.modules == "auto" && reproc::run(create_cmd(release._module_compiler)).first == 0;

    spdlog::info("profile.dev._module_support={}", dev._module_support);
    spdlog::info("profile.release._module_support={}", release._module_support);
}
