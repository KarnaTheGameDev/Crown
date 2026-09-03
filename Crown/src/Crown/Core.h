#pragma once

#ifndef CROWN_PLATFORM_WINDOWS
	#error Crown only supports Windows!
#endif

// Crown is a static library, so nothing needs exporting. Keeping the macro
// means the class declarations do not have to change if it ever becomes a DLL
// again - though that would bring back the problem this solved: ImGui keeps
// its context in a global, and a DLL boundary gives each module its own.
#define CROWN_API

#define BIT(x) (1 << x)

#define CROWN_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)

#ifdef CROWN_DEBUG
	#define CROWN_ASSERT(x, ...) { if(!(x)) { CROWN_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define CROWN_CORE_ASSERT(x, ...) { if(!(x)) { CROWN_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define CROWN_ASSERT(x, ...)
	#define CROWN_CORE_ASSERT(x, ...)
#endif