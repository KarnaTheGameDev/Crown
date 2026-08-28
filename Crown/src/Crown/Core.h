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

#define BIT(x) (1 << x)

#define CROWN_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)

#ifdef CROWN_DEBUG
	#define CROWN_ASSERT(x, ...) { if(!(x)) { CROWN_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define CROWN_CORE_ASSERT(x, ...) { if(!(x)) { CROWN_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define CROWN_ASSERT(x, ...)
	#define CROWN_CORE_ASSERT(x, ...)
#endif