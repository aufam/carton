module;

#include <spdlog/spdlog.h>
#include <reproc++/run.hpp>

module carton;

static Carton &from_registry(Carton &self, const std::string &name) {
    auto &r = self.root ? self.root->registry : self.registry;

    // follow alias
    auto it = r.find(name);
    auto pp = it != r.end() ? &it->second : nullptr;
    while (pp != nullptr) {
        auto it = r.find(pp->package.name);
        auto op = it != r.end() ? &it->second : nullptr;
        if (pp == op)
            break;
        pp = op;
    }

    if (pp == nullptr)
        throw ferr("Cannot find `{}` in the package registry, needed to build {:?}", name, self.package.name);

    return *pp;
}

std::vector<Library *>
Carton::configure_v2(const Profile &profile, const std::vector<std::string> &features, bool default_features) {
    resolve_package();

    const auto extra_features = get_requested_features(features);

    const bool first = libraries.count(package.name) == 0;
    if (!resolved && !first)
        throw ferr("multiple package definition of {:?}", package.name);

    if (first) {
        std::string libname = package.name;

        for (auto &[name, d] : this->dependencies) {
            if (d.optional || d.version.empty())
                continue;

            // fallback to existing version if any
            if (auto &version = this->cache->resolved_versions[name]; version.empty())
                version = d.version;
            else
                d.version = version;

            libname += "." + d.build_name();
        }

        libraries[package.name] = Library::New({
            .dep        = lib,
            .edition    = package.edition,
            .build_name = libname,
        });
    }

    auto &lib = *libraries[package.name];
    for (auto &[name, d] : this->dependencies) {
        // skip if
        if (!default_features && name == "default")
            continue;
        if (d.optional)
            continue;

        if (first && d.empty()) {
            lib.add_interface(d);
        }

        else if (first && !d.version.empty()) {
            auto &p    = from_registry(*this, name);
            auto  deps = p.configure_v2(profile, d.features, d.default_features.value_or(true));
            lib.add_dependencies(deps);
        }

        else if (first) {
            const auto dep_name = package.name + "." + name;
            resolve_dep(dep_name, d);
            libraries[dep_name] = Library::New({
                .dep        = d,
                .edition    = package.edition,
                .build_name = lib.build_name + "." + name,
            });

            lib.add_dependency(*libraries[dep_name]);
        }
    }

    if (extra_features.empty())
        return {&lib};

    std::vector<Library *> res = {&lib};

    for (auto &[name, d] : this->dependencies) {
        // skip if
        if (!default_features && name == "default")
            continue;
        if (d.optional && std::find(extra_features.begin(), extra_features.end(), name) == extra_features.end())
            continue;

        const auto dep_name   = package.name + "." + name;
        const bool dep_exists = libraries.count(dep_name) > 0;
        if (dep_exists) {
            res.push_back(libraries[dep_name].get());
            continue;
        }

        resolve_dep(dep_name, d);
        libraries[dep_name]        = Library::New({
            .dep        = d,
            .edition    = package.edition,
            .build_name = lib.build_name + "." + d.build_name(),
        });
        libraries[dep_name]->title = lib.title;
        libraries[dep_name]->add_dependency(lib, true);

        if (!d.version.empty()) {
            auto &p    = from_registry(*this, name);
            auto  deps = p.configure_v2(profile, d.features, d.default_features.value_or(true));
            libraries[dep_name]->add_dependencies(deps);
        }

        res.push_back(libraries[dep_name].get());
    }

    return res;
}
