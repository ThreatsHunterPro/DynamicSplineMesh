#pragma once
#include "CoreMinimal.h"

#pragma region Custom includes

#pragma region Components

#include "Components/ArrowComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"

#pragma endregion

#pragma region Enums

#include "ENUM_CompositionMethod.h"
#include "ENUM_PlacementMethod.h"
#include "ENUM_RotationMethod.h"
#include "ENUM_CheckGroundMethod.h"

#pragma endregion

#pragma region Structs

#include "STRUCT_MeshComposition.h"
#include "STRUCT_MeshRotation.h"
#include "STRUCT_AngleMeshRotation.h"
#include "STRUCT_GroupMeshRotation.h"
#include "STRUCT_SplineMeshValues.h"

#pragma endregion

#pragma endregion 

#include "GameFramework/Actor.h"
#include "DynamicSplineMeshActor.generated.h"

USTRUCT()
struct FBridge
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Bridge values")
		FVector startLocation = FVector();

	UPROPERTY(VisibleAnywhere, Category = "Bridge values")
		FVector endLocation = FVector();

	FBridge() { }
	FBridge(FVector _startLocation, FVector _endLocation)
	{
		startLocation = _startLocation;
		endLocation = _endLocation;
	}

	FVector ComputeMiddleLocation() const
	{
		const float _x = ( startLocation.X + endLocation.X) / 2.0f;
		const float _y = ( startLocation.Y + endLocation.Y) / 2.0f;
		const float _z = ( startLocation.Z + endLocation.Z) / 2.0f;
		return FVector(_x, _y, _z);
	}
};

UCLASS()
class DYNAMICSPLINEMESH_API ADynamicSplineMeshActor : public AActor
{
	GENERATED_BODY()
	
	#pragma region Spline

	/* Version of this tool */
	UPROPERTY(VisibleAnywhere, Category = "Spline | Spline")
		FName version = "v2.0";

	/* The current spline */
	UPROPERTY(EditAnywhere, Category = "Spline | Spline")
		USplineComponent* spline = nullptr;

	/* Show spline orientation */
	UPROPERTY(VisibleAnywhere, Category = "Spline | Spline")
		UArrowComponent* directionalArrow = nullptr;

	#pragma endregion

	#pragma region Lag

	/*
	 * Rate used to space checks
	 * It works only when anti-lag is activated
	 */
	UPROPERTY(EditAnywhere, Category = "Spline | Lag", meta = (ClampMin = "0.01", ClampMax = "10.0"))
		float updateTimerRate = 1.0f;

	/*
	 * Timer used to update the spline
	 * It works only when anti-lag is activated
	 */
	UPROPERTY()
		FTimerHandle updateTimer = FTimerHandle();

	/*
	 * Transform registered at the previous check
	 * Allow to check if the current actor has moved
	 * It works only when anti-lag is activated
	 */
	UPROPERTY()
		FTransform previousTransform = FTransform();

	#pragma endregion

	#pragma region Composition

	/*
	 * The lenght of the spline
	 * Updated automatically after the spline update
	 * Modifiable to set the new spline lenght
	 */
	UPROPERTY(EditAnywhere, Category = "Spline | Composition")
		float lenght = 100.0f;

	/* The spline point type */
	UPROPERTY(EditAnywhere, Category = "Spline | Composition")
		TEnumAsByte<ESplinePointType::Type> splinePointType = TEnumAsByte<ESplinePointType::Type>();
	
	/* The composition method of the spline */
	UPROPERTY(EditAnywhere, Category = "Spline | Composition")
		TEnumAsByte<ECompositionMethod> compositionMethod = TEnumAsByte<ECompositionMethod>();

	/*
	 * The mesh composition used for the spline mesh
	 * Is active only when the composition method is set to "Fill" 
	 */
	UPROPERTY(EditAnywhere, Category = "Spline | Composition", meta = (EditCondition = "compositionMethod == ECompositionMethod::FILL", EditConditionHides))
		FMeshComposition meshComposition = FMeshComposition();
	
	/*
	 * The array of meshes composition used for the spline mesh
	 * Is active only when the composition method is set to "USUAL" or "RANDOM"
	 */
	UPROPERTY(EditAnywhere, Category = "Spline | Composition", meta = (EditCondition = "compositionMethod == ECompositionMethod::USUAL || compositionMethod == ECompositionMethod::RANDOM", EditConditionHides))
		TArray<FMeshComposition> meshesComposition = TArray<FMeshComposition>();

