// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "State/TcsStateHandle.h"

#include "TcsStateBehaviorFragment.generated.h"

class UTcsStateSubsystem;



/**
 * 行为回调上下文（纯 C++ 瞬时值记录）。实例身份与数值按回调开始时复制，不携带实例指针。
 * 回调可能增删状态或注销定义；需要当前实例时必须经 Subsystem 与 Handle 重新查询。
 */
struct FTcsStateBehaviorContext
{
// 身份与数值
#pragma region State

public:
	// 当前世界的状态门面（只在本次回调期间使用，不延长门面生命周期）
	UTcsStateSubsystem* Subsystem = nullptr;

	// 接收本次行为回调的状态实例
	FTcsStateHandle Handle;

	// 该实例所属单位
	FTcsCombatEntityHandle Unit;

	// 状态定义身份
	FGameplayTag DefTag;

	// 回调开始时的层数与等级
	int32 Stacks = 1;
	int32 Level = 0;

#pragma endregion
};



/**
 * 状态行为 Fragment 契约（D3-7）：兴趣 Tag 精确匹配 + 单一泛化回调。
 * 配置由 Def 持有，实例不持策略或订阅句柄；回调为 const，实例副作用经 Ctx.Subsystem 操作。
 * 框架不提供具体行为。基类可默认构造且回调为空实现，Hidden 隐藏基类选择项；禁纯虚声明。
 */
USTRUCT(meta = (Hidden))
struct TCSSTATE_API FTcsStateBehaviorFragment
{
	GENERATED_BODY()

// 兴趣事件
#pragma region Interests

public:
	// 精确匹配的事件 Tag 列表（TcsEvent 根；不展开父标签，同一片段的重复兴趣只登记一次）
	UPROPERTY(EditAnywhere, Category = "Tcs|State|Behavior")
	TArray<FGameplayTag> Interests;

#pragma endregion


// 行为入口
#pragma region Behavior

public:
	// 派生片段按反射类型销毁；基类也保证多态析构安全。
	virtual ~FTcsStateBehaviorFragment() = default;

	/**
	 * 处理一个感兴趣的事件。默认无操作；派生类型自行读取载荷，修改实例必须经门面。
	 *
	 * @param EventTag 当前事件 Tag（精确匹配 Interests）。
	 * @param Payload 本次事件载荷（仅供本次回调读取）。
	 * @param Ctx 本片段所属实例的身份与数值快照。
	 */
	virtual void OnStateEvent(
		const FGameplayTag& EventTag,
		const FInstancedStruct& Payload,
		const FTcsStateBehaviorContext& Ctx) const;

#pragma endregion
};
