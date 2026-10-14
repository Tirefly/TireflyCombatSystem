// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsEffectAttributeAccess.h"

#include "Attribute/TcsAttributeStore.h"
#include "Engine/World.h"
#include "TcsAttributeSubsystem.h"



// 属性访问解析点（"怎么找到门面"归 TcsAttribute 的单一查找点：将来补注入契约只换那一处）
FTcsEffectAttributeAccess FTcsEffectAttributeAccess::Resolve(const UWorld* World)
{
	FTcsEffectAttributeAccess Access;
	Access.AttributeSubsystem = UTcsAttributeSubsystem::Resolve(World);
	return Access;
}

bool FTcsEffectAttributeAccess::IsLedgerReady(FTcsCombatEntityHandle Entity) const
{
	// `ResolveStore` 是**非确保**查询口——未注册返回 nullptr，正是本判据要的
	return AttributeSubsystem && AttributeSubsystem->ResolveStore(Entity) != nullptr;
}

bool FTcsEffectAttributeAccess::HasAttribute(FTcsCombatEntityHandle Entity, const FGameplayTag& Attribute) const
{
	const FTcsAttributeStore* Store = AttributeSubsystem ? AttributeSubsystem->ResolveStore(Entity) : nullptr;
	return Store && Store->FindInstance(Attribute) != nullptr;
}

bool FTcsEffectAttributeAccess::ApplyModifier(FTcsCombatEntityHandle Entity, const FTcsAttrModInstance& Modifier) const
{
	return AttributeSubsystem ? AttributeSubsystem->ApplyModifier(Entity, Modifier) : false;
}

void FTcsEffectAttributeAccess::BeginBatch(FTcsCombatEntityHandle Entity) const
{
	if (AttributeSubsystem)
	{
		AttributeSubsystem->BeginBatch(Entity);
	}
}

void FTcsEffectAttributeAccess::Commit(FTcsCombatEntityHandle Entity) const
{
	if (AttributeSubsystem)
	{
		AttributeSubsystem->Commit(Entity);
	}
}

double FTcsEffectAttributeAccess::EvaluateCurrent(FTcsCombatEntityHandle Entity, const FGameplayTag& Attribute) const
{
	return AttributeSubsystem ? AttributeSubsystem->EvaluateCurrent(Entity, Attribute) : 0.0;
}
