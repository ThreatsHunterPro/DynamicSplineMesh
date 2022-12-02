#pragma once
#include "ENUM_AxisRotation.h"
#include "STRUCT_MeshRotation.generated.h"

/* The current way to rotate the meshes on the spline */
USTRUCT(BlueprintType)
struct FMeshRotation
{
	GENERATED_BODY()

	/* The axis used for the rotation */
	UPROPERTY(EditAnywhere, Category = "Spline | Rotation")
		TEnumAsByte<EAxisRotation> axisRotation = TEnumAsByte<EAxisRotation>();

	/* The angle applied to the rotation */
	UPROPERTY(EditAnywhere, Category = "Spline | Rotation")
		float angle = 0.0f;
	
	FMeshRotation() {}
	
	FMeshRotation(TEnumAsByte<EAxisRotation> _axisRotation, const float _angle)
	{
		axisRotation = _axisRotation;
		angle = _angle;
	}
};