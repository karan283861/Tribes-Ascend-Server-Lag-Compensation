#pragma once

#include <vector>
#include <SdkHeaders.h>
#include "validate.hpp"

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

/**
 * @brief Check if an actor derived from AActor is valid
 * 
 * Check the AActor parameter is non null and not Destroy'd (pending deletion)
 * 
 * @param actor Pointer to an actor derived from AActor
 */
bool IsActorValid(AActor* actor);

/**
 * @brief Check if an actor derived from ActorType is valid
 * 
 * @tparam ObjectType A typename that is derived from AActor
 * @param actor Pointer to an actor derived from ActorType
 */
template <typename ActorType>
bool IsValid(ActorType* actor);

/**
 * @brief Specialisation of IsValid(ActorType*) for Controller types
 * 
 * Simply checks via calling IsActorValid
 * 
 * @param controller Pointer to an object which is/derived from Controller
 */
template <>
bool IsValid<Controller>(Controller* controller);

/**
 * @brief Specialisation of IsValid(ActorType*) for Player types
 * 
 * Checks:
 * - IsActorValid
 * - The Player parameter has:
 * 	- PlayerReplicationInfo is non null
 * 	- PlayerReplicationInfo->Team is non null
 * 	- Health is > 0
 * 
 * @param player  Pointer to an object which is/derived from Player
 */
template <>
bool IsValid<Player>(Player* player);

/**
 * @brief Specialisation of IsValid(ActorType*) for Projectile types
 * 
 * Simply checks via calling IsActorValid
 * 
 * @param projectile Pointer to an object which is/derived from Projectile
 */
template <>
bool IsValid<Projectile>(Projectile* projectile);

/**
 * @brief Check if an object derived from UObject is exactly of ObjectType
 * 
 * Directly compares the Class member of the UObject parameter with ObjectType::StaticClass()
 * 
 * @tparam ObjectType A typename that is derived from UObject
 * @param object Pointer to an object derived from UObject
 */
template <typename ObjectType>
bool Is(UObject* object)
{
	if (PERFORM_ERROR_CHECK(!object, "Object is nullptr"))
		return false;
	
	return object && object->Class == ObjectType::StaticClass();
}

/**
 * @brief Check if an object derived from UObject is exactly or derived of ObjectType
 * 
 * Loops through the UObject parameters class chain and compares with ObjectType::StaticClass()
 * 
 * @attention This function should be specialised for efficiency
 * 
 * @tparam ObjectType A typename that is derived from UObject
 * @param object Pointer to an object derived from UObject
 *
 * @warning This function is expensive
 */
template <typename ObjectType>
bool IsA(UObject* object)
{
	if (PERFORM_ERROR_CHECK(!object, "Object is nullptr"))
		return false;
	
	return object && object->IsA(ObjectType::StaticClass());
}

/**
 * @brief Specialisation of IsA(UObject*) for Controller types
 * 
 * @attention Controller is not a final class. There are types derived from it
 * @note The immediate derived types of Controller are final
 * 
 * @param object Pointer to an object derived from UObject
 * 
 * @remark An efficient implementation
 */
template <>
bool IsA<Controller>(UObject* object);

/**
 * @brief Specialisation of IsA(UObject*) for Player types
 * 
 * @attention Player is not a final class. There are types derived from it
 * @note The immediate derived types of Player are final
 * 
 * @param object Pointer to an object derived from UObject
 * 
 * @remark An efficient implementation
 */
template <>
bool IsA<Player>(UObject* object);

/**
 * @brief Specialisation of IsA(UObject*) for Projectile types
 * 
 * @attention Projectile is not a final class. There are types derived from it
 * @note The immediate derived types of Projectile are not final. Also, these
 * derived types are multiple levels below Projectile
 * 
 * @param object Pointer to an object derived from UObject
 * 
 * @warning This function should not be used as it not efficient
 */
template <>
bool IsA<Projectile>(UObject* object);

template<typename ActorType>
bool Validate(ActorType* actor)
{
	// If PERFORM_ERROR_CHECKS is enabled and the parameter is nullptr,
	// we will crash attempting to call ->GetFullName()
	if (PERFORM_ERROR_CHECK(!controller, "Actor is nullptr"))
		return false;

	// Make sure the controller was actually the right type, and not just casted as a Controller
	if (PERFORM_ERROR_CHECK(!controller->IsA(ActorType::StaticClass()), "Actor is not a {}, it is a {}",
							typeid(ActorType).name(), actor->GetFullName()))
		return false;

	// We've ensured controller inherits from Controller, but we need to check if it passes our own IsA check
	if (PERFORM_ERROR_CHECK(!IsA<ActorType>(actor), "Actor failed the IsA<{}> check, it is a {}",
							typeid(ActorType).name(), actor->GetFullName()))
		return false;

	// If this function is called in IsValid, we will be in a recursive loop
	// Finally, we check if it's actually valid
	// if (PERFORM_ERROR_CHECK(!IsValid(actor), "Actor failed the IsValid<{}> check, it is a {}",
	// 						typeid(ActorType).name(), actor->GetFullName()))
	// 	return false;

	return true;
}


// Avoid using Unreal VM.
FVector Add_VectorVector(const FVector &A, const FVector &B);
FVector Subtract_VectorVector(const FVector &A, const FVector &B);
FVector Multiply_VectorFloat(const FVector &A, const float &B);

// Get all instances of a specific UObject type in the GObjects buffer.
template <class T>
std::vector<T*> GetInstancesUObjects(void)
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