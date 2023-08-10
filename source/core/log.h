#pragma once

#include <string_view>
#include <format>

// Logging using std::format
namespace R3
{
	constexpr bool c_shouldOutputErrors = true;
	constexpr bool c_shouldOutputWarnings = true;
	constexpr bool c_shouldOutputInfo = true;

	template <typename... Args>
	void LogWarn(std::string_view rt_fmt_str, Args&&... args)
	{
		if constexpr (c_shouldOutputWarnings)
		{
			std::string m_msg = std::vformat(rt_fmt_str, std::make_format_args(args...));
			printf("Warning! %s\n", m_msg.c_str());	// faster than cout
		}
	}

	template <typename... Args>
	void LogError(std::string_view rt_fmt_str, Args&&... args)
	{
		if constexpr (c_shouldOutputErrors)
		{
			std::string m_msg = std::vformat(rt_fmt_str, std::make_format_args(args...));
			printf("Error! %s\n", m_msg.c_str());	// faster than cout
		}
	}

	template <typename... Args>
	void LogInfo(std::string_view rt_fmt_str, Args&&... args)
	{
		if constexpr (c_shouldOutputInfo)
		{
			std::string m_msg = std::vformat(rt_fmt_str, std::make_format_args(args...));
			printf("%s\n", m_msg.c_str());	// faster than cout
		}
	}
}