module;

#include <spdlog/spdlog.h>

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

    this->no_default_features = run        ? cli.run->no_default_features
                                : build    ? cli.build->no_default_features
                                : manifest ? cli.manifest->no_default_features
                                           : cli.no_default_features;

    const auto pretty_two_spaces = cpx::yy_json::write_flag::pretty_two_spaces;

    try {
        this->configure(profile, features);
    } catch (std::exception &e) {
        spdlog::error("Failed to configure: {}", e.what());
        return 1;
    }

    auto ccs = std::vector<CompileCommand>();
    auto _   = cpx::defer([&ccs]() {
        auto of = std::ofstream("./compile_commands.json");
        of << cpx::yy_json::dump(ccs, pretty_two_spaces);
    });

    try {
        this->build(profile, ccs, build || run);
        if (run)
            return this->run(cli.run->args);
    } catch (std::exception &e) {
        spdlog::error("Failed to build: {}", e.what());
        return 1;
    }

    if (manifest) {
        this->registry.clear(); // TODO
        fmt::println("{}", cpx::yy_json::dump(*this, pretty_two_spaces));
    }

    return 0;
}
