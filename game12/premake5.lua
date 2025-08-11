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
    }

    links {
        "user32",
        "gdi32",
        "shell32",
        "opengl32",

        "src/libs/GameNetworkingSockets/lib/debug/GameNetworkingSockets.lib",
        "src/libs/glfw/glfw3_mt.lib",
        "src/libs/glew/lib/Release/x64/glew32s.lib",
        "src/libs/assimp/lib/x64/assimp-vc143-mt.lib"
    }

    filter "configurations:Debug"
        targetdir "build/bin/debug"
        postbuildcommands {
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/debug/*.dll] %[build/bin/debug]",
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/debug/*.pdb] %[build/bin/debug]",
            "{COPYFILE} %[src/libs/assimp/bin/x64/*.dll] %[build/bin/debug]",
        }

    filter "configurations:Release"
        targetdir "build/bin/release"
        postbuildcommands {
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/release/*.dll] %[build/bin/release]",
            "{COPYFILE} %[src/libs/assimp/bin/x64/*.dll] %[build/bin/debug]",
        }

    filter "configurations:Profile"
        targetdir "build/bin/profile"
        profile "On"
        symbols "On"
        optimize "On"
        postbuildcommands {
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/release/*.dll] %[build/bin/profile]",
            "{COPYFILE} %[src/libs/assimp/bin/x64/*.dll] %[build/bin/debug]",
        }
