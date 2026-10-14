// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Clock/TcsExpiryHeap.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "State/TcsBuffEnums.h"
#include "State/TcsBuffInstanceHandle.h"
#include "State/TcsStateInstance.h"



/**
 * **Buff 运行态记录**（2026-10-14 重构，`PLN-REFACTOR-1` Task 2）：**基底 + Buff 专属字段**。
 *
 * **★ 与旧同名结构的关系（MUST 读，防误读）**：重构前 `FTcsStateInstance` 是**一个**装全部 14 字段的结构；
 * 现在拆成两个——**公共 6 字段原地留下、名字不变**（`FTcsStateInstance`，见 `TcsStateInstance.h`），
 * **Buff 专属的 8 字段（4 域内 + 4 语义）搬进本类型**。⇒ `FTcsStateInstance` **并未消失**，
 * 它只是**从"装全部"变成了"只装公共部分"**。凡**作桶载荷**处一律用**本类型**（见 `TcsStateRegistry.h` 的绑定）。
 *
 * ```
 * FTcsStateInstance（基底，两域公共）  DefTag / Entity / CascadeAnchor / ParamSnapshot / Level / ExpiryEntry
 *        ↑ 继承
 * FTcsBuffInstance（本类型）           + Handle / Source / Instigator / Phase（Buff 域内字段）
 *                                      + Stacks / DurationRemaining / PeriodRemaining / PeriodEntry（Buff 专属语义）
 * ```
 *
 * **★ 为什么用继承、且 MUST NOT 再单开一个 payload 类型**（2026-10-14 用户裁定）：
 * 既然已选继承，把 Buff 字段**再包一层**（`X.Payload.Stacks`）只多一层间接、不增加任何隔离——
 * 共享机制本来就只 include 基底、**看不见**派生类型的字段，隔离已经由继承本身给足了。
 * ⇒ 字段**直接平铺**在本结构里。**MUST NOT 事后补一个 payload 壳**。
 *
 * **★ 四个"域内字段"为什么留在这里、不上提基底**（逐条实测依据，MUST NOT 按名字相似上提）：
 *
 * | 字段 | 为什么不进基底 |
 * |---|---|
 * | `Handle` | **类型不同**：基底是**一个** struct、只能有一个 `Handle` 字段类型，而状态侧是 `FTcsBuffInstanceHandle`、技能侧是 `FTcsSkillEntryHandle` ⇒ 上提必然**抹掉一侧的编译期类型隔离**（`instance-handle-pool` 立了两条"跨池误用必须编译失败"，同裁决 §8） |
 * | `Source` | **与基底的 `CascadeAnchor` 角色相反**：它是"**谁施加的**"、明文 **MUST NOT 当撤销锚点**；技能侧**没有**这个角色（技能侧只有一个句柄，且就是锚点） |
 * | `Instigator` | 状态侧是**存储字段**（可与 `Entity` 不同，有 4 个真实读取点：等级源入参 / 事件载荷 / 叠层更新 / 修正器物化）；技能侧**零存储消费者**（其求值上下文里的 `Instigator` 是**派生值**，不是存储字段）⇒ 上提会造出技能侧的恒空字段 |
 * | `Phase` | 它守的是"**移除广播窗口内重入移除同一句柄**"（`STAT-5`）；技能侧同类问题**已由统一重入守卫解决**（`TTcsEntityBroadcastGuard`）⇒ 技能侧**零消费者**，上提即造死字段 |
 *
 * **两个来源句柄的分工 MUST 守死**（2026-10-05 解耦，R5 Task 6 裁定 1）：
 * - `Source` = **施加方来源句柄**（"谁施加的"）：调用方声明时沿用、未声明时由门面发号（每次一枚新号）。
 *   它承担共存决策的"续杯 / 叠层"判据与事件载荷的归属读数；**MUST NOT 当撤销锚点**——一个来源可以
 *   施加多个不同定义（一条技能给同一目标挂两个 buff），按它撤销会把别条实例的条目一起摘掉；
 * - `CascadeAnchor`（**基底**）= **级联撤销锚点**：建出实例时经 `FTcsSourceHandleRegistry::Allocate()`
 *   取新值（进程内唯一、永不复用），刷新 / 叠层不换——触发行退订与属性修正器摘除都按它一次清干净。
 *
 * **MUST NOT 持策略 / 事件载荷 / 订阅句柄 / `UObject` 引用**（D3-7 v2 与 D2-9 纪律）：
 * 策略归 Def 资产持有、事件载荷归事件、订阅句柄归注册表/总线。这三样一旦进实例，
 * 池化与将来的**操作复制**（状态操作流按值走网络）立即失效——这是该纪律的真实代价来源，不是洁癖。
 *
 * **非反射结构体**（无 `USTRUCT` 宏）：它是**桶内池化纯数据**，本轮零过网与零脚本消费面
 * （同基底的处置与理由）。
 *
 * **持有纪律（引擎事实）**：住池的 `TArray` 连续缓冲，**扩容即搬移** ⇒ 调用方 MUST NOT 跨
 * "可能触发池扩容的调用"缓存本结构指针。
 */
