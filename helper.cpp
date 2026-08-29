#include <plog/Log.h>
#include "helper.hpp"
#include "validate.hpp"

bool IsActorValid(AActor* actor)
{
	return actor && !actor->bPendingDelete && !actor->bDeleteMe;
}

Vector3D Add_VectorVector(const Vector3D& A, const Vector3D& B)
{
	return Vector3D{A.X + B.X, A.Y + B.Y, A.Z + B.Z};
}

Vector3D Subtract_VectorVector(const Vector3D& A, const Vector3D& B)
{
	return Vector3D{A.X - B.X, A.Y - B.Y, A.Z - B.Z};
}

Vector3D Multiply_VectorFloat(const Vector3D& A, const float& B)
{
	return Vector3D{A.X * B, A.Y * B, A.Z * B};
}