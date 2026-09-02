module;

#include <string>
#include <vector>
#include <reproc++/run.hpp>
#include <spdlog/spdlog.h>

module carton;

void CompileCommand::compile() const {
    const fs::path output = this->output;
    const fs::path outdir = output.parent_path();

    const std::string full_cmd = fmt::format(
        "mkdir -p \"{0}\" && "
        "cd \"{1}\" && {2} -fdiagnostics-color=always",
        outdir.string(),
        directory,
        command
    );

    spdlog::info("compiling: cmd={:?}", full_cmd);

    reproc::options opt;
    opt.redirect.out.type = reproc::redirect::pipe;
    opt.redirect.err.type = reproc::redirect::pipe;

    std::string errmsg;
    auto [status, ec] =
        reproc::run(std::vector<std::string_view>{"sh", "-c", full_cmd}, opt, reproc::sink::null, reproc::sink::string(errmsg));

    if (!errmsg.empty())
        fmt::println(stderr, "\n{}", errmsg);
    if (status != 0 || ec)
        throw ferr("Failed to compile {}: command={:?}", this->file, full_cmd);
}
