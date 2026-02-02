#pragma once

#include <string>
#include <vector>
#include "Tribes-Ascend-SDK/SdkHeaders.h"

using Projectile = ATrProjectile;
using Player = ATrPlayerPawn;
using Controller = ATrPlayerController;
using Ping = float;
using Location = FVector;
using Flag = ATrFlagBase;

bool IsPlayerValid(Player *player);

// Avoid using Unreal VM.
FVector Add_VectorVector(const FVector &A, const FVector &B);
FVector Subtract_VectorVector(const FVector &A, const FVector &B);
FVector Multiply_VectorFloat(const FVector &A, const float &B);

// Get all instances of a specific UObject type in the GObjects buffer.
template <class T>
std::vector<T *> GetInstancesUObjects(void)
{
	std::vector<T *> found_uobjects;

	for (int i = 0; i < UObject::GObjObjects()->Count; ++i)
	{
		UObject *object = UObject::GObjObjects()->Data[i];
		if (!object || !object->IsA(T::StaticClass()))
			continue;

		found_uobjects.push_back(reinterpret_cast<T *>(object));
	}
	return found_uobjects;
}

extern const UClass *kControllerClass;
extern const UClass *kPlayerClass;