workspace "Engine"
    configurations { "Debug", "Release" }
    location "build"	

project "game9"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    location "build/engine"
    debugdir "."

    system "Windows"
    architecture "x86_64"

    profile "On"
    flags { "MultiProcessorCompile" }

    files {
        "src/main.cpp",
        "src/libs/imgui/imgui*.cpp",
        "src/libs/miniaudio/miniaudio.c"
    }

    includedirs {
        "src/libs/imgui",
        "src/libs",
        "src/libs/glew/include"
    }

    links {
        "user32",
        "gdi32",
        "shell32",
        "opengl32",

        "src/libs/glfw/glfw3_mt.lib",
        "src/libs/glew/lib/Release/x64/glew32s.lib",
    }

    filter "configurations:Debug"
    targetdir "build/bin/debug"
    defines { "DEBUG" }
    symbols "On"

    filter "configurations:Release"
    targetdir "build/bin/release"
    optimize "On"
