module;

#include <cpx/fmt.h>
#include <spdlog/spdlog.h>
#include <reproc++/run.hpp>
#include <filesystem>

module carton;

std::pair<bool, Cache::Meta> Carton::build(const Profile &profile, std::vector<CompileCommand> &ccs, bool do_build) {
    const auto start = std::chrono::system_clock::now();

    bool relink = false;
    auto hash   = std::unordered_map<std::string, std::string>();
    for (const auto &m : cache->meta) {
        const auto name = m.lib.display_name();
        ccs.insert(ccs.end(), m.precompile_commands.begin(), m.precompile_commands.end());
        ccs.insert(ccs.end(), m.compile_commands.begin(), m.compile_commands.end());
        relink |= CompileCommand::compile_multi(name, m.precompile_commands, hash, true);
    }

    auto       m    = collect_meta(profile, lib);
    const auto name = m.lib.display_name();
    ccs.insert(ccs.end(), m.precompile_commands.begin(), m.precompile_commands.end());
    ccs.insert(ccs.end(), m.compile_commands.begin(), m.compile_commands.end());
    relink |= CompileCommand::compile_multi(name, m.precompile_commands, hash, true);

    if (!do_build)
        return std::make_pair(relink, std::move(m));

    for (const auto &m : cache->meta) {
        const auto name = m.lib.display_name();
        relink |= CompileCommand::compile_multi(name, m.compile_commands, hash);
    }

    relink |= CompileCommand::compile_multi(name, m.compile_commands, hash);

    const auto output = fs::path(m.build_dir) / m.lib.name;
    if (!m.main_o.empty() && (relink || !fs::exists(output))) {
        const auto LINK =
            f("{} {} {} {}",
              profile.cxx,
              profile.lto ? "-flto" : "",
              profile.asan ? "-fsanitize=address,undefined" : "",
              fmt::join(profile.link_flags, " "));

        fs::create_directories(output.parent_path());

        auto link_cmd = f("{} -o '{}' {} {}", LINK, output.string(), fmt::join(m.link_flags, " "), m.main_o);
        spdlog::debug("linking cmd={}", link_cmd);
        print_status("Linking", lib.display_name());

        reproc::options opt;
        opt.redirect.out.type = reproc::redirect::pipe;
        opt.redirect.err.type = reproc::redirect::pipe;

        std::string errmsg;
        auto [status, ec] = reproc::run(
            std::vector<std::string_view>{"sh", "-c", link_cmd}, opt, reproc::sink::null, reproc::sink::string(errmsg)
        );

        if (!errmsg.empty())
            fmt::println(stderr, "\n{}", errmsg);
        if (status != 0 || ec)
            throw ferr("Failed to link: name={} command=`{}`", m.lib.name, link_cmd);
    }

    std::string profile_info = profile.opt_level == 0 ? "unoptimized" : "optimized";
    if (profile.debug)
        profile_info += " + debuginfo";
    if (profile.asan)
        profile_info += " + asan";

    std::chrono::duration<double> elapsed = std::chrono::system_clock::now() - start;
    print_status("Finished", f("`{}` profile [{}] target(s) in {:.2f}", profile.id, profile_info, elapsed.count()));

    return std::make_pair(relink, std::move(m));
}
