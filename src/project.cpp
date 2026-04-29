#include "main.h"
#include <algorithm>
#include <fmt/ranges.h>
#include <fmt/color.h>
#include <fmt/chrono.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <cpx/toml/toruniina_toml.h>

#define f(...)    fmt::format(__VA_ARGS__)
#define ferr(...) std::runtime_error(fmt::format(__VA_ARGS__))
namespace fs = std::filesystem;

static std::string
find_extra_features(Project &p, const std::string &feat, std::vector<std::string> &required_features, bool dep = false) {
    // TODO: support "feat1/feat2" syntax for features of dependencies
    if (feat.rfind("dep:", 0) == 0)
        return find_extra_features(p, feat.substr(4), required_features, true);

    if (auto it = p.features().find(feat); dep || it == p.features().end()) {
        auto d = p.dependencies().find(feat);
        if (d == p.dependencies().end())
            return f("Error building {:?}: feature `{}` is not defined in the package or dependencies", p.package().name(), feat);
        else if (convert_dep(d->second).optional())
            push_unique(required_features, feat);
    } else {
        for (auto &dep : it->second)
            find_extra_features(p, dep, required_features, dep == feat);
    }
    return "";
}

void Project::configure(const Profile &profile, const std::vector<std::string> &features) {
    if (package().name().empty())
        throw ferr("Error building {:?}: name is required", package().name());

    // if (package().version().empty())
    //     throw ferr("Error building {:?}: version is required", package().name());

    switch (package().edition()) {
    case 11:
    case 14:
    case 17:
    case 20:
    case 23:
    case 26:
        break;
    default:
        throw ferr("Error building {:?}: unsupported edition: {}", package().name(), package().edition());
    }

    apply_package_placeholders();

    if (!lib().version().empty())
        throw ferr("Error building {:?}: lib version is already set to {}", package().name(), lib().version());

    if (lib().name().empty())
        lib().name() = package().name();
    resolve_remote_dep(profile, package().name(), lib());

    std::vector<std::string> extra_features;
    if (!no_default_features())
        std::ignore = find_extra_features(*this, "default", extra_features);
    else
        std::ignore = find_extra_features(*this, "nodefault", extra_features);
    for (auto &feat : features) {
        auto err = find_extra_features(*this, feat, extra_features);
        if (!err.empty())
            throw std::runtime_error(err);
    }

    spdlog::info("resolving: dep={:?} extra_features={}", package().name(), extra_features);
    for (auto &[name, dep] : dependencies()) {
        auto &d = convert_dep(dep);
        if (d.name().empty())
            d.name() = name;
        if (no_default_features() && name == "default")
            continue;
        if (!no_default_features() && name == "nodefault")
            continue;
        if (d.optional() && std::find(extra_features.begin(), extra_features.end(), name) == extra_features.end())
            continue;
        if (d.empty()) {
            lib() += d;
            continue;
        }

        if (d.version() == "?") {
            if (pparent == nullptr)
                throw ferr("version {:?} must be in the parent project", d.version());

            auto it = pparent->dependencies().find(d.name());
            if (it == pparent->dependencies().end())
                throw ferr("package {:?} must be in the parent project", d.name());

            d = convert_dep(it->second);
        }

        const Meta *existing = nullptr;
        for (const auto &m : (pmeta ? *pmeta : meta))
            if (m.lib.name() == d.name()) {
                if (m.lib.version() != d.version())
                    throw ferr("version already exist");
                spdlog::info("found {} v{}", m.lib.name(), m.lib.version());
                existing = &m;
                break;
            }

        if (existing) {
            push_unique(lib().flags(), existing->flags);
            push_unique(lib().link_flags(), existing->link_flags);
            push_unique(
                lib().flags(),
                package().edition() >= 20 && profile._module_support ? existing->module_flags : existing->include_flags
            );
            continue;
        }

        try {
            resolve_remote_dep(profile, name, d);
        } catch (const std::exception &e) {
            throw ferr("Error resolving {:?} required by {:?}: {}", name, package().name(), e.what());
        }

        try {
            auto m = collect_meta(profile, d);
            push_unique(lib().flags(), m.flags);
            push_unique(lib().link_flags(), m.link_flags);
            push_unique(lib().flags(), package().edition() >= 20 && profile._module_support ? m.module_flags : m.include_flags);
            (pmeta ? *pmeta : meta).push_back(std::move(m));
        } catch (const std::exception &e) {
            throw ferr("Error collecting meta of {:?} required by {:?}: {}", name, package().name(), e.what());
        }
    }

    lib().version()          = package().version();
    lib().cpp_standard       = package().edition();
    lib().features()         = std::move(extra_features);
    lib().default_features() = !no_default_features();
}

