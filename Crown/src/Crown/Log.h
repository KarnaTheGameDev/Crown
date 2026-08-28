#pragma once
#include "Core.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"
#include <memory>

namespace Crown
{
	class CROWN_API Log
	{
	public:
		static void Init();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}


//Core log macros
#define CROWN_CORE_TRACE(...)    ::Crown::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define CROWN_CORE_INFO(...)     ::Crown::Log::GetCoreLogger()->info(__VA_ARGS__)
#define CROWN_CORE_WARN(...)     ::Crown::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define CROWN_CORE_ERROR(...)    ::Crown::Log::GetCoreLogger()->error(__VA_ARGS__)
#define CROWN_CORE_FATAL(...)    ::Crown::Log::GetCoreLogger()->critical(__VA_ARGS__)

//Client log macros
#define CROWN_TRACE(...)         ::Crown::Log::GetClientLogger()->trace(__VA_ARGS__)
#define CROWN_INFO(...)          ::Crown::Log::GetClientLogger()->info(__VA_ARGS__)
#define CROWN_WARN(...)          ::Crown::Log::GetClientLogger()->warn(__VA_ARGS__)
#define CROWN_ERROR(...)         ::Crown::Log::GetClientLogger()->error(__VA_ARGS__)
#define CROWN_FATAL(...)         ::Crown::Log::GetClientLogger()->critical(__VA_ARGS__)