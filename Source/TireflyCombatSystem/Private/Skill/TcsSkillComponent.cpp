// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillComponent.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"

#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillEntry.h"
#include "TcsLogChannels.h"



UTcsSkillComponent::UTcsSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // 预留 Skill 自有更新入口
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