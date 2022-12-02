#include "DynamicSplineMeshActor.h"
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
	// Show on screen the current version of this tool
	GEngine->AddOnScreenDebugMessage(0, 5.0f, FColor::Blue, *version.ToString());

	// If the anti-lag is activated
	if (useAntiLag)
	{
		FTimerManager& _timerManager = GetWorld()->GetTimerManager();

		// Check if the check timer is already active
		if (!_timerManager.IsTimerActive(updateTimer))
		{
			// If not, start it
			_timerManager.SetTimer(updateTimer, this, &ADynamicSplineMeshActor::CheckForUpdate, updateTimerRate);
		}
	}

	// Otherwise, update directly the spline
	else
	{
		UpdateSpline();
	}
}

void ADynamicSplineMeshActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Get the property that changed
	const FProperty* _propertyThatChanged = PropertyChangedEvent.Property;
	if (!_propertyThatChanged) return;

	// Store its name
	const FName& _propertyName = _propertyThatChanged->GetFName();

	// If the placement method has changed
	if (_propertyName == GET_MEMBER_NAME_CHECKED(ADynamicSplineMeshActor, placementMethod))
	{
		// Display the scale factor
		meshComposition.displayScaleFactor = placementMethod != EXTEND;
		return;
	}

	// If the rotation method has changed
	if (_propertyName == GET_MEMBER_NAME_CHECKED(ADynamicSplineMeshActor, rotationMethod))
	{
		UE_LOG(LogTemp, Warning, TEXT("rotationMethod changed !"));
		ResetMeshRotationValues();
		return;
	}

	// If the group mesh rotation has changed
	if (rotationMethod == GROUP)
	{
		UE_LOG(LogTemp, Warning, TEXT("groupMeshRotation changed !"));
		SetRotationForGroup();
		return;
	}

	// If the angle mesh rotation has changed
	if (rotationMethod == ANGLE)
	{
		UE_LOG(LogTemp, Warning, TEXT("groupMeshRotation changed !"));
		SetRotationForAngle();
		return;
	}
}

#endif

#pragma region Init

void ADynamicSplineMeshActor::SetSplineLenght() const
{
	const int _lastSplinePointIndex = spline->GetNumberOfSplinePoints() - 1;
	const FVector& _newEndLocation = spline->GetLocationAtDistanceAlongSpline(lenght, ESplineCoordinateSpace::World);
	spline->SetLocationAtSplinePoint(_lastSplinePointIndex, _newEndLocation, ESplineCoordinateSpace::World);
}

#pragma endregion

#pragma region Update

void ADynamicSplineMeshActor::UpdateSpline()
{
	// Flush the meshes from the spline
	FlushSpline();
	
	if (snapOnGround)
	{
		SnapSplinePointsOnGround();
		CreateSplinePointsOnGround();
		CreateSplinePointsBetween();
	}

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

	if (snapOnGround)
	{
		const int _splinePointsCount = splinePoints.Num();
		for (int _splinePointIndex = 0; _splinePointIndex < _splinePointsCount; _splinePointIndex++)
		{
			spline->RemoveSplinePoint(splinePoints[_splinePointIndex]);
		}
		splinePoints.Empty();

		raycasts.Empty();
	}
}

#pragma endregion

#pragma region Composition

//TODO faire une méthode commune pour FILL et Random
void ADynamicSplineMeshActor::DuplicateMesh()
{
	// Init local values
	const float _splineLength = spline->GetSplineLength();
	TArray<FMeshComposition> _meshesCompositions = TArray<FMeshComposition>();
	bool _isFull = false;
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
		while (!_isFull)
		{
			// If the spline is full
			if (_totalLength + _sectionLength + gap > _splineLength)
			{
				// Exit 
				_isFull = true;
			}

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
			const FMeshComposition& _meshComposition = GetMeshComposition(_meshIndex);
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
		while (!_isFull)
		{
			// Get the current mesh composition
			const FMeshComposition& _meshComposition = GetMeshComposition();
			if (!IsValid(_meshComposition.mesh)) continue;

			// Get the lenght of a single mesh
			const float _meshLength = _meshComposition.mesh->GetBoundingBox().GetSize().X * _meshComposition.scaleFactor;

			// Check if the spline is full
			if (_totalLength + _meshLength + gap > _splineLength)
			{
				_isFull = true;
			}

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
		const FVector& _startLocation = spline->GetLocationAtDistanceAlongSpline(_startDistance, ESplineCoordinateSpace::Local);
		const FVector& _startTangent = spline->GetTangentAtDistanceAlongSpline(_startDistance, ESplineCoordinateSpace::Local);
		const FVector& _clampedStartTangent = _startTangent.GetClampedToSize(0.0f, _sectionLength);

		// Compute end point value
		const float _endDistance = _sectionLength + _startDistance;
		_previousEnd = _endDistance;
		const FVector& _endLocation = spline->GetLocationAtDistanceAlongSpline(_endDistance, ESplineCoordinateSpace::Local);
		const FVector& _endTangent = spline->GetTangentAtDistanceAlongSpline(_endDistance, ESplineCoordinateSpace::Local);
		const FVector& _clampedEndTangent = _endTangent.GetClampedToSize(0.0f, _sectionLength);

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
		// Compute the start point of the spline
		const FVector& _startLocation = spline->GetLocationAtSplinePoint(_splinePointIndex, ESplineCoordinateSpace::Local);
		const FVector& _startTangent = spline->GetTangentAtSplinePoint(_splinePointIndex, ESplineCoordinateSpace::Local);

		// Compute the end point of the spline
		const FVector& _endLocation = spline->GetLocationAtSplinePoint(_splinePointIndex + 1, ESplineCoordinateSpace::Local);
		const FVector& _endTangent = spline->GetTangentAtSplinePoint(_splinePointIndex + 1, ESplineCoordinateSpace::Local);

		// Get the mesh composition according to the composition method
		const FMeshComposition& _meshComposition = compositionMethod != FILL && meshesComposition.Num() > 0 ? meshesComposition[0] : meshComposition;
		const UStaticMesh* _staticMesh = _meshComposition.mesh;
		if (!IsValid(_staticMesh)) continue;

		// Compute a new SplineMeshValue to be added as a spline mesh
		const float _meshSizeX = _staticMesh->GetBoundingBox().GetSize().X;
		const float _scale = FMath::Abs((_endLocation - _startLocation).Length() / _meshSizeX);
		AddSplineMesh(_meshComposition, FSplineMeshValues(_startLocation, _startTangent, _endLocation, _endTangent, FVector2D(_scale), FVector2D(_scale)), _splinePointIndex);
	}
}

//TODO remove
FMeshComposition ADynamicSplineMeshActor::GetMeshComposition(const int _splinePointIndex) const
{
	switch (compositionMethod)
	{
	default:
		return FMeshComposition();

	case FILL:
		return meshComposition;

	case USUAL:
		// if (_splinePointIndex < meshesComposition.Num())
		// {
		// 	return meshesComposition[_splinePointIndex];
		// }

	case RANDOM:
		const int _randomIndex = FMath::RandRange(0, meshesComposition.Num() - 1);
		if (_randomIndex < meshesComposition.Num())
		{
			return meshesComposition[_randomIndex];
		}
	}

	return FMeshComposition();
}

#pragma endregion

#pragma region Placement

void ADynamicSplineMeshActor::AddSplineMesh(const FMeshComposition& _meshComposition, const FSplineMeshValues& _values, const int _index)
{
	USplineMeshComponent* _splineMesh = NewObject<USplineMeshComponent>(this, USplineMeshComponent::StaticClass());
	if (!_splineMesh) return;

	_splineMesh->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	_splineMesh->RegisterComponent(); //TODO tester de le register que une seule fois

	// Apply mesh
	if (IsValid(_meshComposition.mesh))
	{
		_splineMesh->SetStaticMesh(_meshComposition.mesh);
	}
	
	_splineMesh->AttachToComponent(spline, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	_splineMesh->SetForwardAxis(ESplineMeshAxis::X);
	_splineMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	_splineMesh->SetMobility(EComponentMobility::Movable);
	
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

//TODO implement
void ADynamicSplineMeshActor::ClipMeshes()
{
	// - Projeté en haut a gauche devient le start en X
	// - le projeté en bas à droite devient le end du current en X (= pour la rotation en Y)
	
	const int _meshesCount = splineMeshes.Num();
	for (int _meshIndex = 0; _meshIndex < _meshesCount; _meshIndex++)
	{
		USplineMeshComponent* _splineMesh = splineMeshes[_meshIndex];
		const UStaticMesh* _mesh = _splineMesh->GetStaticMesh();
		if (!IsValid(_mesh)) continue;

		const FVector& _ownerLocation = _splineMesh->GetOwner()->GetActorLocation();
		const FVector& _startLocation = _ownerLocation + _splineMesh->GetStartPosition();
		const float _meshSize = _mesh->GetBoundingBox().GetSize().Z/* * meshesComposition[_meshIndex].scaleFactor*/;
		
		// le point en bas a droite du mesh

		//TODO remove
		// UE_LOG(LogTemp, Warning, TEXT("Start : %s"), *_startLocation.ToString());
		// UE_LOG(LogTemp, Warning, TEXT("Size : %f"), _meshSize);
		// UE_LOG(LogTemp, Warning, TEXT("Result : %s"), *FVector(_startLocation.X + _meshSize, _startLocation.Y, _startLocation.Z).ToString());

		DrawDebugBox(GetWorld(), _startLocation, FVector(10.0f), FColor::Black, false, 10.0f, 0, 3.0f);
		DrawDebugBox(GetWorld(), FVector(_startLocation.X + _meshSize, _startLocation.Y, _startLocation.Z), FVector(10.0f), FColor::Blue, false, 10.0f, 0, 3.0f);

		_splineMesh->SplineParams.StartPos = FVector(_startLocation.X + _meshSize, _startLocation.Y, _startLocation.Z);
		// spline->FindLocationClosestToWorldLocation()
	}
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

void ADynamicSplineMeshActor::SnapSplinePointsOnGround() const
{
	// Faire des raycasts depuis tous les spline points
	const int32 _pointsCount = spline->GetNumberOfSplinePoints();
	for (int _splinePointIndex = 0; _splinePointIndex <= _pointsCount; _splinePointIndex++)
	{
		FHitResult _hitResult = FHitResult();
		const FVector& _splinePointLocation = spline->GetLocationAtSplinePoint(_splinePointIndex, ESplineCoordinateSpace::World);
		const FVector& _startLocation = _splinePointLocation + FVector(0.0f, 0.0f, zGroundCheckOffset);
		const FVector& _endLocation = _startLocation + FVector::DownVector * checkGroundDepth;
		const bool _hasHit = UKismetSystemLibrary::LineTraceSingleForObjects(GetWorld(), _startLocation, _endLocation, groundLayer, false, TArray<AActor*>(), EDrawDebugTrace::None, _hitResult, true);
	
		// Si le raycast a touché le sol et si il n'y a pas déjà un spline point à cet endroit
		if (_hasHit && !IsAlreadySplinePointAtLocation(_hitResult.ImpactPoint))
		{
			// On déplace le spline point à la position de l'impact du raycast
			spline->SetLocationAtSplinePoint(_splinePointIndex, _hitResult.ImpactPoint, ESplineCoordinateSpace::World);
		}
	}
}
void ADynamicSplineMeshActor::CreateSplinePointsOnGround()
{
	FHitResult _hitResult = FHitResult();

	// Faire des raycasts depuis les emplacements tout au long de la spline
	for (int _checkGroundPointIndex = 1; _checkGroundPointIndex < GetPlacementCount(); _checkGroundPointIndex++)
	{
		float _distance = checkGroundMethod == POINTS ? FMath::CeilToFloat(spline->GetSplineLength() / checkGroundPointsCount) : checkGroundSpacing;
		_distance *= _checkGroundPointIndex;
		
		const FVector& _startLocation = spline->GetLocationAtDistanceAlongSpline(_distance, ESplineCoordinateSpace::World) + FVector(0.0f, 0.0f, zGroundCheckOffset);
		const FVector& _endLocation = _startLocation + FVector::DownVector * checkGroundDepth;
		const bool _hasHit = UKismetSystemLibrary::LineTraceSingleForObjects(GetWorld(), _startLocation, _endLocation, groundLayer, false, TArray<AActor*>(), EDrawDebugTrace::None, _hitResult, true);

		 // Si le raycast a touché le sol
		 if (_hasHit)
		 {
		 	// Si le Z de l'impact est différent de celui du spline point précédent et si il n'y a pas déjà un spline point à cet endroit
		 	const float _key = spline->FindInputKeyClosestToWorldLocation(_hitResult.ImpactPoint);
		 	const FVector& _previousSplinePointLocation = spline->GetLocationAtSplineInputKey(FMath::FloorToFloat(_key), ESplineCoordinateSpace::World);
		 	if (_hitResult.ImpactPoint.Z != _previousSplinePointLocation.Z && !IsAlreadySplinePointAtLocation(_hitResult.ImpactPoint))
		 	{
		 		// Créer un nouveau spline point à la position de l'impact du raycast
		 		AddSplinePoint(_hitResult.ImpactPoint, ESplinePointType::Linear);
		 		
		 		// Si la position en Z est inférieure à celle du spline point précédent
		 		const FVector& _newSplinePointInAirLocation = FVector(_hitResult.ImpactPoint.X, _hitResult.ImpactPoint.Y, _previousSplinePointLocation.Z);
		 		if (_hitResult.ImpactPoint.Z < _previousSplinePointLocation.Z && !IsAlreadySplinePointAtLocation(_newSplinePointInAirLocation))
		 		{
		 			// Création d'un nouveau spline point au dessus de celui qui à été créé
		 			AddSplinePoint(_newSplinePointInAirLocation, ESplinePointType::Linear);
		 		}

		 		// Si la position en Z est supérieure à celle du spline point précédent
		 		const FVector& _newSplinePointOnGroundLocation = raycasts.Num() ? raycasts.Last() : _previousSplinePointLocation;
		 		if (_hitResult.ImpactPoint.Z > _previousSplinePointLocation.Z && !IsAlreadySplinePointAtLocation(_newSplinePointOnGroundLocation))
		 		{
		 			// Création d'un nouveau spline point au sol à côté du précédent
		 			AddSplinePoint(_newSplinePointOnGroundLocation, ESplinePointType::Linear);

		 			// Création d'un nouveau spline point au dessus de celui qui à été créé
		 			const FVector& _additionnalSplinePointLocation = FVector(_newSplinePointOnGroundLocation.X, _newSplinePointOnGroundLocation.Y, _hitResult.ImpactPoint.Z);
		 			AddSplinePoint(_additionnalSplinePointLocation, ESplinePointType::Linear);
		 		}
		 	}

		    else
		    {
		    	// Add to the unused raycast register
		    	raycasts.Add(_hitResult.ImpactPoint);
		    }
		}
	}
}
void ADynamicSplineMeshActor::CreateSplinePointsBetween()
{
	const int32 _pointsCount = spline->GetNumberOfSplinePoints();
	for (int _splinePointIndex = 1; _splinePointIndex <= _pointsCount; _splinePointIndex++)
	{
		const FVector& _currentSplinePointLocation = spline->GetLocationAtSplinePoint(_splinePointIndex, ESplineCoordinateSpace::World);
		const FVector& _previousSplinePointLocation = spline->GetLocationAtSplinePoint(_splinePointIndex - 1, ESplineCoordinateSpace::World);

		// Si il y a deux spline point à la même hauteur
		if (_currentSplinePointLocation.Z == _previousSplinePointLocation.Z)
		{
			//TODO check to remove
			// On enlève le précédent
			// DrawDebugSphere(GetWorld(), _currentSplinePointLocation, 20.0f, 4, FColor::Green, false, 10.0f, 0, 10.0f);
			// spline->RemoveSplinePoint(_splinePointIndex - 1, false);
		}

		// Si le spline point actuel à un Z plus haut que celui du précédent
		if (_currentSplinePointLocation.Z > _previousSplinePointLocation.Z)
		{
			const FVector& _newSplinePointOnGroundLocation = raycasts.Num() ? raycasts.Last() : _currentSplinePointLocation;
			if (!IsAlreadySplinePointAtLocation(_newSplinePointOnGroundLocation))
			{
				// Création d'un nouveau spline point au sol à côté du précédent
				AddSplinePoint(_newSplinePointOnGroundLocation, ESplinePointType::Linear);
			}
		
			const FVector& _newSplinePointInAirLocation = FVector(_newSplinePointOnGroundLocation.X, _newSplinePointOnGroundLocation.Y, _currentSplinePointLocation.Z);
			if (!IsAlreadySplinePointAtLocation(_newSplinePointInAirLocation))
			{
				// Création d'un nouveau spline point au dessus du précédent
				AddSplinePoint(_newSplinePointInAirLocation, ESplinePointType::Linear);
			}
		}
	}
}
void ADynamicSplineMeshActor::AddSplinePoint(const FVector& _location, ESplinePointType::Type _type)
{
	const float _key = FMath::CeilToInt32(spline->FindInputKeyClosestToWorldLocation(_location));
	spline->AddSplinePointAtIndex(_location, _key, ESplineCoordinateSpace::World);
	spline->SetSplinePointType(_key, _type);
	splinePoints.Add(_key);
}

/* SnapOnGround en dur
  
void ABTP_PlacementManager::SnapSplinePointsOnGround() const
{
	// Faire des raycasts depuis tous les spline points
	const int32 _pointsCount = spline->GetNumberOfSplinePoints();
	for (int _splinePointIndex = 0; _splinePointIndex <= _pointsCount; _splinePointIndex++)
	{
		FHitResult _hitResult = FHitResult();
		const FVector& _splinePointLocation = spline->GetLocationAtSplinePoint(_splinePointIndex, ESplineCoordinateSpace::World);
		const FVector& _startLocation = _splinePointLocation + FVector(0.0f, 0.0f, zGroundCheckOffset);
		const FVector& _endLocation = _startLocation + FVector::DownVector * checkGroundDepth;
		const bool _hasHit = UKismetSystemLibrary::LineTraceSingleForObjects(GetWorld(), _startLocation, _endLocation, groundLayer, false, TArray<AActor*>(), EDrawDebugTrace::None, _hitResult, true);
	
		// Si le raycast a touché le sol et si il n'y a pas déjà un spline point à cet endroit
		if (_hasHit && !IsAlreadySplinePointAtLocation(_hitResult.ImpactPoint))
		{
			// On déplace le spline point à la position de l'impact du raycast
			spline->SetLocationAtSplinePoint(_splinePointIndex, _hitResult.ImpactPoint, ESplineCoordinateSpace::World, true);
		}
	}
}
void ABTP_PlacementManager::CreateSplinePointsOnGround()
{
	int32 _pointsCount = spline->GetNumberOfSplinePoints();
	FHitResult _hitResult = FHitResult();

	// Faire des raycasts depuis les emplacements tout au long de la spline
	for (int _checkGroundPointIndex = 1; _checkGroundPointIndex < GetPlacementCount(); _checkGroundPointIndex++)
	{
		float _distance = checkGroundMethod == REGULAR ? FMath::CeilToFloat(spline->GetSplineLength() / checkGroundPointsCount) : checkGroundSpacing;
		_distance *= _checkGroundPointIndex;
		
		const FVector& _startLocation = spline->GetLocationAtDistanceAlongSpline(_distance, ESplineCoordinateSpace::World) + FVector(0.0f, 0.0f, zGroundCheckOffset);
		const FVector& _endLocation = _startLocation + FVector::DownVector * checkGroundDepth;
		const bool _hasHit = UKismetSystemLibrary::LineTraceSingleForObjects(GetWorld(), _startLocation, _endLocation, groundLayer, false, TArray<AActor*>(), EDrawDebugTrace::None, _hitResult, true);
	
		//DrawDebugSphere(GetWorld(), _startLocation, 30.0f, 4, FColor::Blue, false, 10.0f, 0, 2.0f);
		//DrawDebugLine(GetWorld(), _startLocation, _endLocation, FColor::Green, false, 10.0f, 0, 6.0f);

		 // Si le raycast a touché le sol
		 if (_hasHit && _pointsCount > 0)
		 {
		 	// Si le Z de l'impact est différent de celui du spline point précédent et si il n'y a pas déjà un spline point à cet endroit
		 	const FVector& _previousSplinePointLocation = spline->GetLocationAtSplinePoint(_pointsCount - 2, ESplineCoordinateSpace::World);
		 	if (_hitResult.ImpactPoint.Z != _previousSplinePointLocation.Z && !IsAlreadySplinePointAtLocation(_hitResult.ImpactPoint))
		 	{
		 		// Créer un nouveau spline point à la position de l'impact du raycast
		 		// DrawDebugSphere(GetWorld(), _hitResult.ImpactPoint, 20.0f, 4, FColor::Red, false, 10.0f, 0, 10.0f);
		 		AddSplinePoint(_pointsCount - 1, _hitResult.ImpactPoint);
		 		_pointsCount++;
		 		
		 		// Si la position en Z est inférieure à celle du spline point précédent
		 		const FVector& _newSplinePointInAirLocation = FVector(_hitResult.ImpactPoint.X, _hitResult.ImpactPoint.Y, _previousSplinePointLocation.Z);
		 		if (_hitResult.ImpactPoint.Z < _previousSplinePointLocation.Z && !IsAlreadySplinePointAtLocation(_newSplinePointInAirLocation))
		 		{
		 			// Création d'un nouveau spline point au dessus de celui qui à été créer
		 			AddSplinePoint(_pointsCount - 2, _newSplinePointInAirLocation, ESplinePointType::Linear);
		 			_pointsCount++;
		 		}

		 		// Si la position en Z est supérieure à celle du spline point précédent
		 		const FVector& _newSplinePointOnGroundLocation = raycasts.Num() ? raycasts.Last() : _previousSplinePointLocation;
		 		if (_hitResult.ImpactPoint.Z > _previousSplinePointLocation.Z && !IsAlreadySplinePointAtLocation(_newSplinePointOnGroundLocation))
		 		{
		 			// Création d'un nouveau spline point au sol à côté du précédent
		 			AddSplinePoint(_pointsCount - 2, _newSplinePointOnGroundLocation, ESplinePointType::Linear);
		 			_pointsCount++;
		 		
		 			const FVector& _additionnalSplinePointLocation = FVector(_newSplinePointOnGroundLocation.X, _newSplinePointOnGroundLocation.Y, _hitResult.ImpactPoint.Z);
		 			AddSplinePoint(_pointsCount - 2, _additionnalSplinePointLocation, ESplinePointType::Linear);
		 			_pointsCount++;
		 		}
		 	}

		    else
		    {
		    	// Add to the unused raycast register
		    	raycasts.Add(_hitResult.ImpactPoint);
		    }
		}
	}
}
void ABTP_PlacementManager::CreateSplinePointsBetween()
{
	int32 _pointsCount = spline->GetNumberOfSplinePoints();
	for (int _splinePointIndex = 1; _splinePointIndex <= _pointsCount; _splinePointIndex++)
	{
		const FVector& _currentSplinePointLocation = spline->GetLocationAtSplinePoint(_splinePointIndex, ESplineCoordinateSpace::World);
		const FVector& _previousSplinePointLocation = spline->GetLocationAtSplinePoint(_splinePointIndex - 1, ESplineCoordinateSpace::World);

		// Si il y a deux spline point à la même hauteur
		if (_currentSplinePointLocation.Z == _previousSplinePointLocation.Z)
		{
			// On enlève le précédent
			DrawDebugSphere(GetWorld(), _currentSplinePointLocation, 20.0f, 4, FColor::Red, false, 10.0f, 0, 10.0f);
			// spline->RemoveSplinePoint(_splinePointIndex - 1, false);
		}

		// Si le spline point actuel à un Z plus au que celui du précédent
		if (_currentSplinePointLocation.Z > _previousSplinePointLocation.Z)
		{
			const FVector& _newSplinePointOnGroundLocation = raycasts.Num() ? raycasts.Last() : _currentSplinePointLocation;
			if (!IsAlreadySplinePointAtLocation(_newSplinePointOnGroundLocation))
			{
				// Création d'un nouveau spline point au sol à côté du précédent
				AddSplinePoint(_pointsCount - 1, _newSplinePointOnGroundLocation, ESplinePointType::Linear);
				_pointsCount++;
			}
		
			const FVector& _newSplinePointInAirLocation = FVector(_newSplinePointOnGroundLocation.X, _newSplinePointOnGroundLocation.Y, _currentSplinePointLocation.Z);
			if (!IsAlreadySplinePointAtLocation(_newSplinePointInAirLocation))
			{
				// Création d'un nouveau spline point au dessus du précédent
				AddSplinePoint(_pointsCount - 1, _newSplinePointInAirLocation, ESplinePointType::Linear);
				_pointsCount++;
			}
		}
	}
}
void ABTP_PlacementManager::AddSplinePoint(const int _index, const FVector& _location, ESplinePointType::Type _type)
{
	spline->AddSplinePointAtIndex(_location, _index, ESplineCoordinateSpace::World);
	spline->SetSplinePointType(_index, _type);
	splinePoints.Add(_index);
} */

#pragma endregion

#pragma region Others

bool ADynamicSplineMeshActor::IsAlreadySplinePointAtLocation(const FVector _location) const
{
	const int32 _pointsCount = spline->GetNumberOfSplinePoints();
	for (int _splinePointIndex = 0; _splinePointIndex <= _pointsCount; _splinePointIndex++)
	{
		if (spline->GetLocationAtSplinePoint(_splinePointIndex, ESplineCoordinateSpace::World).Equals(_location))
		{
			return true;
		}
	}
	
	return false;
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

/* ==== VERTEX ====
	
// // Récupérer la position de tous les vertex du mesh
// if (_splineMesh->GetStaticMesh()->GetRenderData()->LODResources.Num() > 0)
// {
// 	FPositionVertexBuffer* _vertexBuffer = &_splineMesh->GetStaticMesh()->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
// 	const uint32 _verticesCount = _vertexBuffer->GetNumVertices();
//
// 	for (uint32 _verticeIndex = 0; _verticeIndex < _verticesCount; _verticeIndex++)
// 	{
// 		UE::Math::TVector<float> _transformVector = _vertexBuffer->VertexPosition(_verticeIndex);
// 		UE::Math::TVector<float> _transformVector = _vertexBuffer->BindPositionVertexBuffer(_verticeIndex);
// 		UE::Math::TVector<double> _worldSpaceVertexLocation = GetActorLocation() + GetTransform().TransformVector(UE::Math::TVector<double>(_transformVector));
// 	}
// }
//
// // Poser les vertex qui collide sur le point le plus proche hors de la collision
// UWorld* const World = GetWorld();
// FVector AdjustedLocation = GetActorLocation();
// FRotator AdjustedRotation = GetActorRotation();
// if (World->FindTeleportSpot(this, AdjustedLocation, AdjustedRotation))
// {
// 	SetActorLocationAndRotation(AdjustedLocation, AdjustedRotation, false, nullptr, ETeleportType::TeleportPhysics);
// } */