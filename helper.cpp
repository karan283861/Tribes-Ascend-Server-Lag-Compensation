#include <plog/Log.h>
#include "helper.hpp"
#include "validate.hpp"

bool IsActorValid(AActor* actor)
{
	// Basically checking the actor is not marked for destruction by the engine
	return actor && !actor->bPendingDelete && !actor->bDeleteMe;
}

template <>
bool IsValid(Controller* controller)
{
	return IsActorValid(controller);
}

template <>
bool IsValid(Player* player)
{
	// Not having a PlayerReplicationInfo means the Player pawn is unpossessed (no controller)
	// A player pawn can be dead but still not marked for destruction, so we ensure health is valid
	return IsActorValid(player) && player->PlayerReplicationInfo && player->PlayerReplicationInfo->Team && player->Health > 0;
}

template <>
bool IsValid(Projectile* projectile)
{
	return IsActorValid(projectile);
}

template <>
bool IsA<Controller>(AActor* actor)
{
	if (PERFORM_ERROR_CHECK(!actor, "Actor is nullptr"))
		return false;

	return actor && actor->Class == Controller::StaticClass();
}

template <>
bool IsA<Player>(AActor* actor)
{
	if (PERFORM_ERROR_CHECK(!actor, "Actor is nullptr"))
		return false;

	return actor && actor->Class == Player::StaticClass();
}

#if defined(PERFORM_ERROR_CHECKS)
template <>
bool IsA<Projectile>(AActor* actor)
{
	if (PERFORM_ERROR_CHECK(!actor, "Actor is nullptr"))
		return false;

	// The problem is that inheritence chains for Projectiles can vary quite a lot
	// e.g.:  TrProj_BoltLauncher -> TrProjectile
	// TrProj_AssaultRifle_MKD -> TrProj_AssaultRifle -> TrProjectile
	// TrProj_EMPGrenade_MKD -> TrProj_EMPGrenade -> TrProj_Grenade -> TrProjectile
	// And so on.
	// We can insert the expensive logic here, but we should really avoid using it

	// PLOG_WARNING << "This function should NOT be called!";

	// WARNING: uobject->IsA is "expensive"... look for alternatives for release builds
	// At the moment it can just return "true", we don't need this function except
	// to suppress compilation errors in LagCompensation::GetActorInformation,
	// LagCompensation::AllocateActorInformation, LagCompensation::FreeActorInformation

	return actor && actor->IsA(Projectile::StaticClass());
}
#endif

FVector Add_VectorVector(const FVector &A, const FVector &B)
{
	return FVector{A.X + B.X, A.Y + B.Y, A.Z + B.Z};
}

FVector Subtract_VectorVector(const FVector &A, const FVector &B)
{
	return FVector{A.X - B.X, A.Y - B.Y, A.Z - B.Z};
}

FVector Multiply_VectorFloat(const FVector &A, const float &B)
{
	return FVector{A.X * B, A.Y * B, A.Z * B};
}