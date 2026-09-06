module;

#include <spdlog/spdlog.h>

module carton;
import cpx.fmt;
import cpx.toruniina_toml;

void Carton::resolve_package(const std::string &working_dir) {
    if (configured)
        return;

    spdlog::trace("resolve_package before working_dir={:?} lib={}", working_dir, lib);
    if (package.name.empty())
        throw ferr("{:?}: name is required", package.name);

    // c++ standard version
    switch (package.edition) {
    case 11:
    case 14:
    case 17:
    case 20:
    case 23:
    case 26: break;
    default: throw ferr("{:?}: unsupported edition: {}", package.name, package.edition);
    }

    apply_placeholders();

    if (!lib.path.empty()) {
        spdlog::info("resolving dep={:?} path={:?}", package.name, lib.path);
        lib.path = resolve_path(cache->directory, lib.path);

        if (fs::path path = lib.path; path.is_relative())
            lib.path = (fs::path(working_dir) / path).lexically_normal().string();
    } else if (!lib.url.empty()) {
        spdlog::info("resolving dep={:?} url={:?}", package.name, lib.url);
        lib.path = resolve_path(cache->directory, lib.url);
    } else if (!lib.git.empty()) {
        auto &tag = !lib.tag.empty() ? lib.tag : !lib.branch.empty() ? lib.branch : lib.tag;
        spdlog::info("resolving dep={:?} git={:?} tag={:?}", package.name, lib.git, tag);
        lib.path = git_clone(cache->directory, lib.git, tag);
    } else if (!lib.version.empty()) {
        throw ferr("path|git|url is not defined");
    } else {
        lib.path = working_dir;
    }

    const bool dirchange = lib.path != working_dir;
    fs::path   root      = fs::path(lib.path);

    if (auto sub = fs::path(lib.path) / "carton.toml"; dirchange && fs::exists(sub)) {
        const auto working_dir  = std::exchange(lib.path, "");
        const auto toml_version = cpx::toruniina_toml::spec::v(1, 1, 0);

        cpx::toruniina_toml::parse_from_file(sub.string(), *this, toml_version);
        resolve_package(working_dir);
        return;
    }

    if (lib.mod.empty() && fs::exists(root / "src" / "lib.cppm"))
        lib.mod = collect_cppm_globs(lib.path, "src", lib.src.empty() ? &lib.src : nullptr);
    if (lib.src.empty() && fs::is_directory(root / "src"))
        lib.src = {"src/*"};
    if (lib.inc.empty() && fs::is_directory(root / "include"))
        lib.inc = {"public:include"};

    spdlog::trace("resolve_package after working_dir={:?} lib={}", working_dir, lib);
}
