module;

#include <spdlog/spdlog.h>
#include <reproc++/run.hpp>

module carton;

std::vector<Target *> Carton::configure_package(const Profile &profile, const Dependency &d) {
    resolve_package(working_dir);

    const auto extra_features = get_requested_features(d.features, d.default_features.value_or(true));

    const bool first = targets.count(package.name) == 0;
    if (!resolved && !first)
        throw ferr("multiple package definition of {:?}", package.name);

    // configure only once in case this target is used by multiple dependencies
    if (first) {
        for (auto &[name, d] : dependencies) {
            if (d.optional || d.version.empty())
                continue;

            // fallback to existing version if any
            if (auto &v = this->cache->resolved_versions[name]; v.empty())
                v = d.version;
            else
                d.version = v;
        }

        auto &target       = *(targets[package.name] = Target::New(lib));
        // TODO
        target.name        = package.name;                    // for path alias in macro-prefix-map
        target.title       = package.name + " v" + d.version; // for display
        target.output_name = package.name;                    // for lib or executable name
        target.output_dir  = package.name + "-v" + d.version;
        target.edition     = package.edition;
        target.working_dir = lib.working_dir;
    }

    auto &lib = *targets[package.name];

    for (auto &[name, d] : dependencies) {
        if (d.optional || name == "default")
            continue;

        const auto dep_name = package.name + "." + name;
        if (targets.count(dep_name))
            continue;

        auto p = resolve_dep(dep_name, d);

        auto &target = *(targets[dep_name] = Target::New(d));
        lib.add_dependency(target);

        target.name        = name;
        target.output_name = dep_name;
        target.edition     = package.edition;
        target.working_dir = d.working_dir;
        if (this->lib.working_dir == d.working_dir) {
            target.name       = lib.name;
            target.title      = lib.title;
            target.output_dir = lib.output_dir;
        } else if (!d.tag.empty()) {
            target.title      = name + " (" + d.git + "@" + d.tag + ")";
            target.output_dir = name + "-" + d.tag;
        } else if (!d.branch.empty()) {
            target.title      = name + " (" + d.git + "@" + d.branch + ")";
            target.output_dir = name + "-" + d.branch;
        } else if (!d.commit.empty()) {
            target.title      = name + " (" + d.git + "@" + d.commit + ")";
            target.output_dir = name + "-" + d.commit;
        } else if (!d.url.empty()) {
            target.title      = name + " (" + d.url + ")";
            target.output_dir = name + "-" + d.commit;
        } else if (!d.path.empty()) {
            target.title      = name + " (" + d.path + ")";
            target.output_dir = lib.output_dir + "." + name;
        } else {
            target.title      = lib.title;
            target.output_dir = lib.output_dir;
        }

        if (p) {
            auto deps = p->configure_package(profile, d);
            target.add_dependencies(deps);
        }

        target.configure(profile, *cache);
    }

    if (first) {
        lib.configure(profile, *cache);
    }

    std::vector<Target *> res = {&lib};

    for (auto &[name, d] : this->dependencies) {
        if (d.optional) {
            if (std::find(extra_features.begin(), extra_features.end(), name) == extra_features.end())
                continue;
        } else {
            if (name != "default" || !default_features)
                continue;
        }

        const auto dep_name   = package.name + "." + name;
        const bool dep_exists = libraries.count(dep_name) > 0;
        if (dep_exists) {
            res.push_back(libraries[dep_name].get());
            continue;
        }

        auto p = resolve_dep(dep_name, d);

        auto &target = *(libraries[dep_name] = Library::New(d));
        res.push_back(&target);
        target.add_dependency(lib);

        target.name        = name;
        target.output_name = dep_name;
        target.edition     = package.edition;
        target.working_dir = d.working_dir;
        if (this->lib.working_dir == d.working_dir) {
            target.name       = lib.name;
            target.title      = lib.title;
            target.output_dir = lib.output_dir;
        } else if (!d.tag.empty()) {
            target.title      = name + " (" + d.git + "@" + d.tag + ")";
            target.output_dir = name + "-" + d.tag;
        } else if (!d.branch.empty()) {
            target.title      = name + " (" + d.git + "@" + d.branch + ")";
            target.output_dir = name + "-" + d.branch;
        } else if (!d.commit.empty()) {
            target.title      = name + " (" + d.git + "@" + d.commit + ")";
            target.output_dir = name + "-" + d.commit;
        } else if (!d.url.empty()) {
            target.title      = name + " (" + d.url + ")";
            target.output_dir = name + "-" + d.commit;
        } else if (!d.path.empty()) {
            target.title      = name + " (" + d.path + ")";
            target.output_dir = lib.output_dir + "." + name;
        } else {
            target.title      = lib.title;
            target.output_dir = lib.output_dir;
        }

        if (p) {
            auto deps = p->configure_v2(d.working_dir, d.features, d.default_features.value_or(true));
            target.add_dependencies(deps);
        }
    }

    resolved = true;
    return res;
}

std::vector<Library *> Carton::configure_bins() {
    resolve_bin();

    std::vector<Library *> res;
    for (auto &bin : bins) {
        const auto bin_name = package.name + "." + bin.name;

        auto &target       = *(libraries[bin_name] = Library::New(lib.working_dir, bin));
        target.name        = package.name;
        target.title       = package.name + " v" + package.version;
        target.output_name = bin_name;
        target.output_dir  = package.name + "-v" + package.version;
        target.edition     = package.edition;
        target.working_dir = lib.working_dir;

        auto deps = configure_v2(lib.working_dir, bin.required_features, true);
        target.add_dependencies(deps);

        res.push_back(&target);
    }

    return res;
}
