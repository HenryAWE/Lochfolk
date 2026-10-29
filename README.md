# Lochfolk
Yet another C++ virtual filesystem library.

## Brief Examples
### 1. Loading ZIP Archives

```c++
using namespace lochfolk::vfs_literals;

lochfolk::virtual_file_system vfs;

vfs.mount_archive("/archive"_pv, "system/path/to/archive.zip");

// Read a whole file as text (CRLF is converted to LF by default)
assert(vfs.read_string("/archive/info.txt"_pv) == "archive\n");

// Read bytes through a file handle
{
    auto fh = vfs.fopen("/archive/data/value.txt"_pv);

    std::byte buf[32];
    const lochfolk::io_result n = fh->read(buf);

    assert(!n.is_npos());
    assert(n.bytes_transferred() == 14);
}
```

### 2. Loading System Files

```c++
using namespace lochfolk::vfs_literals;

lochfolk::virtual_file_system vfs;

vfs.mount_dir("/data"_pv, "test_vfs_data/dir/");
vfs.mount_file("/data/example.txt"_pv, "test_vfs_data/example.txt");
vfs.list_files(std::cerr);

assert(vfs.is_directory("/data/nested"_pv));

assert(vfs.read_string("/data/a.txt"_pv) == "AAA\n");
assert(vfs.read_string("/data/nested/b.txt"_pv) == "BBB\n");

{
    auto fh = vfs.fopen("/data/example.txt"_pv);

    std::byte buf[4];
    fh->read(buf);
    assert(std::string(reinterpret_cast<const char*>(buf), 4) == "1013");
}
```

### 3. Loading from String Constants

```c++
using namespace lochfolk::vfs_literals;

lochfolk::virtual_file_system vfs;

vfs.mount_string("/data/text/example.txt"_pv, "123 456");

assert(vfs.exists("/data/text/example.txt"_pv));
assert(!vfs.is_directory("/data/text/example.txt"_pv));

{
    auto fh = vfs.fopen("/data/text/example.txt"_pv);

    assert(fh->readable());
    assert(!fh->writable());
    assert(fh->file_size() == 7);
}
```

## File Handles

`virtual_file_system::fopen` returns a `std::unique_ptr<lochfolk::file_handle>`
and is the only way to read or write file contents. A handle is independent of
the file system it came from: it stays usable after the virtual file system is
modified or destroyed.

```c++
auto fh = vfs.fopen(
    "/data/example.txt"_pv,
    lochfolk::file_flag::read_write, // readable | writable
    true                             // convert CRLF to LF while reading
);

std::byte buf[64];
const lochfolk::io_result n = fh->read(buf); // 0 == EOF, npos == error

fh->seek(0, lochfolk::whence::begin);
fh->seek(-4, lochfolk::whence::end);
```

- `file_flag::readable`, `file_flag::writable` and `read_write` can be combined
  with `operator|`.
- **Writing is only supported for files mounted from the host file system.**
  String constants and archive entries are always read-only; asking for
  `file_flag::writable` yields a read-only handle (check
  `file_handle::writable()` or the result of `file_handle::write()`).
- `convert_crlf` translates CRLF to LF while reading, on every platform and for
  every kind of entry. It affects reads only; `file_size()` still reports the
  raw size.
- `seek()` uses raw offsets, and `whence::begin/current/end` replace
  `std::ios_base::seekdir`.
- Errors are reported as `io_result::npos` from `read()`/`write()`, as
  `virtual_file_system::error` for invalid virtual paths, and as
  `std::system_error` when a host file cannot be opened.

## Acknowledgments
This library uses the following third party libraries:

- [minizip-ng](https://github.com/zlib-ng/minizip-ng): For loading ZIP archive.
- [GoogleTest](https://github.com/google/googletest): For testing.

## License
[MIT License](./LICENSE)
