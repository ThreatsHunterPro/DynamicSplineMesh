#pragma once

/* The different methods of composition */
UENUM(BlueprintType)
enum ECompositionMethod
{
	FILL UMETA(DisplayName = "Fill"),
	USUAL UMETA(DisplayName = "Usual"),
	RANDOM UMETA(DisplayName = "Random")
};