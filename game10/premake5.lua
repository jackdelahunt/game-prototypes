workspace "Engine"
    configurations { "Debug", "Release", "Profile" }
    location "build"	

project "game10"
    kind "WindowedApp"
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
        "src/libs/imgui/backends/imgui_impl_dx11.cpp",
        "src/libs/imgui/backends/imgui_impl_win32.cpp",
    }

    includedirs {
        "src/libs",
        "src/libs/imgui",
    }

    links {
        "user32",
        "gdi32",
        "shell32",
        "D3D11",
        "src/libs/glfw/glfw3_mt.lib",
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
