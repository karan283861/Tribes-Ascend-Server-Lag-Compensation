#include <cstdlib>
#include <plog/Log.h>
#include "helper.hpp"

template <>
bool Is<Controller>(AActor* actor)
{
	return actor->Class == Controller::StaticClass();
}

template <>
bool Is<Player>(AActor* actor)
{
	return actor->Class == Player::StaticClass();
}

template <>
bool Is<Projectile>(AActor* actor)
{
	// WARNING: This is only returning true to suppress compilation errors
	// The function is currently unimplemented, so DO NOT CALL IT
	return true;
}

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