workspace "Engine"
    configurations { "Debug", "Release", "Profile" }
    location "build"	

project "game10"
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
        "src/libs/imgui/imgui*.cpp",
        "src/libs/miniaudio/miniaudio.c"
    }

    includedirs {
        "src/libs/imgui",
        "src/libs",
        "src/libs/glew/include",
        "src/libs/assimp/include",
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
