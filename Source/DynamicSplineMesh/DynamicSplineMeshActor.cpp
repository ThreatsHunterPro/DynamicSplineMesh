#include "DynamicSplineMeshActor.h"

#include "LevelEditorActions.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

ADynamicSplineMeshActor::ADynamicSplineMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	
	spline = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
	spline->SetupAttachment(RootComponent);

	directionalArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrowComponent"));
	directionalArrow->SetupAttachment(spline);
}

#if WITH_EDITOR

void ADynamicSplineMeshActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	// Get TimerManager
	FTimerManager& _timerManager = GetWorld()->GetTimerManager();
	
	// Check if the check timer is already active
	if (_timerManager.IsTimerActive(updateTimer))
	{
		// If yes, reset it
		_timerManager.ClearTimer(updateTimer);
	}
	
	// Start a new timer
	_timerManager.SetTimer(updateTimer, this, &ADynamicSplineMeshActor::UpdateSpline, updateTimerRate);
}
void ADynamicSplineMeshActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Get the property that changed
	const FProperty* _propertyThatChanged = PropertyChangedEvent.Property;
	if (!_propertyThatChanged) return;

	// Store its name
	const FName& _propertyName = _propertyThatChanged->GetFName();

	// If the spline lenght has changed
	if (_propertyName == GET_MEMBER_NAME_CHECKED(ADynamicSplineMeshActor, lenght))
	{
		SetSplineLenght(lenght);
		UpdateSpline();
	}
		
	// If the placement method has changed
	else if (_propertyName == GET_MEMBER_NAME_CHECKED(ADynamicSplineMeshActor, placementMethod))
	{
		// Display the scale factor
		meshComposition.useScaleFactor = placementMethod != EXTEND;
	}
	
	// If the placement method has changed
	else if (_propertyName == GET_MEMBER_NAME_CHECKED(ADynamicSplineMeshActor, placementMethod))
	{
		// Display the scale factor
		meshComposition.useScaleFactor = placementMethod != EXTEND;
	}

	// If the rotation method has changed
	else if (_propertyName == GET_MEMBER_NAME_CHECKED(ADynamicSplineMeshActor, rotationMethod))
	{
		ResetMeshRotationValues();

		// If the group mesh rotation has changed
		if (rotationMethod == GROUP)
		{
			SetRotationForGroup();
		}

		// If the angle mesh rotation has changed
		else if (rotationMethod == ANGLE)
		{
			SetRotationForAngle();
		}
	}
}

#endif

#pragma region Init

void ADynamicSplineMeshActor::ResetSpline()
{
	// Flush all debugs
	FlushDebug();
	
	// Clear the spline points on the spline
	SetSplineLenght(150.0f);

	// Reset all rotation values
	ResetMeshRotationValues();

	// Stop ground checking
	snapOnGround = false;

	// Stop bridge mode
	isBridge = false;

	// Update the spline
	UpdateSpline();
}
void ADynamicSplineMeshActor::SetSplineLenght(const float _lenght)
{
	// Update the spline lenght
	lenght = _lenght;
	
	// Reset all spline points from the spline
	spline->ClearSplinePoints();
	
	// Restore first spline point at the spline location
	spline->AddSplineWorldPoint(GetActorLocation());
	
	// Restore the last spline point in the forward of the spline with lenght as distance
	spline->AddSplineWorldPoint(GetActorLocation() + GetActorForwardVector() * lenght);
}

#pragma endregion

#pragma region Update

void ADynamicSplineMeshActor::UpdateSpline()
{
	lenght = spline->GetSplineLength();
	
	// Flush the meshes from the spline
	FlushSpline();

	// Snap the spline on the ground
	SnapOnGround();

	// Enable bridge mode
	MakeBridge();
	
	// Select the method associated with the placement method
	switch (placementMethod)
	{
		case DUPLICATE:
			DuplicateMesh();
			break;

		case EXTEND:
			ExtendMesh();
			break;

		default:
			break;
	}
}
void ADynamicSplineMeshActor::FlushSpline()
{
	// Run through the spline meshes
	const int _splineMeshCount = splineMeshes.Num();
	for (int _splineMeshIndex = 0; _splineMeshIndex < _splineMeshCount; _splineMeshIndex++)
	{
		// Get the current spline mesh and destroy it
		USplineMeshComponent* _currentSplineMesh = splineMeshes[_splineMeshIndex];
		if (!_currentSplineMesh) continue;
		_currentSplineMesh->DestroyComponent();
	}

	// Clear the spline meshes
	splineMeshes.Empty();
}

#pragma endregion

#pragma region Composition

void ADynamicSplineMeshActor::DuplicateMesh()
{
	// Init local values
	const float _splineLength = spline->GetSplineLength();
	TArray<FMeshComposition> _meshesCompositions = TArray<FMeshComposition>();
	float _totalLength = 0.0f;

	// If the composition method is set to "Fill"
	if (compositionMethod == FILL)
	{
		// Get the mesh that will compose the spline
		const UStaticMesh* _mesh = meshComposition.mesh;
		if (!IsValid(_mesh)) return;

		// Get the length of a single mesh
		const float _sectionLength = _mesh->GetBoundingBox().GetSize().X * meshComposition.scaleFactor;

		// As long as the meshes can pass on the spline
		while (true)
		{
			// If the spline is full
			if (_totalLength + _sectionLength + gap > _splineLength) break;

			// Add a mesh to the spline
			_totalLength += _sectionLength + gap;
			_meshesCompositions.Add(meshComposition);
		}
	}

	// If the composition method is set to "Usual"
	else if (compositionMethod == USUAL)
	{
		// Run through the meshes composition
		const int _meshesCount = meshesComposition.Num();
		for (int _meshIndex = 0; _meshIndex < _meshesCount; _meshIndex++)
		{
			// Get the current mesh composition
			const FMeshComposition& _meshComposition = meshesComposition[_meshIndex];
			if (!IsValid(_meshComposition.mesh)) continue;

			// Get the lenght of a single mesh
			const float _meshLength = _meshComposition.mesh->GetBoundingBox().GetSize().X * _meshComposition.scaleFactor;

			// Check if the spline is full
			if (_totalLength + _meshLength + gap > _splineLength) break;

			// Add a mesh to the spline
			_totalLength += _meshLength + gap;
			_meshesCompositions.Add(_meshComposition);
		}
	}

	// If the composition method is set to "Random"
	else
	{
		// As long as the meshes can pass on the spline
		while (true)
		{
			// Get the current mesh composition
			const FMeshComposition& _meshComposition = GetRandomMeshComposition();
			if (!IsValid(_meshComposition.mesh)) continue;

			// Get the lenght of a single mesh
			const float _meshLength = _meshComposition.mesh->GetBoundingBox().GetSize().X * _meshComposition.scaleFactor;

			// Check if the spline is full
			if (_totalLength + _meshLength + gap > _splineLength) break;

			// Add a mesh to the spline
			_totalLength += _meshLength + gap;
			_meshesCompositions.Add(_meshComposition);
		}
	}

	// Run through the meshes composition
	float _previousEnd = 0.0f;
	const int _meshLengthCount = _meshesCompositions.Num();
	for (int _meshCompositionIndex = 0; _meshCompositionIndex < _meshLengthCount; _meshCompositionIndex++)
	{
		// Get mesh composition values
		const FMeshComposition& _meshComposition = _meshesCompositions[_meshCompositionIndex];
		const UStaticMesh* _staticMesh = _meshComposition.mesh;
		if (!IsValid(_staticMesh)) continue;
		const float _scale = _meshComposition.scaleFactor;

		// Compute start point value
		const float _sectionLength = _staticMesh->GetBoundingBox().GetSize().X * _scale;
		const float _startDistance = _meshCompositionIndex > 0 ? _previousEnd + gap : 0.0f;
		FVector _startLocation = spline->GetLocationAtDistanceAlongSpline(_startDistance, ESplineCoordinateSpace::Local);
		const FVector& _startTangent = spline->GetTangentAtDistanceAlongSpline(_startDistance, ESplineCoordinateSpace::Local);
		const FVector& _clampedStartTangent = _startTangent.GetClampedToSize(0.0f, _sectionLength);

		// Compute end point value
		const float _endDistance = _sectionLength + _startDistance;
		_previousEnd = _endDistance;
		FVector _endLocation = spline->GetLocationAtDistanceAlongSpline(_endDistance, ESplineCoordinateSpace::Local);
		const FVector& _endTangent = spline->GetTangentAtDistanceAlongSpline(_endDistance, ESplineCoordinateSpace::Local);
		const FVector& _clampedEndTangent = _endTangent.GetClampedToSize(0.0f, _sectionLength);

		if (snapOnGround)
		{
			// Get the lenght of a single mesh
			const float _meshHeight = _meshComposition.mesh->GetBoundingBox().GetSize().Z * _meshComposition.scaleFactor;
			
			// Compute the mesh offset
			const FVector& _meshOffset = GetActorUpVector() * (_meshHeight / 2.0f);

			// Update start and end spline mesh locations
			_startLocation += _meshOffset;
			_endLocation += _meshOffset;
		}
		
		// Add a new spline mesh
		AddSplineMesh(_meshComposition, FSplineMeshValues(_startLocation, _clampedStartTangent, _endLocation, _clampedEndTangent, FVector2D(_scale), FVector2D(_scale)), _meshCompositionIndex);
	}
}
void ADynamicSplineMeshActor::ExtendMesh()
{
	// Run through the spline points 
	const int32 _pointsCount = spline->GetNumberOfSplinePoints();
	for (int _splinePointIndex = 0; _splinePointIndex < _pointsCount - 1; _splinePointIndex++)
	{
		// Get the mesh composition according to the composition method
		const FMeshComposition& _meshComposition = compositionMethod != FILL && meshesComposition.Num() > 0 ? meshesComposition[0] : meshComposition;
		const UStaticMesh* _staticMesh = _meshComposition.mesh;
		if (!IsValid(_staticMesh)) continue;
		
		// Compute the start point of the spline
		FVector _startLocation = spline->GetLocationAtSplinePoint(_splinePointIndex, ESplineCoordinateSpace::Local);
		const FVector& _startTangent = spline->GetTangentAtSplinePoint(_splinePointIndex, ESplineCoordinateSpace::Local);

		// Compute the end point of the spline
		FVector _endLocation = spline->GetLocationAtSplinePoint(_splinePointIndex + 1, ESplineCoordinateSpace::Local);
		const FVector& _endTangent = spline->GetTangentAtSplinePoint(_splinePointIndex + 1, ESplineCoordinateSpace::Local);

		// Compute a new SplineMeshValue to be added as a spline mesh
		const float _meshSizeX = _staticMesh->GetBoundingBox().GetSize().X;
		const float _scale = FMath::Abs((_endLocation - _startLocation).Length() / _meshSizeX);

		if (snapOnGround)
		{
			// Get the lenght of a single mesh
			const float _meshHeight = _meshComposition.mesh->GetBoundingBox().GetSize().Z * _meshComposition.scaleFactor;
			
			// Compute the mesh offset
			const FVector& _meshOffset = GetActorUpVector() * (_meshHeight / 2.0f);

			// Update start and end spline mesh locations
			_startLocation += _meshOffset;
			_endLocation += _meshOffset;
		}
		
		AddSplineMesh(_meshComposition, FSplineMeshValues(_startLocation, _startTangent, _endLocation, _endTangent, FVector2D(_scale), FVector2D(_scale)), _splinePointIndex);
	}
}
FMeshComposition ADynamicSplineMeshActor::GetRandomMeshComposition() const
{
	const int _randomIndex = FMath::RandRange(0, meshesComposition.Num() - 1);
	return meshesComposition[_randomIndex];
}
void ADynamicSplineMeshActor::RandomizeSpline()
{
	const int _splineMeshCount = splineMeshes.Num();
	for	(int _splineMeshIndex = 0; _splineMeshIndex < _splineMeshCount; _splineMeshIndex++)
	{
		const int _randomIndex = FMath::RandRange(0, meshesComposition.Num() - 1);
		if (meshesComposition.Num() > _randomIndex)
		{
			splineMeshes[_splineMeshIndex]->SetStaticMesh(meshesComposition[_randomIndex].mesh);
		}
	}
}

#pragma endregion

#pragma region Placement

void ADynamicSplineMeshActor::AddSplineMesh(const FMeshComposition& _meshComposition, const FSplineMeshValues& _values, const int _index)
{
	USplineMeshComponent* _splineMesh = NewObject<USplineMeshComponent>(this, USplineMeshComponent::StaticClass());
	if (!_splineMesh) return;

	// Apply mesh
	if (IsValid(_meshComposition.mesh))
	{
		_splineMesh->SetStaticMesh(_meshComposition.mesh);
	}

	_splineMesh->SetMobility(EComponentMobility::Movable);
	_splineMesh->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	_splineMesh->RegisterComponentWithWorld(GetWorld());
	_splineMesh->AttachToComponent(spline, FAttachmentTransformRules::KeepRelativeTransform);
	_splineMesh->SetForwardAxis(ESplineMeshAxis::X);
	_splineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	RotateSplineMesh(_splineMesh, _values, _index);
	_splineMesh->SetStartScale(_values.startScale, true);
	_splineMesh->SetEndScale(_values.endScale, true);
	
	splineMeshes.Add(_splineMesh);
}
void ADynamicSplineMeshActor::RotateSplineMesh(USplineMeshComponent* _splineMesh, const FSplineMeshValues& _values, const unsigned int _index) const
{
	if (rotationMethod == NONE)
	{
		_splineMesh->SetStartAndEnd(_values.start, _values.startTangent, _values.end, _values.startTangent);
		return;
	}
	
	const FMeshRotation& _meshRotation = GetMeshRotation(_index);
	
	if (_meshRotation.axisRotation == ROTATE_X)
	{
		const float _angle = FMath::DegreesToRadians(_meshRotation.angle);
		_splineMesh->SetStartRoll(_angle);
		_splineMesh->SetEndRoll(_angle);
		_splineMesh->SetStartAndEnd(_values.start, _values.startTangent, _values.end, _values.endTangent);
		return;
	}

	const float _size = (_values.end - _values.start).Size();
	const FVector& _newEndLocation = _values.start + _size * GetRotatedVector(_meshRotation);
	const float _xTangents = _newEndLocation.X - _values.start.X;
	const FVector& _newTangentsLocation = _meshRotation.axisRotation == ROTATE_Y ? FVector(_xTangents, 0.0f, _newEndLocation.Z)
																				  : FVector(_xTangents, _newEndLocation.Y, 0.0f);
	
	_splineMesh->SetStartAndEnd(_values.start, _newTangentsLocation, _newEndLocation, _newTangentsLocation);
}

#pragma endregion

#pragma region Rotation

void ADynamicSplineMeshActor::ResetMeshesRotation()
{
	#pragma region MeshesRotation
	
	// Run through all meshes rotation
	const int _meshRotationCount = meshesRotation.Num();
	for (int _meshRotationIndex = 0; _meshRotationIndex < _meshRotationCount; _meshRotationIndex++)
	{
		// Reset all meshRotation
		meshesRotation[_meshRotationIndex] = FMeshRotation();
	}

	// Clear meshes rotation
	meshesRotation.Empty();

	#pragma endregion

	#pragma region GroupMeshRotation

	// Run through all group mesh rotation
	const int _groupMeshRotationCount = groupMeshRotation.Num();
	for (int _groupMeshRotationIndex = 0; _groupMeshRotationIndex < _groupMeshRotationCount; _groupMeshRotationIndex++)
	{
		groupMeshRotation[_groupMeshRotationIndex] = FGroupMeshRotation();
	}

	// Clear meshes rotation
	groupMeshRotation.Empty();

	#pragma endregion

	#pragma region AngleMeshRotation
	
	// Run through all angle mesh rotation
	const int _angleMeshRotationCount = angleMeshRotation.Num();
	for (int _angleMeshRotationIndex = 0; _angleMeshRotationIndex < _angleMeshRotationCount; _angleMeshRotationIndex++)
	{
		angleMeshRotation[_angleMeshRotationIndex] = FAngleMeshRotation();
	}

	// Clear meshes rotation
	angleMeshRotation.Empty();

	#pragma endregion
}
void ADynamicSplineMeshActor::SetRotationForGroup()
{
	// Run through all group mesh rotation
	const int _groupMeshCount = groupMeshRotation.Num();
	for (int _groupMeshIndex = 0; _groupMeshIndex < _groupMeshCount; _groupMeshIndex++)
	{
		// Get the current group mesh rotation
		FGroupMeshRotation& _groupMesh = groupMeshRotation[_groupMeshIndex];

		// If the end index is before the start index continue
		if (_groupMesh.endIndex < _groupMesh.startIndex)
		{
			_groupMesh.endIndex = _groupMesh.startIndex;
			continue;
		}
		
		for (int _index = _groupMesh.startIndex; _index <= _groupMesh.endIndex; _index++)
		{
			// If the current index is over the spline mesh count
			if (_index > splineMeshes.Num()) break;

			while (_index >= meshesRotation.Num())
			{
				UE_LOG(LogTemp, Warning, TEXT("while"));
				meshesRotation.Add(FMeshRotation());
			}

			meshesRotation.EmplaceAt(_index, _groupMesh.meshRotation);
		}
	}
}
void ADynamicSplineMeshActor::SetRotationForAngle()
{
	const int _angleMeshCount = angleMeshRotation.Num();
	for (int _angleMeshIndex = 0; _angleMeshIndex < _angleMeshCount; _angleMeshIndex++)
	{
		const FAngleMeshRotation& _angleMesh = angleMeshRotation[_angleMeshIndex];
		
		const int _indexesCount = _angleMesh.indexes.Num();
		for (int _index = 0; _index < _indexesCount; _index++)
		{
			const int _i = _angleMesh.indexes[_index];
			if (_i > splineMeshes.Num()) continue;
			
			while (_i > meshesRotation.Num())
			{
				meshesRotation.Add(FMeshRotation());
			}

			meshesRotation.EmplaceAt(_i, _angleMesh.meshRotation);
		}
	}
}

#pragma endregion

#pragma region Ground

void ADynamicSplineMeshActor::SnapOnGround()
{
	if (!snapOnGround || groundLayer.IsEmpty()) return;
		
	TArray<FVector> _splinePoints = TArray<FVector>();
	
	if (checkGroundMethod == POINTS)
	{
		const int _pointsCount = checkGroundPointsCount;
		const float _gap = lenght / _pointsCount;

		for (int _splinePointIndex = 0; _splinePointIndex <= _pointsCount; _splinePointIndex++)
		{
			CheckGround(_splinePoints, _splinePointIndex * _gap, checkGroundDepth);
		}
	}

	else
	{
		float _distance = 0.0f;
		while (_distance <= lenght)
		{
			CheckGround(_splinePoints, _distance, checkGroundDepth);
			_distance += checkGroundSpacing;
		}
	}

	spline->ClearSplinePoints();
		
	const int _splinePointsCount = _splinePoints.Num();
	for (int _splinePointIndex = 0; _splinePointIndex < _splinePointsCount; _splinePointIndex++)
	{
		spline->AddSplineWorldPoint(_splinePoints[_splinePointIndex]);
		spline->SetSplinePointType(_splinePointIndex, splinePointType);
	}
}
void ADynamicSplineMeshActor::CheckGround(TArray<FVector>& _splinePoints, float _distance, float _depth)
{
	FHitResult _hitResult = FHitResult();
	const FVector& _splinePointLocation = spline->GetWorldLocationAtDistanceAlongSpline(_distance);
	const FVector& _startLocation = _splinePointLocation + FVector::UpVector * zGroundCheckOffset;
	const FVector& _endLocation = _startLocation + FVector::DownVector * _depth;
	const bool _hasHit = UKismetSystemLibrary::LineTraceSingleForObjects(GetWorld(), _startLocation, _endLocation, groundLayer, false, TArray<AActor*>(), EDrawDebugTrace::None, _hitResult, true);

	if (_hasHit)
	{
		_splinePoints.Add(_hitResult.ImpactPoint);
	}
}

#pragma endregion

#pragma region Bridge

void ADynamicSplineMeshActor::MakeBridge()
{
	if (!isBridge) return;
	
	const int _pointsCount = checkGroundPointsCount;
	const float _gap = lenght / _pointsCount;
	TArray<FVector> _splinePoints = TArray<FVector>();
	
	for (int _splinePointIndex = 0; _splinePointIndex <= _pointsCount; _splinePointIndex++)
	{
		CheckGround(_splinePoints, _splinePointIndex * _gap, bridgeDepth);
	}

	const int _splinePointsCount = _splinePoints.Num();
	FVector _previousSplinePoint = FVector();
	TArray<FBridge> _bridges = TArray<FBridge>();
	FVector _startLocation = FVector();
	bool _hasStarted = false;
	
	for	(int _splinePointIndex = 0; _splinePointIndex < _splinePointsCount; _splinePointIndex++)
	{
		const FVector& _currentSplinePoint = _splinePoints[_splinePointIndex];

		if (_splinePointIndex >= 1 && _currentSplinePoint.Z != _previousSplinePoint.Z)
		{
			if (!_hasStarted)
			{
				_startLocation = FVector(_currentSplinePoint.X - _gap, _currentSplinePoint.Y, _previousSplinePoint.Z);
				_hasStarted = true;
			}
			
			else
			{
				_bridges.Add(FBridge(_startLocation, _currentSplinePoint));
				_hasStarted = false;
			}
		}

		_previousSplinePoint = _currentSplinePoint;
	}

	const int _bridgesCount = _bridges.Num();
	for	(int _bridgeIndex = 0; _bridgeIndex < _bridgesCount; _bridgeIndex++)
	{
		const FBridge& _bridge = _bridges[_bridgeIndex];
		const float _finalTension = reverseTension ? -tension : tension;
		const FVector& _middleLocation = _bridge.ComputeMiddleLocation() + FVector::UpVector * _finalTension;
		const int _inputKey = spline->FindInputKeyClosestToWorldLocation(_middleLocation) + 1;
		
		spline->AddSplinePointAtIndex(_middleLocation, _inputKey, ESplineCoordinateSpace::World);
		spline->SetSplinePointType(_inputKey, ESplinePointType::Curve);
	}
}

#pragma endregion