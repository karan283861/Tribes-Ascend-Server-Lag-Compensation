#include <algorithm>
#include <format>
#include <Windows.h>
#include <plog/Log.h>
#include "lag_compensation.hpp"
#include "helper.hpp"
#include "native_hooks.hpp"
#include "processinternal_hooks.hpp"

// #define ACTORTYPE_POSTVALIDATION(POINTER_TO_ACTOR, STATEMENT_ON_ERROR)

// #define IS_ACTOR_AND_ACTOR_INFORMATION_VALID(POINTER_TO_ACTOR, STATEMENT_ON_ERROR)                              \
// 	IS_ACTOR_VALID(POINTER_TO_ACTOR, STATEMENT_ON_ERROR)                                                        \
// 	if (PERFORM_ERROR_CHECK(!GetActorInformation(POINTER_TO_ACTOR),                                             \
// 							"{} is a {} and is deemed valid but has no actor information attached",             \
// 							#POINTER_TO_ACTOR,                                                                  \
// 							typeid(decltype(*POINTER_TO_ACTOR)).name()))                                        \
// 		STATEMENT_ON_ERROR;                                                                                     \
//                                                                                                                 \
// 	if (PERFORM_ERROR_CHECK(!GetActorInformation(POINTER_TO_ACTOR)->actor_,                                     \
// 							"{} is a {} but the attached actor information points to a nullptr",                \
// 							#POINTER_TO_ACTOR,                                                                  \
// 							typeid(decltype(*POINTER_TO_ACTOR)).name()))                                        \
// 		STATEMENT_ON_ERROR;                                                                                     \
//                                                                                                                 \
// 	if (PERFORM_ERROR_CHECK(POINTER_TO_ACTOR != GetActorInformation(POINTER_TO_ACTOR)->actor_,                  \
// 							"{} is a {} but the attached actor information points to a different actor (a {})", \
// 							#POINTER_TO_ACTOR,                                                                  \
// 							typeid(decltype(*POINTER_TO_ACTOR)).name(),                                         \
// 							GetActorInformation(POINTER_TO_ACTOR)->actor_->GetFullName()))                      \
// 		STATEMENT_ON_ERROR;                                                                                     \
//                                                                                                                 \
// 	if (PERFORM_ERROR_CHECK(!GetActorInformation(POINTER_TO_ACTOR)->IsValid(),                                  \
// 							"The attached actor information is deemed invalid"))                                \
// 		STATEMENT_ON_ERROR;

// #if defined(PERFORM_ERROR_CHECKS)
// template <IsAnActor ActorType>
// bool IsActorInformationValid(ActorType* actor)
// {
// 	auto type_name{std::string{typeid(ActorType).name()}};

// 	if (PERFORM_ERROR_CHECK(!actor, "Actor is a {} and is nullptr", type_name))
// 		return false;

// 	auto casted_actor{reinterpret_cast<ActorType*>(actor)};
// 	auto actor_information{GetActorInformation(casted_actor)};
// 	if (PERFORM_ERROR_CHECK(!actor_information, "{} deemed valid has no actor information attached",
// 							typeid(ActorType).name()))
// 		return false;

// 	return actor_information->IsValid();
// }
// #endif

LagCompensation&
LagCompensation::GetInstance(void)
{
	static LagCompensation lag_compensation{};
	return lag_compensation;
}

