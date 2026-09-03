module;

#include <string>
#include <vector>
#include <memory>

export module carton:target;
import :package;
import :profile;
import :dependency;
import :compile_command;
import :binary;
import :cache;
import cpx;

export struct Target {
    std::string name;
    std::string title;
    std::string working_dir;
    std::string output_name;
    std::string output_dir;

    int                      edition = 0;
    std::vector<std::string> src;
    std::vector<std::string> mod;
    std::vector<std::string> flags;
    std::vector<std::string> public_flags;
    std::vector<std::string> modules;
    std::vector<std::string> link_flags;

    std::vector<Target *> dependencies;

    Target() = default;

    static std::unique_ptr<Target> New(const Dependency &dep);
    static std::unique_ptr<Target> New(const std::string &working_dir, const Binary &bin);

    void add_dependency(Target &other);
    void add_dependencies(const std::vector<Target *> &others);

    void configure(const Profile &profile, Cache &cache);
};
