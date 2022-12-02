#pragma once
#include "STRUCT_SplineMeshValues.generated.h"

/* */
USTRUCT(BlueprintType)
struct FSplineMeshValues
{
	GENERATED_BODY()

	UPROPERTY()
		FVector start = FVector(0.0f);

	UPROPERTY()
		FVector startTangent = FVector(0.0f);

	UPROPERTY()
		FVector end = FVector(0.0f);

	UPROPERTY()
		FVector endTangent = FVector(0.0f);

	UPROPERTY()
		FVector2D startScale = FVector2D(0.0f);

	UPROPERTY()
		FVector2D endScale = FVector2D(0.0f);
	
	FSplineMeshValues() {}

	FSplineMeshValues(const FVector& _start, const FVector& _startTangent, const FVector& _end, const FVector& _endTangent, FVector2D _startScale = FVector2D(1.0), FVector2D _endScale = FVector2D(1.0))
	{
		start = _start;
		startTangent = _startTangent;
		end = _end;
		endTangent = _endTangent;
		startScale = _startScale;
		endScale = _endScale;
	}
};