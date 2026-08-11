#include <algorithm>
#include <format>
#include <Windows.h>
#include <plog/Log.h>
#include "lag_compensation.hpp"
#include "helper.hpp"
#include "native_hooks.hpp"

template <>
void __fastcall LagCompensation::ActorTick(Player* player, void* unused, float delta_seconds, ELevelTick tick_type);
template <>
void __fastcall LagCompensation::ActorTick(Projectile* projectile, void* unused, float delta_seconds, ELevelTick tick_type);

LagCompensation &LagCompensation::GetInstance(void)
{
	static LagCompensation lag_compensation{};
	return lag_compensation;
}

LagCompensation::LagCompensation()
{
	pings_to_tick_in_latest_tick_.reserve(window_in_ms_);
	list_of_players_in_latest_tick_.reserve(kEstimatedMaxPlayers);

	for (auto i{std::size_t{0}}; i < list_of_lag_compensated_projectiles_by_ping_in_latest_tick_.size(); i++)
	{
		if (i <= kMinimumPingThreshold || i >= window_buffer_size_)
		{
			continue;
		}

		// Pings *SHOULD* always be a multiple of 4, so we can ignore anything not divisible 4
		// WARNING: This hasn't been confirmed
		if (i % 4 == 0)
		{
			list_of_lag_compensated_projectiles_by_ping_in_latest_tick_[i].reserve(kEstimatedMaxProjectilesPerPing);
		}
	}

	std::fill(team_per_ping_.begin(), team_per_ping_.end(), kUninitialisedTeam);

	// Members initialised, let's now hook the VMTs and overwrite AActor::Tick
	auto get_vmt_function_index{[](void* object, size_t index) -> void*
								{
									if (object)
									{
										auto vmt{*reinterpret_cast<size_t**>(object)};
										auto vmt_at_index{vmt + index};
										return reinterpret_cast<void*>(*vmt_at_index);
									}
									return nullptr;
								}};

	auto overwrite_vmt_at_index{[](void* object, size_t index, void* function) -> bool
								{
									if (object)
									{
										auto vmt{*reinterpret_cast<size_t**>(object)};
										auto vmt_at_index{vmt + index};
										DWORD old_protection{};

										if (!VirtualProtect(vmt_at_index, sizeof(void*), PAGE_READWRITE, &old_protection))
										{
											return false;
										}

										*vmt_at_index = reinterpret_cast<size_t>(function);

										DWORD unused{};
										VirtualProtect(vmt_at_index, sizeof(void*), old_protection, &unused);

										return true;
									}
									return false;
								}};
	constexpr size_t vmt_aactor_tick_index{98};

	auto hook_type_vmt_at_index{[&get_vmt_function_index, &overwrite_vmt_at_index]<typename ActorType>(size_t vmt_function_index, void* original_function, void* function) -> void
								{
									auto objects{GetInstancesUObjects<ActorType>()};
									for (auto &object : objects)
									{
										if (get_vmt_function_index(object, vmt_function_index) == function)
										{
											PLOG_INFO << std::format("{} already has VMT index {} pointing to hook function", object->GetFullName(), vmt_function_index);
											continue;
										}

										if (get_vmt_function_index(object, vmt_function_index) != original_function)
										{
											PLOG_WARNING << std::format("{} does not point to original function at VMT index {}", object->GetFullName(), vmt_function_index);
										}
										else
										{
											if (overwrite_vmt_at_index(object, vmt_function_index, function))
											{
												PLOG_INFO << std::format("Hooked VMT index {} for {}", vmt_function_index, object->GetFullName());
											}
											else
											{
												PLOG_ERROR << std::format("Failed to hook VMT index {} for {}", vmt_function_index, object->GetFullName());
											}
										}
									}
								}};

	hook_type_vmt_at_index.operator()<Player>(vmt_aactor_tick_index, original_pawn_tick, reinterpret_cast<void*>(&ActorTick<Player>));

	hook_type_vmt_at_index.operator()<Projectile>(vmt_aactor_tick_index, original_actor_tick, reinterpret_cast<void*>(&ActorTick<Projectile>));
}

