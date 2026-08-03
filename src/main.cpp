#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

import carton;
import cpx.cli11;

int main(int argc, char **argv) {
    // spdlog
    auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("carton", std::move(sink)));
    spdlog::set_pattern("%^%l%$: %v");

    // cli
    Cli cli   = {};
    cli.cache = std::getenv("HOME") + std::string("/.carton");
    cpx::cli11::parse("C++ package manager and build system", argc, argv, cli);
    spdlog::set_level(cli.log_level);

    // update registry
    if (auto &args = cli.update; args.has_value())
        return Carton::Update();

    // init a project
    if (auto &args = cli.init; args.has_value())
        return Carton::Init(*args);

    // context
    auto ctx = Carton::New(cli.cache);

    // add dependency
    if (auto &args = cli.add; args.has_value())
        return ctx.add(*args);

    // execute
    return ctx.execute(cli);
}
