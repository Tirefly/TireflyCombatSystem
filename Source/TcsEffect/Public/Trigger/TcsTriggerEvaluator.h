// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EventBus/TcsEventHandler.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "Trigger/TcsEffectTrigger.h"
#include "Trigger/TcsTriggerCondition.h"
#include "Trigger/TcsTriggerPayloadReader.h"

#include "TcsTriggerEvaluator.generated.h"

class UTcsEffectSubsystem;



/**
 * 触发求值器（M4a 触发行求值；**共享 Handler**——裁决 2a：事件类型 → Handler 对象执行，
 * 事件 struct 上不自绑 delegate）。
 *
 * **为什么是单个共享对象而非每行一个 Handler**：行数据是**纯配置**（`FTcsEffectTriggerDef`），
 * 行为全部集中在求值器；每行一个 Handler 会让"行为"散进内容资产，且订阅表随行数膨胀。
 * 总线按 `EventTag` 订阅一次（计数配对，见门面），回调内遍历该 Tag 的全部行。
 *
 * **四道门（MUST 按此序，顺序有意义——04 §3）**：
 * `事件 Tag 路由 → ExecutionGate → GateTags → Conditions → 起链`。
 * `ExecutionGate`（网络闸）最廉价先判；`Conditions`（可含宿主自定义求值）最贵最后判。
 * 任一道不过 = **不触发本行**（不短路其它行——同行之间彼此独立）。
 *
 * **持有与生命周期**：由门面在首次登记时创建、`UPROPERTY` 持有——总线订阅表持**弱引用**
 * （`FTcsEventSubscription::Handler` 是 `TWeakObjectPtr`），不 root 会被 GC 掉、订阅静默失效。
 * 本对象**无实例状态**（唯一成员是指回门面的弱引用）——所有行数据都住门面的登记表，
 * 故"共享"是天然成立的，不需要每行一份。
 */
UCLASS()
class TCSEFFECT_API UTcsTriggerEvaluator : public UTcsEventHandler
{
	GENERATED_BODY()

// 装配
#pragma region Setup

public:
	/**
	 * 装配：持门面弱引用（起链、取行、点灯、随机值都要经它）。
	 * 由门面在创建本对象后立即调用一次。
	 *
	 * @param InOwner 门面（通常即 `this` 的 Outer）。
	 */
	void Initialize(UTcsEffectSubsystem* InOwner);

#pragma endregion


// 求值入口
#pragma region Evaluation

public:
	/**
	 * 事件入口（总线订阅回调）：四道门逐行求值 → 命中即起链。
	 *
	 * **快照 + 代际校验纪律**：遍历的行句柄先快照，逐行重解析——本轮遍历期间被摘除的行跳过，
	 * 期间新登记的行不入本轮（与总线派发"派发中订阅/退订不影响本轮遍历"同款）。
	 *
	 * @param EventTag 命中的事件 Tag（总线传入）。
	 * @param Payload 事件载荷（立即通道零复制透传）。
	 */
	virtual void HandleEvent_Implementation(FGameplayTag EventTag, const FInstancedStruct& Payload) override;

#pragma endregion


// 内核
#pragma region Core

private:
	// 事件载荷 → 主体信息（走载荷读取器注册表；无读取器 = 默认构造 + Verbose）
	FTcsTriggerPayloadInfo ReadPayloadInfo(const FInstancedStruct& Payload) const;

	// 门② 执行闸（R4：TEG_Always 与 TEG_AuthorityOnly 同判——单机形态下本地即权威）
	static bool PassesExecutionGate(const FTcsEffectTriggerDef& Def);

	// 门③ 行级点灯（GateTags **全部**点亮才通过；空数组 = 无开关，恒通过）
	bool PassesGateTags(const FTcsEffectTriggerDef& Def) const;

	// 门④ 条件（走条件注册表；随机值取自门面的种子流——D0-1：求值内部不取随机数）。
	// **非 const**：取随机值会推进门面的随机流（可复现的确定序列，见 `FTcsTriggerRegistry::NextRandomValue`）
	bool PassesConditions(const FTcsEffectTriggerDef& Def, const FTcsTriggerContext& Context);

	// 门面弱引用（运行态生命周期不长于子系统）
	TWeakObjectPtr<UTcsEffectSubsystem> Owner;

#pragma endregion
};
