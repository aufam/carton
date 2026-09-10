module;

#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>

export module carton:util;
import fmt;
import carton.fs;

export auto git_clone(const std::string &cache, const std::string &git, const std::string &tag) -> std::string;
export auto resolve_path(const std::string &cache, const std::string &path) -> std::string;
export void expand_path(const std::string &working_dir, std::vector<std::string> &sources, bool check_exist = true);

export auto collect_cppm_globs(const fs::path &working_dir, const fs::path &src_dir, std::vector<std::string> *cpps)
    -> std::vector<std::string>;

export template <typename... Args>
auto f(fmt::format_string<Args...> fmt, Args &&...args) {
    return fmt::format(fmt, std::forward<Args>(args)...);
}

export template <typename... Args>
auto ferr(fmt::format_string<Args...> fmt, Args &&...args) {
    return std::runtime_error(fmt::format(fmt, std::forward<Args>(args)...));
}

export void print_status(const std::string_view title, std::string_view status) {
    fmt::print(stderr, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::green), "{:>12} ", title);
    fmt::println(stderr, "{}", status);
}

export void print_progress(const std::string_view title, size_t current, size_t total) {
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

export void print_end_progress() {
    fmt::print(stderr, "\r\033[2K");
    fflush(stderr);
}

export void push_unique(std::vector<std::string> &vec, const std::string &value, bool front = false) {
    if (value.empty())
        return;
    if (std::find(vec.begin(), vec.end(), value) == vec.end()) {
        if (front)
            vec.insert(vec.begin(), value);
        else
            vec.push_back(value);
    }
}

export void push_unique(std::vector<std::string> &vec, const std::vector<std::string> &values, bool front = false) {
    for (const auto &value : values)
        push_unique(vec, value, front);
}

export template <typename T>
void push_unique(std::vector<T *> &vec, T *value, bool front = false) {
    if (value == nullptr)
        return;
    if (std::find(vec.begin(), vec.end(), value) == vec.end()) {
        if (front)
            vec.insert(vec.begin(), value);
        else
            vec.push_back(value);
    }
}

export template <typename T>
void push_unique(std::vector<T *> &vec, const std::vector<T *> &values, bool front = false) {
    for (const auto &value : values)
        push_unique(vec, value, front);
}

export void push_back_unique(std::vector<std::string> &vec, const std::string &value) {
    if (value.empty())
        return;

    if (auto it = std::find(vec.begin(), vec.end(), value); it != vec.end())
        vec.erase(it);

    vec.push_back(value);
}

export void push_back_unique(std::vector<std::string> &vec, const std::vector<std::string> &values) {
    for (const auto &value : values) {
        push_back_unique(vec, value);
    }
}