struct FTcsBuffInstance : public FTcsStateInstance
{
// Buff 域内字段
#pragma region DomainFields

public:
	// 桶内句柄（本实例的身份；`Index` / `Generation` 的语义由所属桶定义）
	FTcsBuffInstanceHandle Handle;

	// 施加方来源句柄（"谁施加的"——续杯 / 叠层判据 + 事件载荷；非 0；**不是**撤销锚点，见类注释）
	FTcsSourceHandle Source;

	// 发起者实体（"谁发起的"——可与被施加方不同；消费者是等级源 / 事件载荷 / 修正器物化）
	FTcsCombatEntityHandle Instigator;

	// 实例阶段（在册稳态 = `EBP_Active`；`EBP_Expiring` 是移除广播窗口的判据载体，见 `STAT-5`）
	EBuffPhase Phase = EBuffPhase::EBP_Active;

#pragma endregion


// Buff 专属语义
#pragma region StacksAndTime

public:
	/**
	 * 当前层数（从 1 起）。
	 *
	 * **层模型 = "层数记在一条实例内、组内至多一条"**（`state-stacking-policies` 的核心口径）：
	 * 同组再次施加时**不新建实例**，而是刷新或叠层（判据见五轴 `StackPolicy`）。
	 * **★ 该模型是否改为"层 = 独立实例"属独立议题**（台账 `STAT-7`）：真实内容需求
	 * （"多枚减益共享同一层数上限"）尚未出现，按「零消费者不预建」**本轮不动**
	 * （判据**不是**"改起来贵"——成本不进决策判据）。
	 */
	int32 Stacks = 1;

	// 剩余时长（秒；`EBDP_Infinite` 时不使用——无到期条目）
	double DurationRemaining = 0.0;

	// 距下个周期的剩余时间（秒；值每次周期回调后重置为满额）
	double PeriodRemaining = 0.0;

	/**
	 * **周期**到期条目锚点（取消锚；`Period > 0` 时非空）。
	 *
	 * **为什么与时值条目（基底的 `ExpiryEntry`）分开两个字段**：`Finite` 与 `Period > 0`
	 * 是**可同时成立的两个语义**（标准 DoT = 有限时值 + 周期跳伤），挤在一个锚点里会让
	 * "撤销旧条目"分不清撤的是哪一条（`ExtendDuration` 只该动时值条目，`PeriodRefresh`
	 * 只该动周期条目）。
	 *
	 * **★ 这段分离理由 MUST 逐字保留**——它是一条**真实的引擎语义约束**，不是排版偏好。
	 */
	FTcsTimeEntryHandle PeriodEntry;

#pragma endregion
};