LagCompensation::ActorObjectPoolTraits<Projectile>::InformationType* LagCompensation::AddProjectile(Projectile* projectile)
{
	auto projectile_information{AllocateActorInformation(projectile)};
	if (!projectile_information)
	{
		// This is fine. AllocateActorInformation can fail if ActorInformation<Projectile>::ShouldAllocate returns false
		return nullptr;
	}

	auto instigator{reinterpret_cast<Controller*>(projectile->Instigator)};
	auto controller{reinterpret_cast<Controller*>(projectile->InstigatorController)};

	auto ping_in_ms{controller->PlayerReplicationInfo->ExactPing * 4};
	Team team{controller->PlayerReplicationInfo->Team->TeamIndex};

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
		if (PERFORM_ERROR_CHECK(!controller_information, "Failed to allocate actor information"))
		{
			FreeActorInformation(projectile);
			return nullptr;
		}
		controller_information->last_ping_in_ms_ = projectile_information->ping_in_ms_;
	}

	return projectile_information;
}

template <>
bool LagCompensation::OnActorTick(Projectile* projectile)
{
	if (!IsValid(projectile))
	{
		return false;
	}

	auto projectile_information{GetActorInformation(projectile)};
	if (!projectile_information)
	{
		return false;
	}

	auto &projectile_list{list_of_lag_compensated_projectiles_by_ping_in_latest_tick_[projectile_information->ping_in_ms_]};

	if (projectile_list.empty())
	{
		pings_to_tick_in_latest_tick_.push_back(projectile_information->ping_in_ms_);
	}

	projectile_list.push_back(projectile);

	if (team_per_ping_[projectile_information->ping_in_ms_] == LagCompensation::kUninitialisedTeam)
	{
		team_per_ping_[projectile_information->ping_in_ms_] = projectile_information->team_;
	}
	else if (team_per_ping_[projectile_information->ping_in_ms_] != projectile_information->team_)
	{
		team_per_ping_[projectile_information->ping_in_ms_] = LagCompensation::kInvalidTeam;
	}

	return true;
}

template <>
bool LagCompensation::OnActorTick(Player* player)
{
	// WARNING: This will crash if player is nullptr... we assume at the very least anything passed
	// to OnActorTick is non null
	auto controller{reinterpret_cast<Controller*>(player->Controller)};

	// It could be possible that a player has Died but isn't Destroy'ed so it's still ticking
	if (!IsValid(player) || !IsValid(controller))
	{
		return false;
	}

	auto player_information{GetActorInformation(player)};
	if (!player_information)
	{
		player_information = AllocateActorInformation(player);
		if (!player_information)
		{
			return false;
		}
		Team team{player->PlayerReplicationInfo->Team->TeamIndex};
		player_information->team_ = team;
	}

	ActorInformation<Player>::PlayerTickInformation player_tick_information{};
	player_tick_information.location_ = player->Location;
	player_tick_information.velocity_ = player->Velocity;
	player_information->tick_information_.PushBack(std::move(player_tick_information));

	list_of_players_in_latest_tick_.push_back(player);

	return true;
}

