// Copyright Tirefly. All Rights Reserved.

#include "EventBus/TcsEventBusSubsystem.h"

#include "Stats/Stats.h"

UTcsEventBusSubsystem::UTcsEventBusSubsystem()
{
	// 总线全量事件流接入动态多播（A' 反射面：BP/CS 绑定即过滤，过滤在绑定方）
	EventBus.EventObserver = [this](FGameplayTag EventTag, const FInstancedStruct& Payload)
	{
		OnEvent.Broadcast(EventTag, Payload);
	};
}

bool UTcsEventBusSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 仅游戏世界实例化：编辑器预览/检查器世界不创建（无跨 World 静态状态，PIE 安全）
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UTcsEventBusSubsystem::Deinitialize()
{
	// 确定性清理：订阅与队列清空（保留观察钩子，防子系统复用后反射面失联）
	EventBus.Reset();

	Super::Deinitialize();
}

TStatId UTcsEventBusSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTcsEventBusSubsystem, STATGROUP_Tickables);
}

FTcsEventSubscriptionHandle UTcsEventBusSubsystem::Subscribe(
	FGameplayTag Tag, UTcsEventHandler* Handler, ETcsEventDispatch Dispatch)
{
	return EventBus.Subscribe(Tag, Handler, Dispatch);
}

void UTcsEventBusSubsystem::Unsubscribe(FTcsEventSubscriptionHandle Handle)
{
	EventBus.Unsubscribe(Handle);
}

void UTcsEventBusSubsystem::PublishImmediate(FGameplayTag Tag, const FInstancedStruct& Payload)
{
	EventBus.PublishImmediate(Tag, Payload);
}

void UTcsEventBusSubsystem::PublishFrameEnd(FGameplayTag Tag, const FInstancedStruct& Payload)
{
	EventBus.PublishFrameEnd(Tag, Payload);
}

void UTcsEventBusSubsystem::FlushFrameEndQueue()
{
	EventBus.FlushFrameEnd();
}

int32 UTcsEventBusSubsystem::GetPendingFrameEndCount() const
{
	return EventBus.GetPendingFrameEndCount();
}
