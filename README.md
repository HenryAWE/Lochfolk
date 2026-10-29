# Lochfolk
Yet another C++ virtual filesystem library.

## Brief Examples
### 1. Loading ZIP Archives

```c++
using namespace lochfolk::vfs_literals;

lochfolk::virtual_file_system vfs;

vfs.mount_archive("/archive"_pv, "system/path/to/archive.zip");

{
    auto vfss = vfs.open("/archive/info.txt"_pv);

    std::string str;
    vfss >> str;

    assert(str == "archive");
}

{
    auto vfss = vfs.open("/archive/data/value.txt"_pv);

    int v1 = 0, v2 = 0;
    vfss >> v1 >> v2;
    assert(v1 == 182375);
    assert(v2 == 182376);
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

{
    auto vfss = vfs.open("/data/a.txt"_pv);

    std::string str;
    vfss >> str;

    assert(str == "AAA");
}

{
    auto vfss = vfs.open("/data/nested/b.txt"_pv);

    std::string str;
    vfss >> str;

    assert(str == "BBB");
}

{
    auto vfss = vfs.open("/data/example.txt"_pv);

    int v = 0;
    vfss >> v;
    assert(v == 1013);
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
    auto vfss = vfs.open("/data/text/example.txt"_pv);

    int v1 = 0, v2 = 0;
    vfss >> v1 >> v2;
    assert(v1 == 123);
    assert(v2 == 456);
}
```

## Streams and File Handles

`virtual_file_system::open()` returns an `ivfstream`, an input stream over a
`file_handle`. It supports the usual `operator>>`, `getline`, `read`, `seekg`
and `tellg`.

Text mode is the default, as for `std::ifstream`: CRLF is translated to LF, and
the translation is the same on every platform. Pass `std::ios_base::binary`
to read the raw bytes:

```c++
auto vfss = vfs.open("/data/example.txt"_pv);

std::string line;
std::getline(vfss, line);

vfss.seekg(0);
vfss.read(buf, sizeof buf);

// Raw bytes, no translation
auto bin = vfs.open("/data/example.txt"_pv, std::ios_base::in | std::ios_base::binary);
```

`tellg()`/`seekg()` always use the raw byte offsets of the underlying file, so
with CRLF translation a pair advances the position by two. `seekg(0)` and
`seekg(pos)` where `pos` came from `tellg()` behave as expected.

For random access, or to write to host files, use `fopen()`, which returns a
`std::unique_ptr<lochfolk::file_handle>`:

```c++
auto fh = vfs.fopen(
    "/data/example.txt"_pv,
    lochfolk::file_flag::read_write, // readable | writable
    false                            // convert CRLF to LF while reading?
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
