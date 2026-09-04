module;

#include <string>
#include <vector>
#include <memory>

export module carton:target;
import :profile;
import :dependency;
import :binary;
import :cache;
import cpx;

export struct Target {
    /// for path alias in macro-prefix-map
    std::string name;

    /// for display
    std::string title;

    /// absolute path to working directory
    std::string working_dir;

    /// lib or executable name
    std::string output_name;

    /// output directory relative to build cache directory
    std::string output_dir;

    /// absolute path of the executable
    std::string executable;

    int                      edition = 0;
    std::vector<std::string> src;
    std::vector<std::string> mod;
    std::vector<std::string> flags;
    std::vector<std::string> public_flags;
    std::vector<std::string> modules;
    std::vector<std::string> link_flags;

    static constexpr std::tuple __field_tags__ = {
        cpx::field<&Target::name>         = "name",
        cpx::field<&Target::title>        = "title",
        cpx::field<&Target::working_dir>  = "working_dir",
        cpx::field<&Target::output_name>  = "output_name",
        cpx::field<&Target::output_dir>   = "output_dir",
        cpx::field<&Target::edition>      = "edition",
        cpx::field<&Target::src>          = "src",
        cpx::field<&Target::mod>          = "mod",
        cpx::field<&Target::flags>        = "flags",
        cpx::field<&Target::public_flags> = "public_flags",
        cpx::field<&Target::modules>      = "modules",
        cpx::field<&Target::link_flags>   = "link_flags",
    };

    std::vector<Target *> dependencies;

    Target() = default;

    static std::unique_ptr<Target> New(const Dependency &dep);
    static std::unique_ptr<Target> New(const std::string &working_dir, const Binary &bin);

    void add_dependency(Target &other, bool public_ = false);
    void add_dependencies(const std::vector<Target *> &others, bool public_ = false);

    void configure(const Profile &profile, Cache &cache, std::string_view type = "");
};
