/*========================================================================
	Copyright (c) Ars Electronica Futurelab, 2020-2023
========================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "PharusActorInterface.h"
#include "GameFramework/Actor.h"
#include "PharusActor.generated.h"

UCLASS()
class PHARUSLASERTRACKING_API APharusActor : public AActor, public IPharusActorInterface
{
	GENERATED_BODY()
	
public:	
	APharusActor();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;
};
