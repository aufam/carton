#include "main.h"
#include <algorithm>

Dependency &Dependency::operator+=(const Dependency &other) {
    if (this == &other)
        return *this;

    push_unique(src(), other.src());
    push_unique(inc(), other.inc());
    push_unique(flags(), other.flags());
    push_unique(link_flags(), other.link_flags());
    return *this;
}

bool Dependency::empty() const {
    return version().empty() && path().empty() && url().empty() && git().empty();
}

Dependency &convert_dep(Project::Dep &dep) {
    if (auto *version = std::get_if<std::string>(&dep)) {
        Dependency d{};
        d.version() = *version;
        dep         = d;
    }

    return std::get<Dependency>(dep);
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

void push_unique(std::vector<std::string> &vec, const std::string &value) {
    if (std::find(vec.begin(), vec.end(), value) == vec.end())
        vec.push_back(value);
}

void push_unique(std::vector<std::string> &vec, const std::vector<std::string> &values) {
    for (const auto &value : values)
        push_unique(vec, value);
}

void Project::apply_package_placeholders() {
    auto &name    = package().name();
    auto &version = package().version();
    auto  edition = std::to_string(package().edition());

    auto apply_dep = [&](Dependency &d) {
        string_replace(d.version(), "version", version, vars());
        string_replace(d.path(), "version", version, vars());
        string_replace(d.url(), "version", version, vars());
        string_replace(d.git(), "version", version, vars());
        string_replace(d.branch(), "version", version, vars());
        string_replace(d.tag(), "version", version, vars());
        string_replace(d.subdir(), "version", version, vars());
        for (auto &str : d.features())
            string_replace(str, "version", version, vars());
        for (auto &str : d.src())
            string_replace(str, "version", version, vars());
        for (auto &str : d.inc())
            string_replace(str, "version", version, vars());
        for (auto &str : d.flags())
            string_replace(str, "version", version, vars());
        for (auto &str : d.link_flags())
            string_replace(str, "version", version, vars());
        string_replace(d.pre(), "version", version, vars());

        string_replace(d.version(), "name", name, vars());
        string_replace(d.path(), "name", name, vars());
        string_replace(d.url(), "name", name, vars());
        string_replace(d.git(), "name", name, vars());
        string_replace(d.branch(), "name", name, vars());
        string_replace(d.tag(), "name", name, vars());
        string_replace(d.subdir(), "name", name, vars());
        for (auto &str : d.features())
            string_replace(str, "name", name, vars());
        for (auto &str : d.src())
            string_replace(str, "name", name, vars());
        for (auto &str : d.inc())
            string_replace(str, "name", name, vars());
        for (auto &str : d.flags())
            string_replace(str, "name", name, vars());
        for (auto &str : d.link_flags())
            string_replace(str, "name", name, vars());
        string_replace(d.pre(), "name", name, vars());

        string_replace(d.version(), "edition", edition, vars());
        string_replace(d.path(), "edition", edition, vars());
        string_replace(d.url(), "edition", edition, vars());
        string_replace(d.git(), "edition", edition, vars());
        string_replace(d.branch(), "edition", edition, vars());
        string_replace(d.tag(), "edition", edition, vars());
        string_replace(d.subdir(), "edition", edition, vars());
        for (auto &str : d.features())
            string_replace(str, "edition", edition, vars());
        for (auto &str : d.src())
            string_replace(str, "edition", edition, vars());
        for (auto &str : d.inc())
            string_replace(str, "edition", edition, vars());
        for (auto &str : d.flags())
            string_replace(str, "edition", edition, vars());
        for (auto &str : d.link_flags())
            string_replace(str, "edition", edition, vars());
        string_replace(d.pre(), "edition", edition, vars());
    };
    apply_dep(lib());

    for (auto &[_, dep] : dependencies()) {
        apply_dep(convert_dep(dep));
    }

    for (auto &[_, feats] : features()) {
        for (auto &feat : feats) {
            string_replace(feat, "name", name, vars());
            string_replace(feat, "version", version, vars());
            string_replace(feat, "edition", edition, vars());
        }
    }
}
