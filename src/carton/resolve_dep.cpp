module;

#include <spdlog/spdlog.h>

module carton;
import cpx.fmt;
import cpx.toruniina_toml;

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

Carton *Carton::resolve_dep(const std::string &name, Dependency &dep) {
    spdlog::trace("resolve_dep before name={} dep={}", name, dep);
    Carton *res = nullptr;

    if (!dep.path.empty()) {
        spdlog::info("resolving dep={:?} path={:?}", name, dep.path);
        dep.path = resolve_path(cache->directory, dep.path);

        if (fs::path path = dep.path; path.is_relative())
            dep.path = (fs::path(lib.path) / path).lexically_normal().string();
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

    bool     dirchange = dep.path != lib.path;
    fs::path root      = fs::path(dep.path);

    if (auto sub = root / "carton.toml"; dirchange && fs::exists(sub)) {
        constexpr auto toml_version = cpx::toruniina_toml::spec::v(1, 1, 0);

        locals.push_back(std::make_unique<Carton>());
        auto &p = *locals.back();
        p.root  = this->root;
        p.cache = this->cache;

        cpx::toruniina_toml::parse_from_file(sub.string(), p, toml_version);
        res = &p;

        // restore working dir
        dep.path  = lib.path;
        root      = fs::path(dep.path);
        dirchange = false;
    }

    if (dirchange) {
        if (dep.mod.empty() && fs::exists(root / "src" / "lib.cppm"))
            dep.mod = collect_cppm_globs(root, "src", dep.src.empty() ? &dep.src : nullptr);
        if (dep.src.empty() && fs::is_directory(root / "src"))
            dep.src = {"src/*"};
        if (dep.inc.empty() && fs::is_directory(root / "include"))
            dep.inc = {"public:include"};
    }

    expand_path(dep.path, dep.mod);
    expand_path(dep.path, dep.src);
    dep.src.erase(
        std::remove_if(
            dep.src.begin(),
            dep.src.end(),
            [](const std::string &path) { return fs::path(path).filename().string().starts_with("main."); }
        ),
        dep.src.end()
    );

    spdlog::trace("resolve_dep after name={} dep={}", name, dep);
    return res;
}
