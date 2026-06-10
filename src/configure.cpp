module;

#include <cpx/fmt.h>
#include <cpx/toml/toruniina_toml.h>
#include <algorithm>
#include <spdlog/spdlog.h>

module carton;


static std::string
find_extra_features(Carton &p, const std::string &feat, std::vector<std::string> &required_features, bool dep = false) {
    // TODO: support "feat1/feat2" syntax for features of dependencies
    if (feat.starts_with("dep:"))
        return find_extra_features(p, feat.substr(4), required_features, true);

    if (auto it = p.features.find(feat); dep || it == p.features.end()) {
        auto d = p.dependencies.find(feat);
        if (d == p.dependencies.end())
            return f("Error building {:?}: feature `{}` is not defined in the package or dependencies", p.package.name, feat);
        else if (convert_dep(d->second).optional)
            push_unique(required_features, feat);
    } else {
        for (auto &dep : it->second)
            find_extra_features(p, dep, required_features, dep == feat);
    }
    return "";
}

void Carton::configure(const Profile &profile, const std::vector<std::string> &features) {
    if (package.name.empty())
        throw ferr("Error building {:?}: name is required", package.name);

    switch (package.edition) {
    case 11:
    case 14:
    case 17:
    case 20:
    case 23:
    case 26:
        break;
    default:
        throw ferr("Error building {:?}: unsupported edition: {}", package.name, package.edition);
    }

    apply_package_placeholders();

    if (!lib.version.empty())
        throw ferr("Error building {:?}: lib version is already set to {}", package.name, lib.version);

    if (lib.name.empty())
        lib.name = package.name;
    resolve_remote_dep(profile, package.name, lib);

    std::vector<std::string> extra_features;
    if (!no_default_features)
        std::ignore = find_extra_features(*this, "default", extra_features);
    else
        std::ignore = find_extra_features(*this, "nodefault", extra_features);
    for (auto &feat : features) {
        auto err = find_extra_features(*this, feat, extra_features);
        if (!err.empty())
            throw std::runtime_error(err);
    }

    spdlog::info("resolving: dep={:?} extra_features={}", package.name, extra_features);
    for (auto &[name, dep] : dependencies) {
        auto &d = convert_dep(dep);
        if (d.name.empty())
            d.name = name;
        if (no_default_features && name == "default")
            continue;
        if (!no_default_features && name == "nodefault")
            continue;
        if (d.optional && std::find(extra_features.begin(), extra_features.end(), name) == extra_features.end())
            continue;
        if (d.empty()) {
            lib += d;
            continue;
        }

        // TODO: what if the subpackage got discovered first?
        if (!d.version.empty() && d.version.front() == '?') {
            if (pparent == nullptr) {
                d.version = d.version.substr(1);
            } else {
                auto it = pparent->dependencies.find(d.name);
                if (it == pparent->dependencies.end())
                    throw ferr("dep={:?} must be in the parent project", d.name);
                d = convert_dep(it->second);
            }
        }

        const Cache::Meta *existing = nullptr;
        for (const auto &m : cache->meta)
            if (m.lib.name == d.name) {
                if (m.lib.version != d.version)
                    throw ferr("version already exist");
                spdlog::info("found {} v{}", m.lib.name, m.lib.version);
                existing = &m;
                break;
            }

        if (existing) {
            push_unique(lib.flags, existing->flags);
            push_unique(lib.link_flags, existing->link_flags);
            if (package.edition >= 20 && profile._module_support)
                push_unique(lib.flags, existing->module_flags);
            push_unique(lib.flags, existing->include_flags);
            continue;
        }

        try {
            resolve_remote_dep(profile, name, d);
        } catch (const std::exception &e) {
            throw ferr("Error resolving {:?} required by {:?}: {}", name, package.name, e.what());
        }

        try {
            auto m = collect_meta(profile, d);
            push_unique(lib.flags, m.flags);
            push_unique(lib.link_flags, m.link_flags);
            if (package.edition >= 20 && profile._module_support)
                push_unique(lib.flags, m.module_flags);
            push_unique(lib.flags, m.include_flags);
            cache->meta.push_back(std::move(m));
        } catch (const std::exception &e) {
            throw ferr("Error collecting meta of {:?} required by {:?}: {}", name, package.name, e.what());
        }
    }

    lib.version          = package.version;
    lib.cpp_standard     = package.edition;
    lib.features         = std::move(extra_features);
    lib.default_features = !no_default_features;
}
