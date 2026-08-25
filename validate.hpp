#pragma once

#include <format>
#include <string>
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
#define PERFORM_ERROR_CHECK(error_condition, diagnostic_string, ...) PerformErrorCheck((error_condition), std::string(diagnostic_string), std::string(__FUNCSIG__), __VA_ARGS__)
#else
inline constexpr bool kPerformErrorChecks{false};
// This is to prevent evaluation of error_condition when PERFORM_ERROR_CHECKS is undefined (kPerformErrorChecks == false)
#define PERFORM_ERROR_CHECK(error_condition, diagnostic_string, ...) false
#endif

struct DiagnosticMessage
{
	std::string format_string_;
	std::source_location source_location_;
	std::string function_name_{};
	size_t line_{};

	// template <typename StringType>
	DiagnosticMessage(const std::string& format_string, std::source_location source_location = std::source_location::current())
		: format_string_(format_string), source_location_(source_location), function_name_(source_location_.function_name()), line_(source_location_.line())
	{
	}

	operator const std::string&() const
	{
		return format_string_;
	}
};

template <typename... Args>
constexpr bool PerformErrorCheck(bool error_condition, const DiagnosticMessage& diagnostic_message, const std::string& function_signature, Args&&... args)
{
	if constexpr (kPerformErrorChecks)
	{
		if (error_condition)
		{
			auto stack_trace{std::stacktrace::current()};
			std::ostringstream oss_stack_trace{};
			// oss_stack_trace << stack_trace;

			for (auto i{std::size_t(0)}; i < stack_trace.size(); ++i)
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
			auto full_format_string{"Error:\t" + static_cast<std::string>(diagnostic_message) + "\t@\t{}:{}"};
			PLOG_ERROR << "\n" + std::string(10, '=') + "\n"
					   << std::vformat(full_format_string, std::make_format_args(args..., function_signature, diagnostic_message.line_)) << "\n"
					   << std::string(10, '-') << "\n"
					   << oss_stack_trace.str() << "\n"
					   << stack_trace.size() << "\n"
					   << std::string(10, '=') << "\n";
		};
		assert(!error_condition);
		return error_condition;
	}
	return false;
}