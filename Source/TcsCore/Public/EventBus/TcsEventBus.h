// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Pool/TcsInstancePool.h"
#include "StructUtils/InstancedStruct.h"
#include "TcsCoreLogChannel.h"

#include "EventBus/TcsEventHandler.h"

#include "TcsEventBus.generated.h"



// 事件派发通道（枚举值前缀 EED_ 是 EventDispatch 的缩写）
UENUM(BlueprintType)
enum class ETcsEventDispatch : uint8
{
	EED_Immediate = 0	UMETA(DisplayName = "立即", ToolTip = "发布当场同步派发（调用栈内直达订阅者；默认，值 0）"),
	EED_FrameEnd = 1	UMETA(DisplayName = "帧末", ToolTip = "入队后由冲洗点在帧边界按入队序统一派发"),
};



// 事件订阅句柄标签（仅供句柄模板做类型区分）
struct FTcsEventSubTag
{
};



// 事件订阅句柄（退订锚点；句柄配对清理——复用 TTcsInstancePool 机制）
struct TCSCORE_API FTcsEventSubscriptionHandle
{
	// 池内句柄
	TTcsInstanceHandle<FTcsEventSubTag> Inner;

	// 句柄有效性（代际校验由总线在退订/派发时执行）
	bool IsValid() const
	{
		return Inner.IsValid();
	}

	// 相等比较（订阅表配对清理用）
	friend bool operator==(const FTcsEventSubscriptionHandle& A, const FTcsEventSubscriptionHandle& B)
	{
		return A.Inner == B.Inner;
	}
};



// 单条订阅记录（池内实例：Tag 路由 + 派发通道 + 共享 Handler 弱引用）
struct FTcsEventSubscription
{
	// 订阅的事件 Tag
	FGameplayTag Tag;

	// 派发通道（立即/帧末）
	ETcsEventDispatch Dispatch = ETcsEventDispatch::EED_Immediate;

	// 共享 Handler（弱引用——不阻止 GC；失效订阅在派发时惰性摘除）
	TWeakObjectPtr<UTcsEventHandler> Handler;
};



// 帧末队列条目（入队序 = 派发序）
struct FTcsQueuedEvent
{
	// 事件 Tag
	FGameplayTag Tag;

	// 事件载荷（帧末通道持有副本——延迟派发的必要持有）
	FInstancedStruct Payload;
};



/**
 * 事件总线内核（裁决 2a）：Tag 路由 + 订阅表 + 池化订阅记录 + 双通道派发。
 * 纯逻辑类（非 UObject）——门面 UTcsEventBusSubsystem 持有其实例并接入动态多播反射面。
 *
 * 派发纪律：
 * - 派发前快照同 Tag 订阅句柄，逐目标校验代际——派发中订阅/退订不影响本轮遍历（退订者被跳过）；
 * - 立即通道同步递归（链式事件直达，零队列开销）；帧末通道冲洗期间新入队的事件归下一拍（防自激死循环）；
 * - 载荷在立即通道零复制（const 引用透传）；帧末通道入队持有副本（延迟派发的必要持有）。
 */
