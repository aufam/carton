module;

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

std::vector<std::string> Carton::get_requested_features(const std::vector<std::string> &features, bool default_features) {
    spdlog::info("finding extra features for {}, total_features={}", package.name, this->features.size());

    std::vector<std::string> extra_features;
    if (default_features)
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

    return extra_features;
}
