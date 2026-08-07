#pragma once

#include <cstddef>
#include <array>
#include <vector>
#include <tuple>
#include <cassert>
#include <SdkHeaders.h>
#include "Circular-Buffer/circular_buffer.hpp"
#include "helper.hpp"
#include "native_hooks.hpp"
#include "Object-Pool/object_pool.hpp"

template <typename T>
concept HasReset = requires(T &t) {
	t.Reset();
};

class LagCompensation
{
#if defined(PERFORM_ERROR_CHECKS)
	static constexpr bool kPerformErrorChecks = true;
#else
	static constexpr bool kPerformErrorChecks = false;
#endif

	public:
	// Singleton
	LagCompensation(const LagCompensation &) = delete;
	LagCompensation &operator=(const LagCompensation &) = delete;
	LagCompensation(LagCompensation &&) = delete;
	LagCompensation &operator=(LagCompensation &&) = delete;

	static LagCompensation &GetInstance(void);

	protected:
	LagCompensation();

	private:
	// Tick rate - initialise to the default value of 30
	static constexpr float tick_rate_{30.0f};
	static constexpr float tick_delta_in_ms_{1000.0f / tick_rate_};
	// Maximum ping to lag compensate ([0, window_in_ms_))
	static constexpr Ping window_in_ms_{400.0f};
	// Don't perform lag compensation for any ping less than this
	static constexpr Ping kMinimumPingThreshold{4.0f};
	// The + 2 is neccessary to ensure we can compensate for between (kMinimumPingThreshold, window_in_ms_]
	static constexpr size_t buffer_size_{static_cast<size_t>((window_in_ms_ / tick_delta_in_ms_) + 2)};
	static constexpr size_t window_buffer_size_{static_cast<size_t>((window_in_ms_ / tick_delta_in_ms_) + 2)};

	// If the change in ping of a player is not greater than kMaxPingDelta during a tick,
	// then simply use the previous ping (optimisation)
	static constexpr Ping kMaxPingDelta{4.0f};

	static constexpr size_t kEstimatedMaxControllers{64};
	static constexpr size_t kEstimatedMaxPlayers{kEstimatedMaxControllers};
	static constexpr size_t kEstimatedMaxProjectiles{kEstimatedMaxPlayers * 1000};
	static constexpr size_t kEstimatedMaxProjectilesPerPing{kEstimatedMaxPlayers * 100};

	// Store a list of which pings to tick (optimisation)
	std::vector<Ping> pings_to_tick_in_latest_tick_{};
	// Group projectiles into ping buckets
	// All the projectiles in this list were valid at some point in the current engine world tick
	// There is no guarantee they will remain valid through out the whole tick, including
	// when we need to access/use them.
	// A projectile may have become invalid (Explode -> Destroy'ed) as the world tick continued
	// But, they won't have been GC'd yet so we can still check if they're valid
	std::array<std::vector<Projectile*>, static_cast<size_t>(window_in_ms_)> list_of_lag_compensated_projectiles_by_ping_in_latest_tick_{};

	// Check if all projectiles in a ping bucket are from the same team
	std::array<int, static_cast<int>(window_in_ms_)> team_per_ping_{};
	static constexpr Team kUninitialisedTeam{-1};
	static constexpr Team kInvalidTeam{255};

	// All the players in this list were valid (i.e. alive and not marked for desrtruction)
	// at some point in the current engine world tick
	// Similar to the comment above, when accessing the players in this list, the player may have
	// become invalid (Died (but not destroyed) or Destroy'ed). Validity checks are needed
	std::vector<Player*> list_of_players_in_latest_tick_{};

	private:
	template <typename Element, size_t Capacity, bool PerformErrorChecks = kPerformErrorChecks>
	class ManagedCircularBuffer : public CircularBuffer<Element, Capacity, PerformErrorChecks>
	{
		using Base = CircularBuffer<Element, Capacity, PerformErrorChecks>;

		public:
		void Reset(void)
		{
			this->size_ = 0;
			this->write_index_ = 0;
		}
	};

	static constexpr size_t kInvalidObjectPoolIndex{0};

	public:
	struct ActorInformationBase
	{
		ActorInformationBase(const ActorInformationBase &) = delete;
		ActorInformationBase &operator=(const ActorInformationBase &) = delete;
		ActorInformationBase(ActorInformationBase &&) = default;
		ActorInformationBase &operator=(ActorInformationBase &&) = default;
		AActor* actor_{};
		ActorInformationBase() = default;
	};

	// Do NOT store any caches of anything derived from the Actor class
	// This is because (e.g. caching projectiles instigator in ActorInformation<Projectile>):
	// 1. At any time the instigator could be marked for destruction, making it no longer valid
	// 2. After destruction, the GC will delete the pawn data leaving the cached pointer dangling
	// 	2.1* Once the GC deletes something, all internal references to that object should be set to nullptr
	// If access to an actor member is needed, access that data directly when needed and ensure it's valid before using it
	template <typename ActorType>
	struct ActorInformation;

	template <>
	struct ActorInformation<Controller> : public ActorInformationBase
	{
		Ping last_ping_in_ms_{};
	};

	template <>
	struct ActorInformation<Player> : public ActorInformationBase
	{
		class PlayerTickInformation
		{
			public:
			FVector location_{};
			FVector velocity_{};
		};
		Team team_{kInvalidTeam};
		ManagedCircularBuffer<PlayerTickInformation, window_buffer_size_> tick_information_{};
		void Reset(void)
		{
			tick_information_.Reset();
		}
	};

	template <>
	struct ActorInformation<Projectile> : public ActorInformationBase
	{
		Ping ping_in_ms_{};
		Team team_{kInvalidTeam};
	};

