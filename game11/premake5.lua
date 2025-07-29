workspace "Engine"
    configurations { "Debug", "Release", "Profile" }
    location "build"	

project "game11"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "On"

    location "build/engine"
    debugdir "."

    system "Windows"
    architecture "x86_64"

    flags { "MultiProcessorCompile" }

    warnings "Off"

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
        "src/libs/GameNetworkingSockets/lib/Debug/GameNetworkingSockets_s.lib"
    }

    filter "configurations:Debug"

    targetdir "build/bin/debug"
    defines { "DEBUG" }
    symbols "On"

    filter "configurations:Release"
    targetdir "build/bin/release"
    optimize "On"

    filter "configurations:Profile"
    targetdir "build/bin/profile"
    profile "On"
    symbols "On"
    optimize "On"
