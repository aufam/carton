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

void Project::build_dep(const std::vector<std::string> &features, bool subpackage) {
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
    spdlog::debug("resolving: name={:?} extra_features={}", package().name(), extra_features);

    std::vector<std::string> resolved;
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
            compile_dep(d, lib(), profiles().release());
        } catch (const std::exception &e) {
            throw ferr("Error collecting meta of dependency `{}` of package `{}`: {}", name, package().name(), e.what());
        }
    }
    lib().name()             = package().name();
    lib().version()          = package().version();
    lib().cpp_standard       = package().edition();
    lib().features()         = std::move(extra_features);
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

        p.pparent               = this;
        p.ppackages             = &packages();
        p.phash_history         = &hash_history;
        p.cache()               = cache();
        p.no_default_features() = !d.default_features().value_or(true);
        p.profiles()            = profiles();
        p.vars()                = vars();
        try {
            p.build_dep(d.features(), true);
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
        if (d.version() == "auto") {
            if (pparent == nullptr)
                throw ferr("version {:?} must be in the parent project", d.version());

            auto it = pparent->dependencies().find(d.name());
            if (it == pparent->dependencies().end())
                throw ferr("package {:?} must be in the parent project", d.name());

            d = convert_dep(it->second);
        }
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

bool Project::compile_dep(Dependency &d, Dependency &root, const Profile &profile) {
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

    if (&d != &this->lib()) { // otherwise, already done
        if (d.src().empty() && fs::is_directory(working_dir / "src"))
            d.src() = {"src/*"};
        if (d.inc().empty() && fs::is_directory(working_dir / "include"))
            d.inc() = {"public:include"};
    }

    const auto flags_    = f("{}", fmt::join(profile.flags(), " "));
    const auto CXX       = profile.cxx() + (flags_.empty() ? "" : " " + flags_);
    const auto C         = profile.c() + (flags_.empty() ? "" : " " + flags_);
    fs::path   cache     = this->cache();
    fs::path   build_dir = cache / "build" / profile.id() /
                         (d.name() + "-" +
                          (d.branch().empty() && d.tag().empty() ? d.version()
                           : d.branch().empty()                  ? d.tag()
                                                                 : d.branch())) /
                         feature_name;

    std::vector<std::string> flags;
    for (auto &str : d.flags()) {
        if (str.rfind("public:", 0) == 0) {
            auto f = str.substr(std::string("public:").size());
            push_unique(flags, f);
            push_unique(root.flags(), f);
        } else {
            push_unique(flags, str);
        }
    }
    for (auto &str : d.inc()) {
        if (str.rfind("public:", 0) == 0) {
            auto inc = "-I" + (working_dir / str.substr(std::string("public:").size())).string();
            push_unique(flags, inc);
            push_unique(root.flags(), inc);
        } else {
            push_unique(flags, "-I" + (working_dir / str).string());
        }
    }
    for (auto &str : d.lib()) {
        auto lib = (working_dir / str).string();
        push_unique(root.link_flags(), lib);
    }
    for (auto &str : d.link_flags()) {
        push_unique(root.link_flags(), str);
    }

    try {
        auto expanded = expand_path(working_dir.string(), d.src());

        std::vector<CompileCommand> ccs;
        for (fs::path entry : expanded) {
            CompileCommand cc;
            cc.directory() = build_dir.string();
            cc.output()    = entry.string() + ".o";
            cc.depfile()   = entry.string() + ".d";
            cc.file()      = (working_dir / entry).string();
            if (auto ext = entry.extension(); ext == ".cpp" || ext == ".cxx" || ext == ".cc") {
                cc.command() =
                    f("{} -std=c++{} {} -o '{}' -c '{}' -MMD -MP -MF '{}'",
                      CXX,
                      cpp_standard,
                      fmt::join(flags, " "),
                      cc.output(),
                      cc.file(),
                      cc.depfile());

                push_unique(root.link_flags(), (build_dir / cc.output()).string());
                compile_commands().push_back(cc);
                ccs.push_back(cc);
            } else if (ext == ".c" || ext == ".s" || ext == ".asm" || ext == ".S") {
                cc.command() =
                    f("{} {} -o '{}' -c '{}' -MMD -MP -MF '{}'", C, fmt::join(flags, " "), cc.output(), cc.file(), cc.depfile());
                push_unique(root.link_flags(), (build_dir / cc.output()).string());
                compile_commands().push_back(cc);
                ccs.push_back(cc);
            } else if (ext == ".cppm" || ext == ".ixx")
                throw ferr("file={}: carton does not support module yet :(", cc.file());
        }
        return compile_multi(f("{} v{}", d.name(), d.version()), ccs, phash_history ? *phash_history : hash_history);
    } catch (std::exception &e) {
        throw ferr("Cannot resolve dep={:?}, src={}: {}", d.name(), d.src(), e.what());
    }
}

void Project::build() {
    const auto &profile     = profiles().debug();
    const auto  link_flags_ = f("{}", fmt::join(profile.link_flags(), " "));
    const auto  LINK        = profile.cxx() + (link_flags_.empty() ? "" : " " + link_flags_);
    fs::path    cache       = this->cache();

    Dependency dummy_root;

    bool compiled = compile_dep(lib(), dummy_root, profile);

    auto output = cache / "bin" / lib().name();
    if (compiled || !fs::exists(output)) {
        fs::create_directories(output.parent_path());

        fmt::print(fmt::emphasis::bold | fmt::fg(fmt::color::green), "{:>12} ", "Linking");
        fmt::println("{} v{}", lib().name(), lib().version());

        auto link_cmd = f("{} -o '{}' {}", LINK, output.string(), fmt::join(dummy_root.link_flags(), " "));
        spdlog::debug("linking cmd={}", link_cmd);
        if (std::system(link_cmd.c_str()) != 0)
            throw ferr("linking failed: cmd={}", lib().name(), link_cmd);
    }
}