LagCompensation::LagCompensation()
{

	PLOG_INFO << std::format("Lag compensation: Tick rate = {} ms", tick_rate_);
	PLOG_INFO << std::format("Lag compensation: Window = {} ms", window_in_ms_);
	PLOG_INFO << std::format("Lag compensation: Minimum ping threshold = {} ms", kMinimumPingThreshold);
	PLOG_INFO << std::format("Lag compensation: Maximum ping delta = {} ms", kMaxPingDelta);

	pings_to_tick_in_latest_tick_.reserve(window_in_ms_);
	players_in_tick_.reserve(kEstimatedMaxPlayers);

	for (auto i{std::size_t{0}}; i < lag_compensated_projectiles_by_ping_in_tick_.size(); i++)
	{
		if (i <= kMinimumPingThreshold || i >= window_buffer_size_)
		{
			continue;
		}

		// Pings *SHOULD* always be a multiple of 4, so we can ignore anything not divisible 4
		// WARNING: This hasn't been confirmed
		if (i % 4 == 0)
		{
			lag_compensated_projectiles_by_ping_in_tick_[i].reserve(kEstimatedMaxProjectilesPerPing);
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

	auto hook_type_vmt_at_index{[&get_vmt_function_index, &overwrite_vmt_at_index]<IsAnActor ActorType>(size_t vmt_function_index, void* original_function, void* function) -> void
								{
									auto objects{GetInstancesOfUObjects<ActorType>()};
									for (auto& object : objects)
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

	hook_type_vmt_at_index.operator()<Player>(vmt_aactor_tick_index, original_pawn_tick, static_cast<void(__fastcall *)(Player*, void*, float, ELevelTick)>(&LagCompensation::OnActorTick));

	hook_type_vmt_at_index.operator()<Projectile>(vmt_aactor_tick_index, original_actor_tick, static_cast<void(__fastcall *)(Projectile*, void*, float, ELevelTick)>(&LagCompensation::OnActorTick));

	hook_type_vmt_at_index.operator()<Flag>(vmt_aactor_tick_index, original_actor_tick, static_cast<void(__fastcall *)(Flag*, void*, float, ELevelTick)>(&LagCompensation::OnActorTick));

}

LagCompensation::ActorObjectPoolTraits<Projectile>::InformationType* LagCompensation::AddProjectile(Projectile* projectile)
{
	IS_ACTOR_VALID(projectile, return nullptr);

	auto projectile_information{AllocateActorInformation(projectile)};
	if (!projectile_information)
	{
		// This is fine. AllocateActorInformation can fail if ActorInformation<Projectile>::IsValid returns false
		return nullptr;
	}

	// IsValid was true, so the instigator and controller should be valid and of the correct types
	auto player{reinterpret_cast<Player*>(projectile->Instigator)};
	auto controller{reinterpret_cast<Controller*>(projectile->InstigatorController)};

	IS_ACTOR_VALID(controller, return nullptr);
	IS_ACTOR_VALID(player, return nullptr);

	auto ping_in_ms{player->PlayerReplicationInfo->ExactPing * 4};
	Team team{player->PlayerReplicationInfo->Team->TeamIndex};

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
		// Definately an error if AllocateActorInformation failed for a Controller
		if (PERFORM_ERROR_CHECK(!controller_information, "Failed to allocate actor information"))
		{
			FreeActorInformation(projectile);
			return nullptr;
		}
		controller_information->last_ping_in_ms_ = projectile_information->ping_in_ms_;
	}

	IS_ACTOR_INFORMATION_VALID(controller, return nullptr);
	IS_ACTOR_INFORMATION_VALID(projectile, return nullptr);
	return projectile_information;
}

bool LagCompensation::OnActorTick(Projectile* projectile)
{
	IS_ACTOR_VALID(projectile, return false);

	auto projectile_information{GetActorInformation(projectile)};
	if (!projectile_information)
	{
		// Projectile is not lag compensated. Probably from a turret, vehicle, etc
		return false;
	}

	IS_ACTOR_INFORMATION_VALID(projectile, return false);

	auto& projectile_list{lag_compensated_projectiles_by_ping_in_tick_[projectile_information->ping_in_ms_]};

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

bool LagCompensation::OnActorTick(Player* player)
{
	IS_ACTOR_TYPE_VALID(player, return false);

	// It could be possible that a player has Died but isn't Destroy'ed so it's still ticking
	if (!IsValid(player))
	{
		return false;
	}

	IS_ACTOR_VALID(player, return false); // Unneccessary at the moment because this is just IS_ACTOR_TYPE_VALID & IsValid as above

	auto player_information{GetActorInformation(player)};
	if (!player_information) // No information attached currently, lets attach it
	{
		player_information = AllocateActorInformation(player);
		if (!player_information)
		{
			// Failure due to IsValid
			return false;
		}
		Team team{player->PlayerReplicationInfo->Team->TeamIndex};
		player_information->team_ = team;
	}

	ActorTickInformation<Player> player_tick_information{};
	player_tick_information.location_ = player->Location;
	player_information->rewind_information_.PushBack(std::move(player_tick_information));

	players_in_tick_.push_back(player);

	IS_ACTOR_INFORMATION_VALID(player, return false);

	return true;
}

bool LagCompensation::OnActorTick(Flag* player)
{
	return false;
}

// bool LagCompensation::RewindPlayers(Ping ping_in_ms)
// {
// 	if (PERFORM_ERROR_CHECK(!IsPingValid(ping_in_ms), "Ping argument is invalid ({})", ping_in_ms))
// 		return false;

// 	if (PERFORM_ERROR_CHECK(team_per_ping_[ping_in_ms] == kUninitialisedTeam, "A ping of {} has an uninitialised team", ping_in_ms))
// 		;

// 	bool all_projectiles_are_from_same_ping_bucket{team_per_ping_[ping_in_ms] != kInvalidTeam};
// 	auto tick_index{static_cast<int>(ping_in_ms / tick_delta_in_ms_)};
// 	auto prev_index{tick_index + 1};

// 	if (PERFORM_ERROR_CHECK(tick_index < 0, "Calculated tick_index is less than zero ({})", tick_index))
// 		return false;

// 	for (auto& player : players_in_tick_)
// 	{
// 		IS_ACTOR_TYPE_VALID(player, continue);

// 		// All players in players_in_tick_ were valid (alive) before we got here
// 		// Check they're still valid. If they are, then they should have a player information
// 		if (IsValid(player))
// 		{
// 			// Player could have died, so check IS_ACTOR_VALID AFTER IsValid
// 			IS_ACTOR_VALID(player, continue);
// 			IS_ACTOR_INFORMATION_VALID(player, continue);

// 			auto player_information{GetActorInformation(player)};

// 			if (all_projectiles_are_from_same_ping_bucket && player_information->team_ == team_per_ping_[ping_in_ms])
// 			{
// 				continue;
// 			}

// 			if (prev_index < player_information->rewind_information_.Size())
// 			{
// 				const auto& tick_location{player_information->rewind_information_[tick_index].location_};
// 				const auto& prev_location{player_information->rewind_information_[prev_index].location_};

// 				auto delta{Subtract_VectorVector(prev_location, tick_location)};
// 				static constexpr auto interpolate_scalar{0.5};
// 				auto interpolated_location{Add_VectorVector(tick_location,
// 															Multiply_VectorFloat(delta, interpolate_scalar))};
// 				original_world_farmoveactor(global_world, nullptr, player, interpolated_location, 0, 1, 0);
// 			}
// 		}
// 	}

// 	return true;
// }

// void LagCompensation::RestorePlayers(void)
// {
// 	for (auto& player : players_in_tick_)
// 	{
// 		IS_ACTOR_TYPE_VALID(player, continue);
// 		// All players in players_in_tick_ were valid (alive) before we got here
// 		// Check they're still valid. If they are, then they should have a player information
// 		if (IsValid(player))
// 		{
// 			// Player could have died, so check IS_ACTOR_VALID AFTER IsValid
// 			IS_ACTOR_VALID(player, continue);
// 			IS_ACTOR_INFORMATION_VALID(player, continue);

// 			auto player_information{GetActorInformation(player)};

// 			original_world_farmoveactor(global_world, nullptr, player, player_information->rewind_information_[0].location_, 0, 1, 0);
// 		}
// 	}
// }

template<IsAnActor ActorType>
requires IsRewindable<typename LagCompensation::ActorObjectPoolTraits<ActorType>::InformationType>
bool LagCompensation::Rewind(Ping ping_in_ms)
{
	if (PERFORM_ERROR_CHECK(!IsPingValid(ping_in_ms), "Ping argument is invalid ({})", ping_in_ms))
		return false;

	if (PERFORM_ERROR_CHECK(team_per_ping_[ping_in_ms] == kUninitialisedTeam, "A ping of {} has an uninitialised team", ping_in_ms))
		;

	bool all_projectiles_are_from_same_ping_bucket{};

	if constexpr(std::is_same_v<ActorType, Player>)
	{
		all_projectiles_are_from_same_ping_bucket = team_per_ping_[ping_in_ms] != kInvalidTeam;
	}
	auto tick_index{static_cast<int>(ping_in_ms / tick_delta_in_ms_)};
	auto prev_index{tick_index + 1};

	if (PERFORM_ERROR_CHECK(tick_index < 0, "Calculated tick index ({}) is less than zero", tick_index))
		return false;

	if (PERFORM_ERROR_CHECK(prev_index >= window_buffer_size_, "Calculated previous tick index ({}) is greater than or equal to window buffer size ({})", prev_index, window_buffer_size_))
		return false;

	for (auto actor : std::get<ActorPointerList<ActorType>>(actors_in_tick_))
	{
		IS_ACTOR_TYPE_VALID(actor, continue);

		// All players in players_in_tick_ were valid (alive) before we got here
		// Check they're still valid. If they are, then they should have a player information
		if (IsValid(actor))
		{
			// Player could have died, so check IS_ACTOR_VALID AFTER IsValid
			IS_ACTOR_VALID(actor, continue);
			IS_ACTOR_INFORMATION_VALID(actor, continue);

			auto actor_information{GetActorInformation(actor)};

			if constexpr(std::is_same_v<ActorType, Player>)
			{
				if (all_projectiles_are_from_same_ping_bucket && actor_information->team_ == team_per_ping_[ping_in_ms])
				{
					continue;
				}
			}

			if (prev_index < actor_information->rewind_information_.Size())
			{
				const auto& tick_location{actor_information->rewind_information_[tick_index].location_};
				const auto& prev_location{actor_information->rewind_information_[prev_index].location_};

				auto delta{Subtract_VectorVector(prev_location, tick_location)};
				static constexpr auto interpolate_scalar{0.5};
				auto interpolated_location{Add_VectorVector(tick_location,
															Multiply_VectorFloat(delta, interpolate_scalar))};
				original_world_farmoveactor(global_world, nullptr, actor, actor_information->rewind_information_[0].location_, 0, 1, 0);
			}
		}
	}

	return true;
}

template<IsAnActor ActorType>
requires IsRewindable<typename LagCompensation::ActorObjectPoolTraits<ActorType>::InformationType>
void LagCompensation::Restore(void)
{
	for (auto actor : std::get<ActorPointerList<ActorType>>(actors_in_tick_))
	{
		IS_ACTOR_TYPE_VALID(actor, continue);
		// All players in players_in_tick_ were valid (alive) before we got here
		// Check they're still valid. If they are, then they should have a player information
		if (IsValid(actor))
		{
			// Player could have died, so check IS_ACTOR_VALID AFTER IsValid
			IS_ACTOR_VALID(actor, continue);
			IS_ACTOR_INFORMATION_VALID(actor, continue);

			auto actor_informatiopn{GetActorInformation(actor)};

			original_world_farmoveactor(global_world, nullptr, actor, actor_informatiopn->rewind_information_[0].location_, 0, 1, 0);
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
		if (PERFORM_ERROR_CHECK(!IsPingValid(ping_in_ms), "Ping is invalid ({})", ping_in_ms))
			continue;

		auto& list_of_projectiles{lag_compensated_projectiles_by_ping_in_tick_[ping_in_ms]};

		if (PERFORM_ERROR_CHECK(list_of_projectiles.empty(), "List of projectiles for ping {} is empty", ping_in_ms))
			continue;

		performed_rewind_of_players |= Rewind<Player>(ping_in_ms);

		// Even if Rewind encounters an error, we still should tick the projectiles
		for (auto& projectile : list_of_projectiles)
		{
			IS_ACTOR_VALID(projectile, continue); // Hopefully, not critical error if not though

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
		Restore<Player>();
	}

	pings_to_tick_in_latest_tick_.clear();
	players_in_tick_.clear();
	std::fill(team_per_ping_.begin(), team_per_ping_.end(), kUninitialisedTeam);

	// Would it be faster to do below?
	// for (auto& ping : pings_to_tick_in_latest_tick_)
	// {
	// 	team_per_ping_[ping] = kUninitialisedTeam;
	// }
}

UE3_PROCESSINTERNAL_HOOK(LagCompensation::OnProjectileRadialDamage)
{
	auto projectile{reinterpret_cast<Projectile*>(calling_uobject)};

	IS_ACTOR_VALID(projectile, original_processinternal(calling_uobject, unused, stack, result));

	auto instigator{reinterpret_cast<Player*>(projectile->Instigator)};

	auto projectile_information{GetActorInformation(projectile)};

	// A non lag compensated projectile. Could be fired from a turret, vehicle, etc
	if (!projectile_information)
	{
		return original_processinternal(calling_uobject, unused, stack, result);
	}

	IS_ACTOR_INFORMATION_VALID(projectile, return original_processinternal(calling_uobject, unused, stack, result));

	auto rewind{Rewind<Player>(projectile_information->ping_in_ms_)};
	// An invalid instigator means the instigator has Died or Destroy'ed
	if (rewind && IsValidAndIsA<Player>(instigator))
	{
		// The instigator should never change once the projectile is created (UNLESS it is set to nullptr through GC)
		IS_ACTOR_VALID(instigator, Restore<Player>(); return original_processinternal(calling_uobject, unused, stack, result));
		IS_ACTOR_INFORMATION_VALID(instigator, Restore<Player>(); return original_processinternal(calling_uobject, unused, stack, result));

		auto player_information{GetActorInformation(instigator)};
		instigator->SetLocation(player_information->rewind_information_[0].location_);
	}

	// Apply splash (radial) damage.
	original_processinternal(calling_uobject, unused, stack, result);

	if (rewind)
	{
		Restore<Player>();
	}

	// Let UTProjectileDestroyed take care of calling FreeActorInformation
	// FreeActorInformation(projectile);
}

size_t LagCompensation::GetActorInformationIndex(AActor* actor)
{
	if (PERFORM_ERROR_CHECK(!actor, "Actor is nullptr"))
		return kInvalidObjectPoolIndex;

	auto index{*reinterpret_cast<size_t*>(&actor->EditorIconColor)};
	return index;
}

/// @cond DOXYGEN_SHOULD_SKIP_THIS
void __fastcall LagCompensation::OnActorTick(Player* player, void* unused, float delta_seconds, ELevelTick tick_type)
{
	if (!ticking_TG_PreAsyncWork)
	{
		return original_pawn_tick(player, nullptr, delta_seconds, tick_type);
	}

	IS_ACTOR_TYPE_VALID(player, original_pawn_tick(player, nullptr, delta_seconds, tick_type));

	static auto& lag_compensation{LagCompensation::GetInstance()};

	original_pawn_tick(player, nullptr, delta_seconds, tick_type);

	// NOTE: After ticking Player *may* become invalid. Ensure to call engine tick first
	// All Players are ticked in lag compensation
	// NOTE: Validation happens inside the OnActorTick<Player> function
	lag_compensation.OnActorTick(player);
}
/// @endcond

/// @cond DOXYGEN_SHOULD_SKIP_THIS
void __fastcall LagCompensation::OnActorTick(Projectile* projectile, void* unused, float delta_seconds, ELevelTick tick_type)
{
	if (!ticking_TG_PreAsyncWork)
	{
		return original_actor_tick(projectile, nullptr, delta_seconds, tick_type);
	}

	IS_ACTOR_VALID(projectile, original_actor_tick(projectile, nullptr, delta_seconds, tick_type));

	static auto& lag_compensation{LagCompensation::GetInstance()};
	if (!lag_compensation.OnActorTick(projectile))
	{
		// Return value tells us if we should absorb the call the below
		return original_actor_tick(projectile, nullptr, delta_seconds, tick_type);
	}
}
/// @endcond

/// @cond DOXYGEN_SHOULD_SKIP_THIS
void __fastcall LagCompensation::OnActorTick(Flag* flag, void* unused, float delta_seconds, ELevelTick tick_type)
{
	if (!ticking_TG_PreAsyncWork)
	{
		return original_actor_tick(flag, nullptr, delta_seconds, tick_type);
	}

	IS_ACTOR_TYPE_VALID(flag, original_actor_tick(flag, nullptr, delta_seconds, tick_type));

	static auto& lag_compensation{LagCompensation::GetInstance()};
	if (!lag_compensation.OnActorTick(flag))
	{
		// Return value tells us if we should absorb the call the below
		return original_actor_tick(flag, nullptr, delta_seconds, tick_type);
	}
}
/// @endcond