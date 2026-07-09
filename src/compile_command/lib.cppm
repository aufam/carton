module;

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include "../macro.h"

export module carton:compile_command;
import cpx;

export struct CompileCommand {
    std::string file;
    std::string directory;
    std::string command;
    std::string output;

    std::string              depfile;
    std::vector<std::string> modnames;

    static bool compile_multi(
        const std::string                            &name,
        const std::vector<CompileCommand>            &commands,
        const std::map<std::string, std::string>     &mod_names,
        std::unordered_map<std::string, std::string> &hash_history,
        bool                                          precompile = false
    );
};

// clang-format off
CPX_REFLECT(
    (CompileCommand , ),

    ((file      , "file      , omitempty"))
    ((directory , "directory , omitempty"))
    ((command   , "command   , omitempty"))
    ((output    , "output    , omitempty"))
);
// clang-format on
