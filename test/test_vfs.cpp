#include <gtest/gtest.h>
#include <lochfolk/lochfolk.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
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

class failing_handle final : public lochfolk::file_handle
{
public:
    [[nodiscard]]
    lochfolk::file_flag get_flags() const override
    {
        return lochfolk::file_flag::readable;
    }

    [[nodiscard]]
    std::uint64_t file_size() const override
    {
        return 0;
    }

    bool seek(std::int64_t, lochfolk::whence) override
    {
        return false;
    }

    lochfolk::io_result read(std::span<std::byte>) override
    {
        return lochfolk::io_result();
    }

    lochfolk::io_result write(std::span<const std::byte>) override
    {
        return lochfolk::io_result();
    }
};

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

TEST(VFS, MountStringConstant)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    vfs.mount_string("/data/text/example.txt", "123 456");
    vfs.list_files(std::cerr);

    EXPECT_TRUE(vfs.exists("/"));
    EXPECT_TRUE(vfs.is_directory("/"));
    EXPECT_TRUE(vfs.exists("/data"));
    EXPECT_TRUE(vfs.is_directory("/data"));
    EXPECT_TRUE(vfs.exists("/data/text"));
    EXPECT_TRUE(vfs.is_directory("/data/text"));
    EXPECT_TRUE(vfs.exists("/data/text/example.txt"));
    EXPECT_FALSE(vfs.is_directory("/data/text/example.txt"));

    EXPECT_EQ(vfs.file_size("/data/text/example.txt"), 7);

    {
        auto fh = vfs.fopen("/data/text/example.txt");

        EXPECT_TRUE(fh->readable());
        EXPECT_FALSE(fh->writable());
        EXPECT_EQ(fh->file_size(), 7);
        EXPECT_EQ(read_all(*fh), "123 456");
    }

    {
        std::string str = vfs.read_string("/data/text/example.txt");

        EXPECT_EQ(str, "123 456");
    }

    // Random access
    {
        auto fh = vfs.fopen("/data/text/example.txt");

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

    vfs.mount_string("/data/text/example.txt", std::string("1013"));
    EXPECT_TRUE(vfs.exists("/data/text/example.txt"));
    EXPECT_FALSE(vfs.is_directory("/data/text/example.txt"));

    EXPECT_EQ(read_all(vfs, "/data/text/example.txt"), "1013");

    {
        auto vfss = vfs.open("/data/text/example.txt");

        int v = 0;
        vfss >> v;
        EXPECT_EQ(v, 1013);
    }

    // Move construction of the stream
    {
        auto src = vfs.open("/data/text/example.txt");
        EXPECT_TRUE(src.has_buffer());

        auto vfss = std::move(src);
        EXPECT_FALSE(src.has_buffer());
        EXPECT_EQ(src.rdbuf(), nullptr);

        int v = 0;
        vfss >> v;
        EXPECT_EQ(v, 1013);
    }

    // An open handle keeps a snapshot of the mounted contents
    {
        vfs.mount_string("/data/text/example.txt", std::string(64, 'A'));
        auto fh = vfs.fopen("/data/text/example.txt");

        vfs.mount_string("/data/text/example.txt", std::string(64, 'B'));

        EXPECT_EQ(read_all(*fh), std::string(64, 'A'));
    }

    try
    {
        (void)vfs.fopen("/data/not/found");
        FAIL() << "fopen() of a missing path should throw";
    }
    catch(const lochfolk::virtual_file_system::error& e)
    {
        EXPECT_STREQ(e.what(), "\"/data/not/found\" is not found");
    }

    EXPECT_TRUE(vfs.remove("/data/text/example.txt"));
    EXPECT_FALSE(vfs.remove("/data/text/example.txt"));
    EXPECT_FALSE(vfs.exists("/data/text/example.txt"));
    EXPECT_THROW(vfs.fopen("/data/text/example.txt"), lochfolk::virtual_file_system::error);
}

TEST(VFS, MountStringCopiesSource)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    // Every source is modified and destroyed before the entries are read, so
    // storing views instead of copies would be a use-after-free here.
    {
        char local[64] = "local buffer";
        vfs.mount_string("/local.txt", local);
        local[0] = 'X';
    }

    {
        std::string source = "a source string long enough to be heap allocated";

        vfs.mount_string("/view.txt", std::string_view(source));
        source.assign("changed after mounting");
    }

    {
        std::string source = "moved source string";
        vfs.mount_string("/moved.txt", std::move(source));
    }

    EXPECT_EQ(vfs.read_string("/local.txt"), "local buffer");
    EXPECT_EQ(
        vfs.read_string("/view.txt"),
        "a source string long enough to be heap allocated"
    );
    EXPECT_EQ(vfs.read_string("/moved.txt"), "moved source string");
}

TEST(VFS, MountStringViewKeepsSource)
{
    using namespace lochfolk::vfs_literals;

    std::string storage = "view content";
    lochfolk::virtual_file_system vfs;

    vfs.mount_string_view("/view.txt", storage);

    EXPECT_EQ(vfs.file_size("/view.txt"), 12);
    EXPECT_EQ(vfs.read_string("/view.txt"), "view content");

    // No copy is made: an in-place change of the source is visible.
    storage[0] = 'V';
    EXPECT_EQ(vfs.read_string("/view.txt"), "View content");

    // Overwrite semantics are the same as for mount_string().
    vfs.mount_string_view("/view.txt", std::string_view("replacement"), false);
    EXPECT_EQ(vfs.read_string("/view.txt"), "View content");

    vfs.mount_string_view("/view.txt", std::string_view("replacement"));
    EXPECT_EQ(vfs.read_string("/view.txt"), "replacement");

    // A string literal has static storage duration, so a view of it is safe.
    vfs.mount_string_view("/literal.txt", "literal");
    EXPECT_EQ(vfs.read_string("/literal.txt"), "literal");
    EXPECT_EQ(vfs.file_size("/literal.txt"), 7);

    // The const char* overload stores a view too, not a copy.
    char buffer[16] = "char buffer";
    vfs.mount_string_view("/buffer.txt", buffer);
    buffer[0] = 'C';
    EXPECT_EQ(vfs.read_string("/buffer.txt"), "Char buffer");

    // Null pointers are rejected instead of invoking UB in strlen().
    EXPECT_THROW(
        vfs.mount_string_view(
            "/null.txt", static_cast<const char*>(nullptr)
        ),
        std::invalid_argument
    );
    EXPECT_THROW(
        vfs.mount_string("/null.txt", static_cast<const char*>(nullptr)),
        std::invalid_argument
    );
}