void Project::resolve_remote_dep(const Profile &profile, const std::string &name, Dependency &d) {
    if (d.empty())
        throw ferr("assertion failed: {:?} cannot be empty", d.name());

    auto configure_subpackage = [&](Project &p) -> Dependency & {
        if (p.package().edition() > package().edition())
            throw ferr(
                "Error building dependency package={0:?}: {0:?} required std=c++{1} but {2:?} only supports std=c++{3}",
                p.package().name(),
                p.package().edition(),
                package().name(),
                package().edition()
            );

        p.pparent               = this;
        p.ppackages             = &packages();
        p.pmeta                 = &meta;
        p.cache()               = cache();
        p.no_default_features() = !d.default_features().value_or(true);
        p.profiles()            = profiles();
        p.vars()                = vars();
        try {
            p.configure(profile, d.features());
        } catch (const std::exception &e) {
            throw ferr("Error building dependency package={} `{}`: {}", p.package().name(), name, e.what());
        }
        return p.lib();
    };

    if (!d.path().empty()) {
        spdlog::info("resolving dep={:?} path={:?}", name, d.path());
        d.path() = resolve_path(cache(), d.path());
    } else if (!d.url().empty()) {
        spdlog::info("resolving dep={:?} url={:?}", name, d.url());
        d.path() = resolve_path(cache(), d.url());
    } else if (!d.git().empty()) {
        auto &tag = d.tag().empty() ? d.branch() : d.tag();
        spdlog::info("resolving dep={:?} git={:?} tag={:?}", name, d.git(), tag);
        d.path() = git_clone(cache(), d.git(), tag);
    } else if (d.version().empty()) {
        throw std::runtime_error("path|git|version is not defined");
    } else {
        spdlog::info("resolving dep={:?} version={:?}", name, d.version());
        auto &packages = ppackages ? *ppackages : this->packages();

        auto &name_ = d.name().empty() ? name : d.name();
        auto  it    = packages.find(name_);
        if (it == packages.end())
            throw std::runtime_error("Cannot find `" + name_ + "` in the package registry");

        auto p = it->second;
        if (p.lib().empty())
            throw std::runtime_error("The package `" + name_ + "` does not have a library target");

        p.package().version() = d.version();
        auto &lib             = configure_subpackage(p);
        d                     = std::move(lib);
        resolve_remote_dep(profile, name, d);
        return;
    }

    if (fs::path path = d.path(); path.is_relative())
        d.path() = (fs::path(lib().path()) / lib().subdir() / path).lexically_normal().string();

    fs::path working_dir = fs::path(d.path()) / d.subdir();
    if (auto sub = working_dir / "carton.toml"; &lib() != &d && fs::exists(sub)) {
        constexpr auto toml_version = cpx::toml::toruniina_toml::spec::v(1, 1, 0);

        auto p = cpx::toml::toruniina_toml::parse_from_file<Project>(sub.string(), toml_version);
        if (fs::path(p.lib().path()).is_absolute() && fs::path(p.lib().subdir()).is_absolute())
            throw ferr("Path must be relative");
        p.lib().path() = (working_dir / p.lib().path()).string();

        auto &lib = configure_subpackage(p);
        d         = std::move(lib);
    } else {
        std::string name = d.name();
        name.replace(name.begin(), name.end(), '-', '.');
        if (d.mod().empty() && fs::exists(working_dir / "src" / "lib.cppm"))
            d.mod() = {d.name() + ":src/lib.cppm"};
        if (d.src().empty() && fs::is_directory(working_dir / "src"))
            d.src() = {"src/*"};
        if (d.inc().empty() && fs::is_directory(working_dir / "include"))
            d.inc() = {"public:include"};
    }
}

