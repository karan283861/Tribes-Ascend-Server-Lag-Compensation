#pragma once

#include <vector>
#include <SdkHeaders.h>

using Controller = ATrPlayerController;
using Player = ATrPlayerPawn;
using Projectile = ATrProjectile;
using Ping = float;
using Team = int;

bool IsActorValid(AActor* actor);

template <typename ActorType>
bool IsValid(ActorType* actor);

template <typename ActorType>
bool IsA(AActor* actor);

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