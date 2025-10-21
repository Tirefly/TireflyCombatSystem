// Copyright Tirefly. All Rights Reserved.


#include "Skill/TcsSkillComponent.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"



UTcsSkillComponent::UTcsSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // 每0.1秒更新一次冷却
}

void UTcsSkillComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UTcsSkillComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UTcsSkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}