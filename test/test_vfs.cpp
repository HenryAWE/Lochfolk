#include <gtest/gtest.h>
#include <lochfolk/lochfolk.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace
{
std::string read_all(lochfolk::file_handle& fh, std::size_t chunk = 7)
{
    std::string result;
    std::vector<std::byte> buf(chunk);

    for(;;)
    {
        const lochfolk::io_result n = fh.read(std::span<std::byte>(buf));
        if(n.is_npos())
            throw std::runtime_error("lochfolk test: read failed");

        const auto size = static_cast<std::size_t>(n.bytes_transferred());
        if(size == 0)
            break;

        result.append(reinterpret_cast<const char*>(buf.data()), size);
    }

    return result;
}

std::string read_all(
    lochfolk::virtual_file_system& vfs,
    lochfolk::path_view p,
    lochfolk::file_flag flags = lochfolk::file_flag::readable
)
{
    auto fh = vfs.fopen(p, flags);
    return read_all(*fh);
}
} // namespace

TEST(vfs, mount_string_constant)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    vfs.mount_string("/data/text/example.txt"_pv, "123 456");
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
        auto fh = vfs.fopen("/data/text/example.txt"_pv);

        EXPECT_TRUE(fh->readable());
        EXPECT_FALSE(fh->writable());
        EXPECT_EQ(fh->file_size(), 7);
        EXPECT_EQ(read_all(*fh), "123 456");
    }

    {
        std::string str = vfs.read_string("/data/text/example.txt"_pv);

        EXPECT_EQ(str, "123 456");
    }

    // Random access
    {
        auto fh = vfs.fopen("/data/text/example.txt"_pv);

        std::byte buf[3];
        EXPECT_EQ(fh->read(std::span<std::byte>(buf)).bytes_transferred(), 3);
        EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf), 3), "123");

        EXPECT_TRUE(fh->seek(4, lochfolk::whence::begin));
        EXPECT_EQ(fh->read(std::span<std::byte>(buf)).bytes_transferred(), 3);
        EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf), 3), "456");

        EXPECT_TRUE(fh->seek(-3, lochfolk::whence::current));
        EXPECT_EQ(fh->read(std::span<std::byte>(buf)).bytes_transferred(), 3);
        EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf), 3), "456");

        EXPECT_TRUE(fh->seek(0, lochfolk::whence::end));
        EXPECT_EQ(fh->read(std::span<std::byte>(buf)).bytes_transferred(), 0);

        EXPECT_FALSE(fh->seek(-8, lochfolk::whence::begin));
    }

    vfs.mount_string("/data/text/example.txt"_pv, std::string("1013"));
    EXPECT_TRUE(vfs.exists("/data/text/example.txt"_pv));
    EXPECT_FALSE(vfs.is_directory("/data/text/example.txt"_pv));

    EXPECT_EQ(read_all(vfs, "/data/text/example.txt"_pv), "1013");

    // An open handle keeps a snapshot of the mounted contents
    {
        vfs.mount_string("/data/text/example.txt"_pv, std::string(64, 'A'));
        auto fh = vfs.fopen("/data/text/example.txt"_pv);

        vfs.mount_string("/data/text/example.txt"_pv, std::string(64, 'B'));

        EXPECT_EQ(read_all(*fh), std::string(64, 'A'));
    }

    try
    {
        (void)vfs.fopen("/data/not/found"_pv);
        FAIL() << "fopen() of a missing path should throw";
    }
    catch(const lochfolk::virtual_file_system::error& e)
    {
        EXPECT_STREQ(e.what(), "\"/data/not/found\" is not found");
    }

    EXPECT_TRUE(vfs.remove("/data/text/example.txt"_pv));
    EXPECT_FALSE(vfs.remove("/data/text/example.txt"_pv));
    EXPECT_FALSE(vfs.exists("/data/text/example.txt"_pv));
    EXPECT_THROW(vfs.fopen("/data/text/example.txt"_pv), lochfolk::virtual_file_system::error);
}

TEST(vfs, mount_sys_file)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    vfs.mount_file("/text/example.txt"_pv, "test_vfs_data/example.txt");
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
        auto fh = vfs.fopen("/text/example.txt"_pv);

        EXPECT_TRUE(fh->readable());
        EXPECT_FALSE(fh->writable());
        EXPECT_EQ(read_all(*fh), "1013\n");
    }

    {
        std::string str = vfs.read_string("/text/example.txt"_pv);

        EXPECT_EQ(str, "1013\n");
    }

    EXPECT_TRUE(vfs.remove("/text/example.txt"_pv));
    EXPECT_FALSE(vfs.exists("/text/example.txt"_pv));
    // Won't remove the actual system file
    EXPECT_TRUE(std::filesystem::exists("test_vfs_data/example.txt"));
}

