add_requires("gtest")

target("test_path")
    set_warnings("all", "error")
    set_kind("binary")
    set_default(false)
    add_packages("gtest")
    add_deps("lochfolk")
    add_files("test_path.cpp")
    add_tests("test_lochfolk")

target("test_vfs")
    set_warnings("all", "error")
    set_kind("binary")
    set_default(false)
    add_packages("gtest")
    add_deps("lochfolk")
    add_files("test_vfs.cpp")
    add_tests("test_lochfolk")
    after_build(function (target)
        import("lib.detect.find_tool")

        os.cp("$(scriptdir)/example.txt", target:targetdir() .. "/test_vfs_data/")

        os.cp("$(scriptdir)/dir/", target:targetdir() .. "/test_vfs_data/")

        local script = path.absolute(os.scriptdir() .. "/script/make_archive.py")
        local archive_dir = path.absolute(os.scriptdir() .. "/archive")
        local ar_name = path.absolute(target:targetdir() .. "/test_vfs_data/ar.zip")

        local python = find_tool("python3", {version = true})
        if not python then
            raise("python3 not found, please install Python 3 and ensure it is in PATH")
        end

        os.execv(python.program, { script, ar_name, archive_dir })
    end)
