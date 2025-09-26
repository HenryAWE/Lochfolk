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
    assert(v1 == 123);
    assert(v2 == 456);
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

## Acknowledgments
This library uses the following third party libraries:

- [minizip-ng](https://github.com/zlib-ng/minizip-ng): For loading ZIP archive.
- [GoogleTest](https://github.com/google/googletest): For testing.

## License
[MIT License](./LICENSE)
