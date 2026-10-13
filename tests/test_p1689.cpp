module;

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <stdexcept>

module carton;

TEST(P1689Test, Defaults) {
    p1689 p;
    EXPECT_EQ(p.revision, 0);
    EXPECT_EQ(p.version, 1);
    EXPECT_TRUE(p.rules.empty());
}

TEST(P1689Test, NameThrowsOnEmptyRules) {
    p1689 p;
    EXPECT_THROW(
        {
            try {
                p.name();
            } catch (const std::runtime_error &e) {
                EXPECT_STREQ(e.what(), "p1689: rules empty");
                throw;
            }
        },
        std::runtime_error
    );
}

TEST(P1689Test, NameThrowsOnMultipleRules) {
    p1689 p;
    p.rules.push_back({});
    p.rules.push_back({});
    EXPECT_THROW(
        {
            try {
                p.name();
            } catch (const std::runtime_error &e) {
                EXPECT_STREQ(e.what(), "p1689: multiple rules");
                throw;
            }
        },
        std::runtime_error
    );
}

TEST(P1689Test, NameThrowsOnEmptyProvides) {
    p1689 p;
    p.rules.push_back({});
    EXPECT_THROW(
        {
            try {
                p.name();
            } catch (const std::runtime_error &e) {
                EXPECT_STREQ(e.what(), "p1689: provides empty");
                throw;
            }
        },
        std::runtime_error
    );
}

TEST(P1689Test, NameThrowsOnMultipleProvides) {
    p1689       p;
    p1689::Rule rule;
    rule.provides.push_back({.name = "mod1"});
    rule.provides.push_back({.name = "mod2"});
    p.rules.push_back(rule);
    EXPECT_THROW(
        {
            try {
                p.name();
            } catch (const std::runtime_error &e) {
                EXPECT_STREQ(e.what(), "p1689: multiple provides");
                throw;
            }
        },
        std::runtime_error
    );
}

TEST(P1689Test, NameReturnsSingleProvide) {
    p1689       p;
    p1689::Rule rule;
    rule.provides.push_back({.name = "my.awesome.module", .is_interface = true});
    p.rules.push_back(rule);
    EXPECT_EQ(p.name(), "my.awesome.module");
}

TEST(P1689Test, DepsThrowsOnEmptyRules) {
    p1689 p;
    EXPECT_THROW(
        {
            try {
                p.deps();
            } catch (const std::runtime_error &e) {
                EXPECT_STREQ(e.what(), "p1689: rules empty");
                throw;
            }
        },
        std::runtime_error
    );
}

TEST(P1689Test, DepsThrowsOnMultipleRules) {
    p1689 p;
    p.rules.push_back({});
    p.rules.push_back({});
    EXPECT_THROW(
        {
            try {
                p.deps();
            } catch (const std::runtime_error &e) {
                EXPECT_STREQ(e.what(), "p1689: multiple rules");
                throw;
            }
        },
        std::runtime_error
    );
}

TEST(P1689Test, DepsReturnsRequiresList) {
    p1689       p;
    p1689::Rule rule;
    rule.provides.push_back({.name = "my.module"});
    rule.requires_.push_back({.name = "std"});
    rule.requires_.push_back({.name = "fmt"});
    rule.requires_.push_back({.name = "cpx"});
    p.rules.push_back(rule);

    auto                     dependencies = p.deps();
    std::vector<std::string> expected     = {"std", "fmt", "cpx"};
    EXPECT_EQ(dependencies, expected);
}
