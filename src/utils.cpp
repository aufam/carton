module;

#include <spdlog/spdlog.h>
#include <reproc++/run.hpp>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

module carton;
import fmt;

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

    push_unique(src, other.src);
    push_unique(mod, other.mod, true);
    push_unique(inc, other.inc);
    push_unique(flags, other.flags);
    push_unique(link_flags, other.link_flags);
    return *this;
}

void Profiles::check_module_support() {
    // TODO: check clang support module
    auto create_cmd = [](const std::string &cxx) {
        return std::vector<std::string>{"sh", "-c", f("{0} --version | grep clang > /dev/null", cxx)};
    };

    dev._module_support     = dev.modules == "auto" && reproc::run(create_cmd(dev.cxx)).first == 0;
    release._module_support = release.modules == "auto" && reproc::run(create_cmd(release.cxx)).first == 0;

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

void Carton::apply_package_placeholders() {
    auto &name    = package.name;
    auto &version = package.version;
    auto  edition = std::to_string(package.edition);

    auto apply_dep = [&](Dependency &d) {
        string_replace(d.version, "version", version);
        string_replace(d.path, "version", version);
        string_replace(d.url, "version", version);
        string_replace(d.git, "version", version);
        string_replace(d.branch, "version", version);
        string_replace(d.tag, "version", version);
        string_replace(d.subdir, "version", version);
        for (auto &str : d.features)
            string_replace(str, "version", version);
        for (auto &str : d.src)
            string_replace(str, "version", version);
        for (auto &str : d.inc)
            string_replace(str, "version", version);
        for (auto &str : d.flags)
            string_replace(str, "version", version);
        for (auto &str : d.link_flags)
            string_replace(str, "version", version);
        string_replace(d.pre, "version", version);

        string_replace(d.version, "name", name);
        string_replace(d.path, "name", name);
        string_replace(d.url, "name", name);
        string_replace(d.git, "name", name);
        string_replace(d.branch, "name", name);
        string_replace(d.tag, "name", name);
        string_replace(d.subdir, "name", name);
        for (auto &str : d.features)
            string_replace(str, "name", name);
        for (auto &str : d.src)
            string_replace(str, "name", name);
        for (auto &str : d.inc)
            string_replace(str, "name", name);
        for (auto &str : d.flags)
            string_replace(str, "name", name);
        for (auto &str : d.link_flags)
            string_replace(str, "name", name);
        string_replace(d.pre, "name", name);

        string_replace(d.version, "edition", edition);
        string_replace(d.path, "edition", edition);
        string_replace(d.url, "edition", edition);
        string_replace(d.git, "edition", edition);
        string_replace(d.branch, "edition", edition);
        string_replace(d.tag, "edition", edition);
        string_replace(d.subdir, "edition", edition);
        for (auto &str : d.features)
            string_replace(str, "edition", edition);
        for (auto &str : d.src)
            string_replace(str, "edition", edition);
        for (auto &str : d.inc)
            string_replace(str, "edition", edition);
        for (auto &str : d.flags)
            string_replace(str, "edition", edition);
        for (auto &str : d.link_flags)
            string_replace(str, "edition", edition);
        string_replace(d.pre, "edition", edition);
    };
    apply_dep(lib);

    for (auto &[_, dep] : dependencies) {
        apply_dep(dep);
    }

    for (auto &[_, feats] : features) {
        for (auto &feat : feats) {
            string_replace(feat, "name", name);
            string_replace(feat, "version", version);
            string_replace(feat, "edition", edition);
        }
    }
}

std::vector<std::string> expand_path(const std::string &working_dir, std::vector<std::string> &sources) {
    if (sources.empty())
        return {};

    for (auto &src : sources) {
        size_t pos = 0;
        while ((pos = src.find(' ', pos)) != std::string::npos) {
            src.replace(pos, 1, "\\ ");
            pos += 2;
        }
    }

    std::string cmd = fmt::format(
        "cd \"{}\" "
        "&& printf '%s\\n' {}",
        working_dir,
        fmt::join(sources, " ")
    );

    reproc::options options;
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

        if (!std::filesystem::exists(std::filesystem::path(working_dir) / entry)) {
            throw ferr("Expand failed: {:?} does not exist in {:?}", entry.string(), working_dir);
        }

        res.push_back(entry.string());
    }

    return res;
}
