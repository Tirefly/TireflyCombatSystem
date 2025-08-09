// Copyright Tirefly. All Rights Reserved.


#include "Skill/TireflySkillManagerSubsystem.h"
#include "State/TireflyState.h"
#include "State/TireflyStateManagerSubsystem.h"

FTireflyStateDefinition UTireflySkillManagerSubsystem::GetSkillDefinition(FName SkillDefId)
{
	// 通过StateManagerSubsystem获取状态定义
	if (UTireflyStateManagerSubsystem* StateManagerSubsystem = GetWorld()->GetSubsystem<UTireflyStateManagerSubsystem>())
	{
		return StateManagerSubsystem->GetStateDefinition(SkillDefId);
	}
	
	return FTireflyStateDefinition();
}

UTireflyStateInstance* UTireflySkillManagerSubsystem::CreateSkillStateInstance(AActor* Owner, FName SkillDefId, AActor* Instigator)
{
	// 通过StateManagerSubsystem创建状态实例
	if (UTireflyStateManagerSubsystem* StateManagerSubsystem = GetWorld()->GetSubsystem<UTireflyStateManagerSubsystem>())
	{
		return StateManagerSubsystem->CreateStateInstance(Owner, SkillDefId, Instigator);
	}
	
	return nullptr;
}