TEST(VFS, LexicalPathResolution)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;
    vfs.mount_string("/a/b.txt", "content");
    vfs.mount_string("/a/c.txt", "c");

    // Paths are normalized lexically before the tree is walked, exactly like
    // std::filesystem::path::lexically_normal(). The VFS has no symbolic
    // links, so this resolution is exact - including ".." after a component
    // that does not exist (on the host file system the kernel would require
    // every component before ".." to exist).
    EXPECT_TRUE(vfs.exists("/a/b.txt"));
    EXPECT_TRUE(vfs.exists("/a/./b.txt"));
    EXPECT_TRUE(vfs.exists("/a/../a/b.txt"));
    EXPECT_TRUE(vfs.exists("/a/missing/../b.txt"));
    EXPECT_TRUE(vfs.exists("/a/b.txt/."));
    EXPECT_TRUE(vfs.exists("/a/b.txt/..")); // resolves to the directory "/a"
    EXPECT_TRUE(vfs.exists("/a/b.txt/../c.txt"));
    EXPECT_TRUE(vfs.is_directory("/a/./"));

    EXPECT_TRUE(vfs.exists("/."));
    EXPECT_TRUE(vfs.is_directory("/."));
    EXPECT_FALSE(vfs.exists("/a/../../b.txt")); // resolves to "/b.txt"
    EXPECT_FALSE(vfs.exists("/a/../b.txt"));    // resolves to "/b.txt"

    EXPECT_EQ(vfs.read_string("/a/./b.txt"), "content");
    EXPECT_EQ(vfs.read_string("/a/../a/b.txt"), "content");
    EXPECT_EQ(vfs.read_string("/a/missing/../b.txt"), "content");
    EXPECT_EQ(vfs.file_size("/a/./b.txt"), 7u);

    {
        auto fh = vfs.fopen("/a/../a/b.txt");
        EXPECT_EQ(read_all(*fh), "content");
    }
}

TEST(VFS, MountPathNormalization)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    // The mount point is normalized before it is created, so a non-normalized
    // spelling cannot produce an empty-named or literal "."/".." node.
    vfs.mount_string("/mnt/../root.txt", "root");
    EXPECT_TRUE(vfs.exists("/root.txt"));
    EXPECT_FALSE(vfs.exists("/mnt"));

    vfs.mount_string("/a/./b.txt", "b");
    EXPECT_TRUE(vfs.exists("/a/b.txt"));
    EXPECT_EQ(vfs.read_string("/a/./b.txt"), "b");

    // A trailing separator names the same file.
    vfs.mount_string("/trailing/", "t");
    EXPECT_TRUE(vfs.exists("/trailing"));
    EXPECT_FALSE(vfs.is_directory("/trailing"));
    EXPECT_EQ(vfs.read_string("/trailing"), "t");

    std::ostringstream listing;
    vfs.list_files(listing);
    EXPECT_EQ(listing.str().find("  - /"), std::string::npos) << listing.str();

    // Relative paths and mount points that do not name a file are rejected
    // instead of creating a malformed tree.
    EXPECT_THROW(
        vfs.mount_string("relative.txt", "x"),
        lochfolk::virtual_file_system::error
    );
    EXPECT_THROW(
        vfs.mount_string("/", "x"),
        lochfolk::virtual_file_system::error
    );
    EXPECT_THROW(
        vfs.mount_string("/a/..", "x"),
        lochfolk::virtual_file_system::error
    );

    std::ostringstream after;
    vfs.list_files(after);
    EXPECT_EQ(after.str().find("relative.txt"), std::string::npos) << after.str();
    EXPECT_EQ(after.str().find("  - /"), std::string::npos) << after.str();
}

TEST(VFS, MountSysFile)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    vfs.mount_file("/text/example.txt", "test_vfs_data/example.txt");
    vfs.list_files(std::cerr);

    EXPECT_TRUE(vfs.exists("/"));
    EXPECT_TRUE(vfs.is_directory("/"));
    EXPECT_TRUE(vfs.exists("/text"));
    EXPECT_TRUE(vfs.is_directory("/text"));
    EXPECT_TRUE(vfs.exists("/text/example.txt"));
    EXPECT_FALSE(vfs.is_directory("/text/example.txt"));

    EXPECT_EQ(
        vfs.file_size("/text/example.txt"),
        static_cast<std::uint64_t>(std::filesystem::file_size("test_vfs_data/example.txt"))
    );

    {
        auto fh = vfs.fopen("/text/example.txt");

        EXPECT_TRUE(fh->readable());
        EXPECT_FALSE(fh->writable());
        EXPECT_EQ(read_all(*fh), "1013\n");
    }

    {
        std::string str = vfs.read_string("/text/example.txt");

        EXPECT_EQ(str, "1013\n");
    }

    {
        auto vfss = vfs.open("/text/example.txt");

        int date = 0;
        vfss >> date;
        EXPECT_EQ(date, 1013);
    }

    {
        auto vfss = vfs.open("/text/example.txt");

        char buf[4];
        vfss.read(buf, 4);
        EXPECT_EQ(std::string(buf, 4), "1013");
    }

    EXPECT_TRUE(vfs.remove("/text/example.txt"));
    EXPECT_FALSE(vfs.exists("/text/example.txt"));
    // Won't remove the actual system file
    EXPECT_TRUE(std::filesystem::exists("test_vfs_data/example.txt"));
}