Project::Meta Project::collect_meta(const Profile &profile, Dependency &d) {
    [[maybe_unused]]
    const auto lib = 1;

    const auto cpp_standard = d.cpp_standard.value_or(package().edition());

    std::sort(d.features().begin(), d.features().end());
    std::string feature_name = f("{}", fmt::join(d.features(), "-"));
    if (!d.default_features().value_or(true))
        feature_name = feature_name + (feature_name.find("nodefault") != std::string::npos ? ""
                                       : feature_name.empty()                              ? "nodefault"
                                                                                           : "-nodefault");
    if (feature_name.empty())
        feature_name = "-";

    fs::path working_dir = fs::path(d.path()) / d.subdir();
    if (working_dir.empty())
        throw ferr("path and subdir is empty for dep={}", d.name());

    if (!d.pre().empty()) {
        spdlog::info("running pre command for package={:?} dep={:?} pre={:?}", package().name(), d.name(), d.pre());
        std::string cmd = f("cd '{}' && ({}) > /dev/null 2>&1", working_dir.string(), d.pre());
        if (std::system(cmd.c_str()) != 0)
            throw ferr("pre command failed for dep={}: {}", d.name(), d.pre());
    }

    const auto flags_ =
        f("{} -O{} {} {}",
          profile.debug() ? "-g" : "",
          profile.opt_level(),
          profile.lto() ? "-flto" : "",
          fmt::join(profile.flags(), " "));
    const auto CXX       = f("{} {}", profile.cxx(), flags_);
    const auto C         = f("{} {}", profile.c(), flags_);
    fs::path   cache     = this->cache();
    fs::path   build_dir = cache / "build" / profile.id() /
                         (d.name() + "-" +
                          (d.branch().empty() && d.tag().empty() ? d.version()
                           : d.branch().empty()                  ? d.tag()
                                                                 : d.branch())) /
                         feature_name;

    std::vector<std::string> flags;
    std::vector<std::string> export_flags;
    std::vector<std::string> export_inc_flags;
    std::vector<std::string> export_module_flags;
    std::vector<std::string> export_link_flags;
    std::string              main_o;
    for (auto &str : d.flags()) {
        if (str.rfind("public:", 0) == 0) {
            auto f = str.substr(std::string("public:").size());
            push_unique(flags, f);
            push_unique(export_flags, f);
        } else {
            push_unique(flags, str);
        }
    }
    for (auto &str : d.inc()) {
        if (str.rfind("public:", 0) == 0) {
            auto inc = "-I" + (working_dir / str.substr(std::string("public:").size())).string();
            push_unique(flags, inc);
            push_unique(export_inc_flags, inc);
        } else {
            push_unique(flags, "-I" + (working_dir / str).string());
        }
    }
    for (auto &str : d.lib()) {
        auto lib = (working_dir / str).string();
        push_unique(export_link_flags, lib);
    }
    for (auto &str : d.link_flags()) {
        push_unique(export_link_flags, str);
    }

    if (!profile._module_support)
        d.mod().clear();

    try {
        std::vector<CompileCommand> ccs;

        const fs::path    pcm_dir  = build_dir / std::to_string(profile._module_cxx_version);
        const std::string pcm_flag = d.mod().empty() ? "" : f("-fprebuilt-module-path='{}'", pcm_dir.string());
        for (const auto &mod : d.mod()) {
            std::string mod_name;
            fs::path    mod_path;
            if (auto pos = mod.find(':'); pos != std::string::npos) {
                mod_name = mod.substr(0, pos);
                mod_path = mod.substr(pos + 1);
            } else {
                mod_path = mod;
                mod_name = mod_path.filename().stem().string();
            }

            CompileCommand ccm;
            ccm.directory() = build_dir.string();
            ccm.file()      = (working_dir / mod_path).string();
            ccm.output()    = f("{}/{}.pcm", profile._module_cxx_version, mod_name);
            ccm.depfile()   = mod_path.string() + ".d";

            ccm.command() =
                f("{} -std=c++{} -x c++-module {} -o '{}' --precompile '{}' {}",
                  CXX,
                  profile._module_cxx_version,
                  fmt::join(flags, " "),
                  ccm.output(),
                  ccm.file(),
                  pcm_flag);

            ccs.push_back(ccm);

            CompileCommand cc;
            cc.directory() = build_dir.string();
            cc.output()    = mod_path.string() + ".o";
            cc.depfile()   = mod_path.string() + ".d";
            cc.file()      = (working_dir / mod_path).string();
            cc.command() =
                f("{} -std=c++{} {} -o '{}' -c '{}' -MMD -MP -MF '{}' {}",
                  CXX,
                  profile._module_cxx_version,
                  fmt::join(flags, " "),
                  cc.output(),
                  cc.file(),
                  cc.depfile(),
                  pcm_flag);

            push_unique(export_link_flags, (build_dir / cc.output()).string());
            ccs.push_back(cc);
        }

        push_unique(export_module_flags, pcm_flag);
        if (cpp_standard >= 20) {
            push_unique(flags, pcm_flag);
        }

        auto expanded = expand_path(working_dir.string(), d.src());
        for (const fs::path entry : expanded) {
            CompileCommand cc;
            cc.directory() = build_dir.string();
            cc.output()    = entry.string() + ".o";
            cc.depfile()   = entry.string() + ".d";
            cc.file()      = (working_dir / entry).string();

            const auto ext = entry.extension();

            if (ext == ".cpp" || ext == ".cxx" || ext == ".cc") {
                cc.command() =
                    f("{} -std=c++{} {} -o '{}' -c '{}' -MMD -MP -MF '{}'",
                      CXX,
                      cpp_standard,
                      fmt::join(flags, " "),
                      cc.output(),
                      cc.file(),
                      cc.depfile());
            } else if (ext == ".c" || ext == ".s" || ext == ".asm" || ext == ".S") {
                cc.command() =
                    f("{} {} -o '{}' -c '{}' -MMD -MP -MF '{}'", C, fmt::join(flags, " "), cc.output(), cc.file(), cc.depfile());
            }

            if (!cc.command().empty()) {
                if (entry.filename().stem().string() == "main") {
                    if (!main_o.empty())
                        throw ferr("multiple main files in `{}`", working_dir.string());
                    main_o = (build_dir / cc.output()).string();
                } else {
                    push_unique(export_link_flags, (build_dir / cc.output()).string());
                }
                ccs.push_back(cc);
            }
        }
        Meta m;
        m.lib              = d;
        m.flags            = std::move(export_flags);
        m.link_flags       = std::move(export_link_flags);
        m.include_flags    = std::move(export_inc_flags);
        m.module_flags     = std::move(export_module_flags);
        m.main_o           = std::move(main_o);
        m.compile_commands = std::move(ccs);
        return m;
    } catch (std::exception &e) {
        throw ferr("Cannot resolve dep={:?}, src={}: {}", d.name(), d.src(), e.what());
    }
}

