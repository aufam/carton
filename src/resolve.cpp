module;

#include <reproc++/run.hpp>
#include <spdlog/spdlog.h>
#include <unordered_set>
#include <regex>

module carton;
import std.fs;
import fmt;
import cpx;
import cpx.toruniina_toml;

void Carton::resolve_remote_dep(const Profile &profile, const std::string &name, Dependency &d, bool from_registry) {
    if (d.empty())
        throw ferr("assertion failed: {:?} cannot be empty", d.name);

    auto configure_subpackage = [&](Carton &p, bool from_registry) -> Dependency & {
        if (p.package.edition > package.edition)
            throw ferr(
                "Error building dependency package={0:?}: {0:?} required std=c++{1} but {2:?} only supports std=c++{3}",
                p.package.name,
                p.package.edition,
                package.name,
                package.edition
            );

        p.pparent             = this;
        p.cache               = this->cache;
        p.cli                 = this->cli;
        p.no_default_features = !d.default_features.value_or(true);
        p.profiles            = profiles;
        p.lib.features        = d.features;
        try {
            p.configure(profile, d.features, from_registry);
        } catch (const std::exception &e) {
            throw ferr("Error building dependency package={}: {}", p.package.name, e.what());
        }
        return p.lib;
    };

    if (!d.path.empty()) {
        spdlog::info("resolving dep={:?} path={:?}", name, d.path);
        d.path = resolve_path(cli->cache, d.path);
    } else if (!d.url.empty()) {
        spdlog::info("resolving dep={:?} url={:?}", name, d.url);
        d.path = resolve_path(cli->cache, d.url);
    } else if (!d.git.empty()) {
        auto &tag = d.tag.empty() ? d.branch : d.tag;
        spdlog::info("resolving dep={:?} git={:?} tag={:?}", name, d.git, tag);
        d.path = git_clone(cli->cache, d.git, tag);
    } else if (d.version.empty()) {
        throw ferr("path|git|version is not defined");
    } else {
        spdlog::info("resolving dep={:?} version={:?}", name, d.version);
        auto &packages = pparent ? pparent->registry : this->registry;

        auto &name_ = d.name.empty() ? name : d.name;
        auto  it    = packages.find(name_);
        while (it != packages.end()) {
            auto i = packages.find(it->second.package.name);
            if (i == it)
                break;
            it = i;
        }
        if (it == packages.end())
            throw ferr("Cannot find `{}` in the package registry", name_);

        auto p = it->second;
        if (p.lib.empty())
            throw ferr("The package `{}` does not have a library target", name_);

        p.package.version = d.version;
        auto &lib         = configure_subpackage(p, true);
        d                 = std::move(lib);
        d.name            = name;
        return;
    }

    if (fs::path path = d.path; path.is_relative())
        d.path = (fs::path(lib.path) / lib.subdir / path).lexically_normal().string();

    fs::path working_dir = fs::path(d.path) / d.subdir;
    if (auto sub = working_dir / "carton.toml"; from_registry && fs::exists(sub)) {
        constexpr auto toml_version = cpx::toruniina_toml::spec::v(1, 1, 0);

        auto p = cpx::toruniina_toml::parse_from_file<Carton>(sub.string(), toml_version);
        if (fs::path(p.lib.path).is_absolute() || fs::path(p.lib.subdir).is_absolute())
            throw ferr("Path must be relative");
        p.lib.path = (working_dir / p.lib.path).string();

        spdlog::info("got custom config for: p.name={:?}", p.package.name);
        auto &lib    = configure_subpackage(p, false);
        d            = std::move(lib);
        d.name       = name;
        dependencies = std::move(p.dependencies);
        features     = std::move(p.features);
    } else {
        if (d.mod.empty() && fs::exists(working_dir / "src" / "lib.cppm"))
            d.mod = {"src/*.cppm"};
        if (d.src.empty() && fs::is_directory(working_dir / "src"))
            d.src = {"src/*"};
        if (d.inc.empty() && fs::is_directory(working_dir / "include"))
            d.inc = {"public:include"};
    }
}

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
            if (path_str.starts_with(prefix))
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

        if (!fs::exists(out)) {
            fs::create_directories(dir);

            spdlog::debug("downloading: url={} out={}", url, out.string());
            print_status("Downloading", url);

            reproc::options options;
            options.redirect.out.type = reproc::redirect::discard;
            options.redirect.err.type = reproc::redirect::pipe;

            reproc::process process;

            std::error_code ec = process.start(
                std::vector<std::string>{
                    "wget", "--quiet", "--show-progress", "--https-only", "--timeout=10", "--tries=3", "-O", out.string(), url
                },
                options
            );

            if (ec) {
                throw ferr("Failed to start wget: {}", ec.message());
            }

            std::array<char, 4096> buffer;
            std::string            line;

            // wget progress goes to stderr
            while (true) {
                auto [bytes_read, read_ec] =
                    process.read(reproc::stream::err, reinterpret_cast<uint8_t *>(buffer.data()), buffer.size());

                if (read_ec == std::errc::broken_pipe)
                    break;

                if (read_ec)
                    throw ferr("Failed to read wget output: {}", read_ec.message());

                line.append(buffer.data(), bytes_read);

                size_t pos = 0;
                while ((pos = line.find('\n')) != std::string::npos) {
                    std::string current = line.substr(0, pos);
                    line.erase(0, pos + 1);

                    // Example:
                    // " 45% [=========>      ] 1.23M ..."
                    static const std::regex percent_re(R"((\d+)%)");

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
