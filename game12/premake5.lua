workspace "Engine"
    configurations { "Debug", "Release", "Profile" }
    location "build"

    -- everything for each project to inherit
    cppdialect "C++20"
    language "C++"
    staticruntime "Off"
    debugdir "."
    system "Windows"
    architecture "x86_64"
    warnings "Off"
    flags { "MultiProcessorCompile" }

    defines { "WINDOWS", "ENABLE_ASSERTS" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        optimize "On"
        defines { "RELEASE" }

    filter "configurations:Profile"
        profile "On"
        symbols "On"
        optimize "On"

project "game12"
    kind "ConsoleApp"
    location "build/%{prj.name}"
    defines { "CLIENT" }

    files {
        "src/main.cpp",
        "src/win_platform.cpp",
        "src/libs/imgui/imgui*.cpp",
        "src/libs/miniaudio/miniaudio.c"
    }

    includedirs {
        "src/libs",
        "src/libs/imgui",
        "src/libs/glew/include",
        "src/libs/assimp/include",
        "src/libs/yaml-cpp/include",
    }

    links {
        "user32",
        "gdi32",
        "shell32",
        "opengl32",

        "src/libs/glfw/glfw3_mt.lib",
        "src/libs/glew/lib/Release/x64/glew32s.lib",
        "src/libs/assimp/lib/x64/assimp-vc143-mt.lib"
    }

    filter "configurations:Debug"
        targetdir "build/bin/debug"
        links {
            "src/libs/yaml-cpp/lib/debug/yaml-cppd.lib",
            "src/libs/GameNetworkingSockets/lib/debug/GameNetworkingSockets.lib",
        }

        postbuildcommands {
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/debug/*.dll] %[build/bin/debug]",
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/debug/*.pdb] %[build/bin/debug]",
            "{COPYFILE} %[src/libs/assimp/bin/x64/*.dll] %[build/bin/debug]",
        }

    filter "configurations:Release"
        targetdir "build/bin/release"
        links {
            "src/libs/yaml-cpp/lib/release/yaml-cpp.lib",
            "src/libs/GameNetworkingSockets/lib/release/GameNetworkingSockets.lib",
        }

        postbuildcommands {
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/release/*.dll] %[build/bin/release]",
            "{COPYFILE} %[src/libs/assimp/bin/x64/*.dll] %[build/bin/debug]",
        }

    filter "configurations:Profile"
        targetdir "build/bin/profile"
        links {
            "src/libs/yaml-cpp/lib/release/yaml-cpp.lib",
            "src/libs/GameNetworkingSockets/lib/release/GameNetworkingSockets.lib",
        }

        postbuildcommands {
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/release/*.dll] %[build/bin/profile]",
            "{COPYFILE} %[src/libs/assimp/bin/x64/*.dll] %[build/bin/debug]",
        }

project "meta"
    kind "ConsoleApp"
    location "build/%{prj.name}"

    files {
        "meta/meta.cpp",
        "src/libs/tree-sitter/lib/src/lib.c",
        "src/libs/tree-sitter-cpp/src/parser.c",
        "src/libs/tree-sitter-cpp/src/scanner.c",
    }

    includedirs {
        "src",
        "src/meta",
        "src/libs",
        "src/libs/tree-sitter/lib/src",
        "src/libs/tree-sitter/lib/include",
    }

    links {
        "user32",
        "gdi32",
        "shell32",
    }

    filter "configurations:Debug"
        targetdir "build/bin/debug"
    filter "configurations:Release"
        targetdir "build/bin/release"
    filter "configurations:Profile"
        targetdir "build/bin/profile"

project "metaexample"
    kind "ConsoleApp"
    location "build/%{prj.name}"

    files {
        "meta/example/main.cpp",
        "meta/example/foo.cpp",
        "meta/example/meta.h",
        "meta/example/foo.h",
        "meta/example/meta_foo.h",
    }

    includedirs {
        "src",
        "src/meta",
    }

    links {
        "user32",
        "gdi32",
        "shell32",
    }

    filter "configurations:Debug"
        targetdir "build/bin/debug"
    filter "configurations:Release"
        targetdir "build/bin/release"
    filter "configurations:Profile"
        targetdir "build/bin/profile"
