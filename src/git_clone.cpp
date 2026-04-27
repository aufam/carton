#include <cpx/defer.h>
#include <filesystem>
#include <regex>
#include <spdlog/spdlog.h>
#include <fmt/ranges.h>
#include <fmt/color.h>

#define f(...)    fmt::format(__VA_ARGS__)
#define ferr(...) std::runtime_error(fmt::format(__VA_ARGS__))
namespace fs = std::filesystem;

static std::string extract_host_and_path(const std::string &url) {
    std::string cleaned = url;

    if (cleaned.length() >= 4 && cleaned.substr(cleaned.length() - 4) == ".git")
        cleaned = cleaned.substr(0, cleaned.size() - 4);

    // Handle git@github.com:user/repo
    static const std::regex ssh_pattern(R"(git@([^:]+):(.+))");
    std::smatch             ssh_match;
    if (std::regex_match(cleaned, ssh_match, ssh_pattern))
        return ssh_match[1].str() + "/" + ssh_match[2].str();

    // Handle https://[user[:password]@]github.com/user/repo
    static const std::regex https_pattern(R"(https?://(?:[^@]+@)?([^/]+)/(.+))");
    std::smatch             https_match;
    if (std::regex_match(cleaned, https_match, https_pattern)) {
        return https_match[1].str() + "/" + https_match[2].str();
    }

    // Handle github.com/user/repo or user/repo
    return cleaned;
}

static bool is_full_url(const std::string &url) {
    return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0 || url.rfind("git@", 0) == 0;
}

static bool has_host_prefix(const std::string &url) {
    // Check for common domain prefixes (gitlab.com/, bitbucket.org/, etc.)
    return url.find('.') != std::string::npos && url.find('/') != std::string::npos;
}

static std::string normalize_git_url(const std::string &url) {
    if (is_full_url(url))
        return url;

    if (has_host_prefix(url))
        return "https://" + url; // e.g., gitlab.com/user/repo

    // Otherwise assume it's a shorthand like user/repo
    return "https://github.com/" + url;
}

std::string git_clone(const std::string &cache, const std::string &git, const std::string &tag) {
    const std::string url  = normalize_git_url(git);
    const std::string host = extract_host_and_path(url);

    fs::path result_path = fs::path(cache) / "src" / (host + "-" + tag);
    spdlog::debug("url={:?} host={:?} path={:?}", url, host, result_path.string());

    if (!fs::is_directory(result_path)) {
        const std::string cmd = fmt::format(
            ""
            "git -c advice.detachedHead=false clone --quiet --depth 1 {1}\"{2}\" \"{0}\"",
            result_path.string(),
            tag.empty() ? "" : "--branch \"" + tag + "\" ",
            url
        );

        fmt::print(stderr, fmt::emphasis::bold | fmt::fg(fmt::color::green), "{:>12} ", "Cloning");
        fmt::println(stderr, "{}", url);
        spdlog::debug("git clone: cmd={:?}", cmd);
        if (int res = std::system(cmd.c_str()); res)
            throw std::runtime_error(fmt::format("Failed to clone repo from {:?}, return code: {}", url, res));
    } else {
        // check if the existing directory is a valid git repo with the correct remote and tag
        // const std::string check_cmd = fmt::format(
        //     "cd \"{0}\" && "
        //     "git remote get-url origin 2>/dev/null | grep \"{1}\" && "
        //     "(git rev-parse --abrev-ref HEAD 2>/dev/null | grep -q \"{2}\" || "
        //     "git describe --tags --exact-match 2>/dev/null | grep -q \"{2}\")",
        //     result_path.string(),
        //     host,
        //     tag.empty() ? "HEAD" : tag
        // );
        //
        // spdlog::debug("checking existing repo: cmd={:?}", check_cmd);
        // if (int res = std::system(check_cmd.c_str()); res) {
        //     throw std::runtime_error(fmt::format(
        //         "Existing directory {:?} is not a valid git repo with the correct remote and tag. Please remove it and try "
        //         "again.",
        //         result_path.string()
        //     ));
        // }

        // check if the existing repo is dirty
        const std::string dirty_check_cmd = fmt::format(
            "cd \"{0}\" && "
            "git diff --quiet 2>/dev/null && "
            "git diff --cached --quiet 2>/dev/null",
            result_path.string()
        );

        spdlog::debug("checking dirty repo: cmd={:?}", dirty_check_cmd);
        if (int res = std::system(dirty_check_cmd.c_str()); res) {
            spdlog::warn("Existing directory {:?} has uncommitted changes.", result_path.string());
        }
    }
    return result_path;
}
