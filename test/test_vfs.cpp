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


    {
        auto vfss = vfs.read("/data/text/example.txt"_pv);

        int v1 = 0, v2 = 0;
        vfss >> v1 >> v2;
        EXPECT_EQ(v1, 123);
        EXPECT_EQ(v2, 456);
    }

    vfs.mount_string_constant("/data/text/example.txt"_pv, std::string("1013"));
    EXPECT_TRUE(vfs.exists("/data/text/example.txt"_pv));
    EXPECT_FALSE(vfs.is_directory("/data/text/example.txt"_pv));

    {
        auto vfss = vfs.read("/data/text/example.txt"_pv);

        int v = 0;
        vfss >> v;
        EXPECT_EQ(v, 1013);
    }

    try
    {
        (void)vfs.read("/data/not/found"_pv);
    }
    catch(const lochfolk::virtual_file_system::error& e)
    {
        EXPECT_STREQ(e.what(), "\"/data/not/found\" is not found");
    }
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


    {
        auto vfss = vfs.read("/text/example.txt"_pv);

        int date = 0;
        vfss >> date;
        EXPECT_EQ(date, 1013);
    }

    {
        auto vfss = vfs.read("/text/example.txt"_pv);

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
        auto vfss = vfs.read("/data/a.txt"_pv);

        std::string str;
        vfss >> str;

        EXPECT_EQ(str, "AAA");
    }

    {
        auto vfss = vfs.read("/data/nested/b.txt"_pv);

        std::string str;
        vfss >> str;

        EXPECT_EQ(str, "BBB");
    }

    {
        auto vfss = vfs.read("/data/example.txt"_pv);

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
        auto vfss = vfs.read("/archive/info.txt"_pv);

        std::string str;
        vfss >> str;

        EXPECT_EQ(str, "archive");
    }

    {
        auto vfss = vfs.read("/archive/data/value.txt"_pv);

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
