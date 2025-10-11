// Copyright Tirefly. All Rights Reserved.


#include "Skill/TcsSkillManagerSubsystem.h"
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
