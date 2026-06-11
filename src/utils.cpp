module;

#include <cpx/fmt.h>
#include <cpx/defer.h>
#include <spdlog/spdlog.h>
#include <reproc++/run.hpp>
#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

module carton;

auto convert_dep(std::variant<std::string, Dependency> &dep) -> Dependency & {
    if (auto *version = std::get_if<std::string>(&dep)) {
        Dependency d{};
        d.version = *version;
        dep       = d;
    }

    return std::get<Dependency>(dep);
}

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

bool Dependency::empty() const {
    return version.empty() && path.empty() && url.empty() && git.empty();
}

std::string Dependency::display_name() const {
    std::string name = this->name;
    if (!version.empty()) {
        name += " v" + version;
    } else if (!tag.empty()) {
        name += " #" + tag;
    } else if (!branch.empty()) {
        name += " " + branch;
    } else {
        name += " (" + path + ")";
    }
    return name;
}

static void string_replace(
    std::string &str, const std::string &key, const std::string &value, const std::unordered_map<std::string, std::string> &vars
) {
    std::string pattern = "{" + key + "}";
    for (size_t pos = str.find(pattern); pos != std::string::npos; pos = str.find(pattern))
        str.replace(pos, pattern.length(), value);

    size_t pos = 0;
    while ((pos = str.find('{', pos)) != std::string::npos) {
        size_t end_pos = str.find('}', pos);
        if (end_pos == std::string::npos) {
            break; // Unclosed brace, treat as literal
        }

        std::string placeholder = str.substr(pos + 1, end_pos - pos - 1);
        std::string key;
        std::string default_value;
        size_t      colon_pos = placeholder.find(':');

        if (colon_pos != std::string::npos) {
            key           = placeholder.substr(0, colon_pos);
            default_value = placeholder.substr(colon_pos + 1);
        } else {
            key = placeholder;
        }

        auto it = vars.find(key);
        if (it != vars.end()) {
            str.replace(pos, end_pos - pos + 1, it->second);
            pos += it->second.length(); // Move past the replaced string
        } else if (!default_value.empty()) {
            str.replace(pos, end_pos - pos + 1, default_value);
            pos += default_value.length();
        } else {
            pos = end_pos + 1; // Move past the placeholder
        }
    }
}

void Carton::apply_package_placeholders() {
    auto &name    = package.name;
    auto &version = package.version;
    auto  edition = std::to_string(package.edition);

    std::unordered_map<std::string, std::string> vars;

    auto apply_dep = [&](Dependency &d) {
        string_replace(d.version, "version", version, vars);
        string_replace(d.path, "version", version, vars);
        string_replace(d.url, "version", version, vars);
        string_replace(d.git, "version", version, vars);
        string_replace(d.branch, "version", version, vars);
        string_replace(d.tag, "version", version, vars);
        string_replace(d.subdir, "version", version, vars);
        for (auto &str : d.features)
            string_replace(str, "version", version, vars);
        for (auto &str : d.src)
            string_replace(str, "version", version, vars);
        for (auto &str : d.inc)
            string_replace(str, "version", version, vars);
        for (auto &str : d.flags)
            string_replace(str, "version", version, vars);
        for (auto &str : d.link_flags)
            string_replace(str, "version", version, vars);
        string_replace(d.pre, "version", version, vars);

        string_replace(d.version, "name", name, vars);
        string_replace(d.path, "name", name, vars);
        string_replace(d.url, "name", name, vars);
        string_replace(d.git, "name", name, vars);
        string_replace(d.branch, "name", name, vars);
        string_replace(d.tag, "name", name, vars);
        string_replace(d.subdir, "name", name, vars);
        for (auto &str : d.features)
            string_replace(str, "name", name, vars);
        for (auto &str : d.src)
            string_replace(str, "name", name, vars);
        for (auto &str : d.inc)
            string_replace(str, "name", name, vars);
        for (auto &str : d.flags)
            string_replace(str, "name", name, vars);
        for (auto &str : d.link_flags)
            string_replace(str, "name", name, vars);
        string_replace(d.pre, "name", name, vars);

        string_replace(d.version, "edition", edition, vars);
        string_replace(d.path, "edition", edition, vars);
        string_replace(d.url, "edition", edition, vars);
        string_replace(d.git, "edition", edition, vars);
        string_replace(d.branch, "edition", edition, vars);
        string_replace(d.tag, "edition", edition, vars);
        string_replace(d.subdir, "edition", edition, vars);
        for (auto &str : d.features)
            string_replace(str, "edition", edition, vars);
        for (auto &str : d.src)
            string_replace(str, "edition", edition, vars);
        for (auto &str : d.inc)
            string_replace(str, "edition", edition, vars);
        for (auto &str : d.flags)
            string_replace(str, "edition", edition, vars);
        for (auto &str : d.link_flags)
            string_replace(str, "edition", edition, vars);
        string_replace(d.pre, "edition", edition, vars);
    };
    apply_dep(lib);

    for (auto &[_, dep] : dependencies) {
        apply_dep(convert_dep(dep));
    }

    for (auto &[_, feats] : features) {
        for (auto &feat : feats) {
            string_replace(feat, "name", name, vars);
            string_replace(feat, "version", version, vars);
            string_replace(feat, "edition", edition, vars);
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

    reproc::process process;

    spdlog::debug("expanding: cmd={:?}", cmd);
    std::error_code ec = process.start(std::vector<std::string_view>{"sh", "-c", cmd}, options);
    if (ec) {
        throw ferr("Failed to start expanding: {}", ec.message());
    }

    std::array<char, 4096>   buffer;
    std::vector<std::string> res;
    while (true) {
        auto [bytes_read, read_ec] = process.read(reproc::stream::out, reinterpret_cast<uint8_t *>(buffer.data()), buffer.size());
        if (read_ec == std::errc::broken_pipe)
            break;

        if (read_ec)
            throw ferr("Failed to read expand result: {}", read_ec.message());

        std::string_view      line(buffer.data(), bytes_read - 1ul);
        std::filesystem::path entry = line;
        if (!std::filesystem::exists(std::filesystem::path(working_dir) / entry))
            throw ferr("Expand failed: {:?} does not exist in {:?}", entry.string(), working_dir);

        res.push_back(entry.string());
    }

    return res;
}
