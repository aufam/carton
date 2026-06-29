module;

#include <string>
#include <vector>
#include <unordered_map>
#include "macro.h"

export module carton:compile_command;
import cpx;

export struct CompileCommand {
    std::string file;
    std::string directory;
    std::string command;
    std::string output;
    std::string depfile;

    static bool compile_multi(
        const std::string                            &name,
        const std::vector<CompileCommand>            &commands,
        std::unordered_map<std::string, std::string> &hash_history,
        bool                                          precompile = false
    );
};

// clang-format off
CPX_REFLECT(
    (CompileCommand , ),

    ((file      , "file     "))
    ((directory , "directory"))
    ((command   , "command  "))
    ((output    , "output   "))
);
// clang-format on
