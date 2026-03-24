#pragma once

#include <uhook.hpp>

using namespace UE3;

UE3_PROCESSINTERNAL_HOOK(TrProjectileHurtRadiusInternal);
UE3_PROCESSINTERNAL_HOOK(UTGameMatchInProgressBeginState);
UE3_PROCESSINTERNAL_HOOK(TrProjectilePostBeginPlay);
UE3_PROCESSINTERNAL_HOOK(UTProjectileDestroyed);
UE3_PROCESSINTERNAL_HOOK(TrPawnDied);