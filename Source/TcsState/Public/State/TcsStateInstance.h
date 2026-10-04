// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Clock/TcsExpiryHeap.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "State/TcsStateEnums.h"
#include "State/TcsStateHandle.h"



/**
 * 状态实例记录（D3-1 / D3-12）：**池化纯数据**——一个单位的一个在册状态的全部运行态。
 *
 * **为什么 MUST NOT 持策略 / 载荷 / 订阅句柄 / `UObject` 引用**（D3-7 v2 与 D2-9 纪律）：
 * 策略归 Def 资产持有（实例零策略 ⇒ 池化 struct 保持纯度）、事件载荷归事件、
 * 订阅句柄归注册表/总线。这三样一旦进实例，池化与将来的**操作复制**（状态操作流按值走网络）
 * 立即失效——这是该纪律的真实代价来源，不是洁癖。
 *
 * **非反射结构体**（无 `USTRUCT` 宏）：本记录只在 C++ 侧流转，进总线的载荷是
 * `FTcsStateEventPayload`（那个才需要反射）。反射化留给"实例要整体过网"的轮次（NET 系列）。
 *
 * **`Source` 是级联锚点**：施加时经 `FTcsSourceHandleRegistry::Allocate()` 取新值（进程内唯一、
 * 永不复用）——触发行退订与属性修正器摘除都按它一次清干净（同一个状态 = 同一个来源）。
 * 调用方可传入自己的来源句柄（链运行态来源等），无效时才由门面发号。
 */
struct FTcsStateInstance
{
// 身份与归属
#pragma region Identity

public:
	// 定义身份（施加时的 `DefTag`——热路径靠注册表内的定义副本解析，不回查 Def 库）
	FGameplayTag DefTag;

	// 桶内句柄（本实例的身份；`Index` / `Generation` 的语义由所属桶定义）
	FTcsStateHandle Handle;

	/**
	 * 宿主单位（被施加方；本实例所属的桶）。
	 *
	 * **为什么实例自持它**：门面按句柄取实例时（`GetState(Handle)`）句柄里没有单位段，
	 * 若无此字段就只能"遍历所有桶找这个句柄"（O(桶数)）；自持后是 O(1) 定位。
	 * 而且"这个状态在谁身上"本就是实例身份的一部分——R5 Task 4 的修正器物化要用它
	 * 把修正器挂到该单位的属性账本上（同一份信息，不是重复真相）。
	 */
	FTcsCombatEntityHandle Unit;

	// 归属来源句柄（级联撤销锚点；非 0）
	FTcsSourceHandle Source;

	// 发起者实体（"谁发起的"——可与被施加方不同；本轮只落字段，消费者是后续的关系/记录面）
	FTcsCombatEntityHandle Instigator;

#pragma endregion


// 数值与阶段
#pragma region Value

public:
	// 当前层数（从 1 起；五轴堆叠决策归 R5 Task 5）
	int32 Stacks = 1;

	// 生效等级（等级源求值入参；快照构建归 R5 Task 3）
	int32 Level = 0;

	// 实例阶段（在册稳态 = `ESP_Active`）
	EStatePhase Phase = EStatePhase::ESP_Active;

#pragma endregion


// 时间
#pragma region Time

public:
	// 剩余时长（秒；`EDP_Infinite` 时不使用——无到期条目）
	double DurationRemaining = 0.0;

	// 距下个周期的剩余时间（秒；周期机制归 R5 Task 3）
	double PeriodRemaining = 0.0;

	// 参数快照（施加瞬间冻结的生效数值；**类型归 R5 Task 3 落地**——字段先留位，本步不消费）
	// 命名用 `State` 中缀（与 `FTcsStateDefBase` / `FTcsStateEventPayload` 同族），
	// 而非设计文档里的概念名 `FTcsParamSnapshot`（实现名以 `TcsState` 前缀成族；Task 3 定稿时回写文档）
	// FTcsStateParamSnapshot ParamSnapshot;

	// 到期条目锚点（取消锚；**入堆/撤堆归 R5 Task 3**——字段先留位，本步不消费）
	FTcsTimeEntryHandle ExpiryEntry;

#pragma endregion
};
