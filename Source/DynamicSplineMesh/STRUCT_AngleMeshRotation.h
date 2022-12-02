#pragma once
#include "STRUCT_MeshRotation.h"
#include "STRUCT_AngleMeshRotation.generated.h"

/* Rotate the meshes selected by index */
USTRUCT(BlueprintType)
struct FAngleMeshRotation
{
	GENERATED_BODY()

	/* Mesh rotation to apply */
	UPROPERTY(EditAnywhere, Category = "Spline | Rotation")
		FMeshRotation meshRotation = FMeshRotation();

	/* Array of indexes used to select meshes on the spline */
	UPROPERTY(EditAnywhere, Category = "Spline | Rotation")
		TArray<unsigned int> indexes = TArray<unsigned int>();
	
	FAngleMeshRotation() {}
};