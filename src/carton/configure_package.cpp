module;

#include <spdlog/spdlog.h>
#include <reproc++/run.hpp>
#include <set>
#include <xxhash.h>

module carton;

static uint64_t hash64(std::string_view s) {
    XXH3_state_t *state = XXH3_createState();
    XXH3_64bits_reset(state);

    XXH3_64bits_update(state, s.data(), s.size());
    const uint64_t h = XXH3_64bits_digest(state);

    XXH3_freeState(state);

    return h;
}

static std::string display_name(const std::string &name, const Dependency &d) {
    std::string res = name;
    if (!d.version.empty())
        res += " v" + d.version;

    if (!d.git.empty()) {
        res += " (" + d.git;
        if (!d.commit.empty()) {
            res += "@" + d.commit;
        } else if (!d.tag.empty()) {
            res += "@" + d.tag;
        } else if (!d.branch.empty()) {
            res += "@" + d.branch;
        }
        res += ")";
    } else if (!d.url.empty()) {
        res += " (" + d.url + ")";
    } else if (!d.path.empty()) {
        res += " (" + d.path + ")";
    }

    return res;
}

static std::string output_dir(const std::string &name, const Dependency &d) {
    std::string res = name;
    if (!d.version.empty()) {
        res += "-v" + d.version;
    } else if (!d.git.empty()) {
        auto repo = fs::path(d.git).filename().string();
        if (!d.commit.empty()) {
            res += "@" + repo + "@" + d.commit;
        } else if (!d.tag.empty()) {
            res += "-" + repo + "@" + d.tag;
        } else if (!d.branch.empty()) {
            res += "-" + repo + "@" + d.branch;
        }
    } else if (!d.path.empty()) {
        res = f("{}-{:016x}", name, hash64(d.path));
    }

    return res;
}

std::vector<Target *> Carton::configure_package(const Profile &profile, const std::string &working_dir, const Dependency &d) {
    resolve_package(working_dir);

    const auto extra_features = get_requested_features(d.features, d.default_features.value_or(true));

    const bool first = targets.count(package.name) == 0;
    if (!resolved && !first)
        throw ferr("multiple package definition of {:?}", package.name);

    // sort names
    std::set<std::string> nameset;
    for (auto &[name, _] : dependencies)
        nameset.insert(name);

    // configure only once in case this target is used by multiple dependencies
    if (first) {
        for (const auto &name : nameset) {
            auto &d = dependencies.at(name);
            if (d.version.empty())
                continue;

            // fallback to existing version if any
            if (auto &v = this->cache->resolved_versions[name]; v.empty())
                v = d.version;
            else
                d.version = v;
        }

        auto &target       = *(targets[package.name] = Target::New(lib));
        target.name        = package.name;                  // for path alias in macro-prefix-map
        target.title       = display_name(package.name, d); // for display
        target.output_name = package.name;                  // for lib or executable name
        target.output_dir  = output_dir(package.name, d);
        target.edition     = package.edition;
        target.working_dir = lib.path;
    }

    auto &main_target = *targets[package.name];

    std::vector<Target *> required_targets;

    for (const auto &name : nameset) {
        auto &d = dependencies.at(name);

        if (d.optional || name == "default")
            continue;

        const auto target_name = package.name + "." + name;
        if (targets.count(target_name))
            continue;

        auto p = resolve_dep(target_name, d);

        auto &target = *(targets[target_name] = Target::New(d));

        if (this->lib.path == d.path) {
            target.name        = main_target.name;
            target.output_name = target_name;
            target.name        = main_target.name;
            target.title       = main_target.title;
            target.output_dir  = main_target.output_dir;
            target.edition     = main_target.edition;
        } else {
            target.name        = name;
            target.title       = display_name(name, d);
            target.output_name = name;
            target.output_dir  = output_dir(name, d);
            target.edition     = package.edition;
            target.working_dir = d.path;
        }

        if (p) {
            auto deps = p->configure_package(profile, lib.path, d);
            target.add_dependencies(deps);
        }

        target.configure(profile, *cache);
        required_targets.push_back(&target);
    }

    if (first) {
        main_target.add_dependencies(required_targets);
        main_target.configure(profile, *cache);
    }

    std::vector<Target *> res = {&main_target};

    for (const auto &name : nameset) {
        auto &d = dependencies.at(name);

        if (d.optional) {
            if (std::find(extra_features.begin(), extra_features.end(), name) == extra_features.end())
                continue;
        } else {
            if (name != "default" || !d.default_features.value_or(true))
                continue;
        }

        const auto target_name = package.name + "." + name;
        if (targets.count(target_name)) {
            res.push_back(targets[target_name].get());
            continue;
        }

        auto p = resolve_dep(target_name, d);

        auto &target = *(targets[target_name] = Target::New(d));
        res.push_back(&target);
        target.add_dependency(main_target);

        if (this->lib.path == d.path) {
            target.name        = main_target.name;
            target.output_name = target_name;
            target.name        = main_target.name;
            target.title       = main_target.title;
            target.output_dir  = main_target.output_dir;
            target.edition     = main_target.edition;
        } else {
            target.name        = name;
            target.title       = display_name(name, d);
            target.output_name = name;
            target.output_dir  = output_dir(name, d);
            target.edition     = package.edition;
            target.working_dir = d.path;
        }

        if (p) {
            auto deps = p->configure_package(profile, lib.path, d);
            target.add_dependencies(deps);
        }

        target.configure(profile, *cache);
    }

    resolved = true;
    return res;
}