void Project::build(
    const std::string                           &build_dir,
    const Profile                               &profile,
    bool                                         link,
    const std::vector<std::string>              &link_flags,
    bool                                         do_run,
    const std::vector<std::string>              &run_args,
    const std::chrono::system_clock::time_point &start
) {
    const auto LINK   = f("{} {} {}", profile.cxx(), profile.lto() ? "-flto" : "", fmt::join(profile.link_flags(), " "));
    const auto output = fs::path(build_dir) / lib().name();

    if (link || !fs::exists(output)) {
        fs::create_directories(output.parent_path());

        std::string name = lib().name();
        if (!lib().version().empty()) {
            name += " v" + lib().version();
        } else if (!lib().tag().empty()) {
            name += " #" + lib().tag();
        } else if (!lib().branch().empty()) {
            name += " " + lib().branch();
        } else {
            name += " (" + lib().path() + ")";
        }
        fmt::print(stderr, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::green), "{:>12} ", "Linking");
        fmt::println(stderr, "{}", name);

        auto link_cmd = f("{} -o '{}' {}", LINK, output.string(), fmt::join(link_flags, " "));
        spdlog::debug("linking cmd={}", link_cmd);
        if (std::system(link_cmd.c_str()) != 0)
            throw ferr("linking failed: cmd={}", lib().name(), link_cmd);
    }

    std::string profile_info = profile.opt_level() == 0 ? "unoptimized" : "optimized";
    if (profile.debug())
        profile_info += " + debuginfo";

    std::chrono::duration<double> elapsed = std::chrono::system_clock::now() - start;
    fmt::print(stderr, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::green), "{:>12} ", "Finished");
    fmt::println(stderr, "`{}` profile [{}] target(s) in {:.2f}", profile.id(), profile_info, elapsed.count());

    if (do_run) {
        std::string out = output.string();
        for (size_t pos = 0; (pos = out.find(' ', pos)) != std::string::npos;) {
            out.replace(pos, 1, "\\ ");
            pos += 2;
        }

        std::string exe = f("{} {}", out, fmt::join(run_args, " "));
        if (run_args.empty())
            exe.pop_back();

        fmt::print(stderr, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::green), "{:>12} ", "Running");
        fmt::println(stderr, "`{}`", exe);
        std::system(exe.c_str());
    }
}
