module;

#include <string>
#include <vector>
#include <reproc++/run.hpp>
#include <spdlog/spdlog.h>

module carton;

void CompileCommand::compile() const {
    const fs::path output = this->output;
    const fs::path outdir = output.parent_path();

    fs::create_directories(directory / outdir);

    std::string cmd = command;
    if (!is_ar) {
        cmd += " -fdiagnostics-color=always";
    }

    spdlog::info("compiling: cmd={:?}", cmd);

    reproc::options opt;
    opt.redirect.out.type = reproc::redirect::pipe;
    opt.redirect.err.type = reproc::redirect::pipe;
    opt.working_directory = directory.c_str();

    std::string errmsg;
    auto [status, ec] =
        reproc::run(std::vector<std::string_view>{"sh", "-c", cmd}, opt, reproc::sink::null, reproc::sink::string(errmsg));

    if (!errmsg.empty())
        fmt::println(stderr, "\n{}", errmsg);
    if (status != 0 || ec)
        throw ferr("Failed to compile {}: command={:?}", this->file, cmd);
}
