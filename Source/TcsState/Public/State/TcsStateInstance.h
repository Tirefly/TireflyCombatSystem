// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Clock/TcsExpiryHeap.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "State/TcsStateSnapshot.h"



/**
 * **运行态基底**（2026-10-14 重构，`PLN-REFACTOR-1` Task 2）：两域运行态记录的**公共字段**。
 *
 * **命名判据（MUST 读，这不是随手取的名字）**：`State` 在本仓是**共享层**的词根，不是"Buff 专属"——
 * 它与 Def 层**完全同构**：`FTcsStateDefBase` ← `FTcsBuffDef` / `FTcsSkillDefData`，
 * 对应运行态就是 `FTcsStateInstance` ← `FTcsBuffInstance` / `FTcsSkillInstance`。
 * **MUST NOT 取 `FTcsEntityRuntime` 一类含 `Entity` 的名字**：`Entity` 在本仓是**专指**
 * （承载 State/Buff/Skill/Attribute 的**战斗实体**，见 `FTcsCombatEntityHandle`），
 * 而本结构装的是**实体身上的运行态**，不是实体本身——用 `Entity` 命名它属**词汇二义性**
 * （同 `DEC-2026-10-14-entity-vocabulary` 所禁）。
 *
 * **为什么要有这个基底**：在它之前，`FTcsStateInstance` 把**Buff 专属语义**
 * （层数 / 时值 / 周期）与**公共字段**混在一个结构里，导致"状态运行态"这个名字下装的是 Buff 语义，
 * 而技能侧（`FTcsCastRun`）只能**另起一套**——两域各持一份语义相同的字段，机制也只能各写一遍。
 * 基底把公共的那部分提出来，两域各自只留**自己的**语义。
 *
 * **字段选择的唯一判据 = "两域是否同角色"**（**MUST NOT 按名字相似判定**，本仓已有三次踩坑先例）：
 *
 * | 字段 | 为什么在基底 |
 * |---|---|
 * | `DefTag` | 两域皆有定义身份 |
 * | `Entity` | 两侧现状逐字相同，注释都自述"必须自持，O(1) 定位" |
 * | `CascadeAnchor` | 两域同角色——"按它一次摘净"的**级联撤销锚点**（状态侧修正器与内联触发行按它挂；技能侧链挂的账本条目按它回收） |
 * | `ParamSnapshot` | 两侧现状逐字相同 |
 * | `Level` | 两侧现状逐字相同（生效瞬间冻结、运行中不追溯） |
 * | `ExpiryEntry` | 两侧各有一枚"当前计时项的取消锚"（状态侧 = 时值到期；技能侧 = 当前时段到期） |
 *
 * **MUST NOT 进本基底**（各域自己的语义，进了就重犯本基底要修的病）：
 * Buff 侧 `Handle` / `Source` / `Instigator` / `Phase` / `Stacks` / `DurationRemaining` /
 * `PeriodRemaining` / `PeriodEntry`；施法侧 `EntryHandle` / `PhaseIndex` / `ChainRunHandle` 等。
 *
 * **★ 三处"看起来像、实际不是"的邻词，MUST NOT 按名字合并**（逐条实测，2026-10-14）：
 * - **`CascadeAnchor` ≠ 域内的 `Source`**：后者是"**谁施加的**"，明文 **MUST NOT 当撤销锚点**
 *   （同一来源可施加多个不同定义 ⇒ 按它摘会误摘别条实例的条目）。`Source` **不住本基底**。
 * - **`ExpiryEntry` ≠ 技能侧的"时段游标"**：`FTcsCastRun::PhaseIndex` 是**时段推进的下标**，
 *   与本字段（计时项的取消锚）**同词根不同义**，MUST NOT 混为一谈。
 * - **本基底 MUST NOT 持生命周期态**：域内的 `Phase` 守的是"移除广播窗口内重入移除同一句柄"
 *   （`STAT-5`），而技能侧同类问题**已由统一重入守卫解决**（`TTcsEntityBroadcastGuard`）
 *   ⇒ 该字段在技能侧**零消费者**，按"零消费者不预建"留各域自己声明。
 *
 * **非反射结构体**（无 `USTRUCT` 宏）：它只在 C++ 侧流转——反射化的时机是"运行态要整体过网"
 * 的操作复制轮（`WAIT-3` / R7），不是现在。
 *
 * **持有纪律（引擎事实）**：运行态住池的 `TArray` 连续缓冲，**扩容即搬移** ⇒ 调用方 MUST NOT
 * 跨"可能触发池扩容的调用"缓存本结构指针。
 */
