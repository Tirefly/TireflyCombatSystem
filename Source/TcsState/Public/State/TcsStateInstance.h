// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Clock/TcsExpiryHeap.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "State/TcsStateEnums.h"
#include "State/TcsStateHandle.h"
#include "State/TcsStateSnapshot.h"



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
 * **两个句柄的分工 MUST 守死**（2026-10-05 解耦，R5 Task 6 裁定 1）：
 * - `Source` = **施加方来源句柄**（"谁施加的"）：调用方声明时沿用、未声明时由门面发号（每次一枚新号）。
 *   它承担共存决策的"续杯 / 叠层"判据与事件载荷的归属读数；**MUST NOT 当撤销锚点**——一个来源可以
 *   施加多个不同定义（一条技能给同一目标挂两个 buff），按它撤销会把别条实例的条目一起摘掉；
 * - `CascadeAnchor` = **级联撤销锚点**：建出实例时经 `FTcsSourceHandleRegistry::Allocate()` 取新值
 *   （进程内唯一、永不复用），刷新 / 叠层不换——触发行退订与属性修正器摘除都按它一次清干净
 *   （同一条实例 = 同一个锚点）。
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

	// 施加方来源句柄（"谁施加的"——续杯 / 叠层判据 + 事件载荷；非 0）
	FTcsSourceHandle Source;

	// 级联撤销锚点（每实例恒发新号、刷新不换——修正器条目与内联触发行都按它挂、按它一次摘净；非 0）
	FTcsSourceHandle CascadeAnchor;

	// 发起者实体（"谁发起的"——可与被施加方不同；本轮只落字段，消费者是后续的关系/记录面）
	FTcsCombatEntityHandle Instigator;

#pragma endregion


// 数值与阶段
#pragma region Value

public:
	// 当前层数（从 1 起；五轴堆叠决策归 R5 Task 5）
	int32 Stacks = 1;

	// 生效等级（等级源求值入参——`Context.EffectiveLevel` 取它；快照构建归 R5 Task 3）
	int32 Level = 0;

	// 实例阶段（在册稳态 = `ESP_Active`）
	EStatePhase Phase = EStatePhase::ESP_Active;

	/**
	 * 参数快照（施加瞬间冻结的生效数值，D3-12；构建与重建规则见 `state-param-snapshot` 能力）。
	 *
	 * **为什么它不是"策略载体"**（不违反本结构体的纯度纪律）：快照是**已求值结果的副本**，
	 * 值语义、可复制；`SourceRef` 位也只是该行数值来源的副本（供 Debug 与 Live 化），
	 * 不是"实例持策略"（策略仍归 Def 资产）。其内层若含对象引用，由门面的 `AddReferencedObjects`
	 * 统一补引用（本结构体非 `UPROPERTY` 容器，GC 看不见）。
	 */
	FTcsParamSnapshot ParamSnapshot;

#pragma endregion


// 时间
#pragma region Time

public:
	// 剩余时长（秒；`EDP_Infinite` 时不使用——无到期条目）
	double DurationRemaining = 0.0;

	// 距下个周期的剩余时间（秒；周期机制归 R5 Task 3——值每次周期回调后重置为满额）
	double PeriodRemaining = 0.0;

	// 时值到期条目锚点（取消锚；`Finite` 时非空，`Infinite` 时恒空）
	FTcsTimeEntryHandle ExpiryEntry;

	/**
	 * 周期到期条目锚点（取消锚；`Period > 0` 时非空）。
	 *
	 * **为什么与时值条目分开两个字段**：`Finite` 与 `Period > 0` 是可同时成立的两个语义
	 * （标准 DoT = 有限时值 + 周期跳伤），挤在一个锚点里会让"撤销旧条目"分不清撤的是哪一条
	 * （`ExtendDuration` 只该动时值条目，`PeriodRefresh` 只该动周期条目）。
	 */
	FTcsTimeEntryHandle PeriodEntry;

#pragma endregion
};
