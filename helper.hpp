#pragma once

#include <format>
#include <vector>
#include <type_traits>
#include <SdkHeaders.h>
#include "validate.hpp"

/// @brief Alias for the runtime actor type
using Actor = AActor;
/// @brief Alias for the runtime player controller type
using Controller = ATrPlayerController;
/// @brief Alias for the runtime player type
using Player = ATrPlayerPawn;
/// @brief Alias for the runtime projectile type
using Projectile = ATrProjectile;
/// @brief Alias for the RTT ping of a player
using Ping = float;
/// @brief Alias for the team of a player
using Team = int;

template <typename T>
using rcvref = std::remove_cvref_t<T>;

template <typename ActorType>
concept IsAnActor = std::derived_from<ActorType, Actor>;

#define IS_ACTOR_TYPE_VALID(POINTER_TO_ACTOR, STATEMENT_ON_ERROR)                                       \
	if (PERFORM_ERROR_CHECK(!POINTER_TO_ACTOR,                                                          \
							"{} is a {} and is nullptr",                                                \
							#POINTER_TO_ACTOR,                                                          \
							typeid(decltype(*POINTER_TO_ACTOR)).name()))                                \
		STATEMENT_ON_ERROR;                                                                             \
                                                                                                        \
	/* Make sure the actor was actually the right type, and not wrongly casted */                       \
	if (PERFORM_ERROR_CHECK(!POINTER_TO_ACTOR->IsA(rcvref<decltype(*POINTER_TO_ACTOR)>::StaticClass()), \
							"{} is not a {}, it is a {}",                                               \
							#POINTER_TO_ACTOR,                                                          \
							typeid(decltype(*POINTER_TO_ACTOR)).name(),                                 \
							POINTER_TO_ACTOR->GetFullName()))                                           \
		STATEMENT_ON_ERROR;                                                                             \
                                                                                                        \
	/* We also need to check if the actor passes our own IsA check */                                   \
	if (PERFORM_ERROR_CHECK(!IsA<rcvref<decltype(*POINTER_TO_ACTOR)>>(POINTER_TO_ACTOR),                \
							"{} failed the IsA<{}> check, it is a {}",                                  \
							#POINTER_TO_ACTOR,                                                          \
							typeid(decltype(*POINTER_TO_ACTOR)).name(),                                 \
							POINTER_TO_ACTOR->GetFullName()))                                           \
		STATEMENT_ON_ERROR;

#define IS_ACTOR_VALID(POINTER_TO_ACTOR, STATEMENT_ON_ERROR)             \
	IS_ACTOR_TYPE_VALID(POINTER_TO_ACTOR, STATEMENT_ON_ERROR)            \
	if (PERFORM_ERROR_CHECK(::IsValid(POINTER_TO_ACTOR),                 \
							"{} is a {} and failed IsValid check",       \
							#POINTER_TO_ACTOR,                           \
							typeid(decltype(*POINTER_TO_ACTOR)).name())) \
		STATEMENT_ON_ERROR;

/**
 * @brief Check if an actor derived from AActor is valid
 *
 * Check the AActor parameter is non null and not Destroy'd (pending deletion)
 *
 * @param actor Pointer to an actor derived from AActor
 */
bool IsActorValid(Actor* actor);

/**
 * @brief Check if an actor derived from ActorType is valid
 *
 * @tparam ActorType A typename that is derived from AActor
 * @param actor Pointer to an actor derived from ActorType
 */
template <IsAnActor ActorType>
bool IsValid(ActorType* actor)
{
	IS_ACTOR_TYPE_VALID(actor, return false);

	if constexpr (std::is_same_v<ActorType, Controller>)
	{
		auto controller{reinterpret_cast<Controller*>(actor)};
		return IsActorValid(actor);
	}
	else if constexpr (std::is_same_v<ActorType, Player>)
	{
		auto player{reinterpret_cast<Player*>(actor)};
		return IsActorValid(player) &&
			   player->Health > 0 &&
			   player->PlayerReplicationInfo &&
			   player->PlayerReplicationInfo->Team;
	}
	else if constexpr (std::is_same_v<ActorType, Projectile>)
	{
		auto projectile{reinterpret_cast<Projectile*>(actor)};
		return IsActorValid(projectile);
	}
}

