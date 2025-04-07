add_rules("mode.debug", "mode.release")

set_languages("c++20")

add_requires("minizip-ng")

target("lochfolk")
    set_warnings("all", "error")
    set_kind("static")
    add_includedirs("include", { public = true })
    add_packages("minizip-ng")
    add_files("src/*.cpp")

option("unit_test")
    set_default(false)
    set_showmenu(true)
    set_description("Enable unit tests building")

if has_config("unit_test") then
    includes("test")
end
