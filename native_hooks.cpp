#include <format>
#include <plog/Log.h>
#include "SdkHeaders.h"
#include "helper.hpp"
#include "native_hooks.hpp"
#include "lag_compensation.hpp"

static auto ticking_TG_PreAsyncWork{false}; // We are in TG_PreAsyncWork tick group
UWorld* global_world{};

TickActorsPreAsyncWorkPrototype original_tickactors_preasyncwork{reinterpret_cast<TickActorsPreAsyncWorkPrototype>(kTickActorsPreAsyncWorkAddress)};
void TickActorsPreAsyncWorkHook(UWorld* world, float delta_seconds,
								ELevelTick tick_type, FDeferredTickList &deferred_list)
{
	// All players and projectiles appear to tick in the TG_PreAsyncWork tick group
	// We can optimise by having a global flag to capture when we are in TG_PreAsyncWork
	// so we can ignore actors ticked outside of TG_PreAsyncWork

	global_world = world;
	static auto &lag_compensation{LagCompensation::GetInstance()};

	// Prevent lag compensated projectiles from ticking normally via engine calls
	// We will tick them when performing lag compensation
	ticking_TG_PreAsyncWork = true;
	original_tickactors_preasyncwork(world, delta_seconds, tick_type, deferred_list);
	ticking_TG_PreAsyncWork = false;
	// Perform lag compensation
	lag_compensation.Tick(delta_seconds, tick_type);
}

ActorTickPrototype original_actor_tick{reinterpret_cast<ActorTickPrototype>(kActorTickAddress)};
void __fastcall ActorTickHook(AActor* actor, void* unused, float delta_seconds, ELevelTick tick_type)
{
	if (!ticking_TG_PreAsyncWork)
	{
		return original_actor_tick(actor, nullptr, delta_seconds, tick_type);
	}

	static auto &lag_compensation{LagCompensation::GetInstance()};

	auto projectile{reinterpret_cast<Projectile*>(actor)};
	auto player{reinterpret_cast<Player*>(actor)};

	LagCompensation::ActorInformation<Projectile>* projectile_information{};

	if (Is<Player>(player))
	{
		original_actor_tick(actor, nullptr, delta_seconds, tick_type);
		// NOTE: After ticking Player *may* become invalid. Ensure to call engine tick first
		// All Players are ticked in lag compensation
		// NOTE: Validation happens inside the OnActorTick<Player> function
		lag_compensation.OnActorTick(player);
		return;
	}
	else if (lag_compensation.GetActorInformation<true>(projectile))
	{
		// This actor is a projectile with a projectile information attached, so it should be ticked
		// in lag compensation
		lag_compensation.OnActorTick(projectile);
		// Prevent lag compensated projectiles from ticking normally via engine calls.
		return;
	}

	original_actor_tick(actor, nullptr, delta_seconds, tick_type);
}

WorldFarMoveActor original_world_farmoveactor{reinterpret_cast<WorldFarMoveActor>(kWorldFarMoveActorAddress)};