TEST(VFS, MountSysDir)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    vfs.mount_dir("/data", "test_vfs_data/dir/");
    vfs.mount_file("/data/example.txt", "test_vfs_data/example.txt");
    vfs.list_files(std::cerr);

    EXPECT_TRUE(vfs.is_directory("/data/nested"));

    EXPECT_EQ(read_all(vfs, "/data/a.txt"), "AAA\n");
    EXPECT_EQ(read_all(vfs, "/data/nested/b.txt"), "BBB\n");
    EXPECT_EQ(vfs.read_string("/data/nested/b.txt"), "BBB\n");
    EXPECT_EQ(read_all(vfs, "/data/example.txt"), "1013\n");

    {
        auto vfss = vfs.open("/data/a.txt");

        std::string str;
        vfss >> str;
        EXPECT_EQ(str, "AAA");
    }

    {
        auto vfss = vfs.open("/data/nested/b.txt");

        std::string str;
        vfss >> str;
        EXPECT_EQ(str, "BBB");
    }
}

TEST(VFS, MountDirNormalization)
{
    namespace stdfs = std::filesystem;
    using namespace lochfolk::vfs_literals;

    const stdfs::path dir = "test_vfs_data/dir";
    if(!stdfs::is_directory(dir))
        GTEST_SKIP() << "test_vfs_data/dir not found";

    lochfolk::virtual_file_system vfs;

    // The base path is normalized like every other path.
    vfs.mount_dir("/mnt/../data", dir);
    EXPECT_TRUE(vfs.is_directory("/data"));
    EXPECT_EQ(vfs.read_string("/data/a.txt"), "AAA\n");
    EXPECT_EQ(vfs.read_string("/data/nested/b.txt"), "BBB\n");
    EXPECT_FALSE(vfs.exists("/mnt"));

    // Mounting a directory tree at the root is allowed, and a non-normalized
    // spelling of the root is still the root.
    lochfolk::virtual_file_system root_vfs;
    root_vfs.mount_dir("/./", dir);
    EXPECT_EQ(root_vfs.read_string("/a.txt"), "AAA\n");
    EXPECT_EQ(root_vfs.read_string("/nested/b.txt"), "BBB\n");

    lochfolk::virtual_file_system bad_vfs;
    EXPECT_THROW(
        bad_vfs.mount_dir("relative", dir),
        lochfolk::virtual_file_system::error
    );
}

TEST(VFS, MountZIPArchive)
{
    if(!std::filesystem::exists("test_vfs_data/ar.zip"))
        GTEST_SKIP() << "test_vfs_data/ar.zip not found";

    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    vfs.mount_archive("/archive", "test_vfs_data/ar.zip");
    vfs.list_files(std::cerr);

    EXPECT_TRUE(vfs.is_directory("/archive"));

    EXPECT_EQ(read_all(vfs, "/archive/info.txt"), "archive\n");
    EXPECT_EQ(vfs.read_string("/archive/info.txt"), "archive\n");
    EXPECT_EQ(read_all(vfs, "/archive/data/value.txt"), "182375 182376\n");

    {
        auto vfss = vfs.open("/archive/info.txt");

        std::string str;
        vfss >> str;
        EXPECT_EQ(str, "archive");
    }

    {
        auto vfss = vfs.open("/archive/data/value.txt");

        int v1 = 0, v2 = 0;
        vfss >> v1 >> v2;
        EXPECT_EQ(v1, 182375);
        EXPECT_EQ(v2, 182376);
    }

    EXPECT_TRUE(vfs.exists("/archive/info.txt"));
    EXPECT_TRUE(vfs.exists("/archive/data/value.txt"));
    EXPECT_TRUE(vfs.remove("/archive"));
    EXPECT_FALSE(vfs.exists("/archive/info.txt"));
    EXPECT_FALSE(vfs.exists("/archive/data/value.txt"));
}

TEST(VFS, ReadEmptyEntryFromArchive)
{
    using namespace lochfolk::vfs_literals;

    if(!std::filesystem::exists("test_vfs_data/ar.zip"))
        GTEST_SKIP() << "test_vfs_data/ar.zip not found";

    lochfolk::virtual_file_system vfs;
    vfs.mount_archive("/archive", "test_vfs_data/ar.zip");

    ASSERT_TRUE(vfs.exists("/archive/empty.txt"));
    EXPECT_FALSE(vfs.is_directory("/archive/empty.txt"));
    EXPECT_EQ(vfs.file_size("/archive/empty.txt"), 0u);

    // A zero-byte entry must read as empty. mz_zip_entry_read() rejects
    // zero-length reads with MZ_PARAM_ERROR, so the archive reader has to
    // handle this case before calling into minizip.
    EXPECT_EQ(vfs.read_string("/archive/empty.txt"), "");
    EXPECT_EQ(vfs.read_string("/archive/empty.txt", false), "");
    EXPECT_EQ(read_all(vfs, "/archive/empty.txt"), "");

    {
        auto fh = vfs.fopen("/archive/empty.txt");

        EXPECT_EQ(fh->file_size(), 0u);
        EXPECT_EQ(read_all(*fh), "");
    }
}

