add_rules("mode.debug", "mode.release")

set_languages("c++20")

target("Lochfolk")
    set_kind("static")
    add_includedirs("include", { public = true })
    add_files("src/*.cpp")