class TCSCORE_API FTcsEventBus
{
// 订阅管理
#pragma region Subscription

public:
	/**
	 * 订阅事件。
	 *
	 * @param Tag 订阅的事件 Tag。
	 * @param Handler 共享 Handler 对象（通常传 Handler CDO 或宿主共享实例；弱引用持有，不阻止 GC）。
	 * @param Dispatch 派发通道（立即/帧末）。
	 * @return 返回订阅句柄；Handler 为空或 Tag 无效时返回无效句柄（ensure 报错）。
	 */
	FTcsEventSubscriptionHandle Subscribe(FGameplayTag Tag, UTcsEventHandler* Handler, ETcsEventDispatch Dispatch)
	{
		if (!ensureMsgf(Handler != nullptr, TEXT("FTcsEventBus::Subscribe: Handler 为空（Tag=%s）"), *Tag.ToString()))
		{
			return FTcsEventSubscriptionHandle();
		}

		if (!ensureMsgf(Tag.IsValid(), TEXT("FTcsEventBus::Subscribe: Tag 无效")))
		{
			return FTcsEventSubscriptionHandle();
		}

		const TTcsInstanceHandle<FTcsEventSubTag> InnerHandle = SubscriptionPool.Allocate();
		FTcsEventSubscription* Subscription = SubscriptionPool.Resolve(InnerHandle);
		check(Subscription != nullptr);

		Subscription->Tag = Tag;
		Subscription->Dispatch = Dispatch;
		Subscription->Handler = Handler;

		TagIndex.Add(Tag, FTcsEventSubscriptionHandle{ InnerHandle });

		UE_LOG(LogTcsCore, Verbose, TEXT("事件订阅：Tag=%s 通道=%s 句柄=%u/%u"),
			*Tag.ToString(),
			Dispatch == ETcsEventDispatch::EED_Immediate ? TEXT("立即") : TEXT("帧末"),
			InnerHandle.Index, InnerHandle.Generation);

		return FTcsEventSubscriptionHandle{ InnerHandle };
	}

	/**
	 * 退订事件（配对清理：订阅表条目移除 + 池槽位释放）。
	 *
	 * @param Handle 订阅句柄（悬空/无效句柄 ensure 报错并忽略）。
	 */
	void Unsubscribe(FTcsEventSubscriptionHandle Handle)
	{
		if (!ensureMsgf(SubscriptionPool.IsValid(Handle.Inner),
			TEXT("FTcsEventBus::Unsubscribe: 悬空或无效订阅句柄（Index=%u, Generation=%u）"),
			Handle.Inner.Index, Handle.Inner.Generation))
		{
			return;
		}

		FTcsEventSubscription* Subscription = SubscriptionPool.Resolve(Handle.Inner);
		check(Subscription != nullptr);

		const int32 RemovedCount = TagIndex.RemoveSingle(Subscription->Tag, Handle);
		ensureMsgf(RemovedCount == 1,
			TEXT("FTcsEventBus::Unsubscribe: 订阅表配对移除异常（Tag=%s 移除数=%d）"),
			*Subscription->Tag.ToString(), RemovedCount);

		UE_LOG(LogTcsCore, Verbose, TEXT("事件退订：Tag=%s 句柄=%u/%u"),
			*Subscription->Tag.ToString(), Handle.Inner.Index, Handle.Inner.Generation);

		SubscriptionPool.Free(Handle.Inner);
	}

#pragma endregion


// 派发
#pragma region Publish

public:
	/**
	 * 立即通道发布：发布当场同步派发（零队列开销）。
	 *
	 * @param Tag 事件 Tag。
	 * @param Payload 事件载荷（const 引用透传，不复制）。
	 */
	void PublishImmediate(FGameplayTag Tag, const FInstancedStruct& Payload)
	{
		UE_LOG(LogTcsCore, Verbose, TEXT("事件发布（立即）：Tag=%s"), *Tag.ToString());
		DispatchToChannel(Tag, Payload, ETcsEventDispatch::EED_Immediate);
	}