TEST(VFS, MountArchiveRejectsUnsafeEntries)
{
    namespace stdfs = std::filesystem;
    using namespace lochfolk::vfs_literals;

    const char* const archives[] = {
        "test_vfs_data/evil_absolute.zip",
        "test_vfs_data/evil_parent.zip",
        "test_vfs_data/evil_dot.zip",
    };

    for(const char* archive : archives)
    {
        if(!stdfs::exists(archive))
            GTEST_SKIP() << archive << " not found";

        lochfolk::virtual_file_system vfs;
        EXPECT_THROW(
            vfs.mount_archive("/archive", archive),
            lochfolk::virtual_file_system::error
        ) << archive;

        // The unsafe entry must not have been mounted, neither outside the
        // mount point nor with a literal "."/".." component below it.
        EXPECT_FALSE(vfs.exists("/evil_absolute.txt")) << archive;
        EXPECT_FALSE(vfs.exists("/evil_parent.txt")) << archive;
        EXPECT_FALSE(vfs.exists("/evil_dot.txt")) << archive;
        EXPECT_FALSE(vfs.exists("/archive/evil_absolute.txt")) << archive;
        EXPECT_FALSE(vfs.exists("/archive/evil_parent.txt")) << archive;
        EXPECT_FALSE(vfs.exists("/archive/evil_dot.txt")) << archive;
        EXPECT_FALSE(vfs.exists("/archive/../evil_parent.txt")) << archive;
    }

    // All entry names are validated before anything is mounted, so a rejected
    // archive must not leave a partially populated mount point behind.
    if(stdfs::exists("test_vfs_data/evil_mixed.zip"))
    {
        lochfolk::virtual_file_system vfs;
        EXPECT_THROW(
            vfs.mount_archive("/archive", "test_vfs_data/evil_mixed.zip"),
            lochfolk::virtual_file_system::error
        );
        EXPECT_FALSE(vfs.exists("/archive"));
        EXPECT_FALSE(vfs.exists("/archive/good.txt"));
        EXPECT_FALSE(vfs.exists("/good.txt"));
    }

    // A relative mount base is rejected as well.
    if(stdfs::exists("test_vfs_data/ar.zip"))
    {
        lochfolk::virtual_file_system vfs;
        EXPECT_THROW(
            vfs.mount_archive("relative", "test_vfs_data/ar.zip"),
            lochfolk::virtual_file_system::error
        );
    }
}

TEST(VFS, RemoveMissing)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;
    vfs.mount_string("/data/file.txt", "content");

    EXPECT_FALSE(vfs.remove("/missing/file.txt"));
    EXPECT_FALSE(vfs.remove("/data/missing.txt"));
    EXPECT_FALSE(vfs.remove("/data/file.txt/child"));
    EXPECT_TRUE(vfs.exists("/data/file.txt"));

    EXPECT_TRUE(vfs.remove("/data/file.txt//"));
    EXPECT_FALSE(vfs.exists("/data/file.txt"));
}

TEST(VFS, RemoveRootSeparators)
{
    using namespace lochfolk::vfs_literals;

    // A path of separators only refers to the root directory, exactly like
    // "/". It used to throw std::out_of_range from string_view::substr().
    lochfolk::virtual_file_system vfs;
    vfs.mount_string("/a.txt", "a");
    vfs.mount_string("/dir/b.txt", "b");

    EXPECT_TRUE(vfs.remove("//"));
    EXPECT_TRUE(vfs.is_directory("/"));
    EXPECT_FALSE(vfs.exists("/a.txt"));
    EXPECT_FALSE(vfs.exists("/dir"));

    vfs.mount_string("/c.txt", "c");
    EXPECT_TRUE(vfs.remove("///"));
    EXPECT_FALSE(vfs.exists("/c.txt"));

    vfs.mount_string("/d.txt", "d");
    EXPECT_TRUE(vfs.remove("////"));
    EXPECT_FALSE(vfs.exists("/d.txt"));

    vfs.mount_string("/e.txt", "e");
    EXPECT_TRUE(vfs.remove("/"));
    EXPECT_FALSE(vfs.exists("/e.txt"));
}

TEST(VFS, RemoveDotComponents)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;
    vfs.mount_string("/a/b.txt", "b");
    vfs.mount_string("/a/c.txt", "c");
    vfs.mount_string("/keep.txt", "k");

    EXPECT_TRUE(vfs.remove("/a/./b.txt"));
    EXPECT_FALSE(vfs.exists("/a/b.txt"));

    EXPECT_TRUE(vfs.remove("/a/../a/c.txt"));
    EXPECT_FALSE(vfs.exists("/a/c.txt"));

    // Missing components before ".." are resolved lexically, just like for
    // lookups; the path still names "/a/missing.txt" and is not found.
    EXPECT_FALSE(vfs.remove("/a/missing/../missing.txt"));
    EXPECT_TRUE(vfs.exists("/keep.txt"));

    // A path that normalizes to the root removes everything below it.
    EXPECT_TRUE(vfs.remove("/a/.."));
    EXPECT_TRUE(vfs.exists("/"));
    EXPECT_FALSE(vfs.exists("/keep.txt"));
}

TEST(VFS, MountEmptyDir)
{
    namespace stdfs = std::filesystem;
    using namespace lochfolk::vfs_literals;

    stdfs::path empty_dir = "test_vfs_data/empty_dir_for_mount_test";
    std::error_code ec;
    stdfs::remove_all(empty_dir, ec);
    ASSERT_TRUE(stdfs::create_directories(empty_dir));

    {
        lochfolk::virtual_file_system vfs;
        vfs.mount_dir("/empty", empty_dir);

        EXPECT_TRUE(vfs.exists("/empty"));
        EXPECT_TRUE(vfs.is_directory("/empty"));
    }

    stdfs::remove_all(empty_dir, ec);
}

TEST(VFS, FopenErrors)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;
    vfs.mount_string("/dir/file.txt", "x");

    EXPECT_THROW(vfs.fopen("/missing"), lochfolk::virtual_file_system::error);
    EXPECT_THROW(vfs.fopen("/dir"), lochfolk::virtual_file_system::error);
    EXPECT_THROW(
        vfs.fopen("/dir/file.txt", lochfolk::file_flag::unknown),
        std::invalid_argument
    );
    EXPECT_THROW((void)vfs.read_string("/dir"), lochfolk::virtual_file_system::error);
}

