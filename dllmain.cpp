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
#include "hook.hpp"
#include "native_hooks.hpp"
#include "processinternal_hooks.hpp"

// #define HOOK_CALLFUNCTION
#define LOG_FILE_NAME "LagCompensationLog.txt"

constexpr size_t kProcessEventAddress{0x00456F90};
constexpr size_t kProcessInternalAddress{0x00459040};
constexpr size_t kCallFunctionAddress{0x0045AD20};

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
#if defined(_DEBUG) &&defined(HOOK_CALLFUNCTION)
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

	processevent_hooks = UFunctionHooks<ProcessEventPrototype>(original_processevent);
	processinternal_hooks = UFunctionHooks<ProcessInternalPrototype>(original_processinternal);
	callfunction_hooks = UFunctionHooks<CallFunctionPrototype>(original_callfunction);

	// When a projectile is created
	processinternal_hooks.AddHook("Function TribesGame.TrProjectile.PostBeginPlay", TrProjectilePostBeginPlay,
								  FunctionHookType::kPost);

	// When a projectile is destroyed
	processinternal_hooks.AddHook("Function UTGame.UTProjectile.Destroyed", UTProjectileDestroyed,
								  FunctionHookType::kPost);

	// When a projectile explodes to cause radial (splash) damage
	processinternal_hooks.AddHook("Function TribesGame.TrProjectile.HurtRadius_Internal", TrProjectileHurtRadiusInternal,
								  FunctionHookType::kPre, FunctionHookAbsorb::kAbsorb);

	// When a player pawn dies
	processinternal_hooks.AddHook("Function TribesGame.TrPawn.Died", TrPawnDied,
								  FunctionHookType::kPre);
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
