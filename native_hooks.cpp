#include <format>
#include <plog/Log.h>
#include "SdkHeaders.h"
#include "helper.hpp"
#include "native_hooks.hpp"
#include "lag_compensation.hpp"

static auto prevent_projectiles_from_ticking{false};
UWorld* global_world{};

TickActorsPreAsyncWorkPrototype original_tickactors_preasyncwork{reinterpret_cast<TickActorsPreAsyncWorkPrototype>(kTickActorsPreAsyncWorkAddress)};
void TickActorsPreAsyncWorkHook(UWorld* world, float delta_seconds,
								ELevelTick tick_type, FDeferredTickList &deferred_list)
{
	global_world = world;
	static auto &lag_compensation{LagCompensation::GetInstance()};

	// Prevent lag compensated projectiles from ticking normally via engine calls.
	// We will tick them when performing lag compensation.
	prevent_projectiles_from_ticking = true;
	original_tickactors_preasyncwork(world, delta_seconds, tick_type, deferred_list);
	prevent_projectiles_from_ticking = false;
	// Perform lag compensation.
	lag_compensation.Tick(delta_seconds, tick_type);
}

ActorTickPrototype original_actor_tick{reinterpret_cast<ActorTickPrototype>(kActorTickAddress)};
void __fastcall ActorTickHook(AActor* actor, void* unused, float delta_seconds, ELevelTick tick_type)
{
	static auto &lag_compensation{LagCompensation::GetInstance()};

	auto projectile{reinterpret_cast<Projectile*>(actor)};
	auto player{reinterpret_cast<Player*>(actor)};

	LagCompensation::ActorInformation<Projectile>* projectile_information{};

	if (Is<Player>(player))
	{
		original_actor_tick(actor, nullptr, delta_seconds, tick_type);
		lag_compensation.OnActorTick(player);
		return;
	}
	else if (prevent_projectiles_from_ticking && (projectile_information = lag_compensation.GetActorInformation<true>(projectile)))
	{
		lag_compensation.OnActorTick(projectile);
		// Prevent lag compensated projectiles from ticking normally via engine calls.
		return;
	}

	original_actor_tick(actor, nullptr, delta_seconds, tick_type);
}

WorldFarMoveActor original_world_farmoveactor{reinterpret_cast<WorldFarMoveActor>(kWorldFarMoveActorAddress)};