TEST(VFS, FopenMissingSystemFile)
{
    namespace stdfs = std::filesystem;
    using namespace lochfolk::vfs_literals;

    const stdfs::path gone = "test_vfs_data/gone_for_fopen_test.txt";
    {
        std::ofstream ofs(gone, std::ios::binary);
        ofs << "hollow";
    }

    lochfolk::virtual_file_system vfs;
    vfs.mount_file("/gone.txt", gone);
    stdfs::remove(gone);

    EXPECT_THROW(vfs.fopen("/gone.txt"), std::system_error);
}

TEST(VFS, ReadOnlyEntriesIgnoreWritable)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;
    vfs.mount_string("/s.txt", "abc");

    const std::byte data[] = {std::byte{'Z'}};

    {
        auto fh = vfs.fopen("/s.txt", lochfolk::file_flag::read_write);
        EXPECT_FALSE(fh->writable());
        EXPECT_TRUE(fh->write(std::span<const std::byte>(data)).is_npos());
    }

    EXPECT_EQ(vfs.read_string("/s.txt"), "abc");

    if(std::filesystem::exists("test_vfs_data/ar.zip"))
    {
        vfs.mount_archive("/archive", "test_vfs_data/ar.zip");

        auto fh = vfs.fopen("/archive/info.txt", lochfolk::file_flag::read_write);
        EXPECT_FALSE(fh->writable());
        EXPECT_TRUE(fh->write(std::span<const std::byte>(data)).is_npos());
        EXPECT_EQ(vfs.read_string("/archive/info.txt"), "archive\n");
    }
}

TEST(VFS, WritableSystemFile)
{
    namespace stdfs = std::filesystem;
    using namespace lochfolk::vfs_literals;

    const stdfs::path file = "test_vfs_data/writable_for_fopen_test.txt";
    {
        std::ofstream ofs(file, std::ios::binary);
        ofs << "hello";
    }

    lochfolk::virtual_file_system vfs;
    vfs.mount_file("/w.txt", file);

    {
        auto fh = vfs.fopen("/w.txt", lochfolk::file_flag::read_write);

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

    EXPECT_EQ(vfs.file_size("/w.txt"), 11);
    EXPECT_EQ(read_all(vfs, "/w.txt"), "hello world");

    // A write-only handle cannot read
    {
        auto fh = vfs.fopen("/w.txt", lochfolk::file_flag::writable);

        EXPECT_FALSE(fh->readable());
        EXPECT_TRUE(fh->writable());

        std::byte buf[4];
        EXPECT_TRUE(fh->read(std::span<std::byte>(buf)).is_npos());
    }

    stdfs::remove(file);
}

TEST(VFS, ConvertCRLFString)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;
    vfs.mount_string("/crlf.txt", "a\r\nb\r\nc");

    EXPECT_EQ(vfs.read_string("/crlf.txt"), "a\nb\nc");
    EXPECT_EQ(vfs.read_string("/crlf.txt", false), "a\r\nb\r\nc");

    {
        auto fh = vfs.fopen("/crlf.txt", lochfolk::file_flag::readable, true);
        EXPECT_EQ(fh->file_size(), 7); // raw size
        EXPECT_EQ(read_all(*fh), "a\nb\nc");
    }

    {
        auto fh = vfs.fopen("/crlf.txt", lochfolk::file_flag::readable, false);
        EXPECT_EQ(read_all(*fh), "a\r\nb\r\nc");
    }

    // A CR at the end of an internal chunk must be carried over
    {
        std::string padded(4095, 'x');
        padded += "\r\n";
        padded += "tail";
        vfs.mount_string("/edge.txt", padded);

        std::string expected(4095, 'x');
        expected += "\ntail";

        EXPECT_EQ(vfs.read_string("/edge.txt"), expected);
    }

    // A lone CR is not translated
    {
        vfs.mount_string("/lone.txt", "x\ry");
        EXPECT_EQ(vfs.read_string("/lone.txt"), "x\ry");
    }
}

TEST(VFS, ConvertCRLFEdgeCases)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    // A trailing lone CR at end of file is preserved
    vfs.mount_string("/trail.txt", "abc\r");
    EXPECT_EQ(vfs.read_string("/trail.txt"), "abc\r");

    // Empty content
    vfs.mount_string("/empty.txt", "");
    EXPECT_EQ(vfs.read_string("/empty.txt"), "");

    // CR at a chunk boundary followed by something that is not LF
    {
        std::string content(4095, 'x');
        content += "\r";
        content += "y";
        vfs.mount_string("/split.txt", content);
        EXPECT_EQ(vfs.read_string("/split.txt"), content);
    }

    // A chunk consisting only of a lone CR must not look like end of file
    {
        std::string content(4096, 'x');
        content += "\r";
        vfs.mount_string("/tail_cr.txt", content);
        EXPECT_EQ(vfs.read_string("/tail_cr.txt"), content);
    }

    // Seeking rewinds the translation state
    {
        vfs.mount_string("/rewind.txt", "p\r\nq\r\n");

        auto fh = vfs.fopen("/rewind.txt", lochfolk::file_flag::readable, true);
        const std::string first = read_all(*fh, 1);
        EXPECT_EQ(first, "p\nq\n");

        EXPECT_TRUE(fh->seek(0, lochfolk::whence::begin));
        const std::string second = read_all(*fh, 2);
        EXPECT_EQ(second, "p\nq\n");
    }

    // Seeking past the end is allowed and reads return EOF
    {
        auto fh = vfs.fopen("/trail.txt", lochfolk::file_flag::readable, true);
        EXPECT_TRUE(fh->seek(1, lochfolk::whence::end));

        std::byte buf[4];
        EXPECT_EQ(fh->read(std::span<std::byte>(buf)).bytes_transferred(), 0u);
    }
}

