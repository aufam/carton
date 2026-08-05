module;

#include <spdlog/spdlog.h>
#include <reproc++/run.hpp>
#include <algorithm>
#include <string>
#include <vector>
#include <filesystem>

module carton;

void push_unique(std::vector<std::string> &vec, const std::string &value, bool front) {
    if (value.empty())
        return;
    if (std::find(vec.begin(), vec.end(), value) == vec.end()) {
        if (front)
            vec.insert(vec.begin(), value);
        else
            vec.push_back(value);
    }
}

void push_unique(std::vector<std::string> &vec, const std::vector<std::string> &values, bool front) {
    for (const auto &value : values)
        push_unique(vec, value, front);
}

Dependency &Dependency::operator+=(const Dependency &other) {
    if (this == &other)
        return *this;

    if (other.empty()) {
        push_unique(src, other.src);
        push_unique(mod, other.mod, true);
        push_unique(inc, other.inc);
        push_unique(flags, other.flags);
        push_unique(link_flags, other.link_flags);
        if (!other.pre.empty()) {
            pre += pre.empty() ? "" : "\n";
            pre += other.pre;
        }
    } else {
        push_unique(flags, other.public_flags);
        push_unique(link_flags, other.link_flags);
        push_unique(mod_flags, other.mod_flags);
        push_unique(mod_names, other.mod_names);
    }
    return *this;
}

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

static std::string apply_function(std::string value, std::string_view fn) {
    if (fn.empty()) {
        return value;
    }

    if (fn == "underscore") {
        std::replace(value.begin(), value.end(), '.', '_');
        return value;
    }

    if (fn == "dash") {
        std::replace(value.begin(), value.end(), '.', '-');
        return value;
    }

    if (fn == "lower") {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
        return value;
    }

    if (fn == "upper") {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::toupper(c); });
        return value;
    }

    // Unknown function: leave unchanged.
    return value;
}

static void string_replace(std::string &str, std::string_view key, std::string_view value) {
    std::size_t pos = 0;

    while ((pos = str.find('{', pos)) != std::string::npos) {
        auto end = str.find('}', pos);
        if (end == std::string::npos)
            break;

        std::string_view placeholder(str.data() + pos + 1, end - pos - 1);

        auto colon = placeholder.find(':');

        std::string_view placeholder_key = placeholder;
        std::string_view fn;

        if (colon != std::string_view::npos) {
            placeholder_key = placeholder.substr(0, colon);
            fn              = placeholder.substr(colon + 1);
        }

        if (placeholder_key == key) {
            std::string replacement = apply_function(std::string(value), fn);
            str.replace(pos, end - pos + 1, replacement);
            pos += replacement.size();
        } else {
            pos = end + 1;
        }
    }
}

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;

    std::string numberize() const {
        return std::to_string(
            major * 1'000'000 +
            minor *
                ( //
                    minor < 10    ? 100
                    : minor < 100 ? 10
                                  : 1
                ) *
                1000 +
            patch * //
                (   //
                    patch < 10    ? 100
                    : patch < 100 ? 10
                                  : 1
                )
        );
    }
};

std::optional<SemVer> parse_semver(std::string_view s) {
    size_t pos = 0;

    auto parse_number = [&](int &out) -> bool {
        if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos])))
            return false;

        out = 0;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
            out = out * 10 + (s[pos] - '0');
            ++pos;
        }
        return true;
    };

    // Find first digit.
    while (pos < s.size() && !std::isdigit(static_cast<unsigned char>(s[pos]))) {
        ++pos;
    }

    if (pos == s.size())
        return std::nullopt;

    SemVer ver;

    if (!parse_number(ver.major))
        return std::nullopt;

    if (pos < s.size() && s[pos] == '.') {
        ++pos;
        parse_number(ver.minor);

        if (pos < s.size() && s[pos] == '.') {
            ++pos;
            parse_number(ver.patch);
        }
    }

    return ver;
}

static constexpr std::string_view os =
#if defined(_WIN32)
    "win";
#elif defined(__APPLE__)
    "osx";
#elif defined(__linux__)
    "linux";
#else
    "unknown";
#endif


static constexpr std::string_view os_name =
#if defined(_WIN32)
    "Windows";
#elif defined(__APPLE__)
    "Darwin";
#elif defined(__linux__)
    "Linux";
#else
    "unknown";
#endif

static constexpr std::string_view arch =
#if defined(__x86_64__) || defined(_M_X64)
    "x64";
#elif defined(_M_ARM64)
    "arm64"; // Windows
#elif defined(__aarch64__) && defined(__APPLE__)
    "arm64"; // macOS
#elif defined(__aarch64__)
    "aarch64"; // Linux, BSD, etc.
#else
            "unknown";
#endif

static constexpr std::string_view arch_family =
#if defined(__x86_64__) || defined(_M_X64)
    "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
    "arm";
#else
    "unknown";
#endif

