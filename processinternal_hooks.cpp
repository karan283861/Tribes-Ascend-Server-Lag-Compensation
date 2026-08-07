#include <format>
#include <plog/Log.h>
#include <SdkHeaders.h>
#include <uhook.hpp>
#include "helper.hpp"
#include "processinternal_hooks.hpp"
#include "lag_compensation.hpp"
UE3_PROCESSINTERNAL_HOOK(TrProjectileHurtRadiusInternal)
{
	auto projectile{reinterpret_cast<Projectile*>(calling_uobject)};
	auto instigator{reinterpret_cast<Player*>(projectile->Instigator)};

	static auto &lag_compensation{LagCompensation::GetInstance()};
	auto projectile_information{lag_compensation.GetActorInformation(projectile)};

	// A non lag compensated projectile. Could be fired from a turret, vehicle, etc
	if (!projectile_information)
	{
		return original_processinternal(calling_uobject, unused, stack, result);
	}

	auto rewind{lag_compensation.RewindPlayers(projectile_information->ping_in_ms_)};
	// An invalid instigator means the instigator has Died or Destroy'ed
	if (rewind && IsValid(instigator))
	{
		auto player_information{lag_compensation.GetActorInformation(instigator)};
		if (PerformErrorCheck(!player_information, "A player deemed valid has no player information attached"))
		{
			original_processinternal(calling_uobject, unused, stack, result);
			lag_compensation.RestorePlayers();
			lag_compensation.FreeActorInformation(projectile);
			return;
		}

		instigator->SetLocation(player_information->tick_information_[0].location_);
	}

	// Apply splash (radial) damage.
	original_processinternal(calling_uobject, unused, stack, result);

	if (rewind)
	{
		lag_compensation.RestorePlayers();
	}

	lag_compensation.FreeActorInformation(projectile);
}

UE3_PROCESSINTERNAL_HOOK(TrProjectilePostBeginPlay)
{
	original_processinternal(calling_uobject, unused, stack, result);

	auto projectile{reinterpret_cast<Projectile*>(calling_uobject)};
	static auto &lag_compensation{LagCompensation::GetInstance()};

	lag_compensation.AddProjectile(projectile);
}

UE3_PROCESSINTERNAL_HOOK(UTProjectileDestroyed)
{
	original_processinternal(calling_uobject, unused, stack, result);

	auto projectile{reinterpret_cast<Projectile*>(calling_uobject)};
	static auto &lag_compensation{LagCompensation::GetInstance()};
	lag_compensation.FreeActorInformation(projectile);
}

UE3_PROCESSINTERNAL_HOOK(TrPawnDied)
{
	original_processinternal(calling_uobject, unused, stack, result);

	auto player{reinterpret_cast<Player*>(calling_uobject)};
	static auto &lag_compensation{LagCompensation::GetInstance()};

	// No guarantee the pawn is a player (could be a vehicle?)
	// Doesn't really matter, forward it lag compensation which will
	// take care of it if needed
	lag_compensation.FreeActorInformation(player);
}