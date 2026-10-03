// Copyright Tirefly. All Rights Reserved.

#include "TcsEffectSubsystem.h"

#include "Chain/TcsChainEventWaitHandler.h"
#include "EventBus/TcsEventBusSubsystem.h"
#include "TcsEffectLogChannel.h"



// 事件等待（唤醒源②"事件匹配"的登记面）
bool UTcsEffectSubsystem::ArmEventWait(FTcsChainRunHandle Handle, FGameplayTag EventTag)
{
	ensure(IsInGameThread());

	if (!EventTag.IsValid())
	{
		// 调用方（`WaitEvent`）已先校验并降级；走到这里说明有别的调用方漏了校验——留痕不 ensure
		UE_LOG(LogTcsEffect, Warning, TEXT("UTcsEffectSubsystem::ArmEventWait: EventTag 无效——拒绝装锚"));
		return false;
	}

	if (!RunPool.IsValid(Handle.GetInner()))
	{
		return false;
	}

	UTcsEventBusSubsystem* Bus = GetEventBus();
	if (!Bus)
	{
		// 世界拆解期总线不可得：时序而非配置错误（不 ensure——同 `FTcsTriggerRegistry` 口径）
		UE_LOG(LogTcsEffect, Warning, TEXT("UTcsEffectSubsystem::ArmEventWait: 总线门面不可得（事件=%s）——拒绝装锚"),
			*EventTag.ToString());
		return false;
	}

	// 共享 Handler 懒建 + `UPROPERTY` 持有（总线订阅表持弱引用——不 root 会被 GC 掉、订阅静默失效）
	if (!ChainEventWaitHandler)
	{
		ChainEventWaitHandler = NewObject<UTcsChainEventWaitHandler>(this);
		ChainEventWaitHandler->Initialize(this);
		ChainEventWaitRegistry.SetHandler(ChainEventWaitHandler);
	}

	// 先登记等待表（订阅配对在其内部；失败即返回——运行态侧锚不写，不留"锚在场但无人会唤醒"的半状态）
	if (!ChainEventWaitRegistry.AddWaiter(Handle, EventTag, Bus))
	{
		return false;
	}

	// 运行态侧锚（可配平）**最后写**：上面的分配（`NewObject` / 订阅池）都不碰运行态池，
	// 故此处解析出的指针即刻可用；成对写在最后也使"登记成功 ⇒ 锚一定在场"成立
	if (FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner()))
	{
		Run->PendingEventTag = EventTag;
		return true;
	}

	// 运行态在登记与解析之间消失了（本函数单线程、中间无重入 ⇒ 理论上不可达）：防御性回滚，
	// 不把幽灵等待者留在表里（留着会让总线为一条永不唤醒的登记长期持一条订阅）
	ChainEventWaitRegistry.RemoveWaiter(Handle, EventTag, Bus);
	return false;
}

bool UTcsEffectSubsystem::DropEventWait(FTcsChainRunHandle Handle)
{
	ensure(IsInGameThread());

	FTcsChainRun* Run = RunPool.IsValid(Handle.GetInner()) ? RunPool.Resolve(Handle.GetInner()) : nullptr;
	if (!Run)
	{
		return false;
	}

	const FGameplayTag EventTag = Run->PendingEventTag;
	if (!EventTag.IsValid())
	{
		// 没在等事件 = 已配平过（或从未装锚）：正常路径，不 ensure
		return false;
	}

	// 先清运行态锚再摘登记：重复调用第二次会在此直接返回 false，天然幂等
	Run->PendingEventTag = FGameplayTag();
	return ChainEventWaitRegistry.RemoveWaiter(Handle, EventTag, GetEventBus());
}



// 唤醒路由（Handler → 本方法；路由归门面，理由见 D-1：总线派发不传订阅句柄，Handler 无法自辨身份）
void UTcsEffectSubsystem::WakeEventWaiters(FGameplayTag EventTag, const FInstancedStruct& Payload)
{
	ensure(IsInGameThread());

	// 快照：下面的 `ResumeRun` 会执行步骤（可能起链并扩容运行态池，也可能立刻再装同一 tag 的锚）
	// ⇒ MUST NOT 直接遍历表本体，也 MUST NOT 持有运行态指针跨 `ResumeRun`
	TArray<FTcsChainRunHandle> Waiters;
	ChainEventWaitRegistry.CollectWaiters(EventTag, Waiters);

	for (const FTcsChainRunHandle& Waiter : Waiters)
	{
		// 代际校验（快照过期：本轮前序唤醒的链可能已走完释放，或登记已被摘除）
		if (!RunPool.IsValid(Waiter.GetInner()))
		{
			continue;
		}

		// **先摘登记，再唤醒**：使"同一事件不会二次唤醒同一运行态"不依赖步骤行为——步骤若因任何
		// 原因没配平，残留登记就是二次唤醒面。摘除可能触发该 tag 退订：总线派发已快照目标句柄、
		// 派发中退订是安全的（退订者在派发循环里被代际校验跳过）
		ChainEventWaitRegistry.RemoveWaiter(Waiter, EventTag, GetEventBus());

		FTcsChainRun* Run = RunPool.Resolve(Waiter.GetInner());
		if (!Run)
		{
			continue;
		}

		// 命中语义：载荷写进黑板 + 置唤醒判据。**两者都由步骤配平**（门面不清挂起锚——配平纪律）
		Run->Context.EventPayload = Payload;
		Run->bPendingEventHit = true;

		UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 链 %s 事件命中唤醒（事件=%s）"),
			*Run->ChainId.ToString(), *EventTag.ToString());

		// 重入：此后 MUST NOT 再用 `Run`（`ResumeRun` 执行步骤、可能扩容搬移；池 Free 不清零）
		ResumeRun(Waiter);
	}
}