TEST(vfs, mount_sys_dir)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    vfs.mount_dir("/data"_pv, "test_vfs_data/dir/");
    vfs.mount_file("/data/example.txt"_pv, "test_vfs_data/example.txt");
    vfs.list_files(std::cerr);

    EXPECT_TRUE(vfs.is_directory("/data/nested"_pv));

    EXPECT_EQ(read_all(vfs, "/data/a.txt"_pv), "AAA\n");
    EXPECT_EQ(read_all(vfs, "/data/nested/b.txt"_pv), "BBB\n");
    EXPECT_EQ(vfs.read_string("/data/nested/b.txt"_pv), "BBB\n");
    EXPECT_EQ(read_all(vfs, "/data/example.txt"_pv), "1013\n");
}

TEST(vfs, mount_zip_archive)
{
    if(!std::filesystem::exists("test_vfs_data/ar.zip"))
        GTEST_SKIP() << "test_vfs_data/ar.zip not found";

    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    vfs.mount_archive("/archive"_pv, "test_vfs_data/ar.zip");
    vfs.list_files(std::cerr);

    EXPECT_TRUE(vfs.is_directory("/archive"_pv));

    EXPECT_EQ(read_all(vfs, "/archive/info.txt"_pv), "archive\n");
    EXPECT_EQ(vfs.read_string("/archive/info.txt"_pv), "archive\n");
    EXPECT_EQ(read_all(vfs, "/archive/data/value.txt"_pv), "182375 182376\n");

    EXPECT_TRUE(vfs.exists("/archive/info.txt"_pv));
    EXPECT_TRUE(vfs.exists("/archive/data/value.txt"_pv));
    EXPECT_TRUE(vfs.remove("/archive"_pv));
    EXPECT_FALSE(vfs.exists("/archive/info.txt"_pv));
    EXPECT_FALSE(vfs.exists("/archive/data/value.txt"_pv));
}

TEST(vfs, remove_missing)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;
    vfs.mount_string("/data/file.txt"_pv, "content");

    EXPECT_FALSE(vfs.remove("/missing/file.txt"_pv));
    EXPECT_FALSE(vfs.remove("/data/missing.txt"_pv));
    EXPECT_FALSE(vfs.remove("/data/file.txt/child"_pv));
    EXPECT_TRUE(vfs.exists("/data/file.txt"_pv));

    EXPECT_TRUE(vfs.remove("/data/file.txt//"_pv));
    EXPECT_FALSE(vfs.exists("/data/file.txt"_pv));
}

TEST(vfs, mount_empty_dir)
{
    namespace stdfs = std::filesystem;
    using namespace lochfolk::vfs_literals;

    stdfs::path empty_dir = "test_vfs_data/empty_dir_for_mount_test";
    std::error_code ec;
    stdfs::remove_all(empty_dir, ec);
    ASSERT_TRUE(stdfs::create_directories(empty_dir));

    {
        lochfolk::virtual_file_system vfs;
        vfs.mount_dir("/empty"_pv, empty_dir);

        EXPECT_TRUE(vfs.exists("/empty"_pv));
        EXPECT_TRUE(vfs.is_directory("/empty"_pv));
    }

    stdfs::remove_all(empty_dir, ec);
}

TEST(vfs, fopen_errors)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;
    vfs.mount_string("/dir/file.txt"_pv, "x");

    EXPECT_THROW(vfs.fopen("/missing"_pv), lochfolk::virtual_file_system::error);
    EXPECT_THROW(vfs.fopen("/dir"_pv), lochfolk::virtual_file_system::error);
    EXPECT_THROW(
        vfs.fopen("/dir/file.txt"_pv, lochfolk::file_flag::unknown),
        std::invalid_argument
    );
    EXPECT_THROW((void)vfs.read_string("/dir"_pv), lochfolk::virtual_file_system::error);
}

