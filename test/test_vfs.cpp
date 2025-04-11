#include <gtest/gtest.h>
#include <lochfolk/vfs.hpp>

TEST(vfs, mount_string_constant)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    vfs.mount_string_constant("/data/text/example.txt"_pv, "123 456");
    vfs.list_files(std::cerr);

    EXPECT_TRUE(vfs.exists("/"_pv));
    EXPECT_TRUE(vfs.is_directory("/"_pv));
    EXPECT_TRUE(vfs.exists("/data"_pv));
    EXPECT_TRUE(vfs.is_directory("/data"_pv));
    EXPECT_TRUE(vfs.exists("/data/text"_pv));
    EXPECT_TRUE(vfs.is_directory("/data/text"_pv));
    EXPECT_TRUE(vfs.exists("/data/text/example.txt"_pv));
    EXPECT_FALSE(vfs.is_directory("/data/text/example.txt"_pv));

    EXPECT_EQ(vfs.file_size("/data/text/example.txt"_pv), 7);

    {
        auto vfss = vfs.open("/data/text/example.txt"_pv);

        int v1 = 0, v2 = 0;
        vfss >> v1 >> v2;
        EXPECT_EQ(v1, 123);
        EXPECT_EQ(v2, 456);
    }

    vfs.mount_string_constant("/data/text/example.txt"_pv, std::string("1013"));
    EXPECT_TRUE(vfs.exists("/data/text/example.txt"_pv));
    EXPECT_FALSE(vfs.is_directory("/data/text/example.txt"_pv));

    {
        auto vfss = vfs.open("/data/text/example.txt"_pv);

        int v = 0;
        vfss >> v;
        EXPECT_EQ(v, 1013);
    }

    // Test move constructor
    {
        auto src = vfs.open("/data/text/example.txt"_pv);
        auto vfss = std::move(src);
        EXPECT_EQ(src.rdbuf(), nullptr);

        int v = 0;
        vfss >> v;
        EXPECT_EQ(v, 1013);
    }

    try
    {
        (void)vfs.open("/data/not/found"_pv);
    }
    catch(const lochfolk::virtual_file_system::error& e)
    {
        EXPECT_STREQ(e.what(), "\"/data/not/found\" is not found");
    }

    EXPECT_TRUE(vfs.remove("/data/text/example.txt"_pv));
    EXPECT_FALSE(vfs.remove("/data/text/example.txt"_pv));
    EXPECT_FALSE(vfs.exists("/data/text/example.txt"_pv));
}

TEST(vfs, mount_sys_file)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    vfs.mount_sys_file("/text/example.txt"_pv, "test_vfs_data/example.txt");
    vfs.list_files(std::cerr);

    EXPECT_TRUE(vfs.exists("/"_pv));
    EXPECT_TRUE(vfs.is_directory("/"_pv));
    EXPECT_TRUE(vfs.exists("/text"_pv));
    EXPECT_TRUE(vfs.is_directory("/text"_pv));
    EXPECT_TRUE(vfs.exists("/text/example.txt"_pv));
    EXPECT_FALSE(vfs.is_directory("/text/example.txt"_pv));

    EXPECT_EQ(
        vfs.file_size("/text/example.txt"_pv),
        static_cast<std::uint64_t>(std::filesystem::file_size("test_vfs_data/example.txt"))
    );

    {
        auto vfss = vfs.open("/text/example.txt"_pv);

        int date = 0;
        vfss >> date;
        EXPECT_EQ(date, 1013);
    }

    {
        auto vfss = vfs.open("/text/example.txt"_pv);

        char buf[4];
        vfss.read(buf, 4);
        EXPECT_EQ(std::string_view(buf, 4), "1013");
    }
}

TEST(vfs, mount_sys_dir)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    vfs.mount_sys_dir("/data"_pv, "test_vfs_data/dir/");
    vfs.mount_sys_file("/data/example.txt"_pv, "test_vfs_data/example.txt");
    vfs.list_files(std::cerr);

    EXPECT_TRUE(vfs.is_directory("/data/nested"_pv));

    {
        auto vfss = vfs.open("/data/a.txt"_pv);

        std::string str;
        vfss >> str;

        EXPECT_EQ(str, "AAA");
    }

    {
        auto vfss = vfs.open("/data/nested/b.txt"_pv);

        std::string str;
        vfss >> str;

        EXPECT_EQ(str, "BBB");
    }

    {
        auto vfss = vfs.open("/data/example.txt"_pv);

        int date = 0;
        vfss >> date;
        EXPECT_EQ(date, 1013);
    }
}

TEST(vfs, mount_zip_archive)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    vfs.mount_zip_archive("/archive"_pv, "test_vfs_data/ar.zip");
    vfs.list_files(std::cerr);

    EXPECT_TRUE(vfs.is_directory("/archive"_pv));

    {
        auto vfss = vfs.open("/archive/info.txt"_pv);

        std::string str;
        vfss >> str;

        EXPECT_EQ(str, "archive");
    }

    {
        auto vfss = vfs.open("/archive/data/value.txt"_pv);

        int v1 = 0, v2 = 0;
        vfss >> v1 >> v2;
        EXPECT_EQ(v1, 182375);
        EXPECT_EQ(v2, 182376);
    }
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
