#include <algorithm>
#include <format>
#include <plog/Log.h>
#include "helper.hpp"
#include "native_hooks.hpp"
#include "lag_compensation.hpp"

static auto prevent_projectiles_from_ticking{false};

TickActorsPreAsyncWorkPrototype original_tickactors_preasyncwork = reinterpret_cast<TickActorsPreAsyncWorkPrototype>(kTickActorsPreAsyncWorkAddress);
void TickActorsPreAsyncWorkHook(UWorld *world, float delta_seconds,
								ELevelTick tick_type, FDeferredTickList &deferred_list)
{
	static auto &lag_compensation{LagCompensation::GetInstance()};

	// Prevent lag compensated projectiles from ticking normally via engine calls.
	// We will tick them when performing lag compensation.
	prevent_projectiles_from_ticking = true;
	original_tickactors_preasyncwork(world, delta_seconds, tick_type, deferred_list);
	prevent_projectiles_from_ticking = false;
	// Perform lag compensation.
	lag_compensation.Tick(delta_seconds, tick_type);
}

ActorTickPrototype original_actor_tick = reinterpret_cast<ActorTickPrototype>(kActorTickAddress);
void __fastcall ActorTickHook(AActor *actor, void *unused, float delta_seconds, ELevelTick tick_type)
{
	static const auto kPlayerClass{Player::StaticClass()};
	static auto &lag_compensation{LagCompensation::GetInstance()};

	auto projectile{reinterpret_cast<Projectile *>(actor)};
	auto player{reinterpret_cast<Player *>(actor)};

	LagCompensation::ActorInformation *actor_information{};

	// NOTE: Do NOT use IsA function in any repeatedly called function - it's expensive.
	if (actor->Class == kPlayerClass && IsPlayerValid(player))
	{
		original_actor_tick(actor, nullptr, delta_seconds, tick_type);
		lag_compensation.UpdatePlayer(player);
		lag_compensation.list_of_players_in_latest_tick_.push_back(player);
		return;
	}
	else if (prevent_projectiles_from_ticking && (actor_information = lag_compensation.IsActorLagCompensated(projectile, LagCompensation::ActorId::kProjectile)))
	{
		auto projectile_information{reinterpret_cast<LagCompensation::ProjectileInformation *>(actor_information)};
		if (projectile_information->is_owning_player_still_valid_ && !IsPlayerValid(projectile_information->owning_player_))
		{
			projectile_information->is_owning_player_still_valid_ = false;
		}

		if (lag_compensation.list_of_lag_compensated_projectiles_by_ping_in_latest_tick_[projectile_information->ping_in_ms_].size() == 0)
		{
			lag_compensation.pings_to_tick_in_latest_tick_.push_back(projectile_information->ping_in_ms_);
		}

		lag_compensation.list_of_lag_compensated_projectiles_by_ping_in_latest_tick_[projectile_information->ping_in_ms_].push_back(projectile);

		// Prevent lag compensated projectiles from ticking normally via engine calls.
		return;
	}

	original_actor_tick(actor, nullptr, delta_seconds, tick_type);
}