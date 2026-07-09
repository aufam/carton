module;

#include <string>
#include <vector>
#include <map>

export module carton:cache;
import :dependency;
import :compile_command;

export struct Cache {
    std::vector<Dependency *>                       dependencies;
    std::map<std::string, std::vector<std::string>> mods;
    std::map<std::string, std::string>              mod_paths;
    std::map<std::string, std::string>              mod_objs;
};
