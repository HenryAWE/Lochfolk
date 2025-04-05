add_requires("gtest")

target("test_path")
    set_kind("binary")
    set_default(false)
    add_packages("gtest")
    add_deps("lochfolk")
    add_files("test_path.cpp")
    add_tests("test_lochfolk")

target("test_vfs")
    set_kind("binary")
    set_default(false)
    add_packages("gtest")
    add_deps("lochfolk")
    add_files("test_vfs.cpp")
    add_tests("test_lochfolk")
    after_build(function (target)
        os.cp("$(scriptdir)/example.txt", target:targetdir() .. "/test_vfs_data/")

        os.cp("$(scriptdir)/dir/", target:targetdir() .. "/test_vfs_data/")

        local ar_name = path.absolute(target:targetdir() .. "/test_vfs_data/ar.zip")
        import("utils.archive")
        local options = {}
        options.recurse = true
        options.compress = "default"

        local old_dir = os.cd("$(scriptdir)/archive")
        archive.archive(ar_name, ".", options)
        os.cd(old_dir)
    end)
