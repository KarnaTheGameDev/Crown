#pragma once

#ifdef CROWN_PLATFORM_WINDOWS
	#ifdef CROWN_BUILD_DLL
		#define CROWN_API __declspec(dllexport)
	#else
		#define CROWN_API __declspec(dllimport)
	#endif
#else
	#error Crown only supports Windows!
#endif