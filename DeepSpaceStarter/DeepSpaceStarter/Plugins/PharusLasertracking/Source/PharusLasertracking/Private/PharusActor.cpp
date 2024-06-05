/*========================================================================
	Copyright (c) Ars Electronica Futurelab, 2020-2023
========================================================================*/



#include "PharusActor.h"

// Sets default values
APharusActor::APharusActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void APharusActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APharusActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