TEST(VFS, ConvertCRLFHandleChunkBoundary)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    // The previous 4096-byte chunk ends with a CR, which is held back. The
    // next chunk is full and starts with a byte other than LF, so the held
    // CR is emitted in addition to the whole chunk.
    std::string content(4095, 'x');
    content += '\r';
    content += std::string(4096, 'y');
    vfs.mount_string("/handle_edge.txt", content);

    EXPECT_EQ(vfs.read_string("/handle_edge.txt"), content);
    EXPECT_EQ(vfs.read_string("/handle_edge.txt", false), content);

    {
        auto fh = vfs.fopen(
            "/handle_edge.txt", lochfolk::file_flag::readable, true
        );
        EXPECT_EQ(read_all(*fh, 1), content);
    }

    {
        auto fh = vfs.fopen(
            "/handle_edge.txt", lochfolk::file_flag::readable, true
        );
        EXPECT_EQ(read_all(*fh), content);
    }
}

TEST(VFS, ConvertCRLFStreamChunkBoundary)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    // The 8192-byte stream buffer ends with a CR that must be held back; the
    // following chunk starts with a byte other than LF and must not be
    // overwritten by the held CR.
    std::string content(8191, 'x');
    content += '\r';
    content += "a\nb";
    vfs.mount_string("/stream_edge.txt", content);

    {
        auto vfss = vfs.open("/stream_edge.txt");
        std::istreambuf_iterator<char> first(vfss);
        std::istreambuf_iterator<char> last;
        EXPECT_EQ(std::string(first, last), content);
    }

    // The following chunk is full, so the translated output of that chunk is
    // one byte larger than its raw input.
    std::string full(8191, 'x');
    full += '\r';
    full += std::string(8192, 'y');
    vfs.mount_string("/stream_full.txt", full);

    {
        auto vfss = vfs.open("/stream_full.txt");
        std::istreambuf_iterator<char> first(vfss);
        std::istreambuf_iterator<char> last;
        EXPECT_EQ(std::string(first, last), full);
    }
}

TEST(VFS, ConvertCRLFChunkBoundariesDifferential)
{
    using namespace lochfolk::vfs_literals;

    // Reference translator: CRLF becomes LF, a lone CR is preserved.
    const auto reference = [](std::string_view raw)
    {
        std::string result;
        result.reserve(raw.size());

        for(std::size_t i = 0; i < raw.size(); ++i)
        {
            if(raw[i] == '\r' && i + 1 < raw.size() && raw[i + 1] == '\n')
            {
                result.push_back('\n');
                ++i;
            }
            else
            {
                result.push_back(raw[i]);
            }
        }

        return result;
    };

    const auto read_stream =
        [](lochfolk::virtual_file_system& vfs,
           lochfolk::path_view p,
           std::ios_base::openmode mode)
    {
        auto vfss = vfs.open(p, mode);
        std::istreambuf_iterator<char> first(vfss);
        std::istreambuf_iterator<char> last;
        return std::string(first, last);
    };

    // CRs placed at the chunk boundaries of both implementations (4096 bytes
    // for the handle filter, 8192 for the stream buffer), with and without a
    // following LF, plus small inputs.
    const std::string x4095(4095, 'x');
    const std::string x4096(4096, 'x');
    const std::string x8191(8191, 'x');
    const std::string x8192(8192, 'x');

    const std::vector<std::string> cases{
        "",
        "\r",
        "\r\n",
        "a\rb\r\n\r\r\n",
        "tail\r",
        x4095 + '\r' + x4096,
        x4095 + '\r' + '\n' + x4096,
        x4095 + '\r' + x4096 + "\r\n" + x4096,
        x8191 + '\r' + x8192,
        x8191 + '\r' + '\n' + x8192,
        x8191 + '\r' + "a\nb",
        x8191 + '\r',
        x8192 + '\r',
    };

    lochfolk::virtual_file_system vfs;

    for(std::size_t i = 0; i < cases.size(); ++i)
    {
        const std::string& raw = cases[i];
        const std::string expected = reference(raw);
        const std::string path = "/case" + std::to_string(i) + ".txt";
        const lochfolk::path_view pv(path);

        vfs.mount_string(pv, raw);

        // fh_crlf (fopen/read_string) and fh_streambuf (streams) must agree
        // with the reference translation regardless of chunk boundaries.
        EXPECT_EQ(vfs.read_string(pv), expected) << "case " << i;
        EXPECT_EQ(vfs.read_string(pv, false), raw) << "case " << i;
        EXPECT_EQ(read_stream(vfs, pv, std::ios_base::in), expected)
            << "case " << i;
        EXPECT_EQ(
            read_stream(vfs, pv, std::ios_base::in | std::ios_base::binary),
            raw
        ) << "case " << i;
    }
}

TEST(VFS, ConvertCRLFSystemFile)
{
    namespace stdfs = std::filesystem;
    using namespace lochfolk::vfs_literals;

    const stdfs::path file = "test_vfs_data/crlf_for_fopen_test.txt";
    {
        std::ofstream ofs(file, std::ios::binary);
        ofs << "1\r\n2\r\n";
    }

    lochfolk::virtual_file_system vfs;
    vfs.mount_file("/crlf.txt", file);

    EXPECT_EQ(vfs.read_string("/crlf.txt"), "1\n2\n");
    EXPECT_EQ(vfs.read_string("/crlf.txt", false), "1\r\n2\r\n");

    {
        auto fh = vfs.fopen("/crlf.txt", lochfolk::file_flag::readable, true);
        EXPECT_EQ(read_all(*fh), "1\n2\n");
    }

    stdfs::remove(file);
}

TEST(VFS, ConvertCRLFArchive)
{
    using namespace lochfolk::vfs_literals;

    if(!std::filesystem::exists("test_vfs_data/ar.zip"))
        GTEST_SKIP() << "test_vfs_data/ar.zip not found";

    lochfolk::virtual_file_system vfs;
    vfs.mount_archive("/archive", "test_vfs_data/ar.zip");

    ASSERT_TRUE(vfs.exists("/archive/crlf.txt"));
    EXPECT_EQ(vfs.read_string("/archive/crlf.txt"), "line1\nline2\n");
    EXPECT_EQ(vfs.read_string("/archive/crlf.txt", false), "line1\r\nline2\r\n");
}