TEST(vfs, fopen_missing_system_file)
{
    namespace stdfs = std::filesystem;
    using namespace lochfolk::vfs_literals;

    const stdfs::path gone = "test_vfs_data/gone_for_fopen_test.txt";
    {
        std::ofstream ofs(gone, std::ios::binary);
        ofs << "hollow";
    }

    lochfolk::virtual_file_system vfs;
    vfs.mount_file("/gone.txt"_pv, gone);
    stdfs::remove(gone);

    EXPECT_THROW(vfs.fopen("/gone.txt"_pv), std::system_error);
}

TEST(vfs, read_only_entries_ignore_writable)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;
    vfs.mount_string("/s.txt"_pv, "abc");

    const std::byte data[] = {std::byte{'Z'}};

    {
        auto fh = vfs.fopen("/s.txt"_pv, lochfolk::file_flag::read_write);
        EXPECT_FALSE(fh->writable());
        EXPECT_TRUE(fh->write(std::span<const std::byte>(data)).is_npos());
    }

    EXPECT_EQ(vfs.read_string("/s.txt"_pv), "abc");

    if(std::filesystem::exists("test_vfs_data/ar.zip"))
    {
        vfs.mount_archive("/archive"_pv, "test_vfs_data/ar.zip");

        auto fh = vfs.fopen("/archive/info.txt"_pv, lochfolk::file_flag::read_write);
        EXPECT_FALSE(fh->writable());
        EXPECT_TRUE(fh->write(std::span<const std::byte>(data)).is_npos());
        EXPECT_EQ(vfs.read_string("/archive/info.txt"_pv), "archive\n");
    }
}

TEST(vfs, writable_system_file)
{
    namespace stdfs = std::filesystem;
    using namespace lochfolk::vfs_literals;

    const stdfs::path file = "test_vfs_data/writable_for_fopen_test.txt";
    {
        std::ofstream ofs(file, std::ios::binary);
        ofs << "hello";
    }

    lochfolk::virtual_file_system vfs;
    vfs.mount_file("/w.txt"_pv, file);

    {
        auto fh = vfs.fopen("/w.txt"_pv, lochfolk::file_flag::read_write);

        EXPECT_TRUE(fh->readable());
        EXPECT_TRUE(fh->writable());
        EXPECT_EQ(fh->file_size(), 5);
        EXPECT_EQ(read_all(*fh), "hello");

        EXPECT_TRUE(fh->seek(0, lochfolk::whence::end));

        const char extra[] = " world";
        const auto written = fh->write(
            std::as_bytes(std::span<const char>(extra, 6))
        );
        EXPECT_EQ(written.bytes_transferred(), 6);
    }

    EXPECT_EQ(vfs.file_size("/w.txt"_pv), 11);
    EXPECT_EQ(read_all(vfs, "/w.txt"_pv), "hello world");

    // A write-only handle cannot read
    {
        auto fh = vfs.fopen("/w.txt"_pv, lochfolk::file_flag::writable);

        EXPECT_FALSE(fh->readable());
        EXPECT_TRUE(fh->writable());

        std::byte buf[4];
        EXPECT_TRUE(fh->read(std::span<std::byte>(buf)).is_npos());
    }

    stdfs::remove(file);
}

TEST(vfs, convert_crlf_string)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;
    vfs.mount_string("/crlf.txt"_pv, "a\r\nb\r\nc");

    EXPECT_EQ(vfs.read_string("/crlf.txt"_pv), "a\nb\nc");
    EXPECT_EQ(vfs.read_string("/crlf.txt"_pv, false), "a\r\nb\r\nc");

    {
        auto fh = vfs.fopen("/crlf.txt"_pv, lochfolk::file_flag::readable, true);
        EXPECT_EQ(fh->file_size(), 7); // raw size
        EXPECT_EQ(read_all(*fh), "a\nb\nc");
    }

    {
        auto fh = vfs.fopen("/crlf.txt"_pv, lochfolk::file_flag::readable, false);
        EXPECT_EQ(read_all(*fh), "a\r\nb\r\nc");
    }

    // A CR at the end of an internal chunk must be carried over
    {
        std::string padded(4095, 'x');
        padded += "\r\n";
        padded += "tail";
        vfs.mount_string("/edge.txt"_pv, padded);

        std::string expected(4095, 'x');
        expected += "\ntail";

        EXPECT_EQ(vfs.read_string("/edge.txt"_pv), expected);
    }

    // A lone CR is not translated
    {
        vfs.mount_string("/lone.txt"_pv, "x\ry");
        EXPECT_EQ(vfs.read_string("/lone.txt"_pv), "x\ry");
    }
}

