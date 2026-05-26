#pragma once

#include <list>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include "Tribes-Ascend-SDK/SdkHeaders.h"
#include "Circular-Buffer/circular_buffer.hpp"
#include "helper.hpp"
#include "native_hooks.hpp"

class LagCompensation
{
public:
	// Singleton
	LagCompensation(LagCompensation &) = delete;
	LagCompensation &operator=(LagCompensation &) = delete;
	static LagCompensation &GetInstance(void)
	{
		static LagCompensation lag_compensation;
		return lag_compensation;
	}

protected:
	LagCompensation(void) = default;

public:
	// Tick rate - initialise to the default value of 30
	static inline float tick_rate_{30.0f};
	static inline float tick_delta_in_ms_{1000.0f / tick_rate_};
	// Basically, the lag compensation buffer should be able to compensate for at least this ping
	static constexpr Ping window_in_ms_{400.0f};
	// Don't perform lag compensation for any ping less than this
	static constexpr Ping kMinimumPingThreshold{4.0f};
	// The + 2 is neccessary to ensure we can compensate for between (kMinimumPingThreshold, window_in_ms_]
	static inline size_t buffer_size_{static_cast<size_t>((window_in_ms_ / tick_delta_in_ms_) + 2)};

	// If the change in ping of a player is not greater than kMaxPingDelta during a tick,
	// then simply use the previous ping (optimisation)
	static constexpr Ping kMaxPingDelta{4.0f};

	// Store a list of which pings to tick (optimisation)
	std::list<Ping> pings_to_tick_in_latest_tick_{};
	std::vector<std::list<Projectile *>> list_of_lag_compensated_projectiles_by_ping_in_latest_tick_ = std::vector<std::list<Projectile *>>(window_in_ms_);
	// Store all players that were valid (ie. alive) in the lastest tick
	std::list<Player *> list_of_players_in_latest_tick_{};

	enum class ActorId : std::size_t
	{
		kUnknown = 0xDEADBEEF,
		kPlayer = 1,
		kProjectile = 2,
		kController = 3,
		kAny
	};

	class ActorInformation
	{
	public:
		ActorId actor_id_{ActorId::kUnknown};
		virtual ~ActorInformation(void) = 0;
	};

	class ControllerInformation : public ActorInformation
	{
	public:
		Ping last_ping_in_ms_{};
		ControllerInformation(void) { actor_id_ = ActorId::kController; }
	};

	class PlayerInformation : public ActorInformation
	{
	public:
		class PlayerTickInformation
		{
		public:
			FVector location_{};
			FVector velocity_{};
		};
		CircularBuffer<PlayerTickInformation> tick_information_{LagCompensation::buffer_size_};
		PlayerInformation(void) { actor_id_ = ActorId::kPlayer; }
	};

	class ProjectileInformation : public ActorInformation
	{
	public:
		Player *owning_player_{};
		bool is_owning_player_still_valid_{};
		Ping ping_in_ms_{};
		ProjectileInformation(void) { actor_id_ = ActorId::kProjectile; }
	};

	ActorInformation *IsActorLagCompensated(AActor *actor, ActorId filtered_actor_id);
	ActorInformation *GetLagCompensationData(AActor *actor);
	void DestroyLagCompensationData(AActor *actor);

	ActorInformation *LagCompensate(Controller *controller);
	ActorInformation *LagCompensate(Player *player);
	ActorInformation *LagCompensate(Projectile *projectile);

	void UpdatePlayer(Player *player);

	bool RewindPlayers(Ping ping_in_ms);
	void RestorePlayers(void);
	void Tick(float DeltaSeconds, ELevelTick TickType);

	void UpdateTickRateVariables(void);
};