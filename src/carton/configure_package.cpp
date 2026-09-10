module;

#include <spdlog/spdlog.h>
#include <reproc++/run.hpp>
#include <set>
#include <xxhash.h>

module carton;
import cpx.fmt;

static std::string display_name(const std::string &name, const Dependency &d, const Package *package = nullptr) {
    std::string res = name;
    if (!d.version.empty()) {
        res += " v" + d.version;
        return res;
    } else if (package && !package->version.empty()) {
        res += " v" + package->version;
    }

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
        res += "@v" + d.version;
    } else if (!d.git.empty()) {
        fs::path git = d.git;
        if (auto repo = git.filename().stem().string(); repo == name) {
            res += ".git";
        } else {
            res += "-" + repo + ".git";
        }

        if (!d.commit.empty()) {
            res += "@" + d.commit;
        } else if (!d.tag.empty()) {
            res += "@" + d.tag;
        } else if (!d.branch.empty()) {
            res += "@" + d.branch;
        }
    } else if (!d.url.empty()) {
        // TODO: needs to parse URL
        res += "-archive@" + fs::path(d.url).filename().string();
    } else if (!d.path.empty()) {
        res += f("-{:04x}", XXH3_64bits(d.path.data(), d.path.size()) & 0xffff);
    }

    return res;
}

std::vector<Target *> Carton::configure_package(
    const Profile &profile, const std::string &working_dir, const std::vector<std::string> &features, bool default_features
) {
    spdlog::debug(
        "configure_package name={} working_dir={:?} features={}, default_features={}",
        package.name,
        working_dir,
        features,
        default_features
    );

    resolve_package(working_dir);

    const auto extra_features = get_requested_features(features, default_features);

    const bool first = targets.count(package.name) == 0;
    if (!configured && !first)
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
        target.name        = package.name;
        target.title       = display_name(package.name, lib, &package);
        target.output_name = package.name;
        target.output_dir  = output_dir(package.name, lib);
        target.edition     = package.edition;
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

        auto [p, working_dir] = resolve_dep(name, d);

        auto &target = *(targets[target_name] = Target::New(d));

        if (this->lib.path == d.path) {
            target.name        = main_target.name;
            target.output_name = target_name;
            target.title       = main_target.title;
            target.output_dir  = main_target.output_dir;
            target.edition     = main_target.edition;
        } else {
            target.name        = name;
            target.title       = display_name(name, d);
            target.output_name = name;
            target.output_dir  = output_dir(name, d);
            target.edition     = package.edition;
        }

        if (p) {
            auto deps = p->configure_package(profile, working_dir, d.features, d.default_features.value_or(true));
            target.add_dependencies(deps, true);
        }

        target.configure_module(profile, *cache);
        target.configure(profile, *cache);
        required_targets.push_back(&target);
    }

    if (first) {
        main_target.add_dependencies(required_targets);
        main_target.configure_module(profile, *cache);
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
            push_unique(res, targets[target_name].get(), true);
            continue;
        }

        auto [p, working_dir] = resolve_dep(name, d);

        auto &target = *(targets[target_name] = Target::New(d));
        push_unique(res, targets[target_name].get(), true);
        target.add_dependency(main_target, true);

        if (this->lib.path == d.path) {
            target.name        = main_target.name;
            target.output_name = target_name;
            target.title       = main_target.title;
            target.output_dir  = main_target.output_dir;
            target.edition     = main_target.edition;
        } else {
            target.name        = name;
            target.title       = display_name(name, d);
            target.output_name = name;
            target.output_dir  = output_dir(name, d);
            target.edition     = package.edition;
        }

        if (p) {
            auto deps = p->configure_package(profile, working_dir, d.features, d.default_features.value_or(true));
            target.add_dependencies(deps, true);
        }

        target.configure_module(profile, *cache);
        target.configure(profile, *cache);
    }

    configured = true;
    return res;
}
