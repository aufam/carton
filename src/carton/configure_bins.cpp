module;

#include <string>
#include <vector>
#include <spdlog/spdlog.h>

module carton;
import cpx.fmt;

std::vector<Target *> Carton::configure_bins(const Profile &profile) {
    const fs::path root = lib.path;
    if (fs::path main_path = root / "src" / "main.cpp"; bins.empty() && fs::exists(main_path)) {
        Binary main;
        main.name = "main";
        main.path = "src/main.cpp";
        bins.push_back(main);
    }

    spdlog::debug("configure_bin bins={}", bins);

    std::vector<Target *> res;
    for (auto &bin : bins) {
        const auto bin_name = package.name + ".bin." + bin.name;

        auto &target      = *(targets[bin_name] = Target::New(lib.path, bin));
        auto &main_target = *targets.at(package.name);

        target.name        = main_target.name;
        target.title       = main_target.title;
        target.output_name = bin_name;
        target.output_dir  = main_target.output_dir;
        target.edition     = main_target.edition;

        if (target.src.empty())
            ferr("source file cannot be empty for binary target `{}`", bin_name);

        auto deps = configure_package(profile, lib.path, bin.required_features);
        target.add_dependencies(deps, false, true);
        target.configure(profile, *cache, "exe");

        push_unique(res, &target, true);
    }

    return res;
}
