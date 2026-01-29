#include <plog/Log.h>
#include "helper.hpp"

bool IsPlayerValid(Player *player)
{
	if (player && player->PlayerReplicationInfo && player->Health && !player->bDeleteMe)
	{
		return true;
	}
	PLOG_DEBUG << "Player is no longer valid";
	return false;
}

FVector Add_VectorVector(const FVector &A, const FVector &B)
{
	return FVector{A.X + B.X, A.Y + B.Y, A.Z + B.Z};
}

FVector Subtract_VectorVector(const FVector &A, const FVector &B)
{
	return FVector{A.X - B.X, A.Y - B.Y, A.Z - B.Z};
}

FVector Multiply_VectorFloat(const FVector &A, const float &B)
{
	return FVector{A.X * B, A.Y * B, A.Z * B};
}

std::string PrintVector(const FVector &v)
{
	return std::string("X = ").append(std::to_string(v.X)).append(", Y = ").append(std::to_string(v.Y)).append(", Z = ").append(std::to_string(v.Z));
}

extern const UClass *kControllerClass{Controller::StaticClass()};
extern const UClass *kPlayerClass{Player::StaticClass()};