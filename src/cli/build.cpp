module;

#include <spdlog/spdlog.h>
#include <reproc++/run.hpp>

module carton;

static void do_link(const Dependency &d) {
    reproc::options opt;
    opt.redirect.out.type = reproc::redirect::pipe;
    opt.redirect.err.type = reproc::redirect::pipe;
    opt.working_directory = d.ar_command.directory.c_str();

    std::string errmsg;
    auto [status, ec] = reproc::run(
        std::vector<std::string_view>{"sh", "-c", d.ar_command.command}, opt, reproc::sink::null, reproc::sink::string(errmsg)
    );

    if (!errmsg.empty())
        fmt::println(stderr, "\n{}", errmsg);
    if (status != 0 || ec)
        throw ferr("Failed to link: name={} command=`{}`", d.name, d.ar_command.command);
}

void Carton::prebuild(Library &target, const Profile &profile, std::vector<CompileCommand> &ccs) {
    if (target.prebuilt)
        return;

    const int cppm_standard = std::max(20, package.edition);

    if (!target.pre.empty()) {
        spdlog::info("running pre command for package={:?} dep={:?} pre={:?}", package.name, target.name, target.pre);

        reproc::options opt;
        opt.redirect.out.type = reproc::redirect::pipe;
        opt.redirect.err.type = reproc::redirect::pipe;
        opt.working_directory = target.working_dir.c_str();

        std::string errmsg;
        auto [status, ec] = reproc::
            run( //
                std::vector<std::string_view>{"sh", "-c", target.pre},
                opt,
                reproc::sink::null,
                reproc::sink::string(errmsg)
            );

        if (status != 0 || ec) {
            fmt::println(stderr, "{}", errmsg);
            throw ferr("pre command failed for {:?}: {}", target.title, target.pre);
        }
    }

    const auto flags_ =
        f( //
            "{} -O{} {} {} {} "
            "-fmacro-prefix-map=\"{}\"=\"{}\" {}",
            profile.debug ? "-g" : "-DNDEBUG",
            profile.opt_level,
            profile.lto ? "-flto" : "",
            profile.asan ? "-fsanitize=address,undefined" : "",
            profile.arch.empty() ? "" : "-march=" + profile.arch,
            target.working_dir,
            target.name,
            fmt::join(profile.flags, " ")
        );

    const auto CXX = f("{} {}", profile.cxx, flags_);
    const auto C   = f("{} {}", profile.c, flags_);

    const fs::path cache_dir = cache->directory;
    const fs::path build_dir = cache_dir / "build" / profile.name / target.build_name;

    fs::create_directories(build_dir);

    std::vector<std::string> module_ccs;
    module_ccs.reserve(target.mod.size());

    for (const auto &mod : target.mod) {
        auto cc =
            f( //
                "{} {} -std=c++{} -x c++-module {} -c '{}'",
                profile._module_compiler,
                fmt::join(profile.flags, " "),
                cppm_standard,
                fmt::join(target.flags, " "),
                mod
            );
        module_ccs.emplace_back(std::move(cc));
    }

    auto mod_names = sort_modules_p1689(working_dir, target.mod, module_ccs, cache->mods);

    for (size_t i = 0; i < target.mod.size(); ++i) {
        const std::string &mod_name = mod_names[i];
        const fs::path     mod_path = target.mod[i];

        CompileCommand ccm;
        ccm.directory = build_dir.string();
        ccm.file      = (working_dir / mod_path).string();
        ccm.output    = mod_path.string() + ".o";
        ccm.depfile   = mod_path.string() + ".d";
        ccm.modnames  = target.mod_names;

        auto pcm = f("{}-{}.bmi", cppm_standard, mod_name);
        std::replace(pcm.begin(), pcm.end(), ':', '-');

        std::vector<std::string>        pcm_flags;
        std::unordered_set<std::string> visited;
        collect_modules(mod_name, mods, mod_paths, mod_objs, visited, pcm_flags, ccm.modnames, nullptr);
        push_unique(d.mod_flags, pcm_flags);

        ccm.command =
            f( //
                "{} -std=c++{} -x c++-module {} {} -fmodule-output='{}' -o '{}' -c '{}' -MMD -MP -MF '{}'",
                CXX,
                cppm_standard,
                fmt::join(flags, " "),
                fmt::join(d.mod_flags, " "),
                pcm,
                ccm.output,
                ccm.file,
                ccm.depfile
            );

        mod_paths[mod_name] = (build_dir / pcm).string();
        mod_objs[mod_name]  = (build_dir / ccm.output).string();
        ccms.push_back(ccm);
        objs.push_back(mod_objs.at(mod_name));
        push_unique(d.mod_flags, f("-fmodule-file={}='{}'", mod_name, mod_paths.at(mod_name)));
    }
}

