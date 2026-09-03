module;

#include <reproc++/run.hpp>
#include <spdlog/spdlog.h>

module carton;

void expand_path(const std::string &working_dir, std::vector<std::string> &sources, bool check_exist) {
    if (sources.empty())
        return;

    // wrap everything except * with ""
    for (auto &src : sources) {
        src = '"' + src + '"';
        for (auto pos = src.find('*'); pos != std::string::npos; pos = src.find('*', pos + 3)) {
            src.replace(pos, 1, "\"*\"");
        }
    }

    std::string cmd = fmt::format("printf '%s\\n' {}", fmt::join(sources, " "));

    reproc::options options;
    options.working_directory = working_dir.c_str();
    options.redirect.out.type = reproc::redirect::pipe;
    options.redirect.err.type = reproc::redirect::discard;

    spdlog::debug("expanding: cmd={}", cmd);
    reproc::process process;
    std::error_code ec = process.start(std::vector<std::string_view>{"sh", "-c", cmd}, options);
    if (ec)
        throw ferr("Failed to start expanding: {}", ec.message());

    std::string output;
    ec = reproc::drain(process, reproc::sink::string(output), reproc::sink::null);
    if (ec)
        throw ferr("Failed to read expand result: {}", ec.message());

    auto [status, wait_ec] = process.wait(reproc::infinite);
    if (wait_ec)
        throw ferr("Failed waiting for expand process: {}", wait_ec.message());

    if (status != 0)
        throw ferr("Expand command failed with exit code {}", status);

    std::vector<std::string> res;
    std::istringstream       iss(output);
    std::string              line;
    while (std::getline(iss, line)) {
        if (line.empty())
            continue;

        fs::path entry = line;

        if (check_exist && !fs::exists(fs::path(working_dir) / entry)) {
            throw ferr("Expand failed: {:?} does not exist in {:?}", entry.string(), working_dir);
        }

        res.push_back(entry.string());
    }

    sources = res;
}
