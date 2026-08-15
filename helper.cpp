#include <plog/Log.h>
#include "helper.hpp"
#include "validate.hpp"

bool IsActorValid(AActor* actor)
{
	return actor && !actor->bPendingDelete && !actor->bDeleteMe;
}

template <>
bool IsValid<Controller>(Controller* controller)
{
	// If PERFORM_ERROR_CHECKS is enabled and the parameter is nullptr,
	// we will crash attempting to call ->GetFullName()
	if (PERFORM_ERROR_CHECK(!controller, "Controller is nullptr"))
		return false;

	// Make sure the controller was actually the right type, and not just casted as a Controller
	if (PERFORM_ERROR_CHECK(!controller->IsA(Controller::StaticClass()), "Controller is not a {}, it is a {}",
							typeid(Controller).name(), controller->GetFullName()))
		return false;

	// We've ensured controller inherits from Controller, but we need to check if it passes our own IsA check
	if (PERFORM_ERROR_CHECK(!IsA<Controller>(controller), "Controller failed the IsA<{}> check, it is a {}",
							typeid(Controller).name(), controller->GetFullName()))
		return false;

	return IsActorValid(controller);
}

template <>
bool IsValid<Player>(Player* player)
{
	// If PERFORM_ERROR_CHECKS is enabled and the parameter is nullptr,
	// we will crash attempting to call ->GetFullName()
	if (PERFORM_ERROR_CHECK(!player, "Player is nullptr"))
		return false;

	// Make sure the player was actually the right type, and not just casted as a Player
	if (PERFORM_ERROR_CHECK(!player->IsA(Player::StaticClass()), "Player is not a {}, it is a {}",
							typeid(Player).name(), player->GetFullName()))
		return false;

	// We've ensured player inherits from Player, but we need to check if it passes our own IsA check
	if (PERFORM_ERROR_CHECK(!IsA<Player>(player), "Player failed the IsA<{}> check, it is a {}",
							typeid(Player).name(), player->GetFullName()))
		return false;

	// Not having a PlayerReplicationInfo means the Player pawn is unpossessed (no controller)
	// A player pawn can be dead but still not marked for destruction, so we ensure health is valid
	return IsActorValid(player) && player->PlayerReplicationInfo && player->PlayerReplicationInfo->Team && player->Health > 0;
}

template <>
bool IsValid<Projectile>(Projectile* projectile)
{
	// If PERFORM_ERROR_CHECKS is enabled and the parameter is nullptr,
	// we will crash attempting to call ->GetFullName()
	if (PERFORM_ERROR_CHECK(!projectile, "Projectile is nullptr"))
		return false;

	// Make sure the projectile was actually the right type, and not just casted as a Projectile
	if (PERFORM_ERROR_CHECK(!projectile->IsA(Projectile::StaticClass()), "Projectile is not a {}, it is a {}",
							typeid(Projectile).name(), projectile->GetFullName()))
		return false;

	// We've ensured projectile inherits from Projectile, but we need to check if it passes our own IsA check
	if (PERFORM_ERROR_CHECK(!IsA<Projectile>(projectile), "Projectile failed the IsA<{}> check, it is a {}",
							typeid(Projectile).name(), projectile->GetFullName()))
		return false;

	return IsActorValid(projectile);
}

template <>
bool IsA<Controller>(UObject* object)
{
	if (!object)
	{
		return false;
	}

	const auto* controller_class{Controller::StaticClass()};
	const auto* super_class{object->Class->SuperField};
	// Most controllers will be of type Controller, very unlikey they will be a derived type
	auto result{(object->Class == controller_class) ||
		 (super_class && super_class == controller_class)};
	return result;
}

template <>
bool IsA<Player>(UObject* object)
{
	if (!object)
	{
		return false;
	}

	const auto* player_class{Player::StaticClass()};
	const auto* super_class{object->Class->SuperField};
	// Most players will be of type PLayer, very unlikey they will be a derived type
	auto result{(object->Class == player_class) ||
		 (super_class && super_class == player_class)};
	return result;
}

template <>
bool IsA<Projectile>(UObject* object)
{
	if (!object)
	{
		return false;
	}

	return object->IsA(Projectile::StaticClass());
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