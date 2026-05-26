#include <format>
#include <plog/Log.h>
#include <SdkHeaders.h>
#include "helper.hpp"
#include "processinternal_hooks.hpp"
#include "lag_compensation.hpp"

UE3_PROCESSINTERNAL_HOOK(TrProjectileHurtRadiusInternal)
{
	auto projectile{reinterpret_cast<Projectile *>(calling_uobject)};

	static auto &lag_compensation{LagCompensation::GetInstance()};
	auto projectile_information{reinterpret_cast<LagCompensation::ProjectileInformation *>(lag_compensation.GetLagCompensationData(projectile))};

	if (!projectile_information)
	{
		return original_processinternal(calling_uobject, unused, stack, result);
	}

	if (projectile_information->is_owning_player_still_valid_ && !IsPlayerValid(projectile_information->owning_player_))
	{
		projectile_information->is_owning_player_still_valid_ = false;
	}

	auto rewind{lag_compensation.RewindPlayers(projectile_information->ping_in_ms_)};
	if (rewind && projectile_information->is_owning_player_still_valid_)
	{
		auto player_information{reinterpret_cast<LagCompensation::PlayerInformation *>(lag_compensation.GetLagCompensationData(projectile_information->owning_player_))};
		if (player_information)
		{
			projectile_information->owning_player_->SetLocation(player_information->tick_information_.Back().location_);
		}
	}

	// Apply splash (radial) damage.
	original_processinternal(calling_uobject, unused, stack, result);

	if (rewind)
	{
		lag_compensation.RestorePlayers();
	}

	lag_compensation.DestroyLagCompensationData(projectile);
}

UE3_PROCESSINTERNAL_HOOK(TrProjectilePostBeginPlay)
{
	auto projectile{reinterpret_cast<Projectile *>(calling_uobject)};

	static auto &lag_compensation{LagCompensation::GetInstance()};

	auto controller{reinterpret_cast<Controller *>(projectile->InstigatorController)};
	if (controller && controller->Class == kControllerClass)
	{
		lag_compensation.LagCompensate(projectile);
	}
}

UE3_PROCESSINTERNAL_HOOK(UTProjectileDestroyed)
{
	auto projectile{reinterpret_cast<Projectile *>(calling_uobject)};
	static auto &lag_compensation{LagCompensation::GetInstance()};
	lag_compensation.DestroyLagCompensationData(projectile);
}

UE3_PROCESSINTERNAL_HOOK(TrPawnDied)
{
	auto player{reinterpret_cast<Player *>(calling_uobject)};
	static auto &lag_compensation{LagCompensation::GetInstance()};
	lag_compensation.DestroyLagCompensationData(player);
}