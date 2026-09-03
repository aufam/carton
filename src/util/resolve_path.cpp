module;

#include <reproc++/run.hpp>
#include <spdlog/spdlog.h>
#include <unordered_set>
#include <unordered_map>
#include <regex>

module carton;
import cpx;
import cpx.toruniina_toml;

static fs::path get_top_level_path_from_tar(const fs::path &extract_dir, const std::string &tar_file) {
    std::vector<std::string>        result;
    std::unordered_set<std::string> unique_entries;

    auto cache      = std::unordered_map<std::string, std::string>();
    auto cache_path = extract_dir / "cache.toml";
    if (fs::exists(cache_path)) {
        cpx::toruniina_toml::parse_from_file(cache_path.string(), cache);
        if (auto it = cache.find(tar_file); it != cache.end())
            return it->second;
    }

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

    cache[tar_file] = result.front();
    fs::create_directories(extract_dir);
    std::ofstream os(cache_path);
    os << cpx::toruniina_toml::io << cache;
    return result.front();
}

std::string resolve_path(const std::string &cache, const std::string &path_str) {
    const auto [path, is_remote] = [&]() {
        for (std::string prefix : {"https://", "http://", "sftp://", "ftp://"})
            if (path_str.starts_with(prefix))
                return std::pair(fs::path(path_str.substr(prefix.length())), true);
        return std::pair(fs::path(path_str), false);
    }();

    const auto extension     = path.extension().string();
    const bool is_compressed = extension == ".tar" or extension == ".tgz" or extension == ".gz" or extension == ".tbz2" or
                               extension == ".bz2" or extension == ".xz"; // TODO: zip?

    if (is_remote) {
        const fs::path out = fs::path(cache) / "src" / path;
        const fs::path dir = out.parent_path();

        const auto &url_str = path_str;
        const auto  out_str = out.string();

        if (!fs::exists(out)) {
            fs::create_directories(dir);

            spdlog::debug("downloading: url={} out={}", url_str, out_str);
            print_status("Downloading", url_str);

            reproc::options options;
            options.redirect.out.type = reproc::redirect::discard;
            options.redirect.err.type = reproc::redirect::pipe;

            reproc::process process;

            std::error_code ec = process.start(
                std::vector<std::string_view>{
                    "curl",
                    "--fail",
                    "--location",
                    "--show-error",
                    "--progress-bar",
                    "--connect-timeout",
                    "10",
                    "--retry",
                    "3",
                    "--output",
                    out_str,
                    url_str,
                },
                options
            );

            if (ec)
                throw ferr("Failed to start curl: {}", ec.message());

            std::array<char, 4096> buffer;
            std::string            line;

            // curl progress goes to stderr
            while (true) {
                auto [bytes_read, read_ec] =
                    process.read(reproc::stream::err, reinterpret_cast<uint8_t *>(buffer.data()), buffer.size());

                if (read_ec == std::errc::broken_pipe)
                    break;

                if (read_ec)
                    throw ferr("Failed to read curl output: {}", read_ec.message());

                line.append(buffer.data(), bytes_read);

                print_progress("Downloading", 0, 100);
                for (size_t pos = 0; (pos = line.find('\n')) != std::string::npos;) {
                    std::string current = line.substr(0, pos);
                    line.erase(0, pos + 1);

                    // Example:
                    // ##############################################                          65.2%
                    static const std::regex percent_re(R"((\d+(?:\.\d+)?)%)");

                    std::smatch match;
                    if (std::regex_search(current, match, percent_re)) {
                        const size_t percent = std::stoul(match[1]);
                        print_progress("Downloading", percent, 100);
                    }
                }
            }

            int exit_code           = 0;
            std::tie(exit_code, ec) = process.wait(reproc::infinite);

            // finish progress line
            print_end_progress();

            if (ec || exit_code != 0) {
                throw ferr("Failed to download archive from {}, exit code={}", path_str, exit_code);
            }
        }

        return resolve_path(cache, out.string());
    }

    if (is_compressed) {
        const auto extract_dir  = fs::path(cache) / "src" / "extracted";
        const auto extract_path = extract_dir / get_top_level_path_from_tar(extract_dir, path_str);
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
            print_status("Extracting", path_str);
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
