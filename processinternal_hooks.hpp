#pragma once

#include <SdkHeaders.h>
#include <uhook.hpp>


PROCESSINTERNAL_HOOK(TrProjectileHurtRadiusInternal);
PROCESSINTERNAL_HOOK(UTGameMatchInProgressBeginState);
PROCESSINTERNAL_HOOK(TrProjectilePostBeginPlay);
PROCESSINTERNAL_HOOK(UTProjectileDestroyed);
PROCESSINTERNAL_HOOK(TrPawnDied);