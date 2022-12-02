#pragma once
#include "STRUCT_MeshComposition.generated.h"

/* The current way to compose the spline */
USTRUCT(BlueprintType)
struct FMeshComposition
{
	GENERATED_BODY()

	/* Temporary status of the scale factor display */
	UPROPERTY()
		bool displayScaleFactor = true;

	/* The mesh that makes up the spline */
	UPROPERTY(EditAnywhere, Category = "Mesh composition")
		UStaticMesh* mesh = nullptr;

	/*
	 * The scale of the mesh that makes up the spline
	 * Is active only if the placement method isn't set to "Extend"
	 */
	UPROPERTY(EditAnywhere, Category = "Mesh composition", meta = (ClampMin = "0.0", ClampMax = "10000.0", EditCondition = "displayScaleFactor", EditConditionHides))
		float scaleFactor = 1.0f;
	
	FMeshComposition() {}
};