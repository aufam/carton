module;

#include <spdlog/spdlog.h>
#include <reproc++/run.hpp>
#include <algorithm>
#include <map>
#include <unordered_set>

module carton;
import std.fs;
import fmt;

static void collect_modules(
    const std::string                                     &mod,
    const std::map<std::string, std::vector<std::string>> &mods,
    const std::map<std::string, std::string>              &mod_paths,
    const std::map<std::string, std::string>              &mod_objs,
    std::unordered_set<std::string>                       &visited,
    std::vector<std::string>                              &pcm_flags,
    std::vector<std::string>                              &mod_names,
    std::vector<std::string>                              *obj_flags
) {
    if (!visited.insert(mod).second)
        return; // already processed

    auto it_mod = mods.find(mod);
    if (it_mod == mods.end())
        throw ferr("unknown module `{}`", mod);

    for (const auto &dep : it_mod->second) {
        auto it_path = mod_paths.find(dep);
        if (it_path == mod_paths.end()) {
            throw ferr("unknown module `{}` required by module `{}`", dep, mod);
        }
        if (obj_flags) {
            push_unique(*obj_flags, mod_objs.at(dep));
        }

        push_unique(pcm_flags, f("-fmodule-file={}='{}'", dep, it_path->second));
        push_unique(mod_names, dep);
        collect_modules(dep, mods, mod_paths, mod_objs, visited, pcm_flags, mod_names, obj_flags);
    }
}

