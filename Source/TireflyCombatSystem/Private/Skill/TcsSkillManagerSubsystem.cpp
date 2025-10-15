// Copyright Tirefly. All Rights Reserved.


#include "Skill/TcsSkillManagerSubsystem.h"
#include "Skill/TcsSkillComponent.h"
#include "Skill/Modifiers/TcsSkillModifierDefinition.h"
#include "State/TcsState.h"
#include "State/TcsStateManagerSubsystem.h"

FTcsStateDefinition UTcsSkillManagerSubsystem::GetSkillDefinition(FName SkillDefId)
{
	// 通过StateManagerSubsystem获取状态定义
	if (UTcsStateManagerSubsystem* StateManagerSubsystem = GetWorld()->GetSubsystem<UTcsStateManagerSubsystem>())
	{
		return StateManagerSubsystem->GetStateDefinition(SkillDefId);
	}
	
	return FTcsStateDefinition();
}

UTcsStateInstance* UTcsSkillManagerSubsystem::CreateSkillStateInstance(AActor* Owner, FName SkillDefId, AActor* Instigator)
{
	// 通过StateManagerSubsystem创建状态实例
	if (UTcsStateManagerSubsystem* StateManagerSubsystem = GetWorld()->GetSubsystem<UTcsStateManagerSubsystem>())
	{
		return StateManagerSubsystem->CreateStateInstance(Owner, SkillDefId, Instigator);
	}
	
	return nullptr;
}

bool UTcsSkillManagerSubsystem::ApplySkillModifier(AActor* TargetActor, const TArray<FTcsSkillModifierDefinition>& Modifiers, TArray<int32>& OutInstanceIds)
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	if (UTcsSkillComponent* SkillComponent = TargetActor->FindComponentByClass<UTcsSkillComponent>())
	{
		return SkillComponent->ApplySkillModifiers(Modifiers, OutInstanceIds);
	}

	return false;
}

bool UTcsSkillManagerSubsystem::RemoveSkillModifierById(AActor* TargetActor, int32 InstanceId)
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	if (UTcsSkillComponent* SkillComponent = TargetActor->FindComponentByClass<UTcsSkillComponent>())
	{
		return SkillComponent->RemoveSkillModifierById(InstanceId);
	}

	return false;
}

void UTcsSkillManagerSubsystem::UpdateCombatEntity(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		return;
	}

	if (UTcsSkillComponent* SkillComponent = TargetActor->FindComponentByClass<UTcsSkillComponent>())
	{
		SkillComponent->UpdateSkillModifiers();
	}
}
