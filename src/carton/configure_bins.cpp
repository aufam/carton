module;

#include <string>
#include <vector>

module carton;

std::vector<Target *> Carton::configure_bins(const Profile &profile) {
    resolve_bin();

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
        target.working_dir = main_target.working_dir;

        Dependency entry;
        entry.features = bin.required_features;

        auto deps = configure_package(profile, lib.path, entry);
        target.add_dependencies(deps);

        res.push_back(&target);
    }

    return res;
}
