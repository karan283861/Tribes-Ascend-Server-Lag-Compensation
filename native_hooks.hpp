#pragma once

#include <SdkHeaders.h>

inline constexpr size_t kTickActorsPreAsyncWorkAddress{0x00801d10};
inline constexpr size_t kActorTickAddress{0x00802b10};
inline constexpr size_t kWorldFarMoveActorAddress{0x007e8a10};

class FDeferredTickList;
enum class ELevelTick;

using TickActorsPreAsyncWorkPrototype = void (*)(UWorld* world, float delta_seconds,
												 ELevelTick tick_type, FDeferredTickList &deferred_list);
extern TickActorsPreAsyncWorkPrototype original_tickactors_preasyncwork;
void TickActorsPreAsyncWorkHook(UWorld* world, float delta_seconds,
								ELevelTick tick_type, FDeferredTickList &deferred_list);

using ActorTickPrototype = void(__fastcall*)(AActor* actor, void* unused, float delta_seconds, ELevelTick tick_type);
extern ActorTickPrototype original_actor_tick;
void __fastcall ActorTickHook(AActor* actor, void* unused, float delta_seconds, ELevelTick tick_type);

using WorldFarMoveActor = unsigned int(__fastcall*)(UWorld* world, void* unused, AActor* Actor, const FVector &DestLocation, unsigned int test, unsigned int bNoCheck, unsigned int bAttachedMove);
extern WorldFarMoveActor original_world_farmoveactor;

extern UWorld* global_world;