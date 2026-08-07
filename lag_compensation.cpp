#include <format>
#include <plog/Log.h>
#include "lag_compensation.hpp"
#include "helper.hpp"
#include "native_hooks.hpp"

LagCompensation &LagCompensation::GetInstance(void)
{
	static LagCompensation lag_compensation{};
	return lag_compensation;
}

LagCompensation::LagCompensation()
{
	// TODO: Remove commented lines below

	pings_to_tick_in_latest_tick_.reserve(window_in_ms_);
	list_of_players_in_latest_tick_.reserve(kEstimatedMaxPlayers);

	// list_of_lag_compensated_projectiles_by_ping_in_latest_tick_.reserve(window_in_ms_);
	// list_of_lag_compensated_projectiles_by_ping_in_latest_tick_.resize(window_in_ms_);
	for (auto i{std::size_t{0}}; i < list_of_lag_compensated_projectiles_by_ping_in_latest_tick_.size(); i++)
	{
		if (i <= kMinimumPingThreshold || i >= window_buffer_size_)
		{
			continue;
		}

		// Pings *SHOULD* always be a multiple of 4, so we can ignore anything not divisible 4.
		// WARNING: This hasn't been confirmed.
		if (i % 4 == 0)
		{
			list_of_lag_compensated_projectiles_by_ping_in_latest_tick_[i].reserve(kEstimatedMaxProjectilesPerPing);
		}
	}

	// team_per_ping_.reserve(window_in_ms_);
	// team_per_ping_.resize(window_in_ms_);
	std::fill(team_per_ping_.begin(), team_per_ping_.end(), kUninitialisedTeam);
}

LagCompensation::ActorObjectPoolTraits<Projectile>::InformationType* LagCompensation::AddProjectile(Projectile* projectile)
{
	auto instigator{reinterpret_cast<Controller*>(projectile->Instigator)};
	auto controller{reinterpret_cast<Controller*>(projectile->InstigatorController)};

	// Check the controller and instigator is of the right type
	// The instigator may not be a Player (e.g. could be a vehicle or any other type of Pawn)
	if (!(IsValid(controller) && Is<Controller>(controller) && IsValid(instigator) && Is<Player>(instigator)))
	{
		return nullptr;
	}

	auto ping_in_ms{controller->PlayerReplicationInfo->ExactPing * 4};
	Team team{controller->PlayerReplicationInfo->Team->TeamIndex};

	// Only lag compensate if ping is within our lag compensation windows AND the player has simulated projectiles ENABLED
	if (ping_in_ms <= kMinimumPingThreshold || ping_in_ms >= window_in_ms_ || !controller->m_bAllowSimulatedProjectiles)
	{
		return nullptr;
	}

	auto projectile_information{AllocateActorInformation(projectile)};
	projectile_information->ping_in_ms_ = ping_in_ms;
	projectile_information->team_ = team;

	auto controller_information{GetActorInformation(controller)};
	if (controller_information)
	{
		if (abs(controller_information->last_ping_in_ms_ - projectile_information->ping_in_ms_) <= kMaxPingDelta)
		{
			projectile_information->ping_in_ms_ = controller_information->last_ping_in_ms_;
		}
		else
		{
			controller_information->last_ping_in_ms_ = projectile_information->ping_in_ms_;
		}
	}
	else
	{
		controller_information = AllocateActorInformation(controller);
		controller_information->last_ping_in_ms_ = projectile_information->ping_in_ms_;
	}

	return projectile_information;
}

template <>
void LagCompensation::OnActorTick(Projectile* projectile)
{
	// Right now we're checking if the projectile is valid in native hook prior to calling this
	// So we don't need to perform validation in here (unlike OnActorTick<Player>)

	auto projectile_information{GetActorInformation(projectile)};

	if (list_of_lag_compensated_projectiles_by_ping_in_latest_tick_[projectile_information->ping_in_ms_].size() == 0)
	{
		pings_to_tick_in_latest_tick_.push_back(projectile_information->ping_in_ms_);
	}

	list_of_lag_compensated_projectiles_by_ping_in_latest_tick_[projectile_information->ping_in_ms_].push_back(projectile);

	if (team_per_ping_[projectile_information->ping_in_ms_] == LagCompensation::kUninitialisedTeam)
	{
		team_per_ping_[projectile_information->ping_in_ms_] = projectile_information->team_;
	}
	else if (team_per_ping_[projectile_information->ping_in_ms_] != projectile_information->team_)
	{
		team_per_ping_[projectile_information->ping_in_ms_] = LagCompensation::kInvalidTeam;
	}
}

