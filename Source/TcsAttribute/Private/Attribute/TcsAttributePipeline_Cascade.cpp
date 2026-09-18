// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributePipeline.h"

#include "Attribute/TcsAttrModInstance.h"
#include "TcsAttributeLogChannel.h"
#include "TcsAttributeSubsystem.h"



int32 FTcsAttributePipeline::RemoveBySource(FTcsCombatEntityHandle Unit, const FTcsSourceHandle& Source)
{
	FTcsAttributeStore* Store = Owner.ResolveStore(Unit);
	if (!Store)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("FTcsAttributePipeline::RemoveBySource: 单位未注册（Id=%lld）——忽略"),
			Unit.Id);
		return 0;
	}

	// 单槽位表的按来源摘除（返回摘除条数）
	const auto StripBySource = [&Source](TArray<FTcsAttrModInstance>& Slots) -> int32
	{
		const int32 Before = Slots.Num();
		Slots.RemoveAll([&Source](const FTcsAttrModInstance& Modifier) { return Modifier.Source == Source; });
		return Before - Slots.Num();
	};

	int32 RemovedCount = 0;
	bool bAnyInstanceChanged = false;

	// ① 实例槽位（正常路径：来源级联撤销 D2-2）
	for (TPair<FTcsAttributeName, FTcsAttributeInstance>& Pair : Store->Attributes)
	{
		const int32 Removed = StripBySource(Pair.Value.ModifierSlots);
		if (Removed > 0)
		{
			Pair.Value.bDirty = true;
			bAnyInstanceChanged = true;
			RemovedCount += Removed;
		}
	}

	// ② 冻结暂存区（**必须扫描**：来源可能在属性被冻结期间结束——只在槽位里找会让其修正器永久滞留，
	//    属性被解冻时凭空多出数值，比丢数值更难查）
	for (TPair<FTcsAttributeName, FTcsAttributeInstance>& Pair : Store->FrozenAttributes)
	{
		const int32 Removed = StripBySource(Pair.Value.ModifierSlots);
		if (Removed > 0)
		{
			// 冻结实例同样标脏：解冻后必须重算——否则会把"已被撤销来源的旧缓存值"带回（静默多数值）
			Pair.Value.bDirty = true;
			RemovedCount += Removed;
			UE_LOG(LogTcsAttribute, Log,
				TEXT("FTcsAttributePipeline::RemoveBySource: 命中冻结暂存区（单位 %lld，属性 %s 摘除 %d 条——来源在冻结期结束）"),
				Unit.Id, *Pair.Key.Name.ToString(), Removed);
		}
	}

	// 无匹配 = 正常路径（来源可能只挂过已撤销的修正器），不 ensure
	if (RemovedCount == 0)
	{
		return 0;
	}

	// 批内只标脏（提交期统一重算 + 广播）；批外立即 flush（隐式批）
	if (bAnyInstanceChanged && Store->BatchDepth == 0)
	{
		FlushDirty(Unit, *Store);
	}

	return RemovedCount;
}



bool FTcsAttributePipeline::SetBaseValue(
	FTcsCombatEntityHandle Unit, const FTcsAttributeName& Attribute, double NewBaseValue)
{
	FTcsAttributeStore* Store = Owner.ResolveStore(Unit);
	if (!Store)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("FTcsAttributePipeline::SetBaseValue: 单位未注册（Id=%lld）——忽略"),
			Unit.Id);
		return false;
	}

	FTcsAttributeInstance* Instance = Store->FindInstance(Attribute);
	if (!Instance)
	{
		UE_LOG(LogTcsAttribute, Log, TEXT("FTcsAttributePipeline::SetBaseValue: 属性无实例（单位 %lld，属性 %s）——忽略"),
			Unit.Id, *Attribute.Name.ToString());
		return false;
	}

	Instance->BaseValue = NewBaseValue;
	Instance->bDirty = true;

	// 未开批 = 隐式批：立即重算 + 广播
	if (Store->BatchDepth == 0 && PushEvalStack(Attribute))
	{
		Recalculate(Unit, *Store, *Instance, /*bInBatch=*/false);
		PopEvalStack(Attribute);
	}

	return true;
}
