module;

#include <cpx/fmt.h>
#include <cpx/toml/toruniina_toml.h>
#include <cpx/defer.h>
#include <fmt/color.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <unordered_set>

module carton;

namespace fs = std::filesystem;

void Carton::resolve_remote_dep(const Profile &profile, const std::string &name, Dependency &d) {
    if (d.empty())
        throw ferr("assertion failed: {:?} cannot be empty", d.name);

    auto configure_subpackage = [&](Carton &p) -> Dependency & {
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
        try {
        } catch (const std::exception &e) {
            throw ferr("Error building dependency package={} `{}`: {}", p.package.name, name, e.what());
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
        throw std::runtime_error("path|git|version is not defined");
    } else {
        spdlog::info("resolving dep={:?} version={:?}", name, d.version);
        auto &packages = pparent ? pparent->registry : this->registry;

        auto &name_ = d.name.empty() ? name : d.name;
        auto  it    = packages.find(name_);
        if (it == packages.end())
            throw std::runtime_error("Cannot find `" + name_ + "` in the package registry");

        auto p = it->second;
        if (p.lib.empty())
            throw std::runtime_error("The package `" + name_ + "` does not have a library target");

        p.package.version = d.version;
        auto &lib         = configure_subpackage(p);
        d                 = std::move(lib);
        resolve_remote_dep(profile, name, d);
        return;
    }

    if (fs::path path = d.path; path.is_relative())
        d.path = (fs::path(lib.path) / lib.subdir / path).lexically_normal().string();

    fs::path working_dir = fs::path(d.path) / d.subdir;
    if (auto sub = working_dir / "carton.toml"; &lib != &d && fs::exists(sub)) {
        constexpr auto toml_version = cpx::toml::toruniina_toml::spec::v(1, 1, 0);

        auto p = cpx::toml::toruniina_toml::parse_from_file<Carton>(sub.string(), toml_version);
        if (fs::path(p.lib.path).is_absolute() && fs::path(p.lib.subdir).is_absolute())
            throw ferr("Path must be relative");
        p.lib.path = (working_dir / p.lib.path).string();

        auto &lib = configure_subpackage(p);
        d         = std::move(lib);
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

constexpr char awk[] = R"sh(|
	awk -v width=27 '
function bar(p,    i, filled, res) {
    filled = int(p * width / 100)
    res = "["
    for (i = 0; i < width; i++) {
        if (i < filled) res = res "="
        else if (i == filled) res = res ">"
        else res = res " "
    }
    res = res "]"
    return res
}

{
    # extract percentage like "12%" or "100%"
    if (match($0, /([0-9]+)%/, m)) {
        p = m[1]

        printf "\r \033[1;34mDownloading\033[0m %s %s%%", bar(p), p
        fflush()
    }
}
END {
    printf "\r\033[K"
}
' >&2)sh";

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

        if (std::string cmd = fmt::format("[ -f \"{}\" ]", out.string()); std::system(cmd.c_str()) != 0) {
            cmd = fmt::format(
                "mkdir -p \"{0}\" && "
                "wget --quiet --show-progress "
                "--https-only "
                "--timeout=10 "
                "--tries=3 "
                "-O \"{1}\" \"{2}\" 2>&1 {3}",
                dir.string(),
                out.string(),
                url,
                awk
            );

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
