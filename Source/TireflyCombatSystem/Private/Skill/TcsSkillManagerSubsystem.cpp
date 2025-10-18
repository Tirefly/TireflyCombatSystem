// Copyright Tirefly. All Rights Reserved.


#include "Skill/TcsSkillManagerSubsystem.h"
#include "Skill/TcsSkillComponent.h"
#include "Skill/Modifiers/TcsSkillModifierDefinition.h"
#include "Skill/Modifiers/TcsSkillModifierCondition.h"
#include "Skill/Modifiers/TcsSkillModifierExecution.h"
#include "TcsLogChannels.h"
#include "State/TcsState.h"
#include "State/TcsStateManagerSubsystem.h"
#include "TcsDeveloperSettings.h"
#include "Engine/DataTable.h"

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

bool UTcsSkillManagerSubsystem::LoadModifierDefinition(FName ModifierId, FTcsSkillModifierDefinition& OutDef)
{
	const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
	if (!Settings || !Settings->SkillModifierDefTable.IsValid())
	{
		UE_LOG(LogTcsSkill, Error, TEXT("[%s] SkillModifierDefTable is not configured"), *FString(__FUNCTION__));
		return false;
	}

	UDataTable* Table = Settings->SkillModifierDefTable.LoadSynchronous();
	if (!Table)
	{
		UE_LOG(LogTcsSkill, Error, TEXT("[%s] Failed to load SkillModifierDefTable"), *FString(__FUNCTION__));
		return false;
	}

	const FString ContextString = FString::Printf(TEXT("LoadModifierDefinition(%s)"), *ModifierId.ToString());
	const FTcsSkillModifierDefinition* Row = Table->FindRow<FTcsSkillModifierDefinition>(ModifierId, ContextString);

	if (!Row)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("[%s] Modifier not found: %s"), *FString(__FUNCTION__), *ModifierId.ToString());
		return false;
	}

	// 校验定义有效性
	if (!ValidateModifierDefinition(*Row, ModifierId))
	{
		UE_LOG(LogTcsSkill, Error, TEXT("[%s] Validation failed: %s"), *FString(__FUNCTION__), *ModifierId.ToString());
		return false;
	}

	OutDef = *Row;
	return true;
}

bool UTcsSkillManagerSubsystem::ValidateModifierDefinition(const FTcsSkillModifierDefinition& Definition, FName ModifierId) const
{
	bool bValid = true;

	// 校验执行器类型
	if (!Definition.ExecutionType)
	{
		UE_LOG(LogTcsSkill, Error, TEXT("[%s] Modifier '%s' has no ExecutionType"), *FString(__FUNCTION__), *ModifierId.ToString());
		bValid = false;
	}

	// 校验激活条件
	for (TSubclassOf<UTcsSkillModifierCondition> ConditionClass : Definition.ActiveConditions)
	{
		if (!ConditionClass)
		{
			UE_LOG(LogTcsSkill, Error, TEXT("[%s] Modifier '%s' has null Condition entry"), *FString(__FUNCTION__), *ModifierId.ToString());
			bValid = false;
		}
	}

	return bValid;
}

bool UTcsSkillManagerSubsystem::ApplySkillModifierByIds(AActor* TargetActor, const TArray<FName>& ModifierIds, TArray<int32>& OutInstanceIds)
{
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("[%s] Target actor is invalid"), *FString(__FUNCTION__));
		return false;
	}

	// 加载所有修改器定义
	TArray<FTcsSkillModifierDefinition> Modifiers;
	for (const FName& ModId : ModifierIds)
	{
		FTcsSkillModifierDefinition ModDef;
		if (!LoadModifierDefinition(ModId, ModDef))
		{
			UE_LOG(LogTcsSkill, Warning, TEXT("[%s] Failed to load modifier: %s"), *FString(__FUNCTION__), *ModId.ToString());
			continue;
		}
		Modifiers.Add(ModDef);
	}

	if (Modifiers.IsEmpty())
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("[%s] No valid modifiers loaded from IDs"), *FString(__FUNCTION__));
		return false;
	}

	// 应用加载的修改器定义
	return ApplySkillModifier(TargetActor, Modifiers, OutInstanceIds);
}
