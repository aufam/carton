module;

#include <string>
#include <vector>
#include <memory>
#include <algorithm>

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

std::unique_ptr<Library> Library::New(Args &&args) {
    auto  ret = std::make_unique<Library>();
    auto &lib = *ret;

    lib.title       = args.dep.display_name();
    lib.working_dir = args.dep.working_dir;
    lib.edition     = args.edition;
    lib.build_name  = args.build_name;

    lib.add_interface(args.dep);

    return ret;
}

std::unique_ptr<Library> Library::extend_new(const Dependency &dep) const {
    auto  ret = std::make_unique<Library>();
    auto &lib = *ret;

    lib.title       = title;
    lib.working_dir = working_dir;
    lib.edition     = edition;
    lib.build_name  = build_name + "." + dep.build_name();

    return ret;
}

void Library::add_interface(const Dependency &dep) {
    push_unique(src, dep.src);
    push_unique(mod, dep.mod);

    if (!dep.pre.empty()) {
        pre += dep.pre.empty() ? "" : "\n";
        pre += dep.pre;
    }

    for (auto &flag : dep.flags) {
        if (flag.starts_with("public:")) {
            auto fl = flag.substr(7);
            push_unique(flags, fl);
            push_unique(public_flags, fl);
        } else {
            push_unique(flags, flag);
        }
    }

    const auto working_dir = fs::path(this->working_dir);
    for (auto &inc : dep.inc) {
        if (inc.starts_with("public:")) {
            auto fl = "-I" + (working_dir / inc.substr(7)).lexically_normal().string();
            push_unique(flags, fl);
            push_unique(public_flags, fl);
        } else {
            push_unique(flags, "-I" + (working_dir / inc).lexically_normal().string());
        }
    }

    for (auto str : dep.link_flags) {
        if (check_os_continue(str))
            continue;

        push_unique(link_flags, str);
    }

    for (auto str : dep.lib) {
        if (check_os_continue(str))
            continue;

        if (auto path = fs::path(str); path.is_absolute()) {
            auto p = path.lexically_normal().string();
            push_unique(link_flags, p);
        } else {
            auto p = (working_dir / path).lexically_normal().string();
            push_unique(link_flags, p);
        }
    }
}

void Library::add_dependency(Library &other, bool extend) {
    auto it = std::find_if(dependencies.begin(), dependencies.end(), [&](auto p) { return p == &other; });
    if (it == dependencies.end()) {
        if (extend) {
            push_unique(flags, other.flags);
            push_unique(public_flags, other.public_flags);
        } else {
            push_unique(flags, other.public_flags);
        }
        dependencies.push_back(&other);
    }
}

void Library::add_dependencies(const std::vector<Library *> &deps, bool extend) {
    for (auto dep : deps) {
        add_dependency(*dep, extend);
    }
}