	#pragma endregion

	#pragma region Placement

	/* The placement method of the spline */
	UPROPERTY(EditAnywhere, Category = "Spline | Placement")
		TEnumAsByte<EPlacementMethod> placementMethod = TEnumAsByte<EPlacementMethod>();

	/*
	 * The gap between the meshes on the spline
	 * Is active only if the placement method is set to "Duplicate"
	 */
	UPROPERTY(EditAnywhere, Category = "Spline | Placement", meta = (EditCondition = "placementMethod == EPlacementMethod::DUPLICATE", EditConditionHides))
		float gap = 0.0f;

	/* The array of the meshes currently set on the spline */
	UPROPERTY(/*VisibleAnywhere, Category = "Spline | Placement"*/)
		TArray<USplineMeshComponent*> splineMeshes = TArray<USplineMeshComponent*>();

	#pragma endregion

	#pragma region Rotation

	/* The rotation method of the spline */
	UPROPERTY(EditAnywhere, Category = "Spline | Rotation")
		TEnumAsByte<ERotationMethod> rotationMethod = TEnumAsByte<ERotationMethod>();

	/* Applies a defined rotation for all meshes */
	UPROPERTY(EditAnywhere, Category = "Spline | Rotation", meta = (EditCondition = "rotationMethod == ERotationMethod::REGULAR", EditConditionHides))
		FMeshRotation meshRotation = FMeshRotation();

	/* Applies a defined rotation for each meshes */
	UPROPERTY(EditAnywhere, Category = "Spline | Rotation", meta = (EditCondition = "rotationMethod == ERotationMethod::IRREGULAR", EditConditionHides))
		TArray<FMeshRotation> meshesRotation = TArray<FMeshRotation>();

	/* Applies a defined rotation for a group from one index to another */
	UPROPERTY(EditAnywhere, Category = "Spline | Rotation", meta = (EditCondition = "rotationMethod == ERotationMethod::GROUP", EditConditionHides))
		TArray<FGroupMeshRotation> groupMeshRotation = TArray<FGroupMeshRotation>();

	/* Applies a defined rotation for the meshes selected by index */
	UPROPERTY(EditAnywhere, Category = "Spline | Rotation", meta = (EditCondition = "rotationMethod == ERotationMethod::ANGLE", EditConditionHides))
		TArray<FAngleMeshRotation> angleMeshRotation = TArray<FAngleMeshRotation>();

	#pragma endregion

	#pragma region CheckGround

	/* The condition to snap the spline on the ground */
	UPROPERTY(EditAnywhere, Category = "Spline | Ground")
		bool snapOnGround = true;

	/* Vertical offset to start the ground check */
	UPROPERTY(EditAnywhere, Category = "Spline | Ground", meta = (ClampMin = "-1000.0", ClampMax = "1000.0"))
		float zGroundCheckOffset = 0.0f;
	
	/* Depth of the ground check */
	UPROPERTY(EditAnywhere, Category = "Spline | Ground", meta = (ClampMin = "0.0", ClampMax = "1000.0"))
		float checkGroundDepth = 200.0f;
	
	/* Ground layer used for the ground check */
	UPROPERTY(EditAnywhere, Category = "Spline | Ground")
		TArray<TEnumAsByte<EObjectTypeQuery>> groundLayer = TArray<TEnumAsByte<EObjectTypeQuery>>();
	
	/* Ground check method */
	UPROPERTY(EditAnywhere, Category = "Spline | Ground")
		TEnumAsByte<ECheckGroundMethod> checkGroundMethod = TEnumAsByte<ECheckGroundMethod>();
	
	/*
	 * Number of check on the spline
	 * Usable only in the 'Points' check ground method
	 */
	UPROPERTY(EditAnywhere, Category = "Spline | Ground", meta = (ClampMin = "0", ClampMax = "1000", EditCondition = "checkGroundMethod == ECheckGroundMethod::POINTS", EditConditionHides))
		int checkGroundPointsCount = 10;
	
	/*
	 * Gap between the ground checks
	 * Usable only in the 'SPACING' check ground method
	 */
	UPROPERTY(EditAnywhere, Category = "Spline | Ground", meta = (ClampMin = "1.0", ClampMax = "10000.0", EditCondition = "checkGroundMethod == ECheckGroundMethod::SPACING", EditConditionHides))
		float checkGroundSpacing = 500.0f;
	
	#pragma endregion

	#pragma region Bridge

