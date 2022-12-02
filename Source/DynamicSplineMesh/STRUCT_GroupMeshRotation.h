#pragma once
#include "STRUCT_MeshRotation.h"
#include "STRUCT_GroupMeshRotation.generated.h"

/* Rotate a group of meshes selected by index */
USTRUCT(BlueprintType)
struct FGroupMeshRotation
{
	GENERATED_BODY()

	/* Start index used to select the meshes on the spline */
	UPROPERTY(EditAnywhere, Category = "Spline | Rotation")
		int startIndex = 0;

	/* End index used to select the meshes on the spline */
	UPROPERTY(EditAnywhere, Category = "Spline | Rotation")
		int endIndex = 1;

	/* Mesh rotation to apply */
	UPROPERTY(EditAnywhere, Category = "Spline | Rotation")
		FMeshRotation meshRotation = FMeshRotation();
	
	FGroupMeshRotation() {}
};