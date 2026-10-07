// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionSubsystem.h"

#include "Chain/TcsEffectChain.h"
#include "Def/TcsBuffDef.h"
#include "Def/TcsSkillDefData.h"
#include "Trigger/TcsEffectTrigger.h"
#include "UObject/GCObject.h"



void UTcsDefinitionSubsystem::AddReferencedObjects(
	UObject* InThis, FReferenceCollector& Collector)
{
	UTcsDefinitionSubsystem* This = CastChecked<UTcsDefinitionSubsystem>(InThis);

	// 四份内容缓存都非 UPROPERTY（TUniquePtr 容器），GC 的 RefLink 看不见它们。
	// 逐条收集反射属性并调用 struct ARO，保活 FInstancedStruct 内层的宿主对象引用。
	for (const TPair<FGameplayTag, TUniquePtr<FTcsEffectChain>>& Pair : This->ChainDefs)
	{
		if (const FTcsEffectChain* Chain = Pair.Value.Get())
		{
			Collector.AddPropertyReferencesWithStructARO(FTcsEffectChain::StaticStruct(), const_cast<FTcsEffectChain*>(Chain), This);
		}
	}

	for (const TPair<FGameplayTag, TUniquePtr<FTcsEffectTriggerDef>>& Pair : This->TriggerDefs)
	{
		if (const FTcsEffectTriggerDef* TriggerDef = Pair.Value.Get())
		{
			Collector.AddPropertyReferencesWithStructARO(FTcsEffectTriggerDef::StaticStruct(), const_cast<FTcsEffectTriggerDef*>(TriggerDef), This);
		}
	}

	// 状态定义：参数行的数值来源与描述的视图载荷都允许持宿主对象引用。
	for (const TPair<FGameplayTag, TUniquePtr<FTcsBuffDef>>& Pair : This->StateDefs)
	{
		if (const FTcsBuffDef* BuffDef = Pair.Value.Get())
		{
			Collector.AddPropertyReferencesWithStructARO(FTcsBuffDef::StaticStruct(), const_cast<FTcsBuffDef*>(BuffDef), This);
		}
	}

	// 技能定义：除继承内容外，还须覆盖施法时段、查询片段与内联触发行的嵌套 FInstancedStruct。
	for (const TPair<FGameplayTag, TUniquePtr<FTcsSkillDefData>>& Pair : This->SkillDefs)
	{
		if (const FTcsSkillDefData* SkillDef = Pair.Value.Get())
		{
			Collector.AddPropertyReferencesWithStructARO(FTcsSkillDefData::StaticStruct(), const_cast<FTcsSkillDefData*>(SkillDef), This);
		}
	}

	Super::AddReferencedObjects(InThis, Collector);
}
