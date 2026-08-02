#pragma once

#include <cassert>
#include <cstddef>
#include <array>
#include <vector>
#include <tuple>
#include "Tribes-Ascend-SDK/SdkHeaders.h"
#include "Circular-Buffer/circular_buffer.hpp"
#include "helper.hpp"
#include "native_hooks.hpp"
#include "Object-Pool/object_pool.hpp"

template <typename T>
concept HasReset = requires(T &t) {
	t.Reset();
};

template <typename T>
void Reset(T &t)
{
	t.Reset();
}

class LagCompensation
{
#if defined(_DEBUG)
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

	public:
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
	std::array<Ping, static_cast<size_t>(window_in_ms_)> pings_to_tick_in_latest_tick_{};
	// Group projectiles into ping buckets
	std::array<std::vector<Projectile*>, static_cast<size_t>(window_in_ms_)> list_of_lag_compensated_projectiles_by_ping_in_latest_tick_{};
	// Store all players that were valid (ie. alive) in the lastest tick
	std::vector<Player*> list_of_players_in_latest_tick_{};
	// Check if all projectiles in a ping bucket are from the same team
	std::array<int, static_cast<int>(window_in_ms_)> team_per_ping_{};
	static constexpr int kUninitialisedTeam{-1};
	static constexpr int kInvalidTeam{-2};

	struct ActorInformationBase
	{
		ActorInformationBase(const ActorInformationBase &) = delete;
		ActorInformationBase &operator=(const ActorInformationBase &) = delete;
		ActorInformationBase(ActorInformationBase &&) = default;
		ActorInformationBase &operator=(ActorInformationBase &&) = default;
		AActor* actor_{};
		ActorInformationBase() = default;
	};

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
		int team_{};
		ManagedCircularBuffer<PlayerTickInformation, window_buffer_size_> tick_information_{};
		void Reset(void)
		{
			::Reset(tick_information_);
		}
	};

	static_assert(HasReset<ActorInformation<Player>>);

	template <>
	struct ActorInformation<Projectile> : public ActorInformationBase
	{
		Player* owning_player_{};
		bool is_owning_player_still_valid_{};
		Ping ping_in_ms_{};
		int team_{};
	};

	static constexpr size_t kMaxSizeOfLagCompensatedProjectilesListInMB = (sizeof(ActorInformation<Projectile>) *
																		   kEstimatedMaxProjectilesPerPing *
																		   window_in_ms_) /
																		  (1024 * 1024) / 4;

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

	std::tuple<ObjectPoolType<Controller>, ObjectPoolType<Player>, ObjectPoolType<Projectile>> object_pools_ = std::make_tuple(ObjectPoolType<Controller>(ActorObjectPoolTraits<Controller>::InitialCapacity),
																															   ObjectPoolType<Player>(ActorObjectPoolTraits<Player>::InitialCapacity),
																															   ObjectPoolType<Projectile>(ActorObjectPoolTraits<Projectile>::InitialCapacity));

	static constexpr size_t kInvalidObjectPoolIndex{0};

	template <typename ActorType>
	size_t GetActorInformationIndex(ActorType* actor)
	{
		auto index{*reinterpret_cast<size_t*>(&actor->EditorIconColor)};
		return index;
	}

	template <bool CheckActorBelongsToPool = false, typename ActorType>
	ActorObjectPoolTraits<ActorType>::InformationType* GetActorInformation(ActorType* actor)
	{
		auto index{GetActorInformationIndex(actor)};
		if (index == kInvalidObjectPoolIndex)
		{
			return nullptr;
		}

		auto &object_pool{std::get<ObjectPoolType<ActorType>>(object_pools_)};
		auto actor_information{object_pool.At(index)};
		if (!actor_information)
		{
			return nullptr;
		}

		if constexpr (CheckActorBelongsToPool)
		{
			return actor == actor_information->actor_ ? actor_information : nullptr;
		}
		else
		{
			return actor_information;
		}
	}

	template <typename ActorType>
	ActorObjectPoolTraits<ActorType>::InformationType* AllocateActorInformation(ActorType* actor)
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

	template <typename ActorType>
	void FreeActorInformation(ActorType* actor)
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

		// This is necessary. A projectile can be destroyed in TrProjectileHurtRadiusInternal.
		// If we don't set the index back to an invalid value, UTProjectileDestroyed may call
		// this function again which is pointless AND erroneous because we will free the index
		// in the object pool again (this index will be added back into the list of free indexes).
		*reinterpret_cast<size_t*>(&actor->EditorIconColor) = kInvalidObjectPoolIndex;
	}

	ActorObjectPoolTraits<Projectile>::InformationType* AddProjectile(Projectile* projectile);
	ActorObjectPoolTraits<Player>::InformationType* UpdatePlayer(Player* player);

	bool RewindPlayers(Ping ping_in_ms);
	void RestorePlayers(void);
	void Tick(float DeltaSeconds, ELevelTick TickType);
};