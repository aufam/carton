module;

#include <string>
#include <vector>
#include <stdexcept>
#include "macro.h"

export module carton:p1689;
import cpx;

export struct p1689 {
    struct Module {
        std::string name;
        bool        is_interface = false;
    };

    struct Rule {
        std::vector<Module> provides;
        std::vector<Module> requires_;
    };

    std::vector<Rule> rules;

    int revision = 0;
    int version  = 1;

    std::string name() const {
        if (rules.empty())
            throw std::runtime_error("p1689: rules empty");

        if (rules.size() > 1)
            throw std::runtime_error("p1689: multiple rules");

        auto &p = rules.front().provides;
        if (p.empty())
            throw std::runtime_error("p1689: provides empty");

        if (p.size() > 1)
            throw std::runtime_error("p1689: multiple provides");

        return p.front().name;
    }

    std::vector<std::string> deps() const {
        if (rules.empty())
            throw std::runtime_error("p1689: rules empty");

        if (rules.size() > 1)
            throw std::runtime_error("p1689: multiple rules");

        auto &r = rules.front().requires_;

        std::vector<std::string> res;
        res.reserve(r.size());
        for (auto &req : r) {
            res.push_back(req.name);
        }

        return res;
    }
};

// clang-format off
CPX_REFLECT(
    (p1689, ),
    ((rules    , "rules"))
    ((version  , "version"))
    ((revision , "revision"))
);

CPX_REFLECT(
    (p1689::Module, ),
    ((name          , "logical-name"))
    ((is_interface  , "is-interface , skipmissing"))
);

CPX_REFLECT(
    (p1689::Rule, ),
    ((provides   , "provides , skipmissing"))
    ((requires_  , "requires , skipmissing"))
);
// clang-format on
