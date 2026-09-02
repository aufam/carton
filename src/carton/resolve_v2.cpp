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

static Carton &from_registry(Carton &self, const std::string &name) {
    auto &root     = *self.root;
    auto &registry = root.registry;

    // follow alias
    auto it = registry.find(name);
    auto pp = it != registry.end() ? &it->second : nullptr;
    while (pp != nullptr) {
        auto it = registry.find(pp->package.name);
        auto op = it != registry.end() ? &it->second : nullptr;
        if (pp == op)
            break;
        pp = op;
    }

    if (pp == nullptr)
        throw ferr("Cannot find `{}` in the package registry, needed to build {:?}", name, self.package.name);

    pp->root  = &root;
    pp->cache = root.cache;
    return *pp;
}

void Carton::resolve_package(const std::string &working_dir) {
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

    if (!lib.path.empty()) {
        spdlog::info("resolving dep={:?} path={:?}", package.name, lib.path);
        lib.path = resolve_path(cache->directory, lib.path);

        if (fs::path path = lib.path; path.is_relative())
            lib.path = (fs::path(working_dir) / lib.subdir / path).lexically_normal().string();
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
    fs::path   root      = fs::path(lib.path) / lib.subdir;
    lib.working_dir      = root.string();

    if (auto sub = root / "carton.toml"; dirchange && fs::exists(sub)) {
        lib.path   = "";
        lib.subdir = "";

        constexpr auto toml_version = cpx::toruniina_toml::spec::v(1, 1, 0);
        cpx::toruniina_toml::parse_from_file(sub.string(), *this, toml_version);

        resolve_package(lib.working_dir);
        return;
    }

    if (lib.mod.empty() && fs::exists(root / "src" / "lib.cppm"))
        lib.mod = collect_cppm_globs(working_dir, "src", lib.src.empty() ? &lib.src : nullptr);
    if (lib.src.empty() && fs::is_directory(root / "src"))
        lib.src = {"src/*"};
    if (lib.inc.empty() && fs::is_directory(root / "include"))
        lib.inc = {"public:include"};

    expand_path(lib.working_dir, lib.mod);
    expand_path(lib.working_dir, lib.src);
    lib.src.erase(
        std::remove_if(
            lib.src.begin(),
            lib.src.end(),
            [](const std::string &path) { return fs::path(path).filename().string().starts_with("main."); }
        ),
        lib.src.end()
    );
}

Carton *Carton::resolve_dep(const std::string &name, Dependency &dep) {
    Carton *res = nullptr;

    if (!dep.path.empty()) {
        spdlog::info("resolving dep={:?} path={:?}", name, dep.path);
        dep.path = resolve_path(cache->directory, dep.path);

        if (fs::path path = dep.path; path.is_relative())
            dep.path = (fs::path(lib.path) / lib.subdir / path).lexically_normal().string();
    } else if (!dep.url.empty()) {
        spdlog::info("resolving dep={:?} url={:?}", name, dep.url);
        dep.path = resolve_path(cache->directory, dep.url);
    } else if (!dep.git.empty()) {
        auto &tag = !dep.tag.empty() ? dep.tag : !dep.branch.empty() ? dep.branch : dep.tag;
        spdlog::info("resolving dep={:?} git={:?} tag={:?}", name, dep.git, tag);
        dep.path = git_clone(cache->directory, dep.git, tag);
    } else if (!dep.version.empty()) {
        dep.path = lib.path;
        auto &p  = from_registry(*this, name);
        res      = &p;
    } else {
        dep.path = lib.path;
    }

    const bool dirchange   = dep.path != lib.path;
    fs::path   working_dir = fs::path(dep.path) / dep.subdir;
    dep.working_dir        = working_dir.string();

    if (auto sub = working_dir / "carton.toml"; dirchange && fs::exists(sub)) {
        constexpr auto toml_version = cpx::toruniina_toml::spec::v(1, 1, 0);

        locals.push_back(std::make_unique<Carton>());
        auto &p = *locals.back();
        p.root  = this->root;
        p.cache = this->cache;

        cpx::toruniina_toml::parse_from_file(sub.string(), p, toml_version);
        res = &p;
    }

    if (dirchange) {
        if (dep.mod.empty() && fs::exists(working_dir / "src" / "lib.cppm"))
            dep.mod = collect_cppm_globs(working_dir, "src", dep.src.empty() ? &dep.src : nullptr);
        if (dep.src.empty() && fs::is_directory(working_dir / "src"))
            dep.src = {"src/*"};
        if (dep.inc.empty() && fs::is_directory(working_dir / "include"))
            dep.inc = {"public:include"};
    }

    expand_path(dep.working_dir, dep.mod);
    expand_path(dep.working_dir, dep.src);
    dep.src.erase(
        std::remove_if(
            dep.src.begin(),
            dep.src.end(),
            [](const std::string &path) { return fs::path(path).filename().string().starts_with("main."); }
        ),
        dep.src.end()
    );
    return res;
}

void Carton::resolve_bin() {
    if (bin.empty()) {
        const fs::path root = lib.working_dir;
        if (fs::path main_path = root / "src" / "main.cpp"; fs::exists(main_path)) {
            Binary main;
            main.name = "main";
            main.path = main_path.string();
            bins.push_back(main);
            resolve_bin();
        }
    }

    for (auto &bin : bins) {
        const fs::path path = bin.path;
        if (fs::is_directory(path)) {
            bin.src = {bin.path + "/*"};
        } else {
            bin.src = {bin.path};
        }
        expand_path(lib.working_dir, bin.src);
    }
}