TEST(VFS, LargeContent)
{
    using namespace lochfolk::vfs_literals;

    std::string big;
    big.reserve(10000);
    for(int i = 0; i < 10000; ++i)
        big.push_back(static_cast<char>('a' + (i % 26)));

    lochfolk::virtual_file_system vfs;
    vfs.mount_string("/big.txt", big);

    EXPECT_EQ(vfs.read_string("/big.txt"), big);

    {
        auto fh = vfs.fopen("/big.txt");
        EXPECT_EQ(read_all(*fh), big);
    }

    {
        auto fh = vfs.fopen("/big.txt");
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

TEST(VFS, AccessContext)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;
    vfs.mount_string("/data/strings/str.txt", "str");
    vfs.mount_string("/info/info.txt", "1013");

    lochfolk::access_context ctx(vfs);
    EXPECT_EQ(&ctx.get_vfs(), &vfs);

    EXPECT_EQ(ctx.current_path(), "/");

    ctx.current_path("/data");
    EXPECT_EQ(ctx.current_path(), "/data");

    {
        std::string str = ctx.read_string("strings/str.txt");

        EXPECT_EQ(str, "str");
    }

    EXPECT_EQ(ctx.to_fullpath("strings"), "/data/strings");
    EXPECT_EQ(ctx.to_fullpath("./strings"), "/data/strings");
    EXPECT_EQ(ctx.to_fullpath("/data/./strings"), "/data/strings");
    EXPECT_EQ(ctx.to_fullpath("/data/../info/info.txt"), "/info/info.txt");
    EXPECT_EQ(ctx.to_fullpath("../info/info.txt"), "/info/info.txt");

    EXPECT_TRUE(ctx.is_directory("strings"));
    EXPECT_FALSE(ctx.is_directory("strings/str.txt"));

    EXPECT_TRUE(ctx.exists("../info/info.txt"));
    EXPECT_EQ(ctx.file_size("../info/info.txt"), 4);
    EXPECT_FALSE(ctx.exists("../info/str.txt"));

    {
        auto fh = ctx.fopen("../info/info.txt");
        EXPECT_EQ(read_all(*fh), "1013");
    }

    {
        auto vfss = ctx.open("../info/info.txt");

        int v = 0;
        vfss >> v;
        EXPECT_EQ(v, 1013);
    }

    {
        std::string str = ctx.read_string("../info/info.txt");

        EXPECT_EQ(str, "1013");
    }

    EXPECT_TRUE(ctx.remove("../info/info.txt"));
    EXPECT_FALSE(ctx.exists("../info/info.txt"));
    EXPECT_FALSE(ctx.remove("../info/info.txt"));

    ctx.current_path("strings");
    EXPECT_EQ(ctx.current_path(), "/data/strings");
    EXPECT_TRUE(ctx.exists("str.txt"));

    {
        std::string str = ctx.read_string("str.txt");

        EXPECT_EQ(str, "str");
    }

    ctx.current_path("..");
    EXPECT_EQ(ctx.current_path(), "/data/");

    {
        std::string str = ctx.read_string("strings/str.txt");

        EXPECT_EQ(str, "str");
    }

    EXPECT_TRUE(ctx.exists("/data/strings/str.txt"));
    EXPECT_TRUE(ctx.remove("/data"));
    EXPECT_FALSE(ctx.exists("/data/strings/str.txt"));
}

TEST(VFS, AccessContextArchive)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;
    vfs.mount_archive("/archive", "test_vfs_data/ar.zip");
    vfs.list_files(std::cerr);

    lochfolk::access_context ctx(vfs);
    ctx.current_path("archive");
    EXPECT_EQ(ctx.current_path(), "/archive");

    EXPECT_EQ(read_all(*ctx.fopen("/archive/info.txt")), "archive\n");
    EXPECT_EQ(read_all(*ctx.fopen("info.txt")), "archive\n");

    {
        auto vfss = ctx.open("info.txt");

        std::string str;
        vfss >> str;
        EXPECT_EQ(str, "archive");
    }

    EXPECT_EQ(ctx.read_string("info.txt"), "archive\n");
    EXPECT_EQ(ctx.read_string("/archive/info.txt"), "archive\n");

    EXPECT_EQ(read_all(*ctx.fopen("/archive/data/value.txt")), "182375 182376\n");
    EXPECT_EQ(read_all(*ctx.fopen("data/value.txt")), "182375 182376\n");

    EXPECT_TRUE(ctx.exists("/archive/info.txt"));
    EXPECT_TRUE(ctx.exists("info.txt"));
    EXPECT_TRUE(ctx.exists("/archive/data/value.txt"));
    EXPECT_TRUE(ctx.exists("data/value.txt"));

    EXPECT_TRUE(ctx.remove("data/"));
    EXPECT_TRUE(ctx.exists("info.txt"));
    EXPECT_FALSE(ctx.exists("/archive/data/value.txt"));
    EXPECT_FALSE(ctx.exists("data/value.txt"));
}

