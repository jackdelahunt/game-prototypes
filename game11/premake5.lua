workspace "Engine"
    configurations { "Debug", "Release", "Profile" }
    location "build"

    -- everything for each project to inherit
    cppdialect "C++20"
    language "C++"
    staticruntime "On"
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

project "client"
    kind "ConsoleApp"
    location "build/%{prj.name}"
    defines { "CLIENT" }

    files {
        "src/main.cpp",
    }

    includedirs {
        "src/libs",
    }

    links {
        "gdi32.lib",
        "winmm.lib",
        "msvcrt.lib",
        "shell32.lib",
        "src/libs/raylib/lib/raylib.lib",
        "src/libs/GameNetworkingSockets/lib/debug/GameNetworkingSockets.lib"
    }

    filter "configurations:Debug"
        targetdir "build/bin/%{prj.name}/debug"
        postbuildcommands {
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/debug/*.dll] %[build/bin/%{prj.name}/debug]",
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/debug/*.pdb] %[build/bin/%{prj.name}/debug]",
        }

    filter "configurations:Release"
        targetdir "build/bin/%{prj.name}/release"
        postbuildcommands {
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/release/*.dll] %[build/bin/%{prj.name}/release]",
        }

    filter "configurations:Profile"
        targetdir "build/bin/%{prj.name}/profile"
        profile "On"
        symbols "On"
        optimize "On"
        postbuildcommands {
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/release/*.dll] %[build/bin/%{prj.name}/profile]",
        }

project "server"
    kind "ConsoleApp"
    location "build/%{prj.name}"
    defines { "SERVER" }

    files {
        "src/main.cpp",
    }

    includedirs {
        "src/libs",
    }

    links {
        "gdi32.lib",
        "winmm.lib",
        "msvcrt.lib",
        "shell32.lib",
        "src/libs/raylib/lib/raylib.lib",
        "src/libs/GameNetworkingSockets/lib/debug/GameNetworkingSockets.lib"
    }

    filter "configurations:Debug"
        targetdir "build/bin/%{prj.name}/debug"
        postbuildcommands {
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/debug/*.dll] %[build/bin/%{prj.name}/debug]",
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/debug/*.pdb] %[build/bin/%{prj.name}/debug]",
        }

    filter "configurations:Release"
        targetdir "build/bin/%{prj.name}/release"
        postbuildcommands {
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/release/*.dll] %[build/bin/%{prj.name}/release]",
        }

    filter "configurations:Profile"
        targetdir "build/bin/%{prj.name}/profile"
        postbuildcommands {
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/release/*.dll] %[build/bin/%{prj.name}/profile]",
        }