TEST(vfs, convert_crlf_edge_cases)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    // A trailing lone CR at end of file is preserved
    vfs.mount_string("/trail.txt"_pv, "abc\r");
    EXPECT_EQ(vfs.read_string("/trail.txt"_pv), "abc\r");

    // Empty content
    vfs.mount_string("/empty.txt"_pv, "");
    EXPECT_EQ(vfs.read_string("/empty.txt"_pv), "");

    // CR at a chunk boundary followed by something that is not LF
    {
        std::string content(4095, 'x');
        content += "\r";
        content += "y";
        vfs.mount_string("/split.txt"_pv, content);
        EXPECT_EQ(vfs.read_string("/split.txt"_pv), content);
    }

    // A chunk consisting only of a lone CR must not look like end of file
    {
        std::string content(4096, 'x');
        content += "\r";
        vfs.mount_string("/tail_cr.txt"_pv, content);
        EXPECT_EQ(vfs.read_string("/tail_cr.txt"_pv), content);
    }

    // Seeking rewinds the translation state
    {
        vfs.mount_string("/rewind.txt"_pv, "p\r\nq\r\n");

        auto fh = vfs.fopen("/rewind.txt"_pv, lochfolk::file_flag::readable, true);
        const std::string first = read_all(*fh, 1);
        EXPECT_EQ(first, "p\nq\n");

        EXPECT_TRUE(fh->seek(0, lochfolk::whence::begin));
        const std::string second = read_all(*fh, 2);
        EXPECT_EQ(second, "p\nq\n");
    }

    // Seeking past the end is allowed and reads return EOF
    {
        auto fh = vfs.fopen("/trail.txt"_pv, lochfolk::file_flag::readable, true);
        EXPECT_TRUE(fh->seek(1, lochfolk::whence::end));

        std::byte buf[4];
        EXPECT_EQ(fh->read(std::span<std::byte>(buf)).bytes_transferred(), 0u);
    }
}

TEST(vfs, convert_crlf_system_file)
{
    namespace stdfs = std::filesystem;
    using namespace lochfolk::vfs_literals;

    const stdfs::path file = "test_vfs_data/crlf_for_fopen_test.txt";
    {
        std::ofstream ofs(file, std::ios::binary);
        ofs << "1\r\n2\r\n";
    }

    lochfolk::virtual_file_system vfs;
    vfs.mount_file("/crlf.txt"_pv, file);

    EXPECT_EQ(vfs.read_string("/crlf.txt"_pv), "1\n2\n");
    EXPECT_EQ(vfs.read_string("/crlf.txt"_pv, false), "1\r\n2\r\n");

    {
        auto fh = vfs.fopen("/crlf.txt"_pv, lochfolk::file_flag::readable, true);
        EXPECT_EQ(read_all(*fh), "1\n2\n");
    }

    stdfs::remove(file);
}

TEST(vfs, convert_crlf_archive)
{
    using namespace lochfolk::vfs_literals;

    if(!std::filesystem::exists("test_vfs_data/ar.zip"))
        GTEST_SKIP() << "test_vfs_data/ar.zip not found";

    lochfolk::virtual_file_system vfs;
    vfs.mount_archive("/archive"_pv, "test_vfs_data/ar.zip");

    ASSERT_TRUE(vfs.exists("/archive/crlf.txt"_pv));
    EXPECT_EQ(vfs.read_string("/archive/crlf.txt"_pv), "line1\nline2\n");
    EXPECT_EQ(vfs.read_string("/archive/crlf.txt"_pv, false), "line1\r\nline2\r\n");
}

TEST(vfs, large_content)
{
    using namespace lochfolk::vfs_literals;

    std::string big;
    big.reserve(10000);
    for(int i = 0; i < 10000; ++i)
        big.push_back(static_cast<char>('a' + (i % 26)));

    lochfolk::virtual_file_system vfs;
    vfs.mount_string("/big.txt"_pv, big);

    EXPECT_EQ(vfs.read_string("/big.txt"_pv), big);

    {
        auto fh = vfs.fopen("/big.txt"_pv);
        EXPECT_EQ(read_all(*fh), big);
    }

    {
        auto fh = vfs.fopen("/big.txt"_pv);
        std::vector<std::byte> buf(4096);

        EXPECT_TRUE(fh->seek(9000, lochfolk::whence::begin));
        const auto n = fh->read(std::span<std::byte>(buf));
        ASSERT_FALSE(n.is_npos());
        ASSERT_EQ(n.bytes_transferred(), 1000u);
        EXPECT_EQ(
            std::string(reinterpret_cast<const char*>(buf.data()), 1000),
            big.substr(9000)
        );
    }
}