TEST(VFS, StreamTextMode)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    // Three lines: nine raw bytes, six characters after translation
    vfs.mount_string("/t.txt", "a\r\nb\r\nc\r\n");

    // Text mode is the default
    {
        auto vfss = vfs.open("/t.txt");
        std::istreambuf_iterator<char> first(vfss);
        std::istreambuf_iterator<char> last;
        EXPECT_EQ(std::string(first, last), "a\nb\nc\n");
    }

    // tellg/seekg use raw byte offsets
    {
        auto vfss = vfs.open("/t.txt");
        EXPECT_EQ(vfss.tellg(), std::streampos(0));

        char c = 0;
        vfss.get(c);
        EXPECT_EQ(c, 'a');
        EXPECT_EQ(vfss.tellg(), std::streampos(1));

        vfss.get(c); // consumes the CRLF pair
        EXPECT_EQ(c, '\n');
        EXPECT_EQ(vfss.tellg(), std::streampos(3));

        const std::streampos pos = vfss.tellg();
        vfss.get(c);
        EXPECT_EQ(c, 'b');

        // Seeking to a position returned by tellg() continues correctly
        vfss.clear();
        vfss.seekg(pos);
        vfss.get(c);
        EXPECT_EQ(c, 'b');

        // Rewind
        vfss.clear();
        vfss.seekg(0);
        std::istreambuf_iterator<char> first(vfss);
        std::istreambuf_iterator<char> last;
        EXPECT_EQ(std::string(first, last), "a\nb\nc\n");

        vfss.clear();
        vfss.seekg(0, std::ios_base::end);
        EXPECT_EQ(vfss.tellg(), std::streampos(9));
        EXPECT_EQ(vfss.peek(), std::char_traits<char>::eof());
    }

    // A CRLF pair split across the internal buffer boundary
    {
        std::string content(8191, 'x');
        content += "\r\n";
        content += "tail";
        vfs.mount_string("/edge.txt", content);

        std::string expected(8191, 'x');
        expected += "\ntail";

        auto vfss = vfs.open("/edge.txt");
        std::istreambuf_iterator<char> first(vfss);
        std::istreambuf_iterator<char> last;
        EXPECT_EQ(std::string(first, last), expected);
    }

    // A chunk holding only a CR does not look like end of file
    {
        std::string content(8192, 'x');
        content += "\r";
        vfs.mount_string("/tail_cr.txt", content);

        auto vfss = vfs.open("/tail_cr.txt");
        std::istreambuf_iterator<char> first(vfss);
        std::istreambuf_iterator<char> last;
        EXPECT_EQ(std::string(first, last), content);
    }

    // A lone CR is preserved
    {
        vfs.mount_string("/lone.txt", "abc\r");
        auto vfss = vfs.open("/lone.txt");
        std::istreambuf_iterator<char> first(vfss);
        std::istreambuf_iterator<char> last;
        EXPECT_EQ(std::string(first, last), "abc\r");
    }
}

TEST(VFS, StreamReadError)
{
    lochfolk::ivfstream vfss(std::make_unique<failing_handle>());

    char c = 0;
    vfss.get(c);

    EXPECT_TRUE(vfss.bad());
    EXPECT_FALSE(vfss.eof());
}

TEST(VFS, StreamAdapter)
{
    using namespace lochfolk::vfs_literals;

    lochfolk::virtual_file_system vfs;

    std::string big;
    big.reserve(20000);
    for(int i = 0; i < 20000; ++i)
        big.push_back(static_cast<char>('a' + (i % 26)));

    vfs.mount_string("/big.txt", big);
    vfs.mount_string("/empty.txt", "");
    vfs.mount_string("/crlf.txt", "a\r\nb\r\n");

    // Streams default to text mode (CRLF -> LF on every platform)
    {
        auto vfss = vfs.open("/crlf.txt");
        std::istreambuf_iterator<char> first(vfss);
        std::istreambuf_iterator<char> last;
        std::string str(first, last);
        EXPECT_EQ(str, "a\nb\n");
    }

    // ... and binary mode keeps the raw bytes
    {
        auto vfss = vfs.open(
            "/crlf.txt", std::ios_base::in | std::ios_base::binary
        );
        std::istreambuf_iterator<char> first(vfss);
        std::istreambuf_iterator<char> last;
        std::string str(first, last);
        EXPECT_EQ(str, "a\r\nb\r\n");
    }

    // tellg / seekg
    {
        auto vfss = vfs.open("/big.txt");
        EXPECT_EQ(vfss.tellg(), std::streampos(0));

        char buf[16];
        vfss.read(buf, 10);
        EXPECT_EQ(vfss.tellg(), std::streampos(10));

        vfss.seekg(9000);
        EXPECT_EQ(vfss.tellg(), std::streampos(9000));

        vfss.read(buf, 10);
        EXPECT_EQ(std::string(buf, 10), big.substr(9000, 10));

        vfss.seekg(-5, std::ios_base::cur);
        EXPECT_EQ(vfss.tellg(), std::streampos(9005));

        vfss.seekg(-10, std::ios_base::end);
        EXPECT_EQ(vfss.tellg(), std::streampos(19990));

        vfss.read(buf, 10);
        EXPECT_EQ(std::string(buf, 10), big.substr(19990, 10));
    }

    // A long line crossing the internal buffer boundary
    {
        auto vfss = vfs.open("/big.txt");

        std::string line;
        std::getline(vfss, line);
        EXPECT_EQ(line, big);
    }

    // putback works after read()
    {
        auto vfss = vfs.open("/big.txt");

        char buf[3];
        vfss.read(buf, 3);
        EXPECT_EQ(std::string(buf, 3), big.substr(0, 3));

        vfss.unget();
        EXPECT_EQ(vfss.get(), static_cast<int>(big[2]));
        EXPECT_TRUE(vfss.good());
    }

    // Empty file
    {
        auto vfss = vfs.open("/empty.txt");
        EXPECT_EQ(vfss.peek(), std::char_traits<char>::eof());
        EXPECT_TRUE(vfss.eof());
    }

    // Invalid modes and paths
    EXPECT_THROW(
        vfs.open("/big.txt", std::ios_base::out), std::invalid_argument
    );
    EXPECT_THROW(vfs.open("/missing"), lochfolk::virtual_file_system::error);

    vfs.mount_string("/dir/file.txt", "x");
    EXPECT_THROW(vfs.open("/dir"), lochfolk::virtual_file_system::error);

    // Move construction
    {
        auto src = vfs.open("/big.txt");
        EXPECT_TRUE(src.has_buffer());

        auto vfss = std::move(src);
        EXPECT_FALSE(src.has_buffer());
        EXPECT_EQ(src.rdbuf(), nullptr);

        std::string line;
        std::getline(vfss, line);
        EXPECT_EQ(line, big);
    }
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
