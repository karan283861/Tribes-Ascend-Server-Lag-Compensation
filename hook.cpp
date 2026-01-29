#include <format>
#include <string>
#include "hook.hpp"

size_t indent_level{0};

std::string GetIndentedTabString(void)
{
	std::string s;
	for (int i = 0; i < indent_level; i++)
	{
		s.append("\t");
	}
	return s;
}

ProcessEventPrototype original_processevent{reinterpret_cast<ProcessEventPrototype>(0)};
UFunctionHooks<ProcessEventPrototype> processevent_hooks(nullptr);
void __fastcall ProcessEventHook(UObject *calling_uobject,
								 void *unused, UFunction *calling_ufunction,
								 void *parameters, void *result)
{
	indent_level++;
	PLOG_VERBOSE << GetIndentedTabString() << std::format("{0} : {1}", calling_ufunction->GetFullName(), calling_uobject->GetFullName());
	processevent_hooks.ExecuteHook(calling_ufunction, calling_uobject, unused, calling_ufunction, parameters, result);
	indent_level--;
}

ProcessInternalPrototype original_processinternal{reinterpret_cast<ProcessInternalPrototype>(0)};
UFunctionHooks<ProcessInternalPrototype> processinternal_hooks(nullptr);
void __fastcall ProcessInternalHook(UObject *calling_uobject,
									void *unused, FFrame &stack,
									void *result)
{
	indent_level++;
	auto calling_ufunction = reinterpret_cast<UFunction *>(stack.node_);
	PLOG_VERBOSE << GetIndentedTabString() << std::format("{0} : {1}", calling_ufunction->GetFullName(), calling_uobject->GetFullName());
	processinternal_hooks.ExecuteHook(calling_ufunction, calling_uobject, unused, stack, result);
	indent_level--;
}

CallFunctionPrototype original_callfunction{reinterpret_cast<CallFunctionPrototype>(0)};
UFunctionHooks<CallFunctionPrototype> callfunction_hooks(nullptr);
void __fastcall CallFunctionHook(UObject *calling_uobject,
								 void *unused, FFrame &stack,
								 void *result, UFunction *calling_ufunction)
{
	if (!callfunction_hooks.original_function_)
	{
		return original_callfunction(calling_uobject, unused, stack, result, calling_ufunction);
	}

	static constexpr unsigned int kFUNC_Native{0x00000400};
	auto is_native{calling_ufunction->iNative};
	auto is_funcnative{calling_ufunction->FunctionFlags & kFUNC_Native};

	indent_level++;
	PLOG_VERBOSE << GetIndentedTabString() << std::format("{0} {1} : {2}", is_native ? "[NATIVE]" : (is_funcnative ? "[FUNC_Native]" : ""), calling_ufunction->GetFullName(), calling_uobject->GetFullName());
	callfunction_hooks.ExecuteHook(calling_ufunction, calling_uobject, unused, stack, result, calling_ufunction);
	indent_level--;
}