void Carton::prebuild(const Profile &profile, const std::vector<Library *> &libs, std::vector<CompileCommand> &ccs) {
    for (auto *lib : libs) {
        prebuild(profile, lib->dependencies, ccs);
        prebuild(*lib, profile, ccs);
    }
}

void Carton::build(const Profile &profile, std::vector<CompileCommand> &ccs, bool do_build) {
    collect_meta(profile, lib);

    const auto start = std::chrono::system_clock::now();

    bool relink = false;
    auto hash   = std::unordered_map<std::string, std::string>();
    for (const auto &m : cache->dependencies) {
        const auto name = m->display_name();
        ccs.insert(ccs.end(), m->precompile_commands.begin(), m->precompile_commands.end());
        ccs.insert(ccs.end(), m->compile_commands.begin(), m->compile_commands.end());
        relink |= CompileCommand::compile_multi(name, m->precompile_commands, cache->mod_paths, hash, true);
    }

    const auto name = lib.display_name();
    ccs.insert(ccs.end(), lib.precompile_commands.begin(), lib.precompile_commands.end());
    ccs.insert(ccs.end(), lib.compile_commands.begin(), lib.compile_commands.end());
    relink |= CompileCommand::compile_multi(name, lib.precompile_commands, cache->mod_paths, hash, true);

    if (!bin.name.empty()) {
        ccs.insert(ccs.end(), bin.compile_commands.begin(), bin.compile_commands.end());
    }

    if (!do_build)
        return;

    for (const auto &m : cache->dependencies) {
        const auto name      = m->display_name();
        auto       do_relink = CompileCommand::compile_multi(name, m->compile_commands, cache->mod_paths, hash);
        if (do_relink ||
            (!m->ar_command.output.empty() && !fs::exists(fs::path(m->ar_command.directory) / m->ar_command.output))) {
            do_link(*m);
        }
        relink |= do_relink;
    }

    bool do_relink = CompileCommand::compile_multi(name, lib.compile_commands, cache->mod_paths, hash);
    if (do_relink ||
        (!lib.ar_command.output.empty() && !fs::exists(fs::path(lib.ar_command.directory) / lib.ar_command.output))) {
        do_link(lib);
    }
    relink |= do_relink;

    if (!bin.empty()) {
        do_relink = CompileCommand::compile_multi(bin.name, bin.compile_commands, cache->mod_paths, hash);
        if (do_relink ||
            (!bin.ar_command.output.empty() && !fs::exists(fs::path(bin.ar_command.directory) / bin.ar_command.output))) {
            do_link(bin);
        }
        relink |= do_relink;

        const auto output = fs::path(lib.build_dir) / lib.name;
        if (relink || !fs::exists(output)) {
            const auto LINK =
                f( //
                    "{} {} {} {}",
                    profile.cxx,
                    profile.lto ? "-flto" : "",
                    profile.asan ? "-fsanitize=address,undefined" : "",
                    fmt::join(profile.link_flags, " ")
                );

            fs::create_directories(output.parent_path());

            auto link_cmd = f("{} -o '{}' {}", LINK, output.string(), fmt::join(bin.link_flags, " "));
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
                throw ferr("Failed to link: name={} command=`{}`", bin.name, link_cmd);
        }

        std::string profile_info = profile.opt_level == 0 ? "unoptimized" : "optimized";
        if (profile.debug)
            profile_info += " + debuginfo";
        if (profile.asan)
            profile_info += " + asan";

        std::chrono::duration<double> elapsed = std::chrono::system_clock::now() - start;
        print_status("Finished", f("`{}` profile [{}] target(s) in {:.2f}", profile.name, profile_info, elapsed.count()));
    }
}
