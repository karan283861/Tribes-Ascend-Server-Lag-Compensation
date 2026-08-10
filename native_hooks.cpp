#include <plog/Log.h>
#include "SdkHeaders.h"
#include "native_hooks.hpp"
#include "lag_compensation.hpp"

bool ticking_TG_PreAsyncWork{false}; // We are in TG_PreAsyncWork tick group
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

PawnTickPrototype original_pawn_tick{reinterpret_cast<PawnTickPrototype>(kPawnTickAddress)};

WorldFarMoveActor original_world_farmoveactor{reinterpret_cast<WorldFarMoveActor>(kWorldFarMoveActorAddress)};