	private:
	template <typename ActorType>
	struct ActorObjectPoolData;

	template <>
	struct ActorObjectPoolData<Controller>
	{
		static constexpr size_t InitialCapacity{kEstimatedMaxControllers};
	};

	template <>
	struct ActorObjectPoolData<Player>
	{
		static constexpr size_t InitialCapacity{kEstimatedMaxPlayers};
	};

	template <>
	struct ActorObjectPoolData<Projectile>
	{
		static constexpr size_t InitialCapacity{kEstimatedMaxProjectiles};
	};

	template <typename ActorType>
	struct ActorObjectPoolTraits : public ActorObjectPoolData<ActorType>
	{
		using ObjectPoolData = ActorObjectPoolData<ActorType>;
		using InformationType = ActorInformation<ActorType>;
		using ObjectPoolType = DynamicIndexedObjectPool<InformationType, kPerformErrorChecks>;
	};

	template <typename ActorType>
	using ObjectPoolType = ActorObjectPoolTraits<ActorType>::ObjectPoolType;

	std::tuple<ObjectPoolType<Controller>, ObjectPoolType<Player>, ObjectPoolType<Projectile>> object_pools_ = std::make_tuple(CreateObjectPool<Controller>(),
																															   CreateObjectPool<Player>(),
																															   CreateObjectPool<Projectile>());

	template <typename ActorType>
	ObjectPoolType<ActorType> CreateObjectPool(void);

	template <typename ActorType>
	size_t GetActorInformationIndex(ActorType* actor);

	public:
	template <typename ActorType>
	ActorObjectPoolTraits<ActorType>::InformationType* AllocateActorInformation(ActorType* actor);

	template <bool CheckActorBelongsToPool = false, typename ActorType>
	ActorObjectPoolTraits<ActorType>::InformationType* GetActorInformation(ActorType* actor);

	template <typename ActorType>
	void FreeActorInformation(ActorType* actor);

	template <typename ActorType>
	void OnActorTick(ActorType* actor);

	// Currently there's no way optimial way to identify a Projectile in Actor::Tick hook unlike Player
	// So we resort to checking the if the actor has an ActorInformation<Projectile> and such
	// This function is called when a projectile is spawned so we can assigned it an ActorInformation
	ActorObjectPoolTraits<Projectile>::InformationType* AddProjectile(Projectile* projectile);
	bool RewindPlayers(Ping ping_in_ms);
	void RestorePlayers(void);
	void Tick(float DeltaSeconds, ELevelTick TickType);
};

template <typename ActorType>
LagCompensation::ObjectPoolType<ActorType> LagCompensation::CreateObjectPool(void)
{
	using ObjectPoolType = ObjectPoolType<ActorType>;
	using ActorObjectPoolData = ActorObjectPoolTraits<ActorType>::ObjectPoolData;
	return ObjectPoolType(ActorObjectPoolData::InitialCapacity);
}

template <typename ActorType>
size_t LagCompensation::GetActorInformationIndex(ActorType* actor)
{
	auto index{*reinterpret_cast<size_t*>(&actor->EditorIconColor)};
	return index;
}

template <typename ActorType>
LagCompensation::ActorObjectPoolTraits<ActorType>::InformationType* LagCompensation::AllocateActorInformation(ActorType* actor)
{
	auto &object_pool{std::get<ObjectPoolType<ActorType>>(object_pools_)};
	if constexpr (kPerformErrorChecks)
	{
		assert(GetActorInformationIndex(actor) == kInvalidObjectPoolIndex);
	}
	auto index{kInvalidObjectPoolIndex};
	do
	{
		index = object_pool.Allocate();
	} while (index == kInvalidObjectPoolIndex);

	*reinterpret_cast<size_t*>(&actor->EditorIconColor) = index;

	auto &actor_information{object_pool[index]};
	actor_information.actor_ = actor;

	return &actor_information;
}

template <bool CheckActorBelongsToPool, typename ActorType>
LagCompensation::ActorObjectPoolTraits<ActorType>::InformationType* LagCompensation::GetActorInformation(ActorType* actor)
{
	auto index{GetActorInformationIndex(actor)};
	if (index == kInvalidObjectPoolIndex)
	{
		return nullptr;
	}

	auto &object_pool{std::get<ObjectPoolType<ActorType>>(object_pools_)};
	auto actor_information{object_pool.At(index)};

	if constexpr (CheckActorBelongsToPool)
	{
		if (!actor_information)
		{
			return nullptr;
		}
		return actor == actor_information->actor_ ? actor_information : nullptr;
	}
	else
	{
		return actor_information;
	}
}

template <typename ActorType>
void LagCompensation::FreeActorInformation(ActorType* actor)
{
	auto index{GetActorInformationIndex(actor)};
	if (index == kInvalidObjectPoolIndex)
	{
		return;
	}

	auto &object_pool{std::get<ObjectPoolType<ActorType>>(object_pools_)};
	if constexpr (HasReset<typename ActorObjectPoolTraits<ActorType>::InformationType>)
	{
		object_pool.Free<false>(index);
		object_pool[index].Reset();
	}
	else
	{
		object_pool.Free<true>(index);
	}

	// This is necessary. A projectile's actor information will be destroyed in TrProjectileHurtRadiusInternal.
	// If we don't set the index back to an invalid value, UTProjectileDestroyed may call
	// this function again which is pointless AND erroneous because we will free the index
	// in the object pool again (this index will be re-added back into the list of free indexes).
	*reinterpret_cast<size_t*>(&actor->EditorIconColor) = kInvalidObjectPoolIndex;
}