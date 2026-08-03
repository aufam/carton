module;

#include <algorithm>
#include <spdlog/spdlog.h>

module carton;

static std::string
find_extra_features(Carton &p, const std::string &feat, std::vector<std::string> &required_features, bool dep = false) {
    if (feat.starts_with("dep:"))
        return find_extra_features(p, feat.substr(4), required_features, true);

    if (auto it = p.features.find(feat); dep || it == p.features.end()) {
        auto d = p.dependencies.find(feat);
        if (d == p.dependencies.end())
            return f("Error building {:?}: feature `{}` is not defined in the package or dependencies", p.package.name, feat);
        else if (d->second.optional)
            push_unique(required_features, feat);
    } else {
        for (auto &dep : it->second)
            find_extra_features(p, dep, required_features, dep == feat);
    }
    return "";
}

static std::string make_feature_signature(std::vector<std::string> &feats) {
    if (feats.empty())
        return "-";
    std::sort(feats.begin(), feats.end());
    return f("{}", fmt::join(feats, "-"));
}

void Carton::configure(const Profile &profile, const std::vector<std::string> &features, bool from_registry) {
    if (package.name.empty())
        throw ferr("{:?}: name is required", package.name);

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

    if (!lib.version.empty())
        throw ferr("{:?}: lib version is already set to {}", package.name, lib.version);

    lib.name = package.name;
    resolve_remote_dep(profile, lib, from_registry);

    spdlog::info("finding extra features for {}, total_features={}", package.name, this->features.size());
    std::vector<std::string> extra_features;
    if (no_default_features)
        std::ignore = find_extra_features(*this, "nodefault", extra_features);
    else
        std::ignore = find_extra_features(*this, "default", extra_features);

    for (auto &feat : features) {
        if (auto pos = feat.find('/'); pos != std::string::npos) {
            auto subdep  = feat.substr(0, pos);
            auto subfeat = feat.substr(pos + 1);
            if (auto it = dependencies.find(subdep); it != dependencies.end())
                push_unique(it->second.features, subfeat);
            else
                throw ferr("Cannot find `{}` in the dependency table", subdep);
            continue;
        }

        auto err = find_extra_features(*this, feat, extra_features);
        if (!err.empty())
            throw std::runtime_error(err);
    }

    spdlog::info("resolving: dep={:?} extra_features={}", package.name, extra_features);
    for (auto &[name, d] : dependencies) {
        // skip if
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

        const bool d_is_public = d.visibility == "public";

        auto dp = &d;
        if (!d.version.empty()) {
            // configure from registry
            auto &r = root ? root->registry : this->registry;

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
                throw ferr("Cannot find `{}` in the package registry", name);

            const auto feature_signature = make_feature_signature(d.features);

            auto &p = *pp;

            if (p.package.version == d.version && feature_signature == p.lib.feature_signature) {
                lib += p.lib;
                if (d_is_public)
                    push_unique(lib.public_flags, p.lib.public_flags);
                continue;
            } else if (!p.package.version.empty() && p.package.version != d.version)
                throw ferr("Found multiple version of `{}`: [{}, {}]", p.package.name, p.package.version, d.version);
            else if (!p.lib.feature_signature.empty() && p.lib.feature_signature != feature_signature)
                throw ferr("Found multiple feature list of package `{}`: [{}, {}]", p.package.name, p.lib.features, d.features);

            if (p.package.edition > package.edition)
                throw ferr(
                    "Error building dependency package={0:?}: {0:?} required std=c++{1} but {2:?} only supports std=c++{3}",
                    p.package.name,
                    p.package.edition,
                    package.name,
                    package.edition
                );

            p.root                = this;
            p.cache_dir           = this->cache_dir;
            p.cache               = this->cache;
            p.no_default_features = !d.default_features.value_or(true);
            p.profiles            = profiles;
            p.package.version     = d.version;
            p.configure(profile, d.features, true);

            dp = &p.lib;
        } else {
            d.name = package.name + "." + name;
            try {
                resolve_remote_dep(profile, d, true);
            } catch (const std::exception &e) {
                throw ferr("Error resolving {:?} required by {:?}: {}", name, package.name, e.what());
            }
        }

        try {
            collect_meta(profile, *dp);
        } catch (const std::exception &e) {
            throw ferr("Error collecting meta of {:?} required by {:?}: {}", name, package.name, e.what());
        }

        lib += *dp;
        if (d_is_public)
            push_unique(lib.public_flags, dp->public_flags);
        cache->dependencies.push_back(dp);
    }

    lib.version           = package.version;
    lib.cpp_standard      = package.edition;
    lib.features          = features;
    lib.default_features  = !no_default_features;
    lib.feature_signature = make_feature_signature(lib.features);
}