	UPROPERTY(EditAnywhere, Category = "Spline | Bridge")
		bool isBridge = false;

	UPROPERTY(EditAnywhere, Category = "Spline | Bridge")
		bool reverseTension = false;
	
	UPROPERTY(EditAnywhere, Category = "Spline | Bridge", meta = (ClampMin = "0.0", ClampMax = "1000.0"))
		float tension = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Spline | Bridge", meta = (ClampMin = "0.0", ClampMax = "1000.0"))
		float bridgeDepth = 0.0f;

	#pragma endregion
	
public:	
	ADynamicSplineMeshActor();
	
private:
	#if WITH_EDITOR

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	#endif
	
	#pragma region Init

	/* Reset the spline as default */
	UFUNCTION(CallInEditor, Category = "Spline => Editor") void ResetSpline();

	/* Define a new spline lenght */
	void SetSplineLenght(const float _lenght);

	/*
	 * Flush all debugs in the world
	 * Called when the editor button is pressed
	 */
	UFUNCTION(CallInEditor, Category = "Spline => Editor") FORCEINLINE void FlushDebug() const
	{
		FlushPersistentDebugLines(GetWorld());
	}

	#pragma endregion

	#pragma region Update

	/* Flush and reset the spline meshes with the different methods */
	UFUNCTION(CallInEditor, Category = "Spline => Editor") void UpdateSpline();

	/* Destroy all SplineMeshComponent */
	void FlushSpline();

	#pragma endregion

	#pragma region Composition

	/* Applies a duplication method to compose the spline */
	void DuplicateMesh();

	/* Applies a extend method to compose the spline */
	void ExtendMesh();

	/* Get a random mesh to compose the spline */
	FMeshComposition GetRandomMeshComposition() const;

	/* Randomize the meshes of the spline */
	UFUNCTION(CallInEditor, Category = "Spline => Editor", meta = (EditCondition = "composition == EComposition::RANDOM", EditConditionHides)) void RandomizeSpline();

	#pragma endregion

	#pragma region Placement

	/* Add a new mesh to the spline */
	void AddSplineMesh(const FMeshComposition& _meshComposition, const FSplineMeshValues& _values, const int _index);

	/* Rotate a specific mesh on the spline */
	void RotateSplineMesh(USplineMeshComponent* _splineMesh, const FSplineMeshValues& _values, const unsigned int _index) const;

	/* Get mesh rotation vector */
	FORCEINLINE FVector GetRotatedVector(const FMeshRotation& _meshRotation) const
	{
		const float _angle = FMath::DegreesToRadians(_meshRotation.angle);
		switch (_meshRotation.axisRotation)
		{
		case ROTATE_X:
			return FVector(0.0f, -FMath::Cos(_angle) , FMath::Sin(_angle));
				
		case ROTATE_Y:
			return FVector(FMath::Cos(_angle), 0.0f, FMath::Sin(_angle));
				
		case ROTATE_Z:
			return FVector(FMath::Cos(_angle), FMath::Sin(_angle), 0.0f);
			
		default:
			return FVector(0.0f);
		}
	}

	/* Get mesh rotation at a specific index */
	FORCEINLINE FMeshRotation GetMeshRotation(const int _index) const
	{
		return rotationMethod != REGULAR && _index < meshesRotation.Num() ? meshesRotation[_index] : meshRotation;
	}

	#pragma endregion

	#pragma region Rotation

	/*
	 *  Reset meshRotation values
	 *  Called when the rotation has changed
	 */
	FORCEINLINE void ResetMeshRotationValues()
	{
		meshRotation.axisRotation = ROTATE_X;
		meshRotation.angle = 0.0f;
		ResetMeshesRotation();
	}
		
	/*
	 * Reset meshRotation values
	 * Called when the rotation has changed
	 */
	void ResetMeshesRotation();

	/*
	 * Apply the rotation for the selected group
	 * Called when the rotation method is set to "Group"
	 */
	void SetRotationForGroup();

	/*
	 * Apply the rotation for the selected indexes
	 * Called when the rotation method is set to "Angle"
	 */
	void SetRotationForAngle();

	#pragma endregion

	#pragma region Ground

	/* Snap the spline on the ground */
	void SnapOnGround();

	/*
	 * Ground ground at distance along spline
	 * Update '_splinePoints' consequently
	 */
	void CheckGround(TArray<FVector>& _splinePoints, float _distance, float _depth);
	void MakeBridge();

#pragma endregion

	#pragma region Bridge



	#pragma endregion
};