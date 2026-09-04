module;

#include <spdlog/spdlog.h>
#include <chrono>

module carton;
import cpx;
import cpx.yy_json;

static void configure_target(Target &target, const Profile &profile, Cache &cache) {
    for (auto *t : target.dependencies) {
        configure_target(*t, profile, cache);
    }
    target.configure(profile, cache);
}

int Carton::execute(Cli &cli) {
    const bool run   = cli.run.has_value();
    const bool build = cli.build.has_value();

    const bool release = run ? cli.run->release : build ? cli.build->release : cli.release;
    const bool static_ = run ? cli.run->static_ : build ? cli.build->static_ : cli.static_;

    const auto &profile             = release ? this->profiles.release : this->profiles.dev;
    const auto &features            = build ? cli.build->features : cli.features;
    const bool  no_default_features = build ? cli.build->no_default_features : cli.no_default_features;

    const auto pretty_two_spaces = cpx::yy_json::write_flag::pretty_two_spaces;
    const auto sanitizer_flag    = profile.sanitize.empty() ? "" : f("-fsanitize={}", fmt::join(profile.sanitize, ","));

    cache->common_flags =
        f( //
            "{} -O{} {} {} {} {}",
            profile.debug ? "-g" : "-DNDEBUG",
            profile.opt_level,
            profile.lto ? "-flto" : "",
            sanitizer_flag,
            profile.arch.empty() ? "" : "-march=" + profile.arch,
            fmt::join(profile.flags, " ")
        );
    cache->common_link_flags =
        f( //
            "{} {} {} {}",
            profile.debug ? "-g"
            : static_     ? "-static"
                          : "",
            profile.lto ? "-flto" : "",
            sanitizer_flag,
            fmt::join(profile.link_flags, " ")
        );
    cache->cppm_standard = std::max(20, package.edition);

    std::vector<Target *> targets;

    try {
        auto ts = configure_package(profile, fs::current_path().string(), features, !no_default_features);
        push_unique(targets, ts);
    } catch (std::exception &e) {
        spdlog::error("Failed to configure: {}", e.what());
        return 1;
    }

    const auto start = std::chrono::steady_clock::now();

    try {
        auto ts = configure_bins(profile);
        push_unique(targets, ts);
    } catch (std::exception &e) {
        spdlog::error("Failed to configure binaries: {}", e.what());
        return 1;
    }

    for (auto *t : targets) {
        configure_target(*t, profile, *cache);
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
    if (!profile.sanitize.empty())
        profile_info += f(" + sanitize({})", fmt::join(profile.sanitize, ","));

    std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    print_status("Finished", f("`{}` profile [{}] target(s) in {:.2f}", profile.name, profile_info, elapsed.count()));

    if (run)
        try {
            return this->run(*cli.run);
        } catch (std::exception &e) {
            spdlog::error("Failed to run: {}", e.what());
            return 1;
        }

    // TODO: manifest
    // TODO: clean
    // TODO: tidy

    return 0;
}