	/**
	 * 帧末通道发布：入队，由冲洗点按入队序统一派发。
	 *
	 * @param Tag 事件 Tag。
	 * @param Payload 事件载荷（入队持有副本）。
	 */
	void PublishFrameEnd(FGameplayTag Tag, const FInstancedStruct& Payload)
	{
		UE_LOG(LogTcsCore, Verbose, TEXT("事件发布（帧末）：Tag=%s 队列深度=%d"),
			*Tag.ToString(), FrameEndQueue.Num() + 1);

		FTcsQueuedEvent& QueuedEvent = FrameEndQueue.AddDefaulted_GetRef();
		QueuedEvent.Tag = Tag;
		QueuedEvent.Payload = Payload;
	}

#pragma endregion


// 帧末冲洗
#pragma region Flush

public:
	// 冲洗帧末队列（先换出再逐条派发——冲洗期间新入队的事件归下一拍）
	void FlushFrameEnd()
	{
		if (FrameEndQueue.Num() == 0)
		{
			return;
		}

		TArray<FTcsQueuedEvent> PendingEvents = MoveTemp(FrameEndQueue);
		FrameEndQueue.Reset();

		UE_LOG(LogTcsCore, Verbose, TEXT("帧末队列冲洗：批量=%d"), PendingEvents.Num());

		for (const FTcsQueuedEvent& QueuedEvent : PendingEvents)
		{
			DispatchToChannel(QueuedEvent.Tag, QueuedEvent.Payload, ETcsEventDispatch::EED_FrameEnd);
		}
	}

	// 帧末队列深度（观测/测试用）
	int32 GetPendingFrameEndCount() const
	{
		return FrameEndQueue.Num();
	}

#pragma endregion


// 反射面桥接
#pragma region Observer

public:
	// 全量事件观察钩子（门面接入动态多播；订阅表无关——每笔派发均触发，绑定者自行过滤）
	TFunction<void(FGameplayTag, const FInstancedStruct&)> EventObserver;

#pragma endregion


// 生命期
#pragma region Lifetime

public:
	// 清空订阅与队列（保留观察钩子——世界销毁/PIE 结束的确定性清理）
	void Reset()
	{
		SubscriptionPool = TTcsInstancePool<FTcsEventSubscription, FTcsEventSubTag>();
		TagIndex.Reset();
		FrameEndQueue.Reset();
	}

#pragma endregion


// 内部
#pragma region Internal

private:
	// 按通道派发同 Tag 订阅（快照 + 代际校验 + 失效订阅惰性摘除）
	void DispatchToChannel(FGameplayTag Tag, const FInstancedStruct& Payload, ETcsEventDispatch Dispatch)
	{
		// 快照目标句柄：派发中订阅/退订不影响本轮遍历
		TArray<FTcsEventSubscriptionHandle> Targets;
		TagIndex.MultiFind(Tag, Targets);

		for (const FTcsEventSubscriptionHandle& Target : Targets)
		{
			// 快照过期：派发前已被退订
			if (!SubscriptionPool.IsValid(Target.Inner))
			{
				continue;
			}

			FTcsEventSubscription* Subscription = SubscriptionPool.Resolve(Target.Inner);
			check(Subscription != nullptr);

			// 通道不匹配（同 Tag 两通道各自独立订阅）
			if (Subscription->Dispatch != Dispatch)
			{
				continue;
			}

			// 惰性摘除：Handler 已回收的僵尸订阅
			if (!Subscription->Handler.IsValid())
			{
				UE_LOG(LogTcsCore, Verbose, TEXT("总线惰性摘除僵尸订阅：Tag=%s 句柄=%u"),
					*Subscription->Tag.ToString(), Target.Inner.Index);
				Unsubscribe(Target);
				continue;
			}

			Subscription->Handler->HandleEvent(Tag, Payload);
		}

		// 全量事件流喂观察者（与订阅表无关；立即通道同步喂、帧末通道由冲洗点逐条喂）
		if (EventObserver)
		{
			EventObserver(Tag, Payload);
		}
	}

#pragma endregion


// 存储
#pragma region Storage

private:
	// 订阅记录池（句柄配对清理）
	TTcsInstancePool<FTcsEventSubscription, FTcsEventSubTag> SubscriptionPool;

	// Tag 路由订阅表（Tag → 订阅句柄）
	TMultiMap<FGameplayTag, FTcsEventSubscriptionHandle> TagIndex;

	// 帧末队列（入队序 = 派发序）
	TArray<FTcsQueuedEvent> FrameEndQueue;

#pragma endregion
};
