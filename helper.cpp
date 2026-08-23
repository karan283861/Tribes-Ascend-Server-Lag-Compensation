#include <plog/Log.h>
#include "helper.hpp"
#include "validate.hpp"

bool IsActorValid(AActor* actor)
{
	return actor && !actor->bPendingDelete && !actor->bDeleteMe;
}

FVector Add_VectorVector(const FVector& A, const FVector& B)
{
	return FVector{A.X + B.X, A.Y + B.Y, A.Z + B.Z};
}

FVector Subtract_VectorVector(const FVector& A, const FVector& B)
{
	return FVector{A.X - B.X, A.Y - B.Y, A.Z - B.Z};
}

FVector Multiply_VectorFloat(const FVector& A, const float& B)
{
	return FVector{A.X * B, A.Y * B, A.Z * B};
}