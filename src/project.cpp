#include "main.h"
#include <algorithm>
#include <fmt/ranges.h>
#include <fmt/color.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <cpx/toml/toruniina_toml.h>

#define f(...)    fmt::format(__VA_ARGS__)
#define ferr(...) std::runtime_error(fmt::format(__VA_ARGS__))
namespace fs = std::filesystem;

static void find_requested_features(
    const Project &p, const std::string &feat, std::vector<std::string> &required_features, bool dep = false
) {
    // TODO: support "feat1/feat2" syntax for features of dependencies
    if (feat.rfind("dep:", 0) == 0)
        return find_requested_features(p, feat.substr(4), required_features, true);

    if (auto it = p.features().find(feat); dep || it == p.features().end()) {
        auto d = p.dependencies().find(feat);
        if (d == p.dependencies().end())
            throw ferr(
                "Error building {:?}: feature `{}` is not defined in the package or dependencies", p.package().name(), feat
            );
        else
            push_unique(required_features, feat);
    } else {
        for (auto &dep : it->second)
            find_requested_features(p, dep, required_features, dep == feat);
    }
}

void Project::build(const std::vector<std::string> &features, bool subpackage) {
    if (package().name().empty())
        throw ferr("Error building {:?}: name is required", package().name());

    if (package().version().empty())
        throw ferr("Error building {:?}: version is required", package().name());

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

    if (!lib().empty())
        resolve_remote_dep(package().name(), lib());
    else if (!subpackage) {
        fs::path working_dir = fs::current_path();
        lib().path()         = working_dir.string();
        if (lib().src().empty() && fs::is_directory(working_dir / "src"))
            lib().src() = {"src/*"};
        if (lib().inc().empty() && fs::is_directory(working_dir / "include"))
            lib().inc() = {"public:include"};
    }

    std::vector<std::string> required_features;
    if (!no_default_features())
        try {
            find_requested_features(*this, "default", required_features);
        } catch (const std::exception &e) {
            std::ignore = e;
        }
    else
        try {
            find_requested_features(*this, "nodefault", required_features);
        } catch (const std::exception &e) {
            std::ignore = e;
        }
    for (auto &feat : features)
        find_requested_features(*this, feat, required_features);
    spdlog::debug("resolving: name={:?} required_features={}", package().name(), required_features);

    std::vector<std::string> resolved;
    for (auto &[name, dep] : dependencies()) {
        auto &d = convert_dep(dep);
        if (d.name().empty())
            d.name() = name;
        if (no_default_features() && name == "default")
            continue;
        if (!no_default_features() && name == "nodefault")
            continue;
        if (d.optional() && std::find(required_features.begin(), required_features.end(), name) == required_features.end())
            continue;

        try {
            resolve_remote_dep(name, d);
        } catch (const std::exception &e) {
            throw ferr("Error building dependency `{}` of package `{}`: {}", name, package().name(), e.what());
        }
        resolved.push_back(name);
    }

    for (auto &name : resolved) {
        auto &d = convert_dep(dependencies().at(name));
        if (d.empty())
            continue;
        try {
            collect_meta(name, d);
        } catch (const std::exception &e) {
            throw ferr("Error collecting meta of dependency `{}` of package `{}`: {}", name, package().name(), e.what());
        }
    }
    lib().name()             = package().name();
    lib().version()          = package().version();
    lib().cpp_standard       = package().edition();
    lib().features()         = features;
    lib().default_features() = !no_default_features();
}

void Project::resolve_remote_dep(const std::string &name, Dependency &d) {
    constexpr auto toml_version = cpx::toml::toruniina_toml::spec::v(1, 1, 0);

    auto build_subpackage = [&](Project &p) -> Dependency & {
        if (p.package().edition() > package().edition())
            throw ferr(
                "Error building dependency package={0:?}: {0:?} required std=c++{1} but {2:?} only supports std=c++{3}",
                p.package().name(),
                p.package().edition(),
                package().name(),
                package().edition()
            );

        p.ppackages             = &packages();
        p.cache()               = cache();
        p.no_default_features() = !d.default_features().value_or(true);
        p.targets()             = targets();
        p.package().vars()      = package().vars();
        try {
            p.build(d.features(), true);
        } catch (const std::exception &e) {
            throw ferr("Error building dependency package={} `{}`: {}", p.package().name(), name, e.what());
        }
        return p.lib();
    };

    if (d.empty()) {
        lib() += d;
        return;
    }

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
        auto &lib             = build_subpackage(p);
        d                     = std::move(lib);
        resolve_remote_dep(name, d);
        return;
    }

    if (auto sub = fs::path(d.path()) / d.subdir() / "cpp++.toml"; fs::exists(sub)) {
        auto  p   = cpx::toml::toruniina_toml::parse_from_file<Project>(sub.string(), toml_version);
        auto &lib = build_subpackage(p);
        d         = std::move(lib);
    }
}

