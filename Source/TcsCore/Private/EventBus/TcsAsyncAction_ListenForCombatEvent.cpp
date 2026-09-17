// Copyright Tirefly. All Rights Reserved.

#include "EventBus/TcsAsyncAction_ListenForCombatEvent.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TcsCoreLogChannel.h"

UTcsAsyncAction_ListenForCombatEvent* UTcsAsyncAction_ListenForCombatEvent::ListenForCombatEvent(
	UObject* WorldContextObject, FGameplayTag TagFilter, UScriptStruct* PayloadType, ETcsEventMatchType MatchType)
{
	UTcsAsyncAction_ListenForCombatEvent* Action = NewObject<UTcsAsyncAction_ListenForCombatEvent>();
	Action->TagFilter = TagFilter;
	Action->PayloadType = PayloadType;
	Action->MatchType = MatchType;
	Action->World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	return Action;
}

void UTcsAsyncAction_ListenForCombatEvent::Activate()
{
	Super::Activate();

	// 重复激活防护：先解绑既有绑定
	if (UTcsEventBusSubsystem* PreviousSubsystem = BusSubsystem.Get())
	{
		PreviousSubsystem->OnEvent.RemoveDynamic(this, &UTcsAsyncAction_ListenForCombatEvent::HandleBusEvent);
		BusSubsystem.Reset();
	}

	UWorld* WorldPtr = World.Get();
	UTcsEventBusSubsystem* Subsystem = WorldPtr ? WorldPtr->GetSubsystem<UTcsEventBusSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogTcsCore, Warning,
			TEXT("ListenForCombatEvent 激活失败：未取到事件总线子系统（World 无效或非游戏世界；Tag=%s）"),
			*TagFilter.ToString());
		return;
	}

	BusSubsystem = Subsystem;
	Subsystem->OnEvent.AddDynamic(this, &UTcsAsyncAction_ListenForCombatEvent::HandleBusEvent);

	UE_LOG(LogTcsCore, Verbose, TEXT("ListenForCombatEvent 已激活：Tag=%s 匹配=%s 载荷类型=%s"),
		TagFilter.IsValid() ? *TagFilter.ToString() : TEXT("(不限)"),
		MatchType == ETcsEventMatchType::EMT_Exact ? TEXT("精确") : TEXT("部分"),
		PayloadType ? *PayloadType->GetName() : TEXT("(不限)"));
}

void UTcsAsyncAction_ListenForCombatEvent::BeginDestroy()
{
	// 退订门面多播（动态多播对 UObject 绑定持弱引用，显式摘除防条目累积）
	if (UTcsEventBusSubsystem* Subsystem = BusSubsystem.Get())
	{
		Subsystem->OnEvent.RemoveDynamic(this, &UTcsAsyncAction_ListenForCombatEvent::HandleBusEvent);
	}
	BusSubsystem.Reset();

	Super::BeginDestroy();
}

bool UTcsAsyncAction_ListenForCombatEvent::MatchesFilter(
	const FGameplayTag& EventTag, const FInstancedStruct& Payload) const
{
	// Tag 匹配：空过滤 = 不限；精确 = 完全相等；部分 = 层级含（含自身）
	if (TagFilter.IsValid())
	{
		const bool bTagMatched = MatchType == ETcsEventMatchType::EMT_Exact
			? EventTag == TagFilter
			: EventTag.MatchesTag(TagFilter);
		if (!bTagMatched)
		{
			return false;
		}
	}

	// 载荷类型匹配：空类型 = 不限；非空时要求载荷结构为指定类型自身或子类
	if (PayloadType)
	{
		const UScriptStruct* PayloadStruct = Payload.GetScriptStruct();
		if (!PayloadStruct || !PayloadStruct->IsChildOf(PayloadType))
		{
			return false;
		}
	}

	return true;
}

void UTcsAsyncAction_ListenForCombatEvent::HandleBusEvent(FGameplayTag EventTag, FInstancedStruct Payload)
{
	if (!MatchesFilter(EventTag, Payload))
	{
		return;
	}

	OnEvent.Broadcast(EventTag, Payload);
}
