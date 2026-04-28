#include <cpx/defer.h>
#include <spdlog/spdlog.h>
#include <fmt/ranges.h>
#include <fmt/color.h>
#include <filesystem>
#include <unordered_set>

#define f(...)    fmt::format(__VA_ARGS__)
#define ferr(...) std::runtime_error(fmt::format(__VA_ARGS__))
namespace fs = std::filesystem;

static fs::path get_top_level_path_from_tar(const std::string &tar_file) {
    std::vector<std::string>        result;
    std::unordered_set<std::string> unique_entries;

    std::string command = fmt::format("tar tf \"{}\" | cut -d/ -f1 | uniq", tar_file);
    spdlog::debug("checking tar content: cmd={:?}", command);
    auto       pipe = popen(command.c_str(), "r");
    cpx::defer _    = [&]() { pclose(pipe); };

    if (!pipe)
        throw ferr("Failed to run tar command for {:?}", tar_file);

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);
        if (line.empty())
            continue;
        if (line.back() == '\n')
            line.pop_back();
        if (unique_entries.insert(line).second)
            result.emplace_back(line);
    }

    if (result.empty())
        throw ferr("Failed to get the top level path from {:?}", tar_file);

    // TODO: what if the tar_file has multiple paths
    if (result.size() > 1)
        throw ferr("Multiple top level paths from {:?} are not supported. The paths are: {}", tar_file, fmt::join(result, " "));

    return result.front();
}

std::string resolve_path(const std::string &cache, const std::string &path_str) {
    const auto [path, is_remote] = [&]() {
        for (std::string prefix : {"https://", "http://", "sftp://", "ftp://"})
            if (path_str.rfind(prefix, 0) == 0)
                return std::pair(fs::path(path_str.substr(prefix.length())), true);
        return std::pair(fs::path(path_str), false);
    }();

    const auto extension     = path.extension().string();
    const bool is_compressed = extension == ".tar" or extension == ".tgz" or extension == ".gz" or extension == ".tbz2" or
                               extension == ".bz2" or extension == ".xz"; // TODO: zip?

    if (is_remote) {
        const std::string &url = path_str;
        const fs::path     out = fs::path(cache) / "src" / path;
        const fs::path     dir = out.parent_path();

        if (std::string cmd = fmt::format("[ -f \"{}\" ]", out.string()); std::system(cmd.c_str()) != 0) {
            cmd = fmt::format(
                "mkdir -p \"{0}\" && "
                "wget -q --show-progress -O \"{1}\" \"{2}\"",
                dir.string(),
                out.string(),
                url
            );

            fmt::print(stderr, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::green), "{:>12} ", "Downloading");
            fmt::println(stderr, "{}", url);
            spdlog::debug("downloading: cmd={:?}", cmd);
            if (int res = std::system(cmd.c_str()); res)
                throw ferr("Failed to download archive from {:?}, return code: {}", path_str, res);
        }
        return resolve_path(cache, out.string());
    }

    if (is_compressed) {
        const auto extract_dir  = fs::path(cache) / "src" / "extracted";
        const auto extract_path = extract_dir / get_top_level_path_from_tar(path_str);
        const auto get_tar_flag = [&]() {
            if (extension == ".tar") {
                return "-xf";
            } else if (extension == ".gz" or extension == ".tgz") {
                return "-xzf";
            } else if (extension == ".bz2" or extension == ".tbz2") {
                return "-xjf";
            } else if (extension == ".xz") {
                return "-xJf";
            } else {
                throw ferr("Unsupported archive type {:?}", path_str);
            }
        };

        if (std::string cmd = fmt::format("[ -d \"{}\" ]", extract_path.string()); std::system(cmd.c_str())) {
            cmd = fmt::format(
                "mkdir -p \"{0}\" && "
                "tar {2} \"{1}\" -C \"{0}\"",
                extract_dir.string(),
                path_str,
                get_tar_flag()
            );
            fmt::print(stderr, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::green), "{:>12} ", "Extracting");
            fmt::println(stderr, "{}", path_str);
            spdlog::debug("extracting: {:?}", cmd);
            if (int res = std::system(cmd.c_str()); res)
                throw ferr("Failed to extract {:?}, return code: {}", path_str, res);
        }

        return resolve_path(cache, extract_path.string());
    }

    if (std::string cmd = fmt::format("[ -d \"{}\" ]", path.string()); std::system(cmd.c_str()))
        throw ferr("{:?} does not exist or unresolvable", path_str);

    return path_str;
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

    spdlog::debug("expanding: cmd={:?}", cmd);
    auto       pipe = popen(cmd.c_str(), "r");
    cpx::defer _    = [&]() { pclose(pipe); };

    std::vector<std::string> res;
    char                     buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::string buf = buffer;
        buf.pop_back();

        fs::path entry = buf;
        if (!fs::exists(fs::path(working_dir) / entry)) {
            throw ferr("Expand failed: {:?} does not exist in {:?}", entry.string(), working_dir);
        }
        res.push_back(entry.string());
    }

    return res;
}