void Project::collect_meta(const std::string &name, Dependency &d) {
    const auto cpp_standard = d.cpp_standard.value_or(package().edition());

    std::sort(d.features().begin(), d.features().end());
    std::string feature_name = fmt::format("{}", fmt::join(d.features(), "-"));
    if (!d.default_features().value_or(true))
        feature_name = feature_name + (feature_name.empty() ? "nodefault" : "-nodefault");
    if (feature_name.empty())
        feature_name = "-";

    fs::path working_dir = fs::path(d.path()) / d.subdir();
    if (working_dir.empty())
        throw ferr("path and subdir is empty for dep={}", name);

    if (!d.pre().empty()) {
        spdlog::info("running pre command for package={:?} dep={:?} pre={:?}", package().name(), name, d.pre());
        std::string cmd = fmt::format("cd '{}' && ({}) > /dev/null 2>&1", working_dir.string(), d.pre());
        if (std::system(cmd.c_str()) != 0)
            throw ferr("pre command failed for dep={}: {}", name, d.pre());
    }

    if (d.src().empty() && fs::is_directory(working_dir / "src"))
        d.src() = {"src/*"};
    if (d.inc().empty() && fs::is_directory(working_dir / "include"))
        d.inc() = {"public:include"};

    auto    &target    = targets().release();
    fs::path cache     = this->cache();
    fs::path build_dir = cache / "build" / target.id() /
                         (name + "-" +
                          (d.branch().empty() && d.tag().empty() ? d.version()
                           : d.branch().empty()                  ? d.tag()
                                                                 : d.branch())) /
                         feature_name;

    std::vector<std::string> flags;
    for (auto &str : d.flags()) {
        if (str.rfind("public:", 0) == 0) {
            auto f = str.substr(std::string("public:").size());
            push_unique(flags, f);
            push_unique(lib().flags(), f);
        } else {
            push_unique(flags, str);
        }
    }
    for (auto &str : d.inc()) {
        if (str.rfind("public:", 0) == 0) {
            auto inc = "-I" + (working_dir / str.substr(std::string("public:").size())).string();
            push_unique(flags, inc);
            push_unique(lib().flags(), inc);
        } else {
            push_unique(flags, "-I" + (working_dir / str).string());
        }
    }
    for (auto &str : d.lib()) {
        push_unique(lib().link_flags(), (working_dir / str).string());
    }
    for (auto &str : d.link_flags()) {
        push_unique(lib().link_flags(), str);
    }

    try {
        auto expanded = expand_path(working_dir.string(), d.src());
        if (expanded.empty())
            return;

        fmt::print(fmt::emphasis::bold | fmt::fg(fmt::color::green), "{:>12} ", "Compiling");
        fmt::println("{} v{}", d.name(), d.version());
        for (fs::path entry : expanded) {
            CompileCommand cc;
            cc.directory() = build_dir.string();
            cc.output()    = entry.string() + ".o";
            cc.depfile()   = entry.string() + ".d";
            cc.file()      = (working_dir / entry).string();
            if (auto ext = entry.extension(); ext == ".cpp" || ext == ".cxx" || ext == ".cc" || ext == ".cppm") {
                if (ext == ".cppm" && cpp_standard < 20)
                    throw ferr("C++ modules are not supported in std=c++{}, but std=c++{} is used", cpp_standard, entry.string());

                cc.command() =
                    f("{} -std=c++{} {} -o '{}' -c '{}' -MMD -MP -MF '{}'",
                      target.cpp(),
                      cpp_standard,
                      fmt::join(flags, " "),
                      cc.output(),
                      cc.file(),
                      cc.depfile());

                cc.compile();
                push_unique(lib().link_flags(), (working_dir / cc.output()).string());
                compile_commands().push_back(cc);
            } else if (ext == ".c") {
                cc.command() =
                    f("{} {} -o '{}' -c '{}' -MMD -MP -MF '{}'",
                      target.c(),
                      fmt::join(flags, " "),
                      cc.output(),
                      cc.file(),
                      cc.depfile());
                cc.compile();
                push_unique(lib().link_flags(), (working_dir / cc.output()).string());
                compile_commands().push_back(cc);
            }
        }
    } catch (std::exception &e) {
        throw ferr("Cannot resolve dep={:?}, src={}: {}", name, d.src(), e.what());
    }
}