void Carton::collect_meta(const Profile &profile, Dependency &d, bool is_bin) {
    if (d.name.empty())
        throw ferr("assertion error: name empty for `{}`", d.path);

    std::sort(d.features.begin(), d.features.end());

    std::string feature_name = f("{}", fmt::join(d.features, "-"));

    if (!d.default_features.value_or(true))
        feature_name = feature_name + //
                       (              //
                           feature_name.find("nodefault") != std::string::npos ? ""
                           : feature_name.empty()                              ? "nodefault"
                                                                               : "-nodefault"
                       );

    if (feature_name.empty())
        feature_name = "-";

    const int cppm_standard = std::max(20, pparent ? pparent->package.edition : package.edition);

    fs::path working_dir = fs::path(d.path) / d.subdir;
    if (working_dir.empty())
        throw ferr("path and subdir is empty for dep={}", d.name);

    d.working_dir = working_dir.string();

    if (!d.pre.empty()) {
        spdlog::info("running pre command for package={:?} dep={:?} pre={:?}", package.name, d.name, d.pre);

        reproc::options opt;
        opt.redirect.out.type = reproc::redirect::pipe;
        opt.redirect.err.type = reproc::redirect::pipe;
        opt.working_directory = d.working_dir.c_str();

        std::string errmsg;
        auto [status, ec] = reproc::
            run( //
                std::vector<std::string_view>{"sh", "-c", d.pre},
                opt,
                reproc::sink::null,
                reproc::sink::string(errmsg)
            );

        if (status != 0 || ec) {
            fmt::println(stderr, "{}", errmsg);
            throw ferr("pre command failed for dep={}: {}", d.name, d.pre);
        }
    }

    const auto flags_ =
        f( //
            "{} -O{} {} {} "
            "-fmacro-prefix-map=\"{}\"=\"{}\" -march=native {}",
            profile.debug ? "-g" : "-DNDEBUG",
            profile.opt_level,
            profile.lto ? "-flto" : "",
            profile.asan ? "-fsanitize=address,undefined" : "",
            working_dir.string(),
            d.name,
            fmt::join(profile.flags, " ")
        );

    const auto CXX = f("{} {}", profile.cxx, flags_);
    const auto C   = f("{} {}", profile.c, flags_);

    const fs::path cache     = cli->cache;
    const fs::path build_dir = //
        cache / "build" / profile.name /
        ( //
            d.name + "-" +
            ( //
                d.branch.empty() && d.tag.empty() ? d.version
                : d.branch.empty()                ? d.tag
                                                  : d.branch
            )
        ) /
        feature_name;

    fs::create_directories(build_dir);
    d.build_dir = build_dir.string();

    std::vector<std::string> flags;
    for (auto &str : d.flags) {
        if (str.starts_with("public:")) {
            auto f = str.substr(std::string("public:").size());
            push_unique(flags, f);
            push_unique(d.public_flags, f);
        } else {
            push_unique(flags, str);
        }
    }
    for (auto &str : d.inc) {
        if (str.starts_with("public:")) {
            auto inc = "-I" + (working_dir / str.substr(std::string("public:").size())).string();
            push_unique(flags, inc);
            push_unique(d.public_flags, inc);
        } else {
            push_unique(flags, "-I" + (working_dir / str).string());
        }
    }

    const auto check_os_continue = [](std::string &str) {
        if (str.find(':') != std::string::npos) {
            std::string_view os =
#if defined(_WIN32)
                "win:";
#elif defined(__APPLE__)
                "osx:";
#elif defined(__linux__)
                "linux:";
#else
#    error "unknown OS"
#endif
            if (str.starts_with(os)) {
                str = str.substr(os.size());
            } else {
                return true;
            }
        }
        return false;
    };

    for (auto &str : d.lib) {
        if (check_os_continue(str))
            continue;
        // TODO
        if (auto path = fs::path(str); path.is_absolute()) {
            auto lib = path.string();
            push_unique(d.link_flags, lib);
        } else {
            auto lib = (working_dir / path).string();
            push_unique(d.link_flags, lib);
        }
    }
    for (auto &str : d.link_flags) {
        if (check_os_continue(str))
            continue;
        push_unique(d.link_flags, str);
    }

    if (!profile._module_support)
        d.mod.clear();

    std::vector<std::string> objs;

    auto &ccs  = d.compile_commands;
    auto &ccms = d.precompile_commands;

    auto &mods      = this->cache->mods;
    auto &mod_paths = this->cache->mod_paths;
    auto &mod_objs  = this->cache->mod_objs;
    try {
        auto modules = expand_path(d.working_dir, d.mod);

        std::vector<std::string> module_ccs;
        module_ccs.reserve(modules.size());

        for (const auto &mod : modules) {
            auto cc =
                f( //
                    "{} {} -std=c++{} -x c++-module {} -c '{}'",
                    profile._module_compiler,
                    fmt::join(profile.flags, " "),
                    cppm_standard,
                    fmt::join(flags, " "),
                    mod
                );
            module_ccs.emplace_back(std::move(cc));
        }

        auto mod_names = sort_modules_p1689(working_dir, modules, module_ccs, mods);

        // auto modules   = expand_path(working_dir.string(), d.mod);
        // auto mod_names = sort_modules(working_dir, modules, mods);

        for (size_t i = 0; i < modules.size(); ++i) {
            const std::string &mod_name = mod_names[i];
            const fs::path     mod_path = modules[i];

            CompileCommand ccm;
            ccm.directory = d.build_dir;
            ccm.file      = (working_dir / mod_path).string();
            ccm.output    = mod_path.string() + ".o";
            ccm.depfile   = mod_path.string() + ".d";
            ccm.modnames  = d.mod_names;

            auto pcm = f("{}-{}.pcm", cppm_standard, mod_name);
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

        for (const fs::path entry : expand_path(working_dir.string(), d.src)) {
            CompileCommand cc;
            cc.directory = d.build_dir;
            cc.output    = entry.string() + ".o";
            cc.depfile   = entry.string() + ".d";
            cc.file      = (working_dir / entry).string();

            const auto ext = entry.extension();

            if (!is_bin && entry.filename().stem().string() == "main")
                continue;

            if (ext == ".cpp" || ext == ".cxx" || ext == ".cc" || ext == ".mm") {
                cc.modnames = d.mod_names;
                push_unique(cc.modnames, mod_names);

                cc.command =
                    f( //
                        "{} -std=c++{} {} {} -o '{}' -c '{}' -MMD -MP -MF '{}'",
                        CXX,
                        d.cpp_standard,
                        fmt::join(flags, " "),
                        fmt::join(d.mod_flags, " "),
                        cc.output,
                        cc.file,
                        cc.depfile
                    );
            } else if (ext == ".c" || ext == ".s" || ext == ".asm" || ext == ".S" || ext == ".m") {
                cc.command =
                    f( //
                        "{} {} -o '{}' -c '{}' -MMD -MP -MF '{}'",
                        C,
                        fmt::join(flags, " "),
                        cc.output,
                        cc.file,
                        cc.depfile
                    );
            }

            if (!cc.command.empty()) {
                objs.push_back((build_dir / cc.output).string());
                ccs.push_back(cc);
            }
        }

        if (!objs.empty()) {
            auto &cc     = d.ar_command;
            cc.directory = d.build_dir;
            if (modules.empty())
                cc.output = "lib" + d.name + ".a";
            else
                cc.output = "lib" + d.name + std::to_string(cppm_standard) + ".a";

            cc.file    = "__dummy__.c";
            cc.command = f("ar rcs '{}' '{}'", cc.output, fmt::join(objs, "' '"));

            push_unique(d.link_flags, (build_dir / cc.output).string());
        }
    } catch (std::exception &e) {
        throw ferr("Cannot resolve dep={:?}, src={}, mod={}: {}", d.name, d.src, d.mod, e.what());
    }
}
