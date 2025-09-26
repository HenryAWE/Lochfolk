#include <gtest/gtest.h>
#include <lochfolk/path.hpp>
#include <algorithm>
#include <sstream>

TEST(path, constructor)
{
    auto ss_helper = [](const lochfolk::path& p)
    {
        std::stringstream ss;
        ss << p;
        return std::move(ss).str();
    };

    {
        lochfolk::path p;
        EXPECT_TRUE(p.empty());
        EXPECT_EQ(ss_helper(p), "");
    }

    {
        lochfolk::path p = "/data/a.txt";
        EXPECT_EQ(p.string(), "/data/a.txt");
        EXPECT_EQ(ss_helper(p), "\"/data/a.txt\"");
    }

    {
        lochfolk::path p = "audio/a.wav";
        EXPECT_EQ(p.string(), "audio/a.wav");
        EXPECT_EQ(ss_helper(p), "\"audio/a.wav\"");
    }

    {
        lochfolk::path p = "data";
        EXPECT_EQ(p.string(), "data");
        EXPECT_EQ(ss_helper(p), "\"data\"");
    }
}

TEST(path, is_absolute)
{
    {
        lochfolk::path p;
        EXPECT_FALSE(p.is_absolute());
    }

    {
        lochfolk::path p = "/data/a.txt";
        EXPECT_TRUE(p.is_absolute());
    }

    {
        lochfolk::path p = "audio/a.wav";
        EXPECT_FALSE(p.is_absolute());
    }

    {
        lochfolk::path p = "data";
        EXPECT_FALSE(p.is_absolute());
    }

    {
        lochfolk::path p = "/data/.";
        EXPECT_FALSE(p.is_absolute());
    }

    {
        lochfolk::path p = "/data/.hidden";
        EXPECT_TRUE(p.is_absolute());
    }

    {
        lochfolk::path p = "/data/..";
        EXPECT_FALSE(p.is_absolute());
    }

    {
        lochfolk::path p = "/data/..a.txt";
        EXPECT_TRUE(p.is_absolute());
    }
}

TEST(path, append)
{
    using namespace lochfolk::vfs_literals;

    {
        lochfolk::path p = "/data";
        p.append("audio/a.wav"_pv);

        EXPECT_EQ(p.string(), "/data/audio/a.wav");
        EXPECT_TRUE(p.is_absolute());
    }

    {
        lochfolk::path p = "/data";
        p.append("audio/a.wav");

        EXPECT_EQ(p.string(), "/data/audio/a.wav");
        EXPECT_TRUE(p.is_absolute());
    }

    {
        lochfolk::path p = "/data";
        p /= "audio/a.wav"_pv;

        EXPECT_EQ(p.string(), "/data/audio/a.wav");
        EXPECT_TRUE(p.is_absolute());
    }

    {
        lochfolk::path p = "/data";
        p /= "audio/a.wav";

        EXPECT_EQ(p.string(), "/data/audio/a.wav");
        EXPECT_TRUE(p.is_absolute());
    }
}

TEST(path, parent_path)
{
    {
        lochfolk::path p = "/data/text/example.txt";
        p = p.parent_path();
        EXPECT_EQ(p.string(), "/data/text");
    }

    {
        lochfolk::path p = "/data/text/";
        p = p.parent_path();
        EXPECT_EQ(p.string(), "/data");
    }

    {
        lochfolk::path p = "/data";
        p = p.parent_path();
        EXPECT_EQ(p.string(), "/");
    }


    {
        lochfolk::path p = "/data/";
        p = p.parent_path();
        EXPECT_EQ(p.string(), "/");
    }


    {
        lochfolk::path p = "/";
        p = p.parent_path();
        EXPECT_EQ(p.string(), "/");
    }


    // If the path is relative and its parent cannot be determined, return itself.
    {
        lochfolk::path p = "data";
        EXPECT_FALSE(p.is_absolute());
        p = p.parent_path();
        EXPECT_EQ(p.string(), "data");
    }

    {
        lochfolk::path p = "data/example.txt";
        EXPECT_FALSE(p.is_absolute());
        p = p.parent_path();
        EXPECT_EQ(p.string(), "data");
    }
}

