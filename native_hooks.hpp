#pragma once

#include "Tribes-Ascend-SDK/SdkHeaders.h"

inline constexpr size_t kTickActorsPreAsyncWorkAddress{0x00801d10};
inline constexpr size_t kActorTickAddress{0x00802b10};

class FDeferredTickList;
enum class ELevelTick;

using TickActorsPreAsyncWorkPrototype = void (*)(UWorld *world, float delta_seconds,
												 ELevelTick tick_type, FDeferredTickList &deferred_list);
extern TickActorsPreAsyncWorkPrototype original_tickactors_preasyncwork;
void TickActorsPreAsyncWorkHook(UWorld *world, float delta_seconds,
								ELevelTick tick_type, FDeferredTickList &deferred_list);

using ActorTickPrototype = void(__fastcall *)(AActor *actor, void *unused, float delta_seconds, ELevelTick tick_type);
extern ActorTickPrototype original_actor_tick;
void __fastcall ActorTickHook(AActor *actor, void *unused, float delta_seconds, ELevelTick tick_type);