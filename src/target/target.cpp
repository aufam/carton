module;

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <reproc++/run.hpp>

module carton;

static bool check_os_continue(std::string &str) {
    if (auto pos = str.find(':'); pos != std::string::npos) {
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

        auto key   = str.substr(0, pos);
        auto value = str.substr(pos + 1);
        if (key == os) {
            str = value;
        } else {
            str = "";
            return true;
        }
    }
    return false;
}

static void apply(Target &self, const Dependency &dep) {
    for (auto &src : dep.src) {
        push_unique(self.src, src);
    }
    for (auto &mod : dep.mod) {
        push_unique(self.src, mod);
    }

    const auto working_dir = fs::path(self.working_dir);

    self.pre = dep.pre;

    for (auto &flag : dep.flags) {
        if (flag.starts_with("public:")) {
            auto fl = flag.substr(7);
            push_unique(self.flags, fl);
            push_unique(self.public_flags, fl);
        } else {
            push_unique(self.flags, flag);
        }
    }

    for (auto &inc : dep.inc) {
        if (inc.starts_with("public:")) {
            auto fl = "-I" + (working_dir / inc.substr(7)).lexically_normal().string();
            push_unique(self.flags, fl);
            push_unique(self.public_flags, fl);
        } else {
            push_unique(self.flags, "-I" + (working_dir / inc).lexically_normal().string());
        }
    }

    for (auto str : dep.link_flags) {
        if (check_os_continue(str))
            continue;

        push_unique(self.link_flags, str);
    }

    for (auto str : dep.lib) {
        if (check_os_continue(str))
            continue;

        if (auto path = fs::path(str); path.is_absolute()) {
            auto p = path.lexically_normal().string();
            push_unique(self.link_flags, p);
        } else {
            auto p = (working_dir / path).lexically_normal().string();
            push_unique(self.link_flags, p);
        }
    }
}

std::unique_ptr<Target> Target::New(const Dependency &dep) {
    auto target         = std::make_unique<Target>();
    target->working_dir = dep.working_dir;
    apply(*target, dep);
    return target;
}

std::unique_ptr<Target> Target::New(const std::string &working_dir, const Binary &bin) {
    auto       target = std::make_unique<Target>();
    const auto root   = fs::path(working_dir);
    for (auto &src : bin.src) {
        push_unique(target->src, (root / src).lexically_normal().string());
    }
    return target;
}

void Target::add_dependency(Target &other) {
    if (edition < other.edition)
        ferr("{:?} cannot depend on {:?}: needs higher c++ version {}", name, other.name, other.edition);

    auto it = std::find_if(dependencies.begin(), dependencies.end(), [&](auto p) { return p == &other; });
    if (it == dependencies.end()) {
        push_unique(flags, other.public_flags);
        push_unique(public_flags, other.public_flags);
        push_unique(modules, other.modules);
        dependencies.push_back(&other);
    }
}

void Target::add_dependencies(const std::vector<Target *> &deps) {
    for (auto dep : deps) {
        add_dependency(*dep);
    }
}