struct FTcsStateInstance
{
// 身份与归属
#pragma region Identity

public:
	// 定义身份（两域各自的内容身份——热路径靠注册表内的定义副本解析，不回查 Def 库）
	FGameplayTag DefTag;

	/**
	 * 宿主实体（被施加方；本运行态所属的桶）。
	 *
	 * **为什么运行态自持它**：门面按句柄取记录时（如 `GetState(Handle)`）**句柄里没有实体段**，
	 * 若无此字段就只能"遍历所有桶找这个句柄"（O(桶数)）；自持后是 O(1) 定位。
	 * 而且"这个运行态在谁身上"本就是它身份的一部分——修正器物化要用它把修正器挂到该实体的属性账本上
	 * （同一份信息，不是重复真相）。
	 */
	FTcsCombatEntityHandle Entity;

	/**
	 * **级联撤销锚点**：本运行态挂出去的外部登记项（修正器条目 / 内联触发行 / 链挂账本条目）
	 * 一律以它作 `Source` ⇒ 撤销时**按它一次摘净**。建出实例时取新值（进程内唯一、永不复用），
	 * 刷新 / 叠层**不换**（同一条实例 = 同一个锚点）。
	 *
	 * **MUST NOT 与"施加方来源"混用**：那个是"**谁施加的**"，一个来源可施加多个不同定义
	 * ⇒ 按它摘除会**误摘别的实例的条目**。该字段（域内的 `Source`）**不住本基底**。
	 */
	FTcsSourceHandle CascadeAnchor;

#pragma endregion


// 快照与等级
#pragma region Snapshot

public:
	/**
	 * 参数快照（D3-12）：**生效瞬间冻结**的数值副本（构建与重建规则见 `state-param-snapshot` 能力）。
	 *
	 * **为什么它不是"策略载体"**（不违反"运行态是纯数据"的纪律）：快照是**已求值结果的副本**，
	 * 值语义、可复制；`SourceRef` 位也只是该行数值来源的副本（供 Debug 与 Live 化），
	 * 不是"实例持策略"（策略仍归 Def 资产）。其内层若含对象引用，由门面的 `AddReferencedObjects`
	 * 统一补引用（本结构体不是 `UPROPERTY` 容器，GC 看不见）。
	 */
	FTcsParamSnapshot ParamSnapshot;

	// 生效等级（等级源求值入参——`Context.EffectiveLevel` 取它；生效瞬间冻结，运行中升级不追溯）
	int32 Level = 0;

#pragma endregion


// 时间锚
#pragma region TimeAnchor

public:
	/**
	 * **当前计时项的到期取消锚**（两域同角色，各域语义由自己解释）。
	 *
	 * - 状态 / Buff 域 = **时值到期**条目（`Finite` 时非空；`Infinite` 时恒空）；
	 * - 施法域 = **当前时段**的到期条目（每次时段推进重挂）。
	 *
	 * **撤销纪律（两域共同，MUST 守）**：一律经 `UTcsClockSubsystem::PushExpiry` / `CancelExpiry`，
	 * **MUST NOT** 直接触达 `FTcsExpiryHeap::AdvanceTo`（它是泵私有）。
	 *
	 * **★ 一条真实的 ensure 陷阱（既有实测，MUST 守）**：`CancelExpiry` 对**悬空**句柄会
	 * `ensureMsgf`（留红字）。而"到期回调正在跑"那一刻它自己的条目**仍然有效**
	 * （泵在回调**返回之后**才释放）⇒ **撤销 MUST 发生在回调入口复位该锚点之后**，
	 * 使终结路径看到的永远是"活的或空的"、**永不会是"已死的"**。
	 */
	FTcsTimeEntryHandle ExpiryEntry;

#pragma endregion
};
