#include <format>
#include <string>
#include <cassert>
#include <source_location>
#include <plog/Log.h>

#if defined(PERFORM_ERROR_CHECKS)
inline constexpr bool kPerformErrorChecks = true;
#define PERFORM_ERROR_CHECK(error_condition, diagnostic_string, ...) PerformErrorCheck(error_condition, diagnostic_string, __VA_ARGS__)
#else
inline constexpr bool kPerformErrorChecks = false;
#define PERFORM_ERROR_CHECK(error_condition, diagnostic_string, ...) false
#endif

struct DiagnosticMessage
{
	std::string format_string_;
	std::source_location source_location_;
	std::string function_name_{};
	size_t line_{};

	template <typename StringType>
	DiagnosticMessage(const StringType &format_string, std::source_location source_location = std::source_location::current())
		: format_string_(format_string), source_location_(source_location), function_name_(source_location_.function_name()), line_(source_location_.line())
	{
	}

	operator const std::string&() const
	{
		return format_string_;
	}
};

template <typename... Args>
constexpr bool PerformErrorCheck(bool error_condition, const DiagnosticMessage &diagnostic_message, Args &&... args)
{
	if (error_condition)
	{
		auto full_format_string{"Error:\t" + static_cast<std::string>(diagnostic_message) + "\t@\t{}:{}"};
		PLOG_ERROR << std::vformat(full_format_string, std::make_format_args(args..., diagnostic_message.function_name_, diagnostic_message.line_));
	};
	assert(!error_condition);
	return error_condition;
	return false;
}