/**
 * @brief Check if an object derived from UObject is exactly of ObjectType
 *
 * Directly compares the Class member of the UObject parameter with ObjectType::StaticClass()
 *
 * @tparam ObjectType A typename that is derived from UObject
 * @param object Pointer to an object derived from UObject
 */
template <typename ObjectType>
ObjectType* Is(UObject* object)
{
	return (object && object->Class == ObjectType::StaticClass()) ? reinterpret_cast<ObjectType*>(object) : nullptr;
}

/**
 * @brief Check if an actor is/derived from ActorType
 *
 * Loops through the UObject parameters class chain and compares with ActorType::StaticClass()
 *
 * @attention This function should be specialised for efficiency
 *
 * @tparam ActorType A typename that is derived from AActor
 * @param actor Pointer to an object derived from AActor
 *
 * @warning This function is expensive
 */
template <IsAnActor ActorType>
ActorType* IsA(Actor* actor)
{
	if (!actor)
	{
		return nullptr;
	}

	auto casted_actor{reinterpret_cast<ActorType*>(actor)};

	if constexpr (std::is_same_v<ActorType, Controller>)
	{
		// Controller is not a final class. There are types derived from it
		// The immediate derived types of Controller are final

		// auto controller{reinterpret_cast<Controller*>(actor)};
		const auto* controller_class{Controller::StaticClass()};
		const auto* super_class{actor->Class->SuperField};

		// Most controllers will be of type Controller, very unlikey they will be a derived type
		auto result = (actor->Class == controller_class) ||
					  (super_class == controller_class);
		return result ? casted_actor : nullptr;
	}
	else if constexpr (std::is_same_v<ActorType, Player>)
	{
		// Player is not a final class. There are types derived from it
		// The immediate derived types of Player ARE final

		// auto player{reinterpret_cast<Player*>(actor)};
		const auto* player_class{Player::StaticClass()};
		const auto* super_class{actor->Class->SuperField};
		// Most players will be of type Player, very unlikey they will be a derived type
		auto result = (actor->Class == player_class) ||
					  (super_class == player_class);
		return result ? casted_actor : nullptr;
	}
	else if constexpr (kPerformErrorChecks)
	{
		// This function should not be used as it not efficient
		// Projectile is not a final class. There are types derived from it
		// The immediate derived types of Projectile are not final
		// Also, these derived types are multiple levels below Projectile
		if constexpr (std::is_same_v<ActorType, Projectile>)
		{
			// auto projectile{reinterpret_cast<Projectile*>(actor)};
			auto result = actor->IsA(Projectile::StaticClass());
			return result ? casted_actor : nullptr;
		}
	}
}

template <IsAnActor ActorType>
ActorType* IsValidAndIsA(Actor* actor)
{
	auto casted_actor{reinterpret_cast<ActorType*>(actor)};
	return IsA<ActorType>(actor) && IsValid(casted_actor) ? casted_actor : nullptr;
}

// Avoid using Unreal VM.
FVector Add_VectorVector(const FVector& A, const FVector& B);
FVector Subtract_VectorVector(const FVector& A, const FVector& B);
FVector Multiply_VectorFloat(const FVector& A, const float& B);

// Get all instances of a specific UObject type in the GObjects buffer.
template <class T>
std::vector<T*> GetInstancesOfUObjects(void)
{
	std::vector<T*> found_uobjects;

	for (int i = 0; i < UObject::GObjObjects()->Count; ++i)
	{
		UObject* object = UObject::GObjObjects()->Data[i];
		if (!object || !object->IsA(T::StaticClass()))
			continue;

		found_uobjects.push_back(reinterpret_cast<T*>(object));
	}
	return found_uobjects;
}