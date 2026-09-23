// Copyright Tirefly. All Rights Reserved.

#include "TcsEffectSubsystem.h"

#include "EventBus/TcsEventBusSubsystem.h"
#include "TcsEffectLogChannel.h"
#include "Trigger/TcsTriggerEvaluator.h"



// 触发行登记
FTcsEffectTriggerHandle UTcsEffectSubsystem::RegisterTriggerRow(const FTcsEffectTriggerInstance& Instance)
{
	ensure(IsInGameThread());

	// 求值器懒建：首个登记时创建并 UPROPERTY 持有（总线订阅表持弱引用——不 root 会被 GC 掉）
	if (!TriggerEvaluator)
	{
		TriggerEvaluator = NewObject<UTcsTriggerEvaluator>(this);
		TriggerEvaluator->Initialize(this);
		TriggerRegistry.SetEvaluator(TriggerEvaluator);
	}

	// 登记表自行校验配置（无效时 ensure + 返回无效句柄）
	return TriggerRegistry.RegisterRow(Instance, GetEventBus());
}

bool UTcsEffectSubsystem::UnregisterTriggerRow(FTcsEffectTriggerHandle Handle)
{
	ensure(IsInGameThread());

	return TriggerRegistry.UnregisterRow(Handle, GetEventBus());
}

int32 UTcsEffectSubsystem::UnregisterTriggerRowsBySource(const FTcsSourceHandle& Source)
{
	ensure(IsInGameThread());

	return TriggerRegistry.UnregisterRowsBySource(Source, GetEventBus());
}

void UTcsEffectSubsystem::SetTriggerGateTag(FGameplayTag GateTag, bool bLit)
{
	ensure(IsInGameThread());

	TriggerRegistry.SetGateTagLit(GateTag, bLit);
}

bool UTcsEffectSubsystem::IsTriggerGateTagLit(FGameplayTag GateTag) const
{
	return TriggerRegistry.IsGateTagLit(GateTag);
}

int32 UTcsEffectSubsystem::GetTriggerRowCount() const
{
	return TriggerRegistry.GetRowCount();
}

void UTcsEffectSubsystem::SetTriggerRandomSeed(int32 Seed)
{
	ensure(IsInGameThread());

	TriggerRegistry.SetRandomSeed(Seed);
}



// 内核（求值器内部面）
void UTcsEffectSubsystem::CollectTriggerRowsForTag(FGameplayTag EventTag, TArray<FTcsEffectTriggerHandle>& OutHandles) const
{
	TriggerRegistry.CollectRowsForTag(EventTag, OutHandles);
}

void UTcsEffectSubsystem::SortTriggerRowsByPriority(TArray<FTcsEffectTriggerHandle>& Handles) const
{
	TriggerRegistry.SortRowsByPriority(Handles);
}

const FTcsEffectTriggerInstance* UTcsEffectSubsystem::FindTriggerRow(FTcsEffectTriggerHandle Handle) const
{
	return TriggerRegistry.FindRow(Handle);
}

double UTcsEffectSubsystem::NextTriggerRandomValue()
{
	return TriggerRegistry.NextRandomValue();
}

UTcsEventBusSubsystem* UTcsEffectSubsystem::GetEventBus() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTcsEventBusSubsystem>() : nullptr;
}
