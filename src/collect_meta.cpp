module;

#include <cpx/fmt.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <filesystem>
#include <map>
#include <unordered_set>

module carton;

namespace fs = std::filesystem;

static void collect_modules(
    const std::string                                     &mod,
    const std::map<std::string, std::vector<std::string>> &mods,
    const std::map<std::string, std::string>              &mod_paths,
    const std::map<std::string, std::string>              &mod_objs,
    std::unordered_set<std::string>                       &visited,
    std::vector<std::string>                              &pcm_flags,
    std::vector<std::string>                              *obj_flags
) {
    if (!visited.insert(mod).second)
        return; // already processed

    auto it_mod = mods.find(mod);
    if (it_mod == mods.end())
        throw std::runtime_error("unknown module `" + mod + "`");

    for (const auto &dep : it_mod->second) {
        auto it_path = mod_paths.find(dep);
        if (it_path == mod_paths.end()) {
            throw std::runtime_error(f("unknown module `{}` required by module `{}`", dep, mod));
        }
        if (obj_flags) {
            push_unique(*obj_flags, mod_objs.at(dep));
        }

        push_unique(pcm_flags, f("-fmodule-file={}='{}'", dep, it_path->second));
        collect_modules(dep, mods, mod_paths, mod_objs, visited, pcm_flags, obj_flags);
    }
}

