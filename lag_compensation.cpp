#include <format>
#include <plog/Log.h>
#include "lag_compensation.hpp"

LagCompensation::ActorInformation::~ActorInformation(void)
{
}

LagCompensation::ActorInformation *LagCompensation::IsActorLagCompensated(AActor *actor, ActorId filtered_actor_id = ActorId::kAny)
{
	auto actor_information{GetLagCompensationData(actor)};

	if (actor_information)
	{
		if (filtered_actor_id == ActorId::kAny)
		{
			if (actor_information->actor_id_ != ActorId::kUnknown && actor_information->actor_id_ < ActorId::kAny)
			{
				return actor_information;
			}
		}
		else if (filtered_actor_id != ActorId::kUnknown)
		{
			if (actor_information->actor_id_ == filtered_actor_id)
			{
				return actor_information;
			}
		}
	}

	return nullptr;
}

LagCompensation::ActorInformation *LagCompensation::GetLagCompensationData(AActor *actor)
{
	return *reinterpret_cast<ActorInformation **>(&actor->EditorIconColor);
}

LagCompensation::ActorInformation *LagCompensation::LagCompensate(Controller *controller)
{
	auto controller_information{new ControllerInformation};
	memcpy(&controller->EditorIconColor, &controller_information, sizeof(size_t));
	return controller_information;
}

LagCompensation::ActorInformation *LagCompensation::LagCompensate(Player *player)
{
	auto player_information{new PlayerInformation};
	memcpy(&player->EditorIconColor, &player_information, sizeof(size_t));
	return player_information;
}

LagCompensation::ActorInformation *LagCompensation::LagCompensate(Projectile *projectile)
{
	auto controller{reinterpret_cast<Controller *>(projectile->InstigatorController)};

	auto ping_in_ms{controller->PlayerReplicationInfo->ExactPing * 4};

	// Only lag compensate if ping is within our lag compensation windows AND the player has simulated projectiles ENABLED
	if (ping_in_ms <= kMinimumPingThreshold || ping_in_ms >= window_in_ms_ || !controller->m_bAllowSimulatedProjectiles)
	{
		return nullptr;
	}

	auto projectile_information{new ProjectileInformation};
	memcpy(&projectile->EditorIconColor, &projectile_information, sizeof(size_t));
	projectile_information->owning_player_ = reinterpret_cast<Player *>(controller->Pawn);
	projectile_information->ping_in_ms_ = ping_in_ms;
	projectile_information->is_owning_player_still_valid_ = IsPlayerValid(projectile_information->owning_player_);

	auto controller_information{reinterpret_cast<ControllerInformation *>(GetLagCompensationData(controller))};
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
		controller_information = reinterpret_cast<ControllerInformation *>(LagCompensate(controller));
		controller_information->last_ping_in_ms_ = projectile_information->ping_in_ms_;
	}
	return projectile_information;
}

void LagCompensation::DestroyLagCompensationData(AActor *actor)
{
	auto information{GetLagCompensationData(actor)};
	if (information)
	{
		delete information;
		*reinterpret_cast<ActorInformation **>(&actor->EditorIconColor) = nullptr;
	}
}

void LagCompensation::UpdatePlayer(Player *player)
{
	auto player_information{reinterpret_cast<PlayerInformation *>(GetLagCompensationData(player))};
	if (!player_information)
	{
		player_information = reinterpret_cast<PlayerInformation *>(LagCompensate(player));
	}

	PlayerInformation::PlayerTickInformation player_tick_information{};
	player_tick_information.location_ = player->Location;
	player_tick_information.velocity_ = player->Velocity;
	player_information->tick_information_.push_back(std::move(player_tick_information));
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
	auto ms_remainder{fmod(ping_in_ms, tick_delta_in_ms_)};

	for (auto &player : list_of_players_in_latest_tick_)
	{
		if (IsPlayerValid(player))
		{
			auto player_information{reinterpret_cast<PlayerInformation *>(GetLagCompensationData(player))};
			if (prev_index < player_information->tick_information_.size())
			{
				const auto &tick_location{player_information->tick_information_.at(tick_index).location_};
				const auto &prev_location{player_information->tick_information_.at(prev_index).location_};

				auto delta{Subtract_VectorVector(prev_location, tick_location)};
				auto interpolate_scalar{ms_remainder / tick_delta_in_ms_};
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
			auto player_information{reinterpret_cast<PlayerInformation *>(GetLagCompensationData(player))};
			player->SetLocation(player_information->tick_information_.back().location_);
		}
	}
}