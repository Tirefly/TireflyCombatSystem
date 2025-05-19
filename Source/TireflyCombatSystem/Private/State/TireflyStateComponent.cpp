// Copyright Tirefly. All Rights Reserved.


#include "TireflyCombatSystem/Public/State/TireflyStateComponent.h"


UTireflyStateComponent::UTireflyStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTireflyStateComponent::BeginPlay()
{
	Super::BeginPlay();
}
