workspace "Engine"
    configurations { "Debug", "Release", "Profile" }
    location "build"	

project "client"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "On"

    location "build/%{prj.name}"
    debugdir "."

    system "Windows"
    architecture "x86_64"

    flags { "MultiProcessorCompile" }

    warnings "Off"

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
        defines { "DEBUG" }
        symbols "On"
        postbuildcommands {
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/debug/*.dll] %[build/bin/%{prj.name}/debug]",
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/debug/*.pdb] %[build/bin/%{prj.name}/debug]",
        }

    filter "configurations:Release"
        targetdir "build/bin/%{prj.name}/release"
        optimize "On"
        defines { "RELEASE" }
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
    language "C++"
    cppdialect "C++20"
    staticruntime "On"

    location "build/%{prj.name}"
    debugdir "."

    system "Windows"
    architecture "x86_64"

    flags { "MultiProcessorCompile" }

    warnings "Off"

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
        defines { "DEBUG" }
        symbols "On"
        postbuildcommands {
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/debug/*.dll] %[build/bin/%{prj.name}/debug]",
            "{COPYFILE} %[src/libs/GameNetworkingSockets/lib/debug/*.pdb] %[build/bin/%{prj.name}/debug]",
        }

    filter "configurations:Release"
        targetdir "build/bin/%{prj.name}/release"
        optimize "On"
        defines { "RELEASE" }
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
