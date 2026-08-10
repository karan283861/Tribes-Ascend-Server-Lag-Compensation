#include <Windows.h>
#include <chrono>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <plog/Log.h>

#include <detours.h>

#include <uhook.hpp>
#include "processinternal_hooks.hpp"
#include "native_hooks.hpp"

#include <SdkHeaders.h>

constexpr size_t kProcessEventAddress{0x00456F90};
constexpr size_t kProcessInternalAddress{0x00459040};
constexpr size_t kCallFunctionAddress{0x0045AD20};

void PerformUFunctionHooks(void)
{
	std::vector<std::pair<std::string, ProcessInternalPrototype>> ufunctions_to_hook{{"Function TribesGame.TrProjectile.PostBeginPlay", TrProjectilePostBeginPlay},
																					 {"Function UTGame.UTProjectile.Destroyed", UTProjectileDestroyed},
																					 {"Function TribesGame.TrProjectile.HurtRadius_Internal", TrProjectileHurtRadiusInternal},
																					 {"Function TribesGame.TrPawn.Died", TrPawnDied}};

	auto get_ufunction = [](const std::string &ufunction_name)
	{
		return UObject::FindObject<UFunction>(ufunction_name.c_str());
	};

	for (const auto [ufunction_name, hook_function] : ufunctions_to_hook)
	{
		auto ufunction_object{get_ufunction(ufunction_name)};
		if (!ufunction_object)
		{
			PLOG_ERROR << std::format("Failed to find UFunction {} by name", ufunction_name);
			continue;
		}
		ufunction_object->Func = hook_function; // Abosrbing
		PLOG_INFO << std::format("Successfully hooked {0}", ufunction_name);
	}
}

void OnDLLProcessAttach()
{
	auto base_address{reinterpret_cast<size_t>(GetModuleHandle(0))};

	auto now{std::chrono::system_clock::now()};
	auto log_with_date_string{std::format("ServerLagCompensation-{:%d-%m-%Y_%H-%M-%S}.txt", now)};
	static plog::RollingFileAppender<plog::TxtFormatter> file_appender(log_with_date_string.c_str());
	plog::init(plog::verbose, &file_appender);

	PLOG_INFO << std::format("Successfully Injected DLL");
	PLOG_INFO << std::format("Base address: {0}", reinterpret_cast<void*>(base_address));

	original_processinternal = reinterpret_cast<ProcessInternalPrototype>(kProcessInternalAddress);

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	// Hook native functions
	// Perform lag compensation after all actors have ticked
	DetourAttach(&(PVOID &)original_tickactors_preasyncwork, TickActorsPreAsyncWorkHook);
	// Prevent ticking of lag compensated projectiles AND store player information per tick
	// DetourAttach(&(PVOID &)original_actor_tick, ActorTickHook);
	// DetourAttach(&(PVOID &)original_pawn_tick, PawnTickHook);

	// Make sure all detours attaches are placed BEFORE this call
	// Make sure UFunctionHooks objects are created AFTER this call
	auto error{DetourTransactionCommit()};

	PerformUFunctionHooks();

	// TODO: Remove below
	// auto set_location_ufunction{UObject::FindObject<UFunction>("Function Engine.Actor.SetLocation")};
	// if (set_location_ufunction)
	// {
	// 	PLOG_INFO << std::format("SetLocation->Func offset = {:x}", reinterpret_cast<char*>(set_location_ufunction->Func) - reinterpret_cast<char*>(base_address));
	// }
	// else
	// {
	// 	PLOG_ERROR << "SetLocation UFunction not found";
	// }
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
