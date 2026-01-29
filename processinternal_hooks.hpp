#pragma once

#include "Tribes-Ascend-SDK/SdkHeaders.h"
#include "hook.hpp"

#define PROCESSINTERNAL_HOOK(functionHookName) void __fastcall functionHookName(UObject *calling_uobject, void *unused, FFrame &stack, void *result)

PROCESSINTERNAL_HOOK(TrProjectileHurtRadiusInternal);
PROCESSINTERNAL_HOOK(UTGameMatchInProgressBeginState);
PROCESSINTERNAL_HOOK(TrProjectilePostBeginPlay);
PROCESSINTERNAL_HOOK(UTProjectileDestroyed);
PROCESSINTERNAL_HOOK(TrPawnDied);