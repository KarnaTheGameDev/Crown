workspace "Crown"
	architecture "x64"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "Crown/vendor/GLFW/include"

include "Crown/vendor/GLFW"

project "Crown"
	location "Crown"
	kind "SharedLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "crpch.h"
	pchsource "Crown/src/crpch.cpp"

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}

	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
		"%{IncludeDir.GLFW}"
	}

	links
	{
		"GLFW",
		"opengl32.lib"
	}

	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }
		defines
		{
			"CROWN_PLATFORM_WINDOWS",
			"CROWN_BUILD_DLL",
			"GLFW_INCLUDE_NONE"
		}
		postbuildcommands
		{
			("{COPY} %{cfg.buildtarget.relpath} \"../bin/" .. outputdir .. "/Sandbox/\"")
		}

	filter "configurations:Debug"
		defines "CROWN_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "CROWN_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "CROWN_DIST"
		runtime "Release"
		optimize "on"

project "Sandbox"
	location "Sandbox"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}

	includedirs
	{
		"Crown/vendor/spdlog/include",
		"Crown/src"
	}

	links
	{
		"Crown"
	}

	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }
		defines
		{
			"CROWN_PLATFORM_WINDOWS"
		}

	filter "configurations:Debug"
		defines "CROWN_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "CROWN_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "CROWN_DIST"
		runtime "Release"
		optimize "on"
