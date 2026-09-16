// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Subsystems/WorldSubsystem.h"

#include "EventBus/FTcsEventBus.h"

#include "UTcsEventBusSubsystem.generated.h"



// 全量战斗事件动态多播（A' 反射面：BP/CS 绑定入口；参数 = 事件 Tag + 载荷，绑定者自行过滤）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTcsOnCombatEvent, FGameplayTag, EventTag, FInstancedStruct, Payload);



/**
 * 事件总线门面（M0 §2.2）：世界级子系统，持有总线内核并接入动态多播反射面。
 *
 * 帧末队列冲洗时机：当前随本子系统 Tick（UWorld::Tick 尾部 TickObjects——晚于全部 Actor tick 组，
 * 即"帧末冲洗"：本帧游戏逻辑期间入队的事件不会在发布当场送达）；
 * Task 3 起改由时钟泵按时序驱动（时钟推进 → 总线冲洗 → 到期堆，PrePhysics），届时本子系统停用自 tick。
 */
UCLASS()
class TCSCORE_API UTcsEventBusSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

// 生命期
#pragma region Lifetime

public:
	// 构造：总线全量事件流接入动态多播
	UTcsEventBusSubsystem();

	// 世界类型过滤：仅游戏世界（Game/PIE/GamePreview）实例化——总线是运行时设施
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// 反初始化：清空订阅与队列（世界销毁/PIE 结束的确定性清理）；必须转发 Super 以正确停用 tick
	virtual void Deinitialize() override;

#pragma endregion


// Tick
#pragma region Tick

public:
	// 帧末冲洗帧末队列（Task 3 起由时钟泵按时序驱动）
	virtual void Tick(float DeltaTime) override;

	// Tick 统计标识
	virtual TStatId GetStatId() const override;

#pragma endregion


// 订阅
#pragma region Subscription

public:
	/**
	 * 订阅事件。
	 *
	 * @param Tag 订阅的事件 Tag。
	 * @param Handler 共享 Handler 对象（通常传 Handler CDO 或宿主共享实例；弱引用持有）。
	 * @param Dispatch 派发通道（立即/帧末）。
	 * @return 返回订阅句柄；参数非法时返回无效句柄。
	 */
	FTcsEventSubscriptionHandle Subscribe(FGameplayTag Tag, UTcsEventHandler* Handler, ETcsEventDispatch Dispatch);

	/**
	 * 退订事件（句柄配对清理）。
	 *
	 * @param Handle 订阅句柄。
	 */
	void Unsubscribe(FTcsEventSubscriptionHandle Handle);

#pragma endregion


// 派发
#pragma region Publish

public:
	/**
	 * 立即通道发布：同步直达订阅者。
	 *
	 * @param Tag 事件 Tag。
	 * @param Payload 事件载荷。
	 */
	void PublishImmediate(FGameplayTag Tag, const FInstancedStruct& Payload);

	/**
	 * 帧末通道发布：入队，帧边界统一派发。
	 *
	 * @param Tag 事件 Tag。
	 * @param Payload 事件载荷。
	 */
	void PublishFrameEnd(FGameplayTag Tag, const FInstancedStruct& Payload);

	// 冲洗帧末队列（泵驱动入口；本子系统 Tick 亦调用）
	void FlushFrameEndQueue();

	// 帧末队列深度（观测/测试用）
	int32 GetPendingFrameEndCount() const;

#pragma endregion


// 反射面
#pragma region Reflection

public:
	// 全量事件流（BP/CS 绑定入口——绑定者按 Tag 自行过滤；与订阅表无关，每笔派发均触发）
	UPROPERTY(BlueprintAssignable, Category = "Tcs|Core|EventBus")
	FTcsOnCombatEvent OnEvent;

#pragma endregion


// 内核
#pragma region Core

private:
	// 总线内核实例
	FTcsEventBus EventBus;

#pragma endregion
};