TEST(path, filename)
{
    using namespace lochfolk::vfs_literals;

    EXPECT_EQ("/foo/bar.txt"_pv.filename(), "bar.txt"_pv);
    EXPECT_EQ("/foo/.bar"_pv.filename(), ".bar"_pv);
    EXPECT_EQ("/foo/bar/"_pv.filename(), ""_pv);
    EXPECT_EQ("/"_pv.filename(), ""_pv);

    EXPECT_EQ(lochfolk::path("/foo/bar.txt").filename(), "bar.txt"_pv);
    EXPECT_EQ(lochfolk::path("/foo/.bar").filename(), ".bar"_pv);
    EXPECT_EQ(lochfolk::path("/foo/bar/").filename(), ""_pv);
    EXPECT_EQ(lochfolk::path("/").filename(), ""_pv);
}

TEST(path, extension)
{
    using namespace lochfolk::vfs_literals;

    EXPECT_EQ("/foo/bar.txt"_pv.extension(), ".txt"_pv);
    EXPECT_EQ("/foo/bar."_pv.extension(), "."_pv);
    EXPECT_EQ("/foo/bar"_pv.extension(), ""_pv);
    EXPECT_EQ("/foo/..bar"_pv.extension(), ".bar"_pv);
    EXPECT_EQ("/foo/.hidden"_pv.extension(), ""_pv);
}

TEST(path, iterator_forward)
{
    using namespace lochfolk::vfs_literals;

    using traits = std::iterator_traits<lochfolk::path_view::const_iterator>;
    static_assert(std::is_convertible_v<traits::iterator_category, std::bidirectional_iterator_tag>);

    auto to_strs = [](const auto& p)
    {
        std::vector<std::string> result;
        for(auto&& i : p)
            result.push_back(i.string());
        return result;
    };

    {
        lochfolk::path_view p = "/data/text/example.txt"_pv;

        std::vector strs = to_strs(p);

        ASSERT_EQ(strs.size(), 4);
        EXPECT_EQ(strs[0], "/");
        EXPECT_EQ(strs[1], "data");
        EXPECT_EQ(strs[2], "text");
        EXPECT_EQ(strs[3], "example.txt");
    }

    {
        lochfolk::path_view p = "//data/text/example.txt"_pv;

        std::vector strs = to_strs(p);

        ASSERT_EQ(strs.size(), 4);
        EXPECT_EQ(strs[0], "/");
        EXPECT_EQ(strs[1], "data");
        EXPECT_EQ(strs[2], "text");
        EXPECT_EQ(strs[3], "example.txt");
    }

    {
        lochfolk::path_view p = "data/text/example.txt"_pv;

        std::vector strs = to_strs(p);

        ASSERT_EQ(strs.size(), 3);
        EXPECT_EQ(strs[0], "data");
        EXPECT_EQ(strs[1], "text");
        EXPECT_EQ(strs[2], "example.txt");
    }

    {
        lochfolk::path_view p = "data//text/example.txt"_pv;

        std::vector strs = to_strs(p);

        ASSERT_EQ(strs.size(), 3);
        EXPECT_EQ(strs[0], "data");
        EXPECT_EQ(strs[1], "text");
        EXPECT_EQ(strs[2], "example.txt");
    }

    {
        lochfolk::path_view p = "data//text/example.txt"_pv;

        std::vector<std::string> vec;
        std::transform(
            p.begin(),
            p.end(),
            std::back_inserter(vec),
            [](auto&& p)
            {
                return p.string();
            }
        );
        EXPECT_EQ(
            to_strs(p),
            vec
        );
    }
}

