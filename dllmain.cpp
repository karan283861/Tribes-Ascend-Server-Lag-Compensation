#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>

#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <plog/Log.h>
#ifndef _DEBUG
// #define PLOG_DISABLE_LOGGING
#endif

#include "Detours/include/detours.h"

#include "helper.hpp"
#include <uhook.hpp>
#include "processinternal_hooks.hpp"
#include "native_hooks.hpp"
#include "lag_compensation.hpp"

// #define HOOK_CALLFUNCTION
#define LOG_FILE_NAME "LagCompensationLog.txt"

constexpr size_t kProcessEventAddress{0x00456F90};
constexpr size_t kProcessInternalAddress{0x00459040};
constexpr size_t kCallFunctionAddress{0x0045AD20};

void ValidateUFunctionHookResult(const HookResult &hook_result, const std::string &name, const bool is_absorbing)
{
	switch (hook_result)
	{
	case HookResult::kSuccess:
	{
		PLOG_INFO << std::format("Successfully hooked {0}{1}", name, is_absorbing ? " [ABSORBING]" : "");
		break;
	}
	case HookResult::kFailedIncorrectHookTypeAndHookAbsorb:
	{
		PLOG_ERROR << std::format("Failed to hook {0} due to incorrect hook type and hook absorb", name);
		break;
	}
	case HookResult::kFailedToFindUFunction:
	{
		PLOG_ERROR << std::format("Failed to hook {0} as the UFunction was not found", name);
		break;
	}
	case HookResult::kFailedUFunctionOutOfBounds:
	{
		PLOG_ERROR << std::format("Failed to hook {0} as the UFunction index was out of bounds", name);
		break;
	}
	case HookResult::kFailedOverMaxHookCount:
	{
		PLOG_ERROR << std::format("Failed to hook {0} as the UFunction already has maximum number of hooks", name);
		break;
	}
	case HookResult::kFailedUnknownHookType:
	{
		PLOG_ERROR << std::format("Failed to hook {0} due to unknown hook type", name);
		break;
	}
	default:
	{
		PLOG_ERROR << std::format("Hooking {0} resulted in unhandled behaviour", name);
		break;
	}
	}
}

void PerformUFunctionHooks()
{
	std::vector<UFunctionHooks<ProcessInternalPrototype>::UFunctionHookInformation> processinternal_hooks_informations{
		// When a projectile is created
		{.name_ = "Function TribesGame.TrProjectile.PostBeginPlay", .hook_function_ = TrProjectilePostBeginPlay, .hook_type_ = FunctionHookType::kPost},
		// When a projectile is destroyed
		{.name_ = "Function UTGame.UTProjectile.Destroyed", .hook_function_ = UTProjectileDestroyed, .hook_type_ = FunctionHookType::kPost},
		// When a projectile explodes to cause radial (splash) damage
		{.name_ = "Function TribesGame.TrProjectile.HurtRadius_Internal", .hook_function_ = TrProjectileHurtRadiusInternal, .hook_type_ = FunctionHookType::kPre, .hook_absorb_ = FunctionHookAbsorb::kAbsorb},
		// When a player pawn dies
		{.name_ = "Function TribesGame.TrPawn.Died", .hook_function_ = TrPawnDied, .hook_type_ = FunctionHookType::kPre}};

	for (const auto &ufunction_hook_information : processinternal_hooks_informations)
	{
		const auto result{processinternal_hooks.AddHook(ufunction_hook_information)};
		ValidateUFunctionHookResult(result,
									ufunction_hook_information.name_,
									ufunction_hook_information.hook_absorb_ == FunctionHookAbsorb::kAbsorb);
	}

	processinternal_hooks.SetOriginalFunction(original_processinternal);
}

void OnDLLProcessAttach()
{
	auto base_address{reinterpret_cast<size_t>(GetModuleHandle(0))};

	std::filesystem::remove(LOG_FILE_NAME);

#if defined(_DEBUG)
	static plog::RollingFileAppender<plog::TxtFormatter> file_appender(LOG_FILE_NAME);
	plog::init(plog::info, &file_appender);
#else
	static plog::RollingFileAppender<plog::TxtFormatter> file_appender(LOG_FILE_NAME);
	plog::init(plog::info, &file_appender);
#endif
	PLOG_INFO << std::format("Successfully Injected DLL");
	PLOG_INFO << std::format("Base address: {0}", reinterpret_cast<void *>(base_address));

	original_processevent = reinterpret_cast<ProcessEventPrototype>(kProcessEventAddress);
	original_processinternal = reinterpret_cast<ProcessInternalPrototype>(kProcessInternalAddress);
	original_callfunction = reinterpret_cast<CallFunctionPrototype>(kCallFunctionAddress);

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

#if defined(_DEBUG)
	DetourAttach(&(PVOID &)original_processevent, ProcessEventHook);
#endif

	DetourAttach(&(PVOID &)original_processinternal, ProcessInternalHook);

	// For some reason multiple hooks on CallFunction (eg. multiple injected dlls) causes buggy behaviour/crashes.
	// We should really only hook CallFunction when tracing UFunctions in the logs (ie. debugging)
#if defined(_DEBUG) && defined(HOOK_CALLFUNCTION)
	DetourAttach(&(PVOID &)original_callfunction, CallFunctionHook);
#endif

	// Hook native functions

	// Perform lag compensation after all actors have ticked
	DetourAttach(&(PVOID &)original_tickactors_preasyncwork, TickActorsPreAsyncWorkHook);
	// Prevent ticking of lag compensated projectiles AND store player information per tick
	DetourAttach(&(PVOID &)original_actor_tick, ActorTickHook);

	// Make sure all detours attaches are placed BEFORE this call
	// Make sure UFunctionHooks objects are created AFTER this call
	auto error{DetourTransactionCommit()};

	PerformUFunctionHooks();

	static auto &lag_compensation{LagCompensation::GetInstance()};
	lag_compensation.UpdateTickRateVariables();
}

BOOL APIENTRY DllMain(HMODULE hModule,
					  DWORD ul_reason_for_call,
					  LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)OnDLLProcessAttach, NULL, NULL, NULL);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
