#pragma once

/* The different methods of rotation */
UENUM(BlueprintType)
enum ERotationMethod
{
	/* No rotation */
	NONE UMETA(DisplayName = "None"),
	
	/* Rotate all meshes */
	REGULAR UMETA(DisplayName = "Regular"),
	
	/* Rotate all meshes differently */
	IRREGULAR UMETA(DisplayName = "Irregular"),
	
	/* Rotate a group of meshes selected by index */
	GROUP UMETA(DisplayName = "Group"),
	
	/* Rotate all meshes selected by index */
	ANGLE UMETA(DisplayName = "Angle")
};