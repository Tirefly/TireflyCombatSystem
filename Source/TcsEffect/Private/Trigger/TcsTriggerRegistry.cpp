// Copyright Tirefly. All Rights Reserved.

#include "Trigger/TcsTriggerRegistry.h"

#include "EventBus/TcsEventBusSubsystem.h"
#include "TcsEffectLogChannel.h"
#include "Trigger/TcsTriggerEvaluator.h"



// 装配
void FTcsTriggerRegistry::SetEvaluator(UTcsTriggerEvaluator* InEvaluator)
{
	Evaluator = InEvaluator;
}



// 登记
FTcsEffectTriggerHandle FTcsTriggerRegistry::RegisterRow(const FTcsEffectTriggerInstance& Instance, UTcsEventBusSubsystem* Bus)
{
	if (!Instance.Def.EventTag.IsValid() || !Instance.Def.EffectChainId.IsValid())
	{
		ensureMsgf(false, TEXT("FTcsTriggerRegistry::RegisterRow: EventTag 或 EffectChainId 无效——拒绝登记"));
		return FTcsEffectTriggerHandle();
	}

	if (!Bus)
	{
		// 世界拆解期（总线已回收）——时序而非配置错误，故不 ensure
		UE_LOG(LogTcsEffect, Warning, TEXT("FTcsTriggerRegistry::RegisterRow: 事件总线不可得（世界拆解期）——拒绝登记"));
		return FTcsEffectTriggerHandle();
	}

	const TTcsInstanceHandle<FTcsEffectTriggerTag> Inner = AllocateSlot();
	FTcsEffectTriggerInstance* Row = Rows.IsValidIndex(static_cast<int32>(Inner.Index)) ? &Rows[Inner.Index] : nullptr;
	check(Row != nullptr);

	*Row = Instance;
	Row->Self.Inner = Inner;

	// 订阅装配：同 Tag 已有订阅则复用（计数配对的"首次出现才订"）
	EnsureSubscription(Row->Def.EventTag, Bus);

	UE_LOG(LogTcsEffect, Log, TEXT("触发登记：行=%u/%u 事件=%s 链=%s 优先级=%d"),
		Inner.Index, Inner.Generation, *Row->Def.EventTag.ToString(),
		*Row->Def.EffectChainId.ToString(), Row->Def.Priority);

	FTcsEffectTriggerHandle Result;
	Result.Inner = Inner;
	return Result;
}

bool FTcsTriggerRegistry::UnregisterRow(FTcsEffectTriggerHandle Handle, UTcsEventBusSubsystem* Bus)
{
	// 代际校验：陈旧句柄是正常竞态（不 ensure），但 MUST 拒绝——否则会摘掉复用该槽位的新行
	if (!Handle.IsValid() || !Rows.IsValidIndex(static_cast<int32>(Handle.Inner.Index)))
	{
		return false;
	}

	if (RowGenerations[Handle.Inner.Index] != Handle.Inner.Generation)
	{
		UE_LOG(LogTcsEffect, Verbose, TEXT("触发摘除：句柄 %u/%u 代际失配（当前 %u）——忽略"),
			Handle.Inner.Index, Handle.Inner.Generation, RowGenerations[Handle.Inner.Index]);
		return false;
	}

	const FGameplayTag EventTag = Rows[Handle.Inner.Index].Def.EventTag;

	// 清槽内容后归还（槽位内容不跨生命周期残留——与池的零策略纪律同款：调用方在归还前自行清理）
	Rows[Handle.Inner.Index] = FTcsEffectTriggerInstance();
	FreeSlot(Handle.Inner);

	UE_LOG(LogTcsEffect, Log, TEXT("触发摘除：行=%u/%u 事件=%s"),
		Handle.Inner.Index, Handle.Inner.Generation, *EventTag.ToString());

	// 订阅计数归零才退订
	DropSubscriptionIfUnused(EventTag, Bus);

	return true;
}

