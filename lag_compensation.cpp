#include <format>
#include <plog/Log.h>
#include "lag_compensation.hpp"

LagCompensation::ActorObjectPoolTraits<Projectile>::InformationType* LagCompensation::AddProjectile(Projectile* projectile)
{
	auto controller{reinterpret_cast<Controller*>(projectile->InstigatorController)};

	auto ping_in_ms{controller->PlayerReplicationInfo->ExactPing * 4};

	// Only lag compensate if ping is within our lag compensation windows AND the player has simulated projectiles ENABLED
	if (ping_in_ms <= kMinimumPingThreshold || ping_in_ms >= window_in_ms_ || !controller->m_bAllowSimulatedProjectiles)
	{
		return nullptr;
	}

	auto projectile_information{AllocateActorInformation(projectile)};
	projectile_information->owning_player_ = reinterpret_cast<Player*>(controller->Pawn);
	projectile_information->ping_in_ms_ = ping_in_ms;
	projectile_information->is_owning_player_still_valid_ = IsPlayerValid(projectile_information->owning_player_);
	projectile_information->team_ = projectile_information->owning_player_->PlayerReplicationInfo->Team->TeamIndex;

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

LagCompensation::ActorObjectPoolTraits<Player>::InformationType* LagCompensation::UpdatePlayer(Player* player)
{
	auto projectile{reinterpret_cast<Projectile*>(player)};
	auto x1 = GetActorInformationIndex(projectile);
	auto x2 = GetActorInformation(projectile);
	auto x3 = AllocateActorInformation(projectile);
	FreeActorInformation(projectile);

	auto player2{reinterpret_cast<Player*>(player)};
	auto x12 = GetActorInformationIndex(player2);
	auto x22 = GetActorInformation(player2);
	auto x32 = AllocateActorInformation(player2);
	FreeActorInformation(player2);

	// Remove above

	auto player_information{GetActorInformation(player)};
	if (!player_information)
	{
		player_information = AllocateActorInformation(player);
	}

	ActorInformation<Player>::PlayerTickInformation player_tick_information{};
	player_tick_information.location_ = player->Location;
	player_tick_information.velocity_ = player->Velocity;
	player_information->tick_information_.PushBack(std::move(player_tick_information));
	player_information->team_ = player->PlayerReplicationInfo->Team->TeamIndex;
	return player_information;
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
}

bool LagCompensation::RewindPlayers(Ping ping_in_ms)
{
	auto tick_index{static_cast<int>(ping_in_ms / tick_delta_in_ms_)};
	auto prev_index{tick_index + 1};

	for (auto &player : list_of_players_in_latest_tick_)
	{
		if (IsPlayerValid(player))
		{
			auto player_information{GetActorInformation(player)};
			if (player_information && prev_index < player_information->tick_information_.Size())
			{
				const auto &tick_location{player_information->tick_information_[tick_index].location_};
				const auto &prev_location{player_information->tick_information_[prev_index].location_};

				auto delta{Subtract_VectorVector(prev_location, tick_location)};
				static constexpr auto interpolate_scalar{0.5};
				auto interpolated_location{Add_VectorVector(tick_location,
															Multiply_VectorFloat(delta, interpolate_scalar))};
				player->SetLocation(interpolated_location);
			}
		}
	}

	return true;
}

void LagCompensation::RestorePlayers(void)
{
	for (auto &player : list_of_players_in_latest_tick_)
	{
		if (IsPlayerValid(player))
		{
			if (auto player_information{GetActorInformation(player)})
			{
				player->SetLocation(player_information->tick_information_[0].location_);
			}
		}
	}
}