static constexpr std::string_view arch_gnu =
#if defined(__x86_64__) || defined(_M_X64)
    "x86_64";
#elif defined(__aarch64__)
    arch;
#endif

static constexpr std::string_view arch_amd =
#if defined(__x86_64__) || defined(_M_X64)
    "amd64";
#elif defined(__aarch64__)
    arch;
#endif

void Carton::apply_package_placeholders() {
    const auto &name    = package.name;
    const auto &version = package.version;
    const auto  edition = std::to_string(package.edition);

    auto v = parse_semver(version)
                 .or_else([&]() -> std::optional<SemVer> {
                     throw ferr("cannot parse version: ", version);
                     return SemVer{};
                 })
                 .value();

    const auto version_major  = std::to_string(v.major);
    const auto version_minor  = std::to_string(v.minor);
    const auto version_patch  = std::to_string(v.patch);
    const auto version_number = v.numberize();

    auto apply_params = [&](Dependency &d, std::string_view key, std::string_view value) {
        string_replace(d.version, key, value);
        string_replace(d.path, key, value);
        string_replace(d.url, key, value);
        string_replace(d.git, key, value);
        string_replace(d.branch, key, value);
        string_replace(d.tag, key, value);
        string_replace(d.subdir, key, value);
        for (auto &str : d.features)
            string_replace(str, key, value);
        for (auto &str : d.src)
            string_replace(str, key, value);
        for (auto &str : d.inc)
            string_replace(str, key, value);
        for (auto &str : d.flags)
            string_replace(str, key, value);
        for (auto &str : d.link_flags)
            string_replace(str, key, value);
        string_replace(d.pre, key, value);
    };

    auto apply_dep = [&](Dependency &d) {
        apply_params(d, "name", name);
        apply_params(d, "version", version);
        apply_params(d, "version.major", version_major);
        apply_params(d, "version.minor", version_minor);
        apply_params(d, "version.patch", version_patch);
        apply_params(d, "version.number", version_number);
        apply_params(d, "edition", edition);
        apply_params(d, "os", os);
        apply_params(d, "os.name", os_name);
        apply_params(d, "arch", arch);
        apply_params(d, "arch.family", arch_family);
        apply_params(d, "arch.gnu", arch_gnu);
        apply_params(d, "arch.amd", arch_amd);
    };
    apply_dep(lib);

    for (auto &[_, dep] : dependencies) {
        apply_dep(dep);
    }

    for (auto &[_, feats] : features) {
        for (auto &feat : feats) {
            string_replace(feat, "name", name);
            string_replace(feat, "version", version);
            string_replace(feat, "version.major", version_major);
            string_replace(feat, "version.minor", version_minor);
            string_replace(feat, "version.patch", version_patch);
            string_replace(feat, "version.number", version_number);
            string_replace(feat, "edition", edition);
            string_replace(feat, "os", os);
            string_replace(feat, "os.name", os_name);
            string_replace(feat, "arch", arch);
            string_replace(feat, "arch.family", arch_family);
            string_replace(feat, "arch.gnu", arch_gnu);
            string_replace(feat, "arch.amd", arch_amd);
        }
    }
}

void expand_path(const std::string &working_dir, std::vector<std::string> &sources, bool check_exist) {
    if (sources.empty())
        return;

    for (auto &src : sources) {
        size_t pos = 0;
        while ((pos = src.find(' ', pos)) != std::string::npos) {
            src.replace(pos, 1, "\\ ");
            pos += 2;
        }
    }

    std::string cmd = fmt::format("printf '%s\\n' {}", fmt::join(sources, " "));

    reproc::options options;
    options.working_directory = working_dir.c_str();
    options.redirect.out.type = reproc::redirect::pipe;
    options.redirect.err.type = reproc::redirect::discard;

    spdlog::debug("expanding: cmd={:?}", cmd);
    reproc::process process;
    std::error_code ec = process.start(std::vector<std::string_view>{"sh", "-c", cmd}, options);
    if (ec)
        throw ferr("Failed to start expanding: {}", ec.message());

    std::string output;
    ec = reproc::drain(process, reproc::sink::string(output), reproc::sink::null);
    if (ec)
        throw ferr("Failed to read expand result: {}", ec.message());

    auto [status, wait_ec] = process.wait(reproc::infinite);
    if (wait_ec)
        throw ferr("Failed waiting for expand process: {}", wait_ec.message());

    if (status != 0)
        throw ferr("Expand command failed with exit code {}", status);

    std::vector<std::string> res;
    std::istringstream       iss(output);
    std::string              line;
    while (std::getline(iss, line)) {
        if (line.empty())
            continue;

        std::filesystem::path entry = line;

        if (check_exist && !std::filesystem::exists(std::filesystem::path(working_dir) / entry)) {
            throw ferr("Expand failed: {:?} does not exist in {:?}", entry.string(), working_dir);
        }

        res.push_back(entry.string());
    }

    sources = res;
}
