#include <gtest/gtest.h>
#include <lochfolk/path.hpp>

TEST(path, constructor)
{
    {
        lochfolk::path p;
        EXPECT_TRUE(p.empty());
    }

    {
        lochfolk::path p = "/data/a.txt";
        EXPECT_EQ(p.string(), "/data/a.txt");
    }

    {
        lochfolk::path p = "audio/a.wav";
        EXPECT_EQ(p.string(), "audio/a.wav");
    }

    {
        lochfolk::path p = "data";
        EXPECT_EQ(p.string(), "data");
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

// TEST(path, split_view)
// {
//     using namespace lochfolk::vfs_literals;

//     {
//         lochfolk::path_view p = "/data/text/example.txt"_pv;
//         auto view = p.split_view();

//         std::vector<std::string> strs(view.begin(), view.end());
//         EXPECT_EQ(strs.size(), 3);
//         EXPECT_EQ(strs[0], "data");
//         EXPECT_EQ(strs[1], "text");
//         EXPECT_EQ(strs[2], "example.txt");
//     }

//     {
//         lochfolk::path p = "/data/text/example.txt";
//         auto view = p.split_view();

//         std::vector<std::string> strs(view.begin(), view.end());
//         EXPECT_EQ(strs.size(), 3);
//         EXPECT_EQ(strs[0], "data");
//         EXPECT_EQ(strs[1], "text");
//         EXPECT_EQ(strs[2], "example.txt");
//     }
// }

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
