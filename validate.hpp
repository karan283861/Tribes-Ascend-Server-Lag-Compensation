#pragma once

#include <format>
#include <string>
#include <sstream>
#include <cassert>
#include <source_location>
#include <stacktrace>
#include <plog/Log.h>

#if defined(_DEBUG)
#define PERFORM_ERROR_CHECKS
#endif

template <typename T>
inline constexpr bool always_false_v{false};

#if defined(PERFORM_ERROR_CHECKS)
inline constexpr bool kPerformErrorChecks{true};
#define PERFORM_ERROR_CHECK(error_condition, format_string, ...) PerformErrorCheck((error_condition), format_string, std::source_location::current(), std::string(__FUNCSIG__), __VA_ARGS__)
#else
inline constexpr bool kPerformErrorChecks{false};
// This is to prevent evaluation of error_condition when PERFORM_ERROR_CHECKS is undefined (kPerformErrorChecks == false)
#define PERFORM_ERROR_CHECK(error_condition, format_string, ...) false
#endif

template <typename... Args>
constexpr bool PerformErrorCheck(bool error_condition, const std::format_string<Args...> format_string, const std::source_location& source_location, const std::string& function_signature, Args&&... args)
{
	if constexpr (kPerformErrorChecks)
	{
		if (error_condition)
		{
			auto stack_trace{std::stacktrace::current()};
			std::ostringstream oss_stack_trace{};
			// oss_stack_trace << stack_trace;

			// We can probably skip entry at index 0 because that should be this function itself
			for (auto i{std::size_t(1)}; i < stack_trace.size(); i++)
			{
				const auto& frame{stack_trace[i]};
				oss_stack_trace << i << "."
								<< "\tnative\t=\t" << frame.native_handle()
								<< "\tdescription\t=\t[" << frame.description() << "]"
								<< "\tfile\t=\t[" << frame.source_file() << "]"
								<< "\tline\t=\t" << frame.source_line()
								<< '\n';
			}

			constexpr auto padding{std::size_t(20)};
			PLOG_ERROR << "\n" + std::string(10, '=') + "\n"
					   << std::format("Error:\t{}\t@\t{}:{}", std::format(format_string, std::forward<Args>(args)...), function_signature, source_location.line()) << "\n"
					   << std::string(10, '-') << "\n"
					   << oss_stack_trace.str() << "\n"
					   << std::string(10, '=') << "\n";
		};
		assert(!error_condition);
		return error_condition;
	}
	return false;
}