Cache::Meta Carton::collect_meta(const Profile &profile, Dependency &d) {
    [[maybe_unused]]
    const auto lib = 1;

    const auto cpp_standard = d.cpp_standard.value_or(package.edition);

    std::sort(d.features.begin(), d.features.end());
    std::string feature_name = f("{}", fmt::join(d.features, "-"));
    if (!d.default_features.value_or(true))
        feature_name = feature_name + (feature_name.find("nodefault") != std::string::npos ? ""
                                       : feature_name.empty()                              ? "nodefault"
                                                                                           : "-nodefault");
    if (feature_name.empty())
        feature_name = "-";

    fs::path working_dir = fs::path(d.path) / d.subdir;
    if (working_dir.empty())
        throw ferr("path and subdir is empty for dep={}", d.name);

    if (!d.pre.empty()) {
        spdlog::info("running pre command for package={:?} dep={:?} pre={:?}", package.name, d.name, d.pre);
        std::string cmd = f("cd '{}' && ({}) > /dev/null 2>&1", working_dir.string(), d.pre);
        if (std::system(cmd.c_str()) != 0)
            throw ferr("pre command failed for dep={}: {}", d.name, d.pre);
    }

    const auto flags_ =
        f("{} -O{} {} {} {}",
          profile.debug ? "-g" : "-DNDEBUG",
          profile.opt_level,
          profile.lto ? "-flto" : "",
          profile.asan ? "-fsanitize=address,undefined" : "",
          fmt::join(profile.flags, " "));
    const auto     CXX       = f("{} {}", profile.cxx, flags_);
    const auto     C         = f("{} {}", profile.c, flags_);
    const fs::path cache     = cli->cache;
    const fs::path build_dir = cache / "build" / profile.id /
                               (d.name + "-" +
                                (d.branch.empty() && d.tag.empty() ? d.version
                                 : d.branch.empty()                ? d.tag
                                                                   : d.branch)) /
                               feature_name;

    std::vector<std::string> flags;
    std::vector<std::string> export_flags;
    std::vector<std::string> export_inc_flags;
    std::vector<std::string> export_module_flags;
    std::vector<std::string> export_link_flags;
    std::string              main_o;
    for (auto &str : d.flags) {
        if (str.starts_with("public:")) {
            auto f = str.substr(std::string("public:").size());
            push_unique(flags, f);
            push_unique(export_flags, f);
        } else {
            push_unique(flags, str);
        }
    }
    for (auto &str : d.inc) {
        if (str.starts_with("public:")) {
            auto inc = "-I" + (working_dir / str.substr(std::string("public:").size())).string();
            push_unique(flags, inc);
            push_unique(export_inc_flags, inc);
        } else {
            push_unique(flags, "-I" + (working_dir / str).string());
        }
    }
    for (auto &str : d.lib) {
        auto lib = (working_dir / str).string();
        push_unique(export_link_flags, lib);
    }
    for (auto &str : d.link_flags) {
        push_unique(export_link_flags, str);
    }

    if (!profile._module_support)
        d.mod.clear();

    std::vector<CompileCommand> ccs;
    std::vector<CompileCommand> ccms;

    const fs::path &pcm_dir = build_dir;
    if (!d.mod.empty()) {
        fs::create_directories(pcm_dir);
    }

    auto &mods      = this->cache->mods;
    auto &mod_paths = this->cache->mod_paths;
    auto &mod_objs  = this->cache->mod_objs;
    try {
        auto modules   = expand_path(working_dir.string(), d.mod);
        auto mod_names = sort_modules(working_dir, modules, mods);
        for (size_t i = 0; i < modules.size(); ++i) {
            const std::string &mod_name = mod_names[i];
            const fs::path     mod_path = modules[i];

            CompileCommand ccm;
            ccm.directory = build_dir.string();
            ccm.file      = (working_dir / mod_path).string();
            ccm.output    = mod_path.string() + ".o";
            ccm.depfile   = mod_path.string() + ".d";

            auto pcm = f("{}.pcm", mod_name);
            std::replace(pcm.begin(), pcm.end(), ':', '-');

            std::vector<std::string>        pcm_flags;
            std::unordered_set<std::string> visited;
            collect_modules(mod_name, mods, mod_paths, mod_objs, visited, pcm_flags, nullptr);

            ccm.command =
                f("{} -std=c++{} -x c++-module {} {} -fmodule-output='{}' -o '{}' -c '{}' -MMD -MP -MF '{}'",
                  CXX,
                  std::max(package.edition, 20),
                  fmt::join(flags, " "),
                  fmt::join(pcm_flags, " "),
                  pcm,
                  ccm.output,
                  ccm.file,
                  ccm.depfile);

            mod_paths[mod_name] = (build_dir / pcm).string();
            mod_objs[mod_name]  = (build_dir / ccm.output).string();
            ccms.push_back(ccm);
        }

        for (const fs::path entry : expand_path(working_dir.string(), d.src)) {
            CompileCommand cc;
            cc.directory = build_dir.string();
            cc.output    = entry.string() + ".o";
            cc.depfile   = entry.string() + ".d";
            cc.file      = (working_dir / entry).string();

            const auto ext = entry.extension();

            std::vector<std::string>        pcm_flags;
            std::unordered_set<std::string> visited;
            for (auto &dep : collect_module_deps(working_dir.string(), entry.string())) {
                auto it = mod_paths.find(dep);
                if (it == mod_paths.end()) {
                    throw std::runtime_error(f("unknown module `{}` required by file `{}`", dep, entry.string()));
                }
                push_unique(export_link_flags, mod_objs.at(dep));
                push_unique(pcm_flags, f("-fmodule-file={}='{}'", dep, it->second));
                collect_modules(dep, mods, mod_paths, mod_objs, visited, pcm_flags, &export_link_flags);
            }

            if (ext == ".cpp" || ext == ".cxx" || ext == ".cc") {
                cc.command =
                    f("{} -std=c++{} {} {} -o '{}' -c '{}' -MMD -MP -MF '{}'",
                      CXX,
                      cpp_standard,
                      fmt::join(flags, " "),
                      fmt::join(pcm_flags, " "),
                      cc.output,
                      cc.file,
                      cc.depfile);
            } else if (ext == ".c" || ext == ".s" || ext == ".asm" || ext == ".S") {
                cc.command =
                    f("{} {} -o '{}' -c '{}' -MMD -MP -MF '{}'", C, fmt::join(flags, " "), cc.output, cc.file, cc.depfile);
            }

            if (!cc.command.empty()) {
                if (entry.filename().stem().string() == "main") {
                    if (!main_o.empty())
                        throw ferr("multiple main files in `{}`", working_dir.string());
                    main_o = (build_dir / cc.output).string();
                } else {
                    push_unique(export_link_flags, (build_dir / cc.output).string());
                }
                ccs.push_back(cc);
            }
        }
        Cache::Meta m;
        m.build_dir           = build_dir;
        m.lib                 = d;
        m.flags               = std::move(export_flags);
        m.link_flags          = std::move(export_link_flags);
        m.include_flags       = std::move(export_inc_flags);
        m.module_flags        = std::move(export_module_flags);
        m.main_o              = std::move(main_o);
        m.compile_commands    = std::move(ccs);
        m.precompile_commands = std::move(ccms);
        return m;
    } catch (std::exception &e) {
        throw ferr("Cannot resolve dep={:?}, src={}, mod={}: {}", d.name, d.src, d.mod, e.what());
    }
}
