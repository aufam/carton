module;

#include <spdlog/spdlog.h>
#include <chrono>

module carton;
import cpx;
import cpx.yy_json;

int Carton::execute(Cli &cli) {
    const bool  manifest = cli.manifest.has_value();
    const bool  run      = cli.run.has_value();
    const bool  build    = cli.build.has_value();
    const bool  release  = run ? cli.run->release : build ? cli.build->release : manifest ? cli.manifest->release : cli.release;
    const auto &profile  = release ? this->profiles.release : this->profiles.dev;
    const auto &features = run        ? cli.run->features
                           : build    ? cli.build->features
                           : manifest ? cli.manifest->features
                                      : cli.features;

    const bool no_default_features = run        ? cli.run->no_default_features
                                     : build    ? cli.build->no_default_features
                                     : manifest ? cli.manifest->no_default_features
                                                : cli.no_default_features;

    const auto pretty_two_spaces = cpx::yy_json::write_flag::pretty_two_spaces;

    cache->common_flags =
        f( //
            "{} -O{} {} {} {} {}",
            profile.debug ? "-g" : "-DNDEBUG",
            profile.opt_level,
            profile.lto ? "-flto" : "",
            profile.asan ? "-fsanitize=address,undefined" : "",
            profile.arch.empty() ? "" : "-march=" + profile.arch,
            fmt::join(profile.flags, " ")
        );
    cache->cppm_standard = std::max(20, package.edition);

    try {
        Dependency d;
        d.features         = features;
        d.default_features = !no_default_features;

        std::ignore = this->configure_package(profile, fs::current_path().string(), d);
    } catch (std::exception &e) {
        spdlog::error("Failed to configure: {}", e.what());
        return 1;
    }

    const auto start = std::chrono::steady_clock::now();

    try {
        std::ignore = this->configure_bins(profile);
    } catch (std::exception &e) {
        spdlog::error("Failed to configure binaries: {}", e.what());
        return 1;
    }

    auto &ccs = cache->compile_commands;

    auto _ = cpx::defer([&ccs]() {
        auto of = std::ofstream("./compile_commands.json");
        of << cpx::yy_json::dump(ccs, pretty_two_spaces);
    });

    try {
        CompileCommand::compile_multi(ccs, true);
    } catch (std::exception &e) {
        spdlog::error("Failed to compile modules: {}", e.what());
        return 1;
    }

    if (build)
        try {
            CompileCommand::compile_multi(ccs, false);
        } catch (std::exception &e) {
            spdlog::error("Failed to compile: {}", e.what());
            return 1;
        }

    std::string profile_info = profile.opt_level == 0 ? "unoptimized" : "optimized";
    if (profile.debug)
        profile_info += " + debuginfo";
    if (profile.asan)
        profile_info += " + asan";

    std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    print_status("Finished", f("`{}` profile [{}] target(s) in {:.2f}", profile.name, profile_info, elapsed.count()));

    if (manifest) {
        this->registry.clear(); // TODO
        fmt::println("{}", cpx::yy_json::dump(*this, pretty_two_spaces));
    }

    return 0;
}
