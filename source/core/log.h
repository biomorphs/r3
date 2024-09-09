#pragma once

#include "callback_array.h"
#include <string_view>
#include <format>

// Logging using std::format
namespace R3
{
	enum class LogType {
		Info,
		Warning,
		Error
	};
	using LogCallback = std::function<void(LogType, const std::string_view)>;
	using LogCallbacks = CallbackArray<LogCallback>;

	namespace Internals
	{
		constexpr bool c_shouldOutputErrors = true;
		constexpr bool c_shouldOutputWarnings = true;
		constexpr bool c_shouldOutputInfo = true;
		inline LogCallbacks& GlobalLogCallbacks()
		{
			static LogCallbacks callbacks;
			return callbacks;
		}
	}

	inline uint64_t LogRegisterCallback(LogCallback cb)
	{
		return Internals::GlobalLogCallbacks().AddCallback(cb);
	}

	inline void LogUnregisterCallback(uint64_t token)
	{
		Internals::GlobalLogCallbacks().RemoveCallback(token);
	}

	template <typename... Args>
	inline void LogWarn(std::string_view rt_fmt_str, Args&&... args)
	{
		if constexpr (Internals::c_shouldOutputWarnings)
		{
			std::string m_msg = std::vformat(rt_fmt_str, std::make_format_args(args...));
			printf("Warning! %s\n", m_msg.c_str());	// faster than cout
			Internals::GlobalLogCallbacks().Run(LogType::Warning, m_msg.c_str());
		}
	}

	template <typename... Args>
	inline void LogError(std::string_view rt_fmt_str, Args&&... args)
	{
		if constexpr (Internals::c_shouldOutputErrors)
		{
			std::string m_msg = std::vformat(rt_fmt_str, std::make_format_args(args...));
			printf("Error! %s\n", m_msg.c_str());	// faster than cout
			Internals::GlobalLogCallbacks().Run(LogType::Error, m_msg.c_str());
		}
	}

	template <typename... Args>
	inline void LogInfo(std::string_view rt_fmt_str, Args&&... args)
	{
		if constexpr (Internals::c_shouldOutputInfo)
		{
			std::string m_msg = std::vformat(rt_fmt_str, std::make_format_args(args...));
			printf("%s\n", m_msg.c_str());	// faster than cout
			Internals::GlobalLogCallbacks().Run(LogType::Info, m_msg.c_str());
		}
	}
}