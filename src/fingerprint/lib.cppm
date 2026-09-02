module;

#include <string>
#include <unordered_map>
#include <fstream>

export module carton:fingerprint;
import cpx;
import cpx.toruniina_toml;
import carton.fs;

export struct Fingerprint {
    std::string cmd;
    std::string file;
    std::string deps;
    std::string mods;

    bool compare(const Fingerprint &other) const {
        return cmd == other.cmd && file == other.file && mods == other.mods;
    }

    static constexpr std::tuple __field_tags__{
        cpx::field<&Fingerprint::cmd>  = "cmd  , skipmissing",
        cpx::field<&Fingerprint::file> = "file , skipmissing",
        cpx::field<&Fingerprint::deps> = "deps , skipmissing",
        cpx::field<&Fingerprint::mods> = "mods , skipmissing",
    };

    static std::unordered_map<std::string, Fingerprint> parse(const fs::path &build_dir) {
        std::unordered_map<std::string, Fingerprint> res;
        if (const fs::path path = build_dir / "fingerprints.toml"; fs::exists(path))
            cpx::toruniina_toml::parse_from_file(path.string(), res);
        return res;
    }

    static void dump(const std::unordered_map<std::string, Fingerprint> &fingerprints, const fs::path &build_dir) {
        std::ofstream os(build_dir / "fingerprints.toml");
        os << cpx::toruniina_toml::io << fingerprints;
    }
};