template <>
void LagCompensation::OnActorTick(Player* player)
{
	auto controller{reinterpret_cast<Controller*>(player->Controller)};

	// It could be possible that a player has Died but isn't Destroy'ed so it's still ticking
	if (!IsValid(player) || !IsValid(controller))
	{
		return;
	}

	auto player_information{GetActorInformation(player)};
	if (!player_information)
	{
		player_information = AllocateActorInformation(player);
	}

	ActorInformation<Player>::PlayerTickInformation player_tick_information{};
	player_tick_information.location_ = player->Location;
	player_tick_information.velocity_ = player->Velocity;
	player_information->tick_information_.PushBack(std::move(player_tick_information));
	player_information->team_ = controller->PlayerReplicationInfo->Team->TeamIndex;

	list_of_players_in_latest_tick_.push_back(player);
}

bool LagCompensation::RewindPlayers(Ping ping_in_ms)
{
	if (kPerformErrorChecks)
	{
		assert(team_per_ping_[ping_in_ms] != kUninitialisedTeam);
	}

	bool all_projectiles_are_from_same_ping_bucket{team_per_ping_[ping_in_ms] != kInvalidTeam};
	auto tick_index{static_cast<int>(ping_in_ms / tick_delta_in_ms_)};
	auto prev_index{tick_index + 1};

	for (auto &player : list_of_players_in_latest_tick_)
	{
		if (IsValid(player))
		{
			auto player_information{GetActorInformation(player)};
			if (all_projectiles_are_from_same_ping_bucket && player_information->team_ == team_per_ping_[ping_in_ms])
			{
				continue;
			}

			if (player_information && prev_index < player_information->tick_information_.Size())
			{
				const auto &tick_location{player_information->tick_information_[tick_index].location_};
				const auto &prev_location{player_information->tick_information_[prev_index].location_};

				auto delta{Subtract_VectorVector(prev_location, tick_location)};
				static constexpr auto interpolate_scalar{0.5};
				auto interpolated_location{Add_VectorVector(tick_location,
															Multiply_VectorFloat(delta, interpolate_scalar))};
				original_world_farmoveactor(global_world, nullptr, player, interpolated_location, 0, 1, 0);
			}
		}
	}

	return true;
}

void LagCompensation::RestorePlayers(void)
{
	for (auto &player : list_of_players_in_latest_tick_)
	{
		if (IsValid(player))
		{
			if (auto player_information{GetActorInformation(player)})
			{
				original_world_farmoveactor(global_world, nullptr, player, player_information->tick_information_[0].location_, 0, 1, 0);
			}
		}
	}
}

void LagCompensation::Tick(float DeltaSeconds, ELevelTick TickType)
{
	bool performed_rewind_of_players{false};

	for (auto ping_in_ms : pings_to_tick_in_latest_tick_)
	{
		auto &list_of_projectiles{list_of_lag_compensated_projectiles_by_ping_in_latest_tick_[ping_in_ms]};

		if (!RewindPlayers(ping_in_ms))
		{
			continue;
		}

		performed_rewind_of_players = true;

		for (auto &projectile : list_of_projectiles)
		{
			// Actually make the projectile Tick through the original native engine function
			original_actor_tick(projectile, nullptr, DeltaSeconds, TickType);
		}

		list_of_projectiles.clear();
	}

	if (performed_rewind_of_players)
	{
		RestorePlayers();
	}

	pings_to_tick_in_latest_tick_.clear();
	list_of_players_in_latest_tick_.clear();
	std::fill(team_per_ping_.begin(), team_per_ping_.end(), kUninitialisedTeam);
}