bool LagCompensation::RewindPlayers(Ping ping_in_ms)
{
	if (PERFORM_ERROR_CHECK(ping_in_ms < 0, "Ping argument is less than zero ({})", ping_in_ms))
		return false;

	if (PERFORM_ERROR_CHECK(ping_in_ms >= window_in_ms_, "Ping argument is equal to or greater than window ({})", ping_in_ms))
		return false;

	if (PERFORM_ERROR_CHECK(team_per_ping_[ping_in_ms] == kUninitialisedTeam, "A ping of {} has an uninitialised team"))
		;

	bool all_projectiles_are_from_same_ping_bucket{team_per_ping_[ping_in_ms] != kInvalidTeam};
	auto tick_index{static_cast<int>(ping_in_ms / tick_delta_in_ms_)};
	auto prev_index{tick_index + 1};

	if (PERFORM_ERROR_CHECK(tick_index < 0, "Calculated tick_index is less than zero ({})", tick_index))
		return false;

	for (auto &player : list_of_players_in_latest_tick_)
	{
		if (PERFORM_ERROR_CHECK(!player, "Player in list_of_players_in_latest_tick_ is nullptr"))
			continue;

		// All players in list_of_players_in_latest_tick_ were valid (alive) before we got here
		// Check they're still valid. If they are, then they should have a player information
		if (IsValid(player))
		{
			auto player_information{GetActorInformation(player)};
			if (PERFORM_ERROR_CHECK(!player_information, "Player deemed valid has no player information attached"))
				continue;

			if (PERFORM_ERROR_CHECK(player_information->tick_information_.Size() == 0, "Player deemed valid has an empty tick information in attached player information"))
				continue;

			if (PERFORM_ERROR_CHECK(player_information->team_ == kUninitialisedTeam || player_information->team_ == kInvalidTeam,
									"Player deemed valid belongs to an invalid team"))
				continue;

			if (all_projectiles_are_from_same_ping_bucket && player_information->team_ == team_per_ping_[ping_in_ms])
			{
				continue;
			}

			if (prev_index < player_information->tick_information_.Size())
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
		if (PERFORM_ERROR_CHECK(!player, "Player in list_of_players_in_latest_tick_ is nullptr"))
			continue;

		// All players in list_of_players_in_latest_tick_ were valid (alive) before we got here
		// Check they're still valid. If they are, then they should have a player information
		if (IsValid(player))
		{
			auto player_information{GetActorInformation(player)};
			if (PERFORM_ERROR_CHECK(!player_information, "Player deemed valid has no player information attached"))
				continue;

			if (PERFORM_ERROR_CHECK(player_information->tick_information_.Size() == 0, "Player deemed valid has an empty tick information in attached player information"))
				continue;
			original_world_farmoveactor(global_world, nullptr, player, player_information->tick_information_[0].location_, 0, 1, 0);
		}
	}
}

void LagCompensation::Tick(float delta_seconds, ELevelTick tick_type)
{
	bool performed_rewind_of_players{false};

	if (PERFORM_ERROR_CHECK(delta_seconds <= 0, "DeltaSeconds is less than or equalt to zero ({})", delta_seconds))
		return;

	for (auto ping_in_ms : pings_to_tick_in_latest_tick_)
	{
		if (PERFORM_ERROR_CHECK(ping_in_ms < 0, "Ping is ping_in_ms is less than zero ({})", ping_in_ms))
			continue;

		auto &list_of_projectiles{list_of_lag_compensated_projectiles_by_ping_in_latest_tick_[ping_in_ms]};

		if (PERFORM_ERROR_CHECK(list_of_projectiles.empty(), "List of projectiles for ping {} is empty", ping_in_ms))
			continue;

		performed_rewind_of_players |= RewindPlayers(ping_in_ms);

		// Even if RewindPlayers encounters an error, we still should tick the projectiles
		for (auto &projectile : list_of_projectiles)
		{
			if (PERFORM_ERROR_CHECK(!IsValid(projectile), "Projectile attempting to tick was deemed invalid"))
				continue;

			// Actually make the projectile Tick through the original native engine function
			if (IsValid(projectile))
			{
				original_actor_tick(projectile, nullptr, delta_seconds, tick_type);
			}
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

	// Would it be faster to do below?
	// for (auto& ping : pings_to_tick_in_latest_tick_)
	// {
	// 	team_per_ping_[ping] = kUninitialisedTeam;
	// }
}

size_t LagCompensation::GetActorInformationIndex(AActor* actor)
{
	if (PERFORM_ERROR_CHECK(!actor, "Actor is nullptr"))
		return kInvalidObjectPoolIndex;

	auto index{*reinterpret_cast<size_t*>(&actor->EditorIconColor)};
	return index;
}

template <>
void __fastcall LagCompensation::ActorTick(Player* player, void* unused, float delta_seconds, ELevelTick tick_type)
{
	if (!ticking_TG_PreAsyncWork)
	{
		return original_pawn_tick(player, nullptr, delta_seconds, tick_type);
	}

	static auto &lag_compensation{LagCompensation::GetInstance()};

	original_pawn_tick(player, nullptr, delta_seconds, tick_type);

	// NOTE: After ticking Player *may* become invalid. Ensure to call engine tick first
	// All Players are ticked in lag compensation
	// NOTE: Validation happens inside the OnActorTick<Player> function
	lag_compensation.OnActorTick(player);
}

template <>
void __fastcall LagCompensation::ActorTick(Projectile* projectile, void* unused, float delta_seconds, ELevelTick tick_type)
{
	if (!ticking_TG_PreAsyncWork)
	{
		return original_actor_tick(projectile, nullptr, delta_seconds, tick_type);
	}

	static auto &lag_compensation{LagCompensation::GetInstance()};
	if (!lag_compensation.OnActorTick(projectile))
	{
		// Return value tells us if we should absorb the call the below
		return original_actor_tick(projectile, nullptr, delta_seconds, tick_type);
	}
}