module;

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

module carton;

TEST(PackageTest, DefaultValues) {
    Package pkg;
    EXPECT_TRUE(pkg.name.empty());
    EXPECT_TRUE(pkg.version.empty());
    EXPECT_EQ(pkg.edition, 17);
    EXPECT_TRUE(pkg.authors.empty());
    EXPECT_TRUE(pkg.description.empty());
    EXPECT_TRUE(pkg.license.empty());
    EXPECT_TRUE(pkg.license_file.empty());
    EXPECT_TRUE(pkg.readme.empty());
    EXPECT_TRUE(pkg.repository.empty());
    EXPECT_TRUE(pkg.homepage.empty());
    EXPECT_TRUE(pkg.documentation.empty());
    EXPECT_TRUE(pkg.keywords.empty());
    EXPECT_TRUE(pkg.versions.empty());
}

TEST(DependencyTest, DefaultValues) {
    Dependency dep;
    EXPECT_TRUE(dep.version.empty());
    EXPECT_TRUE(dep.path.empty());
    EXPECT_TRUE(dep.url.empty());
    EXPECT_TRUE(dep.git.empty());
    EXPECT_TRUE(dep.tag.empty());
    EXPECT_TRUE(dep.branch.empty());
    EXPECT_TRUE(dep.commit.empty());
    EXPECT_TRUE(dep.subdir.empty());
    EXPECT_TRUE(dep.features.empty());
    EXPECT_FALSE(dep.optional);
    EXPECT_FALSE(dep.default_features.has_value());
    EXPECT_TRUE(dep.src.empty());
    EXPECT_TRUE(dep.inc.empty());
    EXPECT_TRUE(dep.lib.empty());
    EXPECT_TRUE(dep.mod.empty());
    EXPECT_TRUE(dep.flags.empty());
    EXPECT_TRUE(dep.link_flags.empty());
    EXPECT_TRUE(dep.pre.empty());
    EXPECT_TRUE(dep.name.empty());
}

TEST(DependencyTest, FromStr) {
    Dependency dep;
    Dependency::__from_str__(dep, "1.2.3");
    EXPECT_EQ(dep.version, "1.2.3");
}

TEST(BinaryTest, DefaultValues) {
    Binary bin;
    EXPECT_TRUE(bin.name.empty());
    EXPECT_TRUE(bin.path.empty());
    EXPECT_TRUE(bin.required_features.empty());
}

TEST(ConfigTest, DefaultProfiles) {
    Config cfg;
    EXPECT_EQ(cfg.profiles.dev.name, "dev");
    EXPECT_EQ(cfg.profiles.release.name, "release");
}

TEST(CliTest, DefaultValues) {
    Cli cli;
    EXPECT_EQ(cli.log_level, spdlog::level::warn);
    EXPECT_FALSE(cli.release);
    EXPECT_FALSE(cli.static_);
    EXPECT_FALSE(cli.no_default_features);
    EXPECT_FALSE(cli.tree);
    EXPECT_TRUE(cli.features.empty());
    EXPECT_FALSE(cli.build.has_value());
    EXPECT_FALSE(cli.run.has_value());
    EXPECT_FALSE(cli.clean.has_value());
    EXPECT_FALSE(cli.version.has_value());
    EXPECT_FALSE(cli.update.has_value());
    EXPECT_FALSE(cli.init.has_value());
    EXPECT_FALSE(cli.add.has_value());
}

TEST(CliTest, BuildAndRunDefaults) {
    Cli::Build build;
    EXPECT_FALSE(build.release);
    EXPECT_FALSE(build.static_);
    EXPECT_FALSE(build.no_default_features);
    EXPECT_TRUE(build.features.empty());

    Cli::Run run;
    EXPECT_FALSE(run.release);
    EXPECT_FALSE(run.static_);
    EXPECT_TRUE(run.bin.empty());
    EXPECT_TRUE(run.args.empty());
}