int32 FTcsTriggerRegistry::UnregisterRowsBySource(const FTcsSourceHandle& Source, UTcsEventBusSubsystem* Bus)
{
	if (!Source.IsValid())
	{
		return 0;
	}

	// 先收集目标句柄再逐个摘除：摘除会改登记表（空闲槽入栈），故不在遍历中改
	TArray<FTcsEffectTriggerHandle> Targets;
	for (uint32 SlotIndex = 0; SlotIndex < static_cast<uint32>(Rows.Num()); ++SlotIndex)
	{
		if ((RowGenerations[SlotIndex] & 1u) == 1u && Rows[SlotIndex].Source == Source)
		{
			FTcsEffectTriggerHandle Handle;
			Handle.Inner.Index = SlotIndex;
			Handle.Inner.Generation = RowGenerations[SlotIndex];
			Targets.Add(Handle);
		}
	}

	int32 RemovedCount = 0;
	for (const FTcsEffectTriggerHandle& Target : Targets)
	{
		if (UnregisterRow(Target, Bus))
		{
			++RemovedCount;
		}
	}

	if (RemovedCount > 0)
	{
		UE_LOG(LogTcsEffect, Log, TEXT("触发级联摘除：来源=%llu 行数=%d"), Source.Id, RemovedCount);
	}

	return RemovedCount;
}

void FTcsTriggerRegistry::Reset(UTcsEventBusSubsystem* Bus)
{
	if (Bus)
	{
		// 全量退订：不留跨世界残留订阅（漏一条 = 总线留死订阅 + 求值器被弱引用持着）
		for (const TPair<FGameplayTag, FTcsEventSubscriptionHandle>& Pair : TagSubscriptions)
		{
			Bus->Unsubscribe(Pair.Value);
		}
	}

	TagSubscriptions.Reset();
	Rows.Reset();
	RowGenerations.Reset();
	FreeSlots.Reset();
	LitGateTags.Reset();
	RandomStream.Reset();
}



// 内核
TTcsInstanceHandle<FTcsEffectTriggerTag> FTcsTriggerRegistry::AllocateSlot()
{
	uint32 SlotIndex;
	if (FreeSlots.Num() > 0)
	{
		SlotIndex = FreeSlots.Pop(EAllowShrinking::No);
		RowGenerations[SlotIndex] += 1;
	}
	else
	{
		SlotIndex = static_cast<uint32>(Rows.Num());
		Rows.Emplace();
		RowGenerations.Add(1);
	}

	TTcsInstanceHandle<FTcsEffectTriggerTag> Handle;
	Handle.Index = SlotIndex;
	Handle.Generation = RowGenerations[SlotIndex];
	return Handle;
}

void FTcsTriggerRegistry::FreeSlot(TTcsInstanceHandle<FTcsEffectTriggerTag> Inner)
{
	// 代际 +1 使旧句柄悬空（与 TTcsInstancePool::Free 同款语义）
	RowGenerations[Inner.Index] += 1;
	FreeSlots.Push(Inner.Index);
}

void FTcsTriggerRegistry::EnsureSubscription(FGameplayTag EventTag, UTcsEventBusSubsystem* Bus)
{
	// 已有订阅则复用——**同一 Tag 多行共用一个订阅**（计数配对）
	if (TagSubscriptions.Contains(EventTag))
	{
		return;
	}

	if (!Evaluator.IsValid())
	{
		ensureMsgf(false, TEXT("FTcsTriggerRegistry::EnsureSubscription: 求值器未装配或已回收——无法订阅"));
		return;
	}

	// 立即通道：收集协议要求"修正提交落在事件发布返回之前"（帧末通道会让修正晚一拍）
	const FTcsEventSubscriptionHandle Subscription = Bus->Subscribe(
		EventTag, Evaluator.Get(), ETcsEventDispatch::EED_Immediate);

	TagSubscriptions.Add(EventTag, Subscription);
}

void FTcsTriggerRegistry::DropSubscriptionIfUnused(FGameplayTag EventTag, UTcsEventBusSubsystem* Bus)
{
	if (HasRowForTag(EventTag))
	{
		return;
	}

	FTcsEventSubscriptionHandle Subscription;
	if (TagSubscriptions.RemoveAndCopyValue(EventTag, Subscription))
	{
		if (Bus)
		{
			Bus->Unsubscribe(Subscription);
		}

		UE_LOG(LogTcsEffect, Verbose, TEXT("触发退订：事件=%s（该 Tag 行数归零）"), *EventTag.ToString());
	}
}
