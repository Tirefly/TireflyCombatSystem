// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsChainEventWaitRegistry.h"

#include "Chain/TcsChainEventWaitHandler.h"
#include "EventBus/TcsEventBusSubsystem.h"
#include "TcsEffectLogChannel.h"



// 装配
void FTcsChainEventWaitRegistry::SetHandler(UTcsChainEventWaitHandler* InHandler)
{
	Handler = InHandler;
}

void FTcsChainEventWaitRegistry::Reset(UTcsEventBusSubsystem* Bus)
{
	// 全量退订：世界反初始化时不走逐条摘除（表要直接丢掉，逐条摘会让 O(N) 退订 + O(N) 查找白跑）
	if (Bus)
	{
		for (const TPair<FGameplayTag, FTcsEventSubscriptionHandle>& Pair : TagSubscriptions)
		{
			Bus->Unsubscribe(Pair.Value);
		}
	}

	TagSubscriptions.Reset();
	TagWaiters.Reset();

	// Handler 弱引用一并清掉：本表被清空即"世界反初始化"，此后不应再有任何登记（下次装配由门面重做）
	Handler = nullptr;
}



// 登记
bool FTcsChainEventWaitRegistry::AddWaiter(FTcsChainRunHandle Handle, FGameplayTag EventTag, UTcsEventBusSubsystem* Bus)
{
	if (!EventTag.IsValid())
	{
		// 调用方（`WaitEvent`）已先校验并降级；走到这里说明有别的调用方漏了校验——留痕不 ensure
		UE_LOG(LogTcsEffect, Warning, TEXT("链事件等待：EventTag 无效——拒绝登记"));
		return false;
	}

	if (!Bus)
	{
		// 世界拆解期总线不可得：时序而非配置错误（口径同 `FTcsTriggerRegistry::RegisterRow`）
		UE_LOG(LogTcsEffect, Warning, TEXT("链事件等待：总线门面不可得（事件=%s）——拒绝登记"), *EventTag.ToString());
		return false;
	}

	if (!Handler.IsValid())
	{
		// 装配漏了 = 代码契约违规（门面创建 Handler 后必须立即 SetHandler）
		ensureMsgf(false, TEXT("FTcsChainEventWaitRegistry::AddWaiter: 共享 Handler 未装配或已回收——无法订阅"));
		return false;
	}

	// 订阅配对：该 tag **首次**出现时订阅一次（同一 tag 的后续等待者复用它）
	if (!TagSubscriptions.Contains(EventTag))
	{
		// 立即通道：等待者要在"事件发布返回之前"就已被唤醒（帧末通道会晚一拍——与触发行同款取舍）
		const FTcsEventSubscriptionHandle Subscription = Bus->Subscribe(
			EventTag, Handler.Get(), ETcsEventDispatch::EED_Immediate);

		if (!Subscription.IsValid())
		{
			// 总线自己的拒绝面（ensure + 无效句柄）已经把原因留在日志里，此处不重复
			return false;
		}

		TagSubscriptions.Add(EventTag, Subscription);

		UE_LOG(LogTcsEffect, Verbose, TEXT("链事件等待：订阅（事件=%s）"), *EventTag.ToString());
	}

	TagWaiters.FindOrAdd(EventTag).Add(Handle);

	UE_LOG(LogTcsEffect, Verbose, TEXT("链事件等待：登记等待者（事件=%s 等待者数=%d）"),
		*EventTag.ToString(), TagWaiters.FindChecked(EventTag).Num());
	return true;
}

bool FTcsChainEventWaitRegistry::RemoveWaiter(FTcsChainRunHandle Handle, FGameplayTag EventTag, UTcsEventBusSubsystem* Bus)
{
	TArray<FTcsChainRunHandle>* Waiters = TagWaiters.Find(EventTag);
	if (!Waiters)
	{
		return false;
	}

	// 值语义配对摘除（身份 = Index + Generation；`FTcsChainRunHandle::operator==`）
	if (Waiters->RemoveSingle(Handle) == 0)
	{
		return false;
	}

	UE_LOG(LogTcsEffect, Verbose, TEXT("链事件等待：摘除等待者（事件=%s 剩余=%d）"),
		*EventTag.ToString(), Waiters->Num());

	DropSubscriptionIfUnused(EventTag, Bus);
	return true;
}



// 查询
void FTcsChainEventWaitRegistry::CollectWaiters(FGameplayTag EventTag, TArray<FTcsChainRunHandle>& OutHandles) const
{
	if (const TArray<FTcsChainRunHandle>* Waiters = TagWaiters.Find(EventTag))
	{
		OutHandles.Append(*Waiters);
	}
}

int32 FTcsChainEventWaitRegistry::GetWaiterCount() const
{
	int32 Total = 0;
	for (const TPair<FGameplayTag, TArray<FTcsChainRunHandle>>& Pair : TagWaiters)
	{
		Total += Pair.Value.Num();
	}
	return Total;
}



// 内核
void FTcsChainEventWaitRegistry::DropSubscriptionIfUnused(FGameplayTag EventTag, UTcsEventBusSubsystem* Bus)
{
	// 仍有等待者则保留订阅（计数未归零）
	if (const TArray<FTcsChainRunHandle>* Waiters = TagWaiters.Find(EventTag))
	{
		if (Waiters->Num() > 0)
		{
			return;
		}
	}

	TagWaiters.Remove(EventTag);

	FTcsEventSubscriptionHandle Subscription;
	if (TagSubscriptions.RemoveAndCopyValue(EventTag, Subscription))
	{
		if (Bus)
		{
			Bus->Unsubscribe(Subscription);
		}

		UE_LOG(LogTcsEffect, Verbose, TEXT("链事件等待：退订（事件=%s 等待者归零）"), *EventTag.ToString());
	}
}
