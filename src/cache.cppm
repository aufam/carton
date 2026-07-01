module;

#include <string>
#include <vector>
#include <map>

export module carton:cache;
import :dependency;
import :compile_command;

export struct Cache {
    struct Meta {
        std::string                 build_dir;
        Dependency                  lib;
        std::vector<std::string>    flags;
        std::vector<std::string>    link_flags;
        std::vector<std::string>    mod_flags;
        std::string                 main_o;
        std::vector<CompileCommand> compile_commands;
        std::vector<CompileCommand> precompile_commands;
    };

    std::vector<Meta>                               meta;
    std::map<std::string, std::vector<std::string>> mods;
    std::map<std::string, std::string>              mod_paths;
    std::map<std::string, std::string>              mod_objs;
};
