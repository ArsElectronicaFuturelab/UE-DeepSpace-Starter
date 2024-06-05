/*========================================================================
	Copyright (c) Ars Electronica Futurelab, 2020-2023
========================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PharusActorInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPharusActorInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class PHARUSLASERTRACKING_API IPharusActorInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void setActorTrackID(int trackID);
};
