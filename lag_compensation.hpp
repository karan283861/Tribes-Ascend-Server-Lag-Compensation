#pragma once

#include <cstddef>
#include <array>
#include <typeinfo>
#include <vector>
#include <tuple>
#include <cassert>
#include <SdkHeaders.h>
#include "validate.hpp"
#include "helper.hpp"
#include <circular_buffer.hpp>
#include <object_pool.hpp>
#include "native_hooks.hpp"
#include "processinternal_hooks.hpp"

template <typename T>
concept HasReset = requires(T& t) {
	t.Reset();
};

template<typename T>
concept HasRewindInformation = requires (T& t) {
	t.rewind_information_;
};

template<typename T>
concept IsRewindableActor = IsAnActor<T> && HasRewindInformation<T>;

#define IS_ACTOR_INFORMATION_VALID(POINTER_TO_ACTOR, STATEMENT_ON_ERROR)                                            \
	{                                                                                                               \
		auto& lag_compensation{LagCompensation::GetInstance()};                                                     \
		auto actor_information{lag_compensation.GetActorInformation(POINTER_TO_ACTOR)};                             \
		if (PERFORM_ERROR_CHECK(!actor_information,                                                                 \
								"{} is a {} and is deemed valid but has no actor information attached",             \
								#POINTER_TO_ACTOR,                                                                  \
								typeid(decltype(*POINTER_TO_ACTOR)).name()))                                        \
			STATEMENT_ON_ERROR;                                                                                     \
                                                                                                                    \
		if (PERFORM_ERROR_CHECK(!actor_information->actor_,                                                         \
								"{} is a {} but the attached actor information points to a nullptr",                \
								#POINTER_TO_ACTOR,                                                                  \
								typeid(decltype(*POINTER_TO_ACTOR)).name()))                                        \
			STATEMENT_ON_ERROR;                                                                                     \
                                                                                                                    \
		if (PERFORM_ERROR_CHECK(POINTER_TO_ACTOR != actor_information->actor_,                                      \
								"{} is a {} but the attached actor information points to a different actor (a {})", \
								#POINTER_TO_ACTOR,                                                                  \
								typeid(decltype(*POINTER_TO_ACTOR)).name(),                                         \
								actor_information->actor_->GetFullName()))                                          \
			STATEMENT_ON_ERROR;                                                                                     \
                                                                                                                    \
		if (PERFORM_ERROR_CHECK(!actor_information->IsValid(),                                                      \
								"The attached actor information is deemed invalid"))                                \
			STATEMENT_ON_ERROR;                                                                                     \
	}

/**	@class LagCompensation
	@brief Singleton class which encapsulates lag compensation functionality.
 */
class LagCompensation
{
	protected:
	/** @name Constructors
		@brief Protected to ensure singleton functionality
	 */
	LagCompensation();

	/**	@name Copy and move operations
		@{
		@brief Deleted to ensure singleton functionality
	 */
	LagCompensation(const LagCompensation&) = delete;
	LagCompensation& operator=(const LagCompensation&) = delete;
	LagCompensation(LagCompensation&&) = delete;
	LagCompensation& operator=(LagCompensation&&) = delete;
	/** @} */

	public:
	/** @brief Get instance of the LagCompensation singleton class
		@return Reference to a static LagCompensation object
	 */
	static LagCompensation& GetInstance(void);

	static bool IsPingValid(Ping ping_in_ms)
	{
		return ping_in_ms > kMinimumPingThreshold && ping_in_ms < window_in_ms_;
	}

	static bool IsTeamValid(Team team)
	{
		return team != kUninitialisedTeam && team != kInvalidTeam;
	}

	private:
	// Tick rate - initialise to the default value of 30
	static constexpr float tick_rate_{30.0f};
	static constexpr float tick_delta_in_ms_{1000.0f / tick_rate_};
	// Maximum ping to lag compensate ([0, window_in_ms_))
	static constexpr Ping window_in_ms_{400.0f};
	// Don't perform lag compensation for any ping less than this
	static constexpr Ping kMinimumPingThreshold{4.0f};
	// The + 2 is neccessary to ensure we can compensate for between (kMinimumPingThreshold, window_in_ms_)
	static constexpr size_t window_buffer_size_{static_cast<size_t>((window_in_ms_ / tick_delta_in_ms_) + 2)};

	// If the change in ping of a player is not greater than kMaxPingDelta during a tick,
	// then simply use the previous ping (optimisation)
	static constexpr Ping kMaxPingDelta{4.0f};

	static constexpr size_t kEstimatedMaxControllers{64};
	static constexpr size_t kEstimatedMaxPlayers{kEstimatedMaxControllers};
	static constexpr size_t kEstimatedMaxProjectiles{kEstimatedMaxPlayers * 1000};
	static constexpr size_t kEstimatedMaxProjectilesPerPing{kEstimatedMaxPlayers * 100};
	static constexpr size_t kEstimatedMaxFlags{3};

	// Store a list of which pings to tick (optimisation)
	std::vector<Ping> pings_to_tick_in_latest_tick_{};
	// Group projectiles into ping buckets
	// All the projectiles in this list were valid at some point in the current engine world tick
	// There is no guarantee they will remain valid through out the whole tick, including
	// when we need to access/use them.
	// A projectile may have become invalid (Explode -> Destroy'ed) as the world tick continued
	// But, they won't have been GC'd yet so we can still check if they're valid
	std::array<std::vector<Projectile*>, static_cast<size_t>(window_in_ms_)> lag_compensated_projectiles_by_ping_in_tick_{};

	// Check if all projectiles in a ping bucket are from the same team
	std::array<int, static_cast<int>(window_in_ms_)> team_per_ping_{};
	static constexpr Team kUninitialisedTeam{-1};
	static constexpr Team kInvalidTeam{255};

	// All the players in this list were valid (i.e. alive and not marked for desrtruction)
	// at some point in the current engine world tick
	// Similar to the comment above, when accessing the players in this list, the player may have
	// become invalid (Died (but not destroyed) or Destroy'ed). Validity checks are needed
	std::vector<Player*> players_in_tick_{};

	std::tuple<std::vector<Player*>, std::vector<Flag*>> actors_in_tick_{};

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
		ActorInformationBase(const ActorInformationBase&) = delete;
		ActorInformationBase& operator=(const ActorInformationBase&) = delete;
		ActorInformationBase(ActorInformationBase&&) = default;
		ActorInformationBase& operator=(ActorInformationBase&&) = default;
		Actor* actor_{};

		protected:
		ActorInformationBase() = default;
	};

	template<IsAnActor ActorType>
	struct ActorTickInformation;

	template<>
	struct ActorTickInformation<Player>
	{
		Vector3D location_{};
		// Vector3D velocity_{};
	};

	template<>
	struct ActorTickInformation<Flag>
	{
		Vector3D location_{};
		bool is_valid_{};
	};

	template<IsAnActor ActorType>
	using RewindInformation = ManagedCircularBuffer<ActorTickInformation<ActorType>, window_buffer_size_>;

	// Do NOT store any caches of anything derived from the Actor class
	// This is because (e.g. caching projectiles instigator in ActorInformation<Projectile>):
	// 1. At any time the instigator could be marked for destruction, making it no longer valid
	// 2. After destruction, the GC will delete the pawn data leaving the cached pointer dangling
	// 	2.1* Once the GC deletes something, all internal references to that object should be set to nullptr
	// If access to an actor member is needed, access that data directly when needed and ensure it's valid before using it
	template <IsAnActor ActorType>
	struct ActorInformation;

	template <>
	struct ActorInformation<Controller> : public ActorInformationBase
	{
		ActorInformation<Controller>() = default;

		Ping last_ping_in_ms_{};

		static constexpr bool IsValid(Controller* controller)
		{
			return true;
		}
#if defined(PERFORM_ERROR_CHECKS)
		bool IsValid(void)
		{
			IS_ACTOR_VALID(static_cast<Controller*>(actor_), return false);

			if (PERFORM_ERROR_CHECK(last_ping_in_ms_ < 0, "Controller information last_ping_in_ms_ is less than zero ({})", last_ping_in_ms_))
				return false;

			if (PERFORM_ERROR_CHECK(!IsPingValid(last_ping_in_ms_), "Controller information last_ping_in_ms_ is invalid ({})", last_ping_in_ms_))
				return false;

			return true;
		}
#endif
	};

	template <>
	struct ActorInformation<Player> : public ActorInformationBase
	{
		Team team_{kInvalidTeam};
		RewindInformation<Player> rewind_information_{};

		void Reset(void)
		{
			rewind_information_.Reset();
		}

		static bool IsValid(Player* player)
		{
			Team team{player->PlayerReplicationInfo->Team->TeamIndex};

			if (PERFORM_ERROR_CHECK(!IsTeamValid(team), "Player belongs to an invalid team ({})", team))
				return false;

			return true;
		}

#if defined(PERFORM_ERROR_CHECKS)
		bool IsValid(void)
		{
			IS_ACTOR_VALID(static_cast<Player*>(actor_), return false);

			if (PERFORM_ERROR_CHECK(rewind_information_.Size() == 0, "Player information has an empty tick information buffer"))
				return false;

			if (PERFORM_ERROR_CHECK(!IsTeamValid(team_), "Player information belongs to an invalid team ({})", team_))
				return false;

			return true;
		}
#endif
	};

	template <>
	struct ActorInformation<Projectile> : public ActorInformationBase
	{
		ActorInformation<Projectile>() = default;

		Ping ping_in_ms_{};
		Team team_{kInvalidTeam};

		static bool IsValid(Projectile* projectile)
		{
			IS_ACTOR_VALID(projectile, return false);

			Controller* controller{};
			Player* player{};

			if (!((controller = IsValidAndIsA<Controller>(projectile->InstigatorController)) && (player = IsValidAndIsA<Player>(projectile->Instigator))))
			{
				return false;
			}

			IS_ACTOR_VALID(controller, return false);
			IS_ACTOR_VALID(player, return false);

			auto ping_in_ms{player->PlayerReplicationInfo->ExactPing * 4};
			Team team{player->PlayerReplicationInfo->Team->TeamIndex};

			if (PERFORM_ERROR_CHECK(ping_in_ms < 0, "Projectile ping is less than zero ({})", ping_in_ms))
				return false;

			if (PERFORM_ERROR_CHECK(!IsTeamValid(team), "Player belongs to an invalid team ({})", team))
				return false;

			// Only lag compensate if ping is within our lag compensation windows AND the player has simulated projectiles ENABLED
			if (!controller->m_bAllowSimulatedProjectiles || !IsPingValid(ping_in_ms))
			{
				return false;
			}

			return true;
		}

#if defined(PERFORM_ERROR_CHECKS)
		bool IsValid(void)
		{
			IS_ACTOR_VALID(static_cast<Projectile*>(actor_), return false);

			if (PERFORM_ERROR_CHECK(ping_in_ms_ < 0, "Projectile information ping_in_ms_ is less than zero ({})", ping_in_ms_))
				return false;

			if (PERFORM_ERROR_CHECK(!IsPingValid(ping_in_ms_), "Projectile information ping_in_ms_ is invalid ({})", ping_in_ms_))
				return false;

			if (PERFORM_ERROR_CHECK(!IsTeamValid(team_), "Projectile information belongs to an invalid team ({})", team_))
				return false;

			return true;
		}
#endif
	};

	template <>
	struct ActorInformation<Flag> : public ActorInformationBase
	{
		RewindInformation<Flag> rewind_information_{};

		void Reset(void)
		{
			rewind_information_.Reset();
		}

		static bool IsValid(Flag* flag)
		{
			return true;
		}

#if defined(PERFORM_ERROR_CHECKS)
		bool IsValid(void)
		{
			IS_ACTOR_VALID(static_cast<Flag*>(actor_), return false);
			return true;
		}
#endif
	};

	private:
	template <IsAnActor ActorType>
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

	template <>
	struct ActorObjectPoolData<Flag>
	{
		static constexpr size_t InitialCapacity{kEstimatedMaxFlags};
	};

	template <IsAnActor ActorType>
	struct ActorObjectPoolTraits : public ActorObjectPoolData<ActorType>
	{
		using ObjectPoolData = ActorObjectPoolData<ActorType>;
		using InformationType = ActorInformation<ActorType>;
		using ObjectPoolType = DynamicIndexedObjectPool<InformationType, kPerformErrorChecks>;
	};

	template <IsAnActor ActorType>
	using ObjectPool = ActorObjectPoolTraits<ActorType>::ObjectPoolType;

	std::tuple<ObjectPool<Controller>, ObjectPool<Player>, ObjectPool<Projectile>, ObjectPool<Flag>> object_pools_ = std::make_tuple(CreateObjectPool<Controller>(),
																															   CreateObjectPool<Player>(),
																															   CreateObjectPool<Projectile>(),
																															   CreateObjectPool<Flag>());

	template <IsAnActor ActorType>
	ObjectPool<ActorType> CreateObjectPool(void);

	size_t GetActorInformationIndex(Actor* actor);

	public:
	template <IsAnActor ActorType>
	ActorObjectPoolTraits<ActorType>::InformationType* AllocateActorInformation(ActorType* actor);

	template <IsAnActor ActorType>
	ActorObjectPoolTraits<ActorType>::InformationType* GetActorInformation(ActorType* actor);

	template <IsAnActor ActorType>
	void FreeActorInformation(ActorType* actor);

	// Currently there's no way optimial way to identify a Projectile in Actor::Tick hook unlike Player
	// So we resort to checking the if the actor has an ActorInformation<Projectile> and such
	// This function is called when a projectile is spawned so we can assigned it an ActorInformation
	ActorObjectPoolTraits<Projectile>::InformationType* AddProjectile(Projectile* projectile);
	bool RewindPlayers(Ping ping_in_ms);
	void RestorePlayers(void);

	template<IsRewindableActor ActorType>
	bool Rewind(Ping ping_in_ms);

	template<IsRewindableActor ActorType>
	void Restore();


	void Tick(float delta_seconds, ELevelTick tick_type);
	UE3_PROCESSINTERNAL_HOOK(OnProjectileRadialDamage);

	private:
	static void __fastcall OnActorTick(Player* player, void* unused, float delta_seconds, ELevelTick tick_type);
	static void __fastcall OnActorTick(Projectile* projectile, void* unused, float delta_seconds, ELevelTick tick_type);
	static void __fastcall OnActorTick(Flag* flag, void* unused, float delta_seconds, ELevelTick tick_type);
	bool OnActorTick(Player* player);
	bool OnActorTick(Projectile* projectile);
	bool OnActorTick(Flag* flag);
};

template <IsAnActor ActorType>
LagCompensation::ObjectPool<ActorType> LagCompensation::CreateObjectPool(void)
{
	using ObjectPool = ObjectPool<ActorType>;
	using ActorObjectPoolData = ActorObjectPoolTraits<ActorType>::ObjectPoolData;
	return ObjectPool(ActorObjectPoolData::InitialCapacity);
}

template <IsAnActor ActorType>
LagCompensation::ActorObjectPoolTraits<ActorType>::InformationType* LagCompensation::AllocateActorInformation(ActorType* actor)
{
	IS_ACTOR_TYPE_VALID(actor, return nullptr);

	auto& object_pool{std::get<ObjectPool<ActorType>>(object_pools_)};
	if (PERFORM_ERROR_CHECK(GetActorInformationIndex(actor) != kInvalidObjectPoolIndex,
							"Attempting to allocate actor information to an actor ({}) which already has actor information attached",
							actor->GetFullName()))
		return nullptr;

	if (!ActorObjectPoolTraits<ActorType>::InformationType::IsValid(actor))
	{
		return nullptr;
	}

	auto index{kInvalidObjectPoolIndex};
	do
	{
		index = object_pool.Allocate();
	} while (index == kInvalidObjectPoolIndex);

	if (PERFORM_ERROR_CHECK(index == kInvalidObjectPoolIndex, "Attempting to allocate actor information at an index of kInvalidObjectPoolIndex"))
		return nullptr;

	*reinterpret_cast<size_t*>(&actor->EditorIconColor) = index;

	auto& actor_information{object_pool[index]};
	actor_information.actor_ = actor;

	return &actor_information;
}

template <IsAnActor ActorType>
LagCompensation::ActorObjectPoolTraits<ActorType>::InformationType* LagCompensation::GetActorInformation(ActorType* actor)
{
	// Surely we should only be trying to get actor information for valid actors?
	// However there's no guarantee
	IS_ACTOR_VALID(actor, return nullptr);

	auto index{GetActorInformationIndex(actor)};
	if (index == kInvalidObjectPoolIndex)
	{
		return nullptr;
	}

	auto& object_pool{std::get<ObjectPool<ActorType>>(object_pools_)};
	auto actor_information{object_pool.At(index)};
	return actor_information;
}

template <IsAnActor ActorType>
void LagCompensation::FreeActorInformation(ActorType* actor)
{
	// We shouldn't check if actor IsValid here (IS_ACTOR_VALID), because failing IsValid is not error
	// We also shouldn't check the actor matches the type (IS_ACTOR_TYPE_VALID), because currently ATrPawn::Died will forward
	// actor casted as a Player when it could be a turret

	auto index{GetActorInformationIndex(actor)};
	if (index == kInvalidObjectPoolIndex)
	{
		return;
	}

	auto& object_pool{std::get<ObjectPool<ActorType>>(object_pools_)};
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
	// NOTE: As of recently, TrProjectileHurtRadiusInternal (OnProjectileRadialDamage) does NOT call
	// FreeActorInformation
	*reinterpret_cast<size_t*>(&actor->EditorIconColor) = kInvalidObjectPoolIndex;
}