module;

#include <string>
#include <vector>
#include <stdexcept>

export module carton:p1689;
import cpx;

export struct p1689 {
    struct Module {
        std::string name;
        bool        is_interface = false;

        static constexpr std::tuple __field_tags__{
            cpx::field<&p1689::Module::name>         = "logical-name",
            cpx::field<&p1689::Module::is_interface> = "is-interface , skipmissing",
        };
    };

    struct Rule {
        std::vector<Module> provides;
        std::vector<Module> requires_;

        static constexpr std::tuple __field_tags__{
            cpx::field<&p1689::Rule::provides>  = "provides , skipmissing",
            cpx::field<&p1689::Rule::requires_> = "requires , skipmissing",
        };
    };

    std::vector<Rule> rules;
    int               revision = 0;
    int               version  = 1;

    static constexpr std::tuple __field_tags__{
        cpx::field<&p1689::rules>    = "rules",
        cpx::field<&p1689::version>  = "version",
        cpx::field<&p1689::revision> = "revision",
    };

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