TEST(path, iterator_backward)
{
    using namespace lochfolk::vfs_literals;

    auto to_strs = [](const auto& p)
    {
        std::vector<std::string> result;
        auto it = p.end();
        while(it != p.begin())
        {
            --it;
            result.push_back((*it).string());
        }
        return result;
    };

    {
        lochfolk::path_view p = "/data/text/example.txt"_pv;

        std::vector strs = to_strs(p);

        ASSERT_EQ(strs.size(), 4);
        EXPECT_EQ(strs[0], "example.txt");
        EXPECT_EQ(strs[1], "text");
        EXPECT_EQ(strs[2], "data");
        EXPECT_EQ(strs[3], "/");
    }

    {
        lochfolk::path_view p = "//data/text/example.txt"_pv;

        std::vector strs = to_strs(p);

        ASSERT_EQ(strs.size(), 4);
        EXPECT_EQ(strs[0], "example.txt");
        EXPECT_EQ(strs[1], "text");
        EXPECT_EQ(strs[2], "data");
        EXPECT_EQ(strs[3], "/");
    }

    {
        lochfolk::path p = "//data/text/example.txt";

        std::vector strs = to_strs(p);

        ASSERT_EQ(strs.size(), 4);
        EXPECT_EQ(strs[0], "example.txt");
        EXPECT_EQ(strs[1], "text");
        EXPECT_EQ(strs[2], "data");
        EXPECT_EQ(strs[3], "/");
    }

    {
        lochfolk::path_view p = "data/text/example.txt"_pv;

        std::vector strs = to_strs(p);

        ASSERT_EQ(strs.size(), 3);
        EXPECT_EQ(strs[0], "example.txt");
        EXPECT_EQ(strs[1], "text");
        EXPECT_EQ(strs[2], "data");
    }

    {
        lochfolk::path_view p = "data/text//example.txt"_pv;

        std::vector strs = to_strs(p);

        ASSERT_EQ(strs.size(), 3);
        EXPECT_EQ(strs[0], "example.txt");
        EXPECT_EQ(strs[1], "text");
        EXPECT_EQ(strs[2], "data");
    }

    {
        lochfolk::path p = "data/text//example.txt";

        std::vector strs = to_strs(p);

        ASSERT_EQ(strs.size(), 3);
        EXPECT_EQ(strs[0], "example.txt");
        EXPECT_EQ(strs[1], "text");
        EXPECT_EQ(strs[2], "data");
    }

    {
        lochfolk::path_view p = "data//text/example.txt"_pv;

        std::vector<std::string> vec;
        std::transform(
            p.rbegin(),
            p.rend(),
            std::back_inserter(vec),
            [](auto&& p)
            {
                return p.string();
            }
        );
        EXPECT_EQ(
            to_strs(p),
            vec
        );
    }
}

TEST(path, lexically_normal)
{
    using namespace lochfolk::vfs_literals;

    auto check_lex_normal = [](const lochfolk::path& p, lochfolk::path_view expected)
    {
        EXPECT_EQ(
            p.lexically_normal(),
            expected
        ) << "p = "
          << std::string_view(p);
    };

    check_lex_normal("", ""_pv);
    check_lex_normal(".hidden", ".hidden"_pv);
    check_lex_normal(".hidden/a", ".hidden/a"_pv);

    check_lex_normal("./a", "a"_pv);
    check_lex_normal("a/./b/..", "a/"_pv);
    check_lex_normal("a/./b/../", "a/"_pv);
    check_lex_normal("/usr//////lib", "/usr/lib"_pv);

    check_lex_normal("a/..", "."_pv);
    check_lex_normal("/a/../b/", "/b/"_pv);
    check_lex_normal("../a", "../a"_pv);
    check_lex_normal("../a/", "../a/"_pv);
    check_lex_normal("../a////", "../a/"_pv);

    check_lex_normal("../.a", "../.a"_pv);
    check_lex_normal("../..a", "../..a"_pv);
    check_lex_normal("../...a", "../...a"_pv);
    check_lex_normal("../..a/", "../..a/"_pv);
    check_lex_normal("../...a/", "../...a/"_pv);
    check_lex_normal("../..a////", "../..a/"_pv);
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
