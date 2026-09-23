// Copyright Tirefly. All Rights Reserved.

#include "Trigger/TcsTriggerRegistry.h"

#include "TcsEffectLogChannel.h"



// GC 补引用
void FTcsTriggerRegistry::AddReferencedObjects(FReferenceCollector& Collector, UObject* ReferencingObject)
{
	// 逐行补引用：本表非 UPROPERTY 成员（GC 的 RefLink 走不到），而行的定义侧含
	// FInstancedStruct（条件/载荷预筛）——其内层可放宿主自定义 struct 的 UPROPERTY 对象引用。
	// 只有**已分配槽位**需要遍历（空闲槽内容已在摘除时清空）。
	for (uint32 SlotIndex = 0; SlotIndex < static_cast<uint32>(Rows.Num()); ++SlotIndex)
	{
		if ((RowGenerations[SlotIndex] & 1u) != 1u)
		{
			continue;
		}

		Collector.AddPropertyReferencesWithStructARO(
			FTcsEffectTriggerDef::StaticStruct(), &Rows[SlotIndex].Def, ReferencingObject);
	}
}



// 查询
int32 FTcsTriggerRegistry::GetRowCount() const
{
	int32 Count = 0;
	for (uint32 SlotIndex = 0; SlotIndex < static_cast<uint32>(RowGenerations.Num()); ++SlotIndex)
	{
		if ((RowGenerations[SlotIndex] & 1u) == 1u)
		{
			++Count;
		}
	}
	return Count;
}

void FTcsTriggerRegistry::CollectRowsForTag(FGameplayTag EventTag, TArray<FTcsEffectTriggerHandle>& OutHandles) const
{
	OutHandles.Reset();

	if (!EventTag.IsValid())
	{
		return;
	}

	// 登记序（槽位下标升序）——排序前的稳定基序
	for (uint32 SlotIndex = 0; SlotIndex < static_cast<uint32>(Rows.Num()); ++SlotIndex)
	{
		if ((RowGenerations[SlotIndex] & 1u) != 1u)
		{
			continue;
		}

		if (Rows[SlotIndex].Def.EventTag != EventTag)
		{
			continue;
		}

		FTcsEffectTriggerHandle Handle;
		Handle.Inner.Index = SlotIndex;
		Handle.Inner.Generation = RowGenerations[SlotIndex];
		OutHandles.Add(Handle);
	}
}

const FTcsEffectTriggerInstance* FTcsTriggerRegistry::FindRow(FTcsEffectTriggerHandle Handle) const
{
	if (!Handle.IsValid() || !Rows.IsValidIndex(static_cast<int32>(Handle.Inner.Index)))
	{
		return nullptr;
	}

	// 代际校验：悬空/已摘除返回 nullptr（正常竞态，不 ensure）
	if (RowGenerations[Handle.Inner.Index] != Handle.Inner.Generation)
	{
		return nullptr;
	}

	return &Rows[Handle.Inner.Index];
}

bool FTcsTriggerRegistry::HasRowForTag(FGameplayTag EventTag) const
{
	for (uint32 SlotIndex = 0; SlotIndex < static_cast<uint32>(Rows.Num()); ++SlotIndex)
	{
		if ((RowGenerations[SlotIndex] & 1u) == 1u && Rows[SlotIndex].Def.EventTag == EventTag)
		{
			return true;
		}
	}
	return false;
}



// 行序
void FTcsTriggerRegistry::SortRowsByPriority(TArray<FTcsEffectTriggerHandle>& Handles) const
{
	// Priority 降序（大者先）；同 Priority 按**槽位下标升序**（登记序——确定性）。
	// 快排不稳定，故显式以 (Priority, Index) 二元组作全序，不依赖输入序。
	Handles.Sort([this](const FTcsEffectTriggerHandle& A, const FTcsEffectTriggerHandle& B)
	{
		const FTcsEffectTriggerInstance* InstanceA = FindRow(A);
		const FTcsEffectTriggerInstance* InstanceB = FindRow(B);
		if (!InstanceA || !InstanceB)
		{
			// 快照后被摘除的行：排序期无定义序，退化为下标序（随后求值期代际校验会跳过它）
			return A.Inner.Index < B.Inner.Index;
		}

		if (InstanceA->Def.Priority != InstanceB->Def.Priority)
		{
			return InstanceA->Def.Priority > InstanceB->Def.Priority;
		}

		return A.Inner.Index < B.Inner.Index;
	});
}



// 点灯
void FTcsTriggerRegistry::SetGateTagLit(FGameplayTag GateTag, bool bLit)
{
	if (!GateTag.IsValid())
	{
		ensureMsgf(false, TEXT("FTcsTriggerRegistry::SetGateTagLit: GateTag 无效——忽略"));
		return;
	}

	if (bLit)
	{
		LitGateTags.Add(GateTag);
	}
	else
	{
		LitGateTags.Remove(GateTag);
	}
}

bool FTcsTriggerRegistry::IsGateTagLit(FGameplayTag GateTag) const
{
	return LitGateTags.Contains(GateTag);
}



// 随机流
void FTcsTriggerRegistry::SetRandomSeed(int32 Seed)
{
	RandomStream.Initialize(Seed);
}

double FTcsTriggerRegistry::NextRandomValue()
{
	return RandomStream.GetFraction();
}
