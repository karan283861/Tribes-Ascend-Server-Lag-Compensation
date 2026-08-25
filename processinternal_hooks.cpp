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
	IS_ACTOR_VALID(projectile, ;)

	static auto& lag_compensation{LagCompensation::GetInstance()};

	lag_compensation.OnProjectileRadialDamage(calling_uobject, unused, stack, result);
}

UE3_PROCESSINTERNAL_HOOK(TrProjectilePostBeginPlay)
{
	auto projectile{reinterpret_cast<Projectile*>(calling_uobject)};
	IS_ACTOR_VALID(projectile, ;)

	original_processinternal(calling_uobject, unused, stack, result);

	static auto& lag_compensation{LagCompensation::GetInstance()};

	// No guarantee the projectile should be lag compensated
	// Doesn't really matter, forward it to lag compensation which will perform validation and take
	// care of it if needed
	lag_compensation.AddProjectile(projectile);
}

UE3_PROCESSINTERNAL_HOOK(UTProjectileDestroyed)
{
	auto projectile{reinterpret_cast<Projectile*>(calling_uobject)};

	// Check IS_ACTOR_VALID BEFORE calling engine function, because projectile will be marked for destruction
	// Actuallly, if we get here, it's already been marked for destruction!
	// IS_ACTOR_VALID(projectile, ;)

	original_processinternal(calling_uobject, unused, stack, result);

	static auto& lag_compensation{LagCompensation::GetInstance()};

	// No guarantee the projectile is actually lag compensated (has an actor information attached)
	// Doesn't really matter, forward it to lag compensation which will perform validation and take
	// care of it if needed
	lag_compensation.FreeActorInformation(projectile);
}

UE3_PROCESSINTERNAL_HOOK(TrPawnDied)
{
	original_processinternal(calling_uobject, unused, stack, result);

	auto player{reinterpret_cast<Player*>(calling_uobject)};
	static auto& lag_compensation{LagCompensation::GetInstance()};

	// A TrPawn may not be a Player, it could be a turret. Thus we don't do a IS_ACTOR_VALID check here

	// No guarantee the pawn is a player (could be a turret?). Doesn't really matter, forward it
	// to lag compensation which will perform validation and take care of it if needed
	lag_compensation.FreeActorInformation(player);
}