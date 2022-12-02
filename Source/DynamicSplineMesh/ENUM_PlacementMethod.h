#pragma once

/* The different methods of placement */
UENUM(BlueprintType)
enum EPlacementMethod
{
	DUPLICATE UMETA(DisplayName = "Duplicate"),
	EXTEND UMETA(DisplayName = "Extend")
};