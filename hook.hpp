#pragma once

#include <format>
#include <plog/Log.h>
#include <unordered_map>
#include <memory>
#include <vector>
#include "Tribes-Ascend-SDK/SdkHeaders.h"

using ProcessEventPrototype = void(__fastcall *)(UObject *calling_uobject,
												 void *unused, UFunction *calling_ufunction,
												 void *parameters, void *result);

void __fastcall ProcessEventHook(UObject *calling_uobject,
								 void *unused, UFunction *calling_ufunction,
								 void *parameters, void *result);

extern ProcessEventPrototype original_processevent;

class FFrame
{
public:
	void *vmt_{};
	char unknown_data_0[12]{};
	UStruct *node_{};
	UObject *object_{};
	unsigned char *code_{};
	char *locals_{};
	int line_nums_{};
	FFrame *previous_frame_{};
};

using ProcessInternalPrototype = void(__fastcall *)(UObject *calling_uobject,
													void *unused, FFrame &stack,
													void *result);

void __fastcall ProcessInternalHook(UObject *calling_uobject,
									void *unused, FFrame &stack,
									void *result);

extern ProcessInternalPrototype original_processinternal;

using CallFunctionPrototype = void(__fastcall *)(UObject *calling_uobject,
												 void *unused, FFrame &stack,
												 void *result, UFunction *calling_ufunction);

void __fastcall CallFunctionHook(UObject *calling_uobject,
								 void *unused, FFrame &stack,
								 void *result, UFunction *calling_ufunction);

extern CallFunctionPrototype original_callfunction;

enum class FunctionHookType
{
	kUnknown,
	kPre,
	kPost,
	kPreAndPost
};

enum class FunctionHookAbsorb
{
	kUnknown,
	kDoNotAbsorb,
	kAbsorb
};

template <typename UFunctionHookPrototype>
class UFunctionHooks
{
	class UFunctionHookInformation
	{
	public:
		std::string name_{};
		UFunction *ufunction_{};
		UFunctionHookPrototype hook_function_{};
		FunctionHookType hook_type_{FunctionHookType::kUnknown};
		FunctionHookAbsorb hook_absorb_{FunctionHookAbsorb::kUnknown};
	};

	class UFunctionHooksInformation
	{
		using Hooks = std::vector<UFunctionHookInformation>;

	public:
		Hooks pre_hooks_{};
		Hooks post_hooks_{};
	};

	// 200000 indexes should be enough to hold all possible UFunction internal object indexes
	std::vector<UFunctionHooksInformation> ufunction_internal_index_to_hook_information = std::vector<UFunctionHooksInformation>(200000);

public:
	UFunctionHookPrototype original_function_{};
	UFunctionHooks(UFunctionHookPrototype original_function) : original_function_(original_function) {};

public:
	void AddHook(const std::string &ufunction_as_string, UFunctionHookPrototype hook_function,
				 FunctionHookType hook_type = FunctionHookType::kPre,
				 FunctionHookAbsorb hook_absorb = FunctionHookAbsorb::kDoNotAbsorb)
	{
		auto ufunction_object{reinterpret_cast<UFunction *>(UObject::FindObject<UFunction>(ufunction_as_string.c_str()))};
		if (ufunction_object)
		{
			PLOG_INFO << std::format("Found UFunction: {0}", ufunction_as_string);
			UFunctionHookInformation ufunction_hook_information{ufunction_as_string, ufunction_object,
																hook_function, hook_type, hook_absorb};
			switch (hook_type)
			{
			case FunctionHookType::kPre:
			{
				ufunction_internal_index_to_hook_information[ufunction_object->ObjectInternalInteger].pre_hooks_.push_back(std::move(ufunction_hook_information));
				break;
			}
			case FunctionHookType::kPost:
			{
				ufunction_internal_index_to_hook_information[ufunction_object->ObjectInternalInteger].post_hooks_.push_back(std::move(ufunction_hook_information));
				break;
			}
			case FunctionHookType::kPreAndPost:
			{
				ufunction_internal_index_to_hook_information[ufunction_object->ObjectInternalInteger].pre_hooks_.push_back(ufunction_hook_information);
				ufunction_internal_index_to_hook_information[ufunction_object->ObjectInternalInteger].post_hooks_.push_back(ufunction_hook_information);
				break;
			}
			}

			if (hook_absorb == FunctionHookAbsorb::kAbsorb)
			{
				PLOG_WARNING << std::format("Added an abosrbing hook to UFunction: {0}", ufunction_as_string);
			}
		}
		else
		{
			PLOG_ERROR << std::format("Failed to find UFunction: {0}", ufunction_as_string);
		}
	}

	template <typename... Args>
	const void ExecuteHook(UFunction *ufunction, Args &&...args)
	{
		if (!original_function_)
		{
			return;
		}

		auto hooks{GetHooks(ufunction)};

		if (!hooks)
		{
			original_function_(std::forward<Args>(args)...);
		}
		else
		{
			auto absorbs{false};
			for (auto &ufunction_hook_information : hooks->pre_hooks_)
			{
				ufunction_hook_information.hook_function_(args...);
				absorbs |= ufunction_hook_information.hook_absorb_ == FunctionHookAbsorb::kAbsorb;
			}

			if (!absorbs)
			{
				original_function_(std::forward<Args>(args)...);

				for (auto &ufunction_hook_information : hooks->post_hooks_)
				{
					ufunction_hook_information.hook_function_(args...);
				}
			}
		}
	}

private:
	const UFunctionHooksInformation *GetHooks(const UFunction *ufunction_object)
	{
		if (ufunction_object && ufunction_object->ObjectInternalInteger < ufunction_internal_index_to_hook_information.size())
		{
			return &ufunction_internal_index_to_hook_information[ufunction_object->ObjectInternalInteger];
		}
		else
		{
			return nullptr;
		}
	}
};

extern UFunctionHooks<ProcessEventPrototype> processevent_hooks;
extern UFunctionHooks<ProcessInternalPrototype> processinternal_hooks;
extern UFunctionHooks<CallFunctionPrototype> callfunction_hooks;