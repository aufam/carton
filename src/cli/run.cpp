module;

#include <spdlog/spdlog.h>
#include <reproc++/run.hpp>

module carton;

int Carton::run(Cli::Run &r) {
    Target *target = nullptr;

    if (!r.bin.empty()) {
        auto it = targets.find(package.name + ".bin." + r.bin);
        if (it == targets.end())
            throw ferr("binary '{}' not found", r.bin);

        target = it->second.get();
    } else if (!r.example.empty()) {
        auto it = targets.find(package.name + ".example." + r.example);
        if (it == targets.end())
            throw ferr("example '{}' not found", r.example);

        target = it->second.get();
    } else if (!bins.empty()) {
        target = targets.at(package.name + ".bin." + bins.front().name).get();
    }

    if (target == nullptr)
        throw ferr("no binaries specified");

    auto &t = *target;

    if (t.executable.empty())
        throw ferr("target `{}` is not an executable", t.name);

    std::string exe = f("{:?}", t.executable);
    if (!r.args.empty())
        exe += f(" {}", fmt::join(r.args, " "));

    print_status("Running", exe);
    auto [status, ec] = reproc::run(std::vector<std::string>{"sh", "-c", exe});
    if (ec)
        return -1;

    if (status >= 0)
        return status;

    return 256;
}
