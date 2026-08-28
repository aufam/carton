module;

#include <spdlog/spdlog.h>

module carton;
import cpx;
import cpx.toruniina_toml;

static std::vector<std::string>
collect_cppm_globs(const fs::path &working_dir, const fs::path &src_dir, std::vector<std::string> *cpps) {
    std::vector<std::string> result;

    auto it = fs::recursive_directory_iterator(working_dir / src_dir);
    for (auto ptr = fs::begin(it); ptr != fs::end(it); ++ptr) {
        const auto &entry = *ptr;
        if (entry.path().filename() != "lib.cppm")
            continue;

        auto relative_dir = fs::relative(entry.path().parent_path(), working_dir);
        result.push_back((relative_dir / "*.cppm").generic_string());
        if (cpps)
            cpps->push_back((relative_dir / "*").generic_string());
    }

    return result;
}

void Carton::resolve_package() {
    if (resolved)
        return;

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

    apply_package_placeholders();

    const bool is_root = fs::path(lib.path).is_absolute();

    if (lib.empty())
        throw ferr("assertion failed: {:?} cannot be empty", package.name);

    if (!lib.path.empty()) {
        spdlog::info("resolving dep={:?} path={:?}", package.name, lib.path);
        lib.path = resolve_path(cache_dir, lib.path);
    } else if (!lib.url.empty()) {
        spdlog::info("resolving dep={:?} url={:?}", package.name, lib.url);
        lib.path = resolve_path(cache_dir, lib.url);
    } else if (!lib.git.empty()) {
        auto &tag = !lib.tag.empty() ? lib.tag : !lib.branch.empty() ? lib.branch : lib.tag;
        spdlog::info("resolving dep={:?} git={:?} tag={:?}", package.name, lib.git, tag);
        lib.path = git_clone(cache_dir, lib.git, tag);
    } else {
        throw ferr("path|git|url is not defined");
    }

    fs::path working_dir = fs::path(lib.path) / lib.subdir;
    lib.working_dir      = working_dir.string();

    if (auto sub = working_dir / "carton.toml"; !is_root && fs::exists(sub)) {
        lib.path   = "";
        lib.subdir = "";

        constexpr auto toml_version = cpx::toruniina_toml::spec::v(1, 1, 0);
        cpx::toruniina_toml::parse_from_file(sub.string(), *this, toml_version);

        const fs::path path = lib.path, subdir = lib.subdir;
        if (path.is_absolute() || subdir.is_absolute())
            throw ferr("Path must be relative");

        lib.path        = (working_dir / path).string();
        working_dir     = path / subdir;
        lib.working_dir = working_dir.string();
    }

    if (lib.mod.empty() && fs::exists(working_dir / "src" / "lib.cppm"))
        lib.mod = collect_cppm_globs(working_dir, "src", lib.src.empty() ? &lib.src : nullptr);
    if (lib.src.empty() && fs::is_directory(working_dir / "src"))
        lib.src = {"src/*"};
    if (lib.inc.empty() && fs::is_directory(working_dir / "include"))
        lib.inc = {"public:include"};

    expand_path(lib.working_dir, lib.mod);
    expand_path(lib.working_dir, lib.src);
}

void Carton::resolve_dep(const std::string &name, Dependency &dep) {
    if (!dep.path.empty()) {
        spdlog::info("resolving dep={:?} path={:?}", name, dep.path);
        dep.path = resolve_path(cache_dir, dep.path);

        if (fs::path path = dep.path; path.is_relative())
            dep.path = (fs::path(lib.path) / lib.subdir / path).lexically_normal().string();
    } else if (!dep.url.empty()) {
        spdlog::info("resolving dep={:?} url={:?}", name, dep.url);
        dep.path = resolve_path(cache_dir, dep.url);
    } else if (!dep.git.empty()) {
        auto &tag = !dep.tag.empty() ? dep.tag : !dep.branch.empty() ? dep.branch : dep.tag;
        spdlog::info("resolving dep={:?} git={:?} tag={:?}", name, dep.git, tag);
        dep.path = git_clone(cache_dir, dep.git, tag);
    } else {
        dep.path = lib.path;
    }

    fs::path working_dir = fs::path(dep.path) / dep.subdir;
    dep.working_dir      = working_dir.string();

    if (auto sub = working_dir / "carton.toml"; fs::exists(sub)) {
        // TODO
        throw ferr("TODO: not implemented yet");
    }

    if (dep.mod.empty() && fs::exists(working_dir / "src" / "lib.cppm"))
        dep.mod = collect_cppm_globs(working_dir, "src", dep.src.empty() ? &dep.src : nullptr);
    if (dep.src.empty() && fs::is_directory(working_dir / "src"))
        dep.src = {"src/*"};
    if (dep.inc.empty() && fs::is_directory(working_dir / "include"))
        dep.inc = {"public:include"};

    expand_path(dep.working_dir, dep.mod);
    expand_path(dep.working_dir, dep.src);
}
