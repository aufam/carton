module;

#include <string>
#include <vector>
#include <memory>

export module carton:library;
import :package;
import :dependency;
import :compile_command;
import cpx;

export struct Library {
    std::string title;
    std::string name;
    std::string working_dir;
    std::string build_name;

    int                         edition = 0;
    std::vector<std::string>    public_flags;
    std::vector<std::string>    mod_flags;
    std::vector<std::string>    modules;
    std::vector<CompileCommand> compile_commands;
    std::vector<CompileCommand> precompile_commands;
    CompileCommand              archive_command;

    std::vector<std::string> src;
    std::vector<std::string> inc;
    std::vector<std::string> lib;
    std::vector<std::string> mod;
    std::vector<std::string> flags;
    std::vector<std::string> link_flags;
    std::string              pre;

    std::vector<Library *> dependencies;

    Library() = default;

    Library(const std::string &name, const Dependency &d);

    struct Args {
        const Dependency &dep;
        int               edition;
        std::string       build_name;
    };
    static std::unique_ptr<Library> New(Args &&);

    auto extend_new(const Dependency &d) const -> std::unique_ptr<Library>;
    void add_interface(const Dependency &d);
    void add_dependency(Library &lib, bool extend = false);
    void add_dependencies(const std::vector<Library *> &libs, bool extend = false);
};