TEST(vfs, access_context)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;
    vfs.mount_string("/data/strings/str.txt"_pv, "str");
    vfs.mount_string("/info/info.txt"_pv, "1013");

    lochfolk::access_context ctx(vfs);
    EXPECT_EQ(&ctx.get_vfs(), &vfs);

    EXPECT_EQ(ctx.current_path(), "/"_pv);

    ctx.current_path("/data"_pv);
    EXPECT_EQ(ctx.current_path(), "/data"_pv);

    {
        std::string str = ctx.read_string("strings/str.txt"_pv);

        EXPECT_EQ(str, "str");
    }

    EXPECT_EQ(ctx.to_fullpath("strings"_pv), "/data/strings"_pv);
    EXPECT_EQ(ctx.to_fullpath("../info/info.txt"_pv), "/info/info.txt"_pv);

    EXPECT_TRUE(ctx.is_directory("strings"_pv));
    EXPECT_FALSE(ctx.is_directory("strings/str.txt"_pv));

    EXPECT_TRUE(ctx.exists("../info/info.txt"_pv));
    EXPECT_EQ(ctx.file_size("../info/info.txt"_pv), 4);
    EXPECT_FALSE(ctx.exists("../info/str.txt"_pv));

    {
        auto fh = ctx.fopen("../info/info.txt"_pv);
        EXPECT_EQ(read_all(*fh), "1013");
    }

    {
        std::string str = ctx.read_string("../info/info.txt"_pv);

        EXPECT_EQ(str, "1013");
    }

    EXPECT_TRUE(ctx.remove("../info/info.txt"_pv));
    EXPECT_FALSE(ctx.exists("../info/info.txt"_pv));
    EXPECT_FALSE(ctx.remove("../info/info.txt"_pv));

    ctx.current_path("strings"_pv);
    EXPECT_EQ(ctx.current_path(), "/data/strings"_pv);
    EXPECT_TRUE(ctx.exists("str.txt"_pv));

    {
        std::string str = ctx.read_string("str.txt"_pv);

        EXPECT_EQ(str, "str");
    }

    ctx.current_path(".."_pv);
    EXPECT_EQ(ctx.current_path(), "/data/"_pv);

    {
        std::string str = ctx.read_string("strings/str.txt"_pv);

        EXPECT_EQ(str, "str");
    }

    EXPECT_TRUE(ctx.exists("/data/strings/str.txt"_pv));
    EXPECT_TRUE(ctx.remove("/data"_pv));
    EXPECT_FALSE(ctx.exists("/data/strings/str.txt"_pv));
}

TEST(vfs, access_context_archive)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;
    vfs.mount_archive("/archive"_pv, "test_vfs_data/ar.zip");
    vfs.list_files(std::cerr);

    lochfolk::access_context ctx(vfs);
    ctx.current_path("archive"_pv);
    EXPECT_EQ(ctx.current_path(), "/archive"_pv);

    EXPECT_EQ(read_all(*ctx.fopen("/archive/info.txt"_pv)), "archive\n");
    EXPECT_EQ(read_all(*ctx.fopen("info.txt"_pv)), "archive\n");

    EXPECT_EQ(ctx.read_string("info.txt"_pv), "archive\n");
    EXPECT_EQ(ctx.read_string("/archive/info.txt"_pv), "archive\n");

    EXPECT_EQ(read_all(*ctx.fopen("/archive/data/value.txt"_pv)), "182375 182376\n");
    EXPECT_EQ(read_all(*ctx.fopen("data/value.txt"_pv)), "182375 182376\n");

    EXPECT_TRUE(ctx.exists("/archive/info.txt"_pv));
    EXPECT_TRUE(ctx.exists("info.txt"_pv));
    EXPECT_TRUE(ctx.exists("/archive/data/value.txt"_pv));
    EXPECT_TRUE(ctx.exists("data/value.txt"_pv));

    EXPECT_TRUE(ctx.remove("data/"_pv));
    EXPECT_TRUE(ctx.exists("info.txt"_pv));
    EXPECT_FALSE(ctx.exists("/archive/data/value.txt"_pv));
    EXPECT_FALSE(ctx.exists("data/value.txt"_pv));
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
