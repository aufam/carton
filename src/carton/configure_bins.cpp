module;

#include <string>
#include <vector>
#include <spdlog/spdlog.h>

module carton;
import cpx.fmt;

std::vector<Target *> Carton::configure_bins(const Profile &profile) {
    spdlog::debug("configure_bin lib={}", lib);
    resolve_bin();

    spdlog::debug("configure_bin bins={}", bins);

    std::vector<Target *> res;
    for (auto &bin : bins) {
        const auto bin_name = package.name + "." + bin.name;

        auto &target      = *(targets[bin_name] = Target::New(lib.path, bin));
        auto &main_target = *targets.at(package.name);

        target.name        = main_target.name;
        target.title       = main_target.title;
        target.output_name = bin_name;
        target.output_dir  = main_target.output_dir;
        target.edition     = main_target.edition;

        Dependency entry;
        entry.features = bin.required_features;

        auto deps = configure_package(profile, lib.path, entry);
        target.add_dependencies(deps);
        target.configure(profile, *cache);

        res.push_back(&target);
    }

    return res;
}
