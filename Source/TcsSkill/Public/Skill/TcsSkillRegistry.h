// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "Runtime/TcsEntityRuntimeBucket.h"
#include "Skill/TcsSkillInstanceHandle.h"
#include "Skill/TcsCooldownState.h"
#include "Skill/TcsNumericParamModifier.h"
#include "Skill/TcsSkillEntryHandle.h"



/**
 * 已学技能条目（D5-13 / `SPEC-04-skill` §3.1 的 `FLearnedSkillEntry`）：**池化纯数据**——
 * 一个实体的一条"会这个技能"的全部持久状态。
 *
 * **非反射结构体**（无 `USTRUCT` 宏）：本记录只在 C++ 侧流转——本轮**零过网需求、零脚本消费面**。
 * 同款先例 = `FTcsStateInstance`（非反射，反射化留给"实例要整体过网"的轮次）。
 * **脚本 / 蓝图侧的读写面另属台账 `SCRIPT-10`**（触发条件型：出现"脚本要读技能等级 / 学习来源 /
 * 在飞 run"的真实消费者时再定形态）——**MUST NOT** 以"将来可能要"为由现在就反射化
 * （那会得到"有类型、无门面可调"的空承诺，且违反"零消费者不预建"纪律）。
 *
 * **字段边界（MUST 守死）**：本结构体**只**持"持久等级 + 学习来源 + 在飞施法句柄 + 冷却轨道状态 +
 * 参数修正器条目"五样——判据恒为**声明点不早于类型存在点**：`TArray<T>` 成员要求 `T` 至少可见，
 * 元素类型不存在时写进来就是**未声明标识符**（硬编译错误）。
 *
 * **两处字段的回补记录（MUST 保留，防后人误读为"当初写漏了"）**：
 * - `NumericParamModInstances`——R6 Task 2 时 `FTcsNumericParamModifier` 尚未落地，故当轮刻意不声明；
 *   参数链轮落地后回补。参数读取面（`GetNumericParam` / `IsSwitchSet` / `GetLevel`）取
 *   "**带 `Level` 键的修正器列表输入**"（形参传入），**MUST NOT** 依赖本结构体持该字段。
 * - `CooldownTracks`——R6 Task 2 **原文即写"MUST NOT 声明冷却轨道状态字段（归 `R6.5-a`）"**，
 *   理由是"其元素类型此刻全仓不存在"。R6.5-1 Task 0 落地 `FTcsCooldownTrackState`、
 *   Task 1 落地其消费者（`TcsCooldownOps`）⇒ **类型与消费者同时到位，回补成立**
 *   （与上一条同款：不是"当初漏了"，是"当初不能写"）。
 */
struct FTcsLearnedSkillEntry
{
// 身份与归属
#pragma region Identity

public:
	/**
	 * 内容身份（**权威**）。
	 *
	 * `EffectiveDefId` 概念已于 D5-9 整体移除——"这个条目用哪份定义"的唯一答案是本字段；
	 * `GetDef()` 走登记表的**解析缓存**（const + 版本校验），热路径零回查定义库。
	 */
	FGameplayTag DefTag;

	// 桶内句柄（本条目的身份；`Index` / `Generation` 的语义由所属桶定义）
	FTcsSkillEntryHandle Handle;

	/**
	 * 宿主实体（学会该技能的实体；本条目所属的桶）。
	 *
	 * **为什么条目自持它**：门面按句柄取条目时句柄里没有实体段，若无此字段就只能"遍历所有桶找这个句柄"
	 * （O(桶数)）；自持后是 O(1) 定位。而且"这个技能在谁身上"本就是条目身份的一部分
	 * （同 `FTcsStateInstance::Entity` 的理由，同一份信息不是重复真相）。
	 */
	FTcsCombatEntityHandle Entity;

	/**
	 * 学习来源（"谁给的"——级联撤销锚点）。
	 *
	 * **只有一个来源句柄，与状态侧刻意不同**：状态实例持 `Source`（施加方来源）+ `CascadeAnchor`
	 * （级联撤销锚点）两个句柄，因为它的修正器与内联触发行要按锚点一次摘净；**账本条目不挂修正器、
	 * 不挂触发行**（`ParamChain` 归参数链轮、行为归状态族）⇒ 无独立级联锚点需求。
	 * **MUST NOT 照抄状态侧的两个句柄**——那会造出一个零消费者的字段。
	 */
	FTcsSourceHandle LearnSource;

#pragma endregion


// 级别
#pragma region Level

public:
	// 持久等级（**激活时快照进施法运行态**——"运行中升级不追溯"；等级来源归宿主业务侧）
	int32 Level = 0;

#pragma endregion


// 在飞施法
#pragma region SkillInstances

public:
	/**
	 * 该条目当前在飞的施法运行句柄集。
	 *
	 * **本轮恒为空数组**（如实登记）：写入者是激活路径（建 run 后回填），归后续任务。
	 * 本字段此刻存在的理由是**数组元素类型必须完整**——它是 `FTcsSkillInstanceHandle` 在本任务落地的唯一原因。
	 */
	TArray<FTcsSkillInstanceHandle> RunHandles;

#pragma endregion


// 冷却轨道状态
#pragma region CooldownTracks

public:
	/**
	 * 该条目当前的冷却轨道状态（**与 Def 侧 `FTcsCooldownPolicy::Tracks` 一一对应、同序**）。
	 *
	 * **冷却的真值住这里**（`D5-15`："真值在 Entry，UI 轮询"）——不住 run 上。
	 * 判据：冷却是**条目级**的持久状态（"这个技能还要等多久"跨施法存在），而 run 是**一次施法**的
	 * 运行态、随终结归还池槽位 ⇒ 冷却若挂 run 上会随施法结束一起消失。
	 *
	 * **同序是硬契约**：`Tracks[i]` 的状态对应 `Policy.Tracks[i]` 的配置——查询口按**下标**寻址
	 * （`GetCooldownRemaining(..., TrackIndex)`）就靠这条。**Def 改配置后旧状态的下标会错位**
	 * （宿主热改技能定义），处置 = 按 `Policy.Tracks` 重新对齐（见 `TcsCooldownOps` 的起冷路径）。
	 *
	 * **空数组 = 该条目当前没有任何冷却在跑**；它与"该技能配了空 Tracks（永远可放）"是两件事
	 * ——前者是**运行态此刻为空**，后者是**定义上不存在轨道**。**MUST NOT 用"数组是否为空"反推**
	 * "这个技能有没有冷却"，那要读 Def。
	 */
	TArray<FTcsCooldownTrackState> CooldownTracks;

#pragma endregion


// 参数账本
#pragma region NumericParamModInstances

public:
	/**
	 * 参数修正器条目集（D5-19 参数账本；形如 M2 `FTcsAttributeInstance::AttrModInstances`
	 * 的"每个条目各持一套"）——它是**激活期物化 / 外部施加 / 按来源级联摘除**三者的共同落点。
	 *
	 * **命名规则（2026-10-09 立，与 M2 同一条）**：容器字段名 = **元素类型名的复数形式**
	 * （`FTcsNumericParamModInstance` → `NumericParamModInstances`）。**`Slot` 一词保留给
	 * "可按下标寻址、可复用、带代际的空位"**——本容器不满足该形状（只有 `.Add` / `.RemoveAll` /
	 * `.Num()` / 范围遍历）。本条原为 `ParamSlots`。
	 *
	 * **元素类型 = 账本侧 `FTcsNumericParamModInstance`（纯 C++、无对象引用）**：
	 * 本结构体住桶内的池 ⇒ GC 看不见它，故**若**这里换成定义侧的 `FTcsNumericParamModifier`
	 * （`Operand` 是可持 `TScriptInterface` 的 `FTcsParamValue`），那些对象会被**静默回收**
	 * （症状 = 参数链里的源变成空引用，不崩溃）。取账本侧形状后，本条**依然**满足下方 GC 纪律
	 * （账本 MUST NOT 持 `UObject` 引用），门面 `AddReferencedObjects` **MUST NOT** 为此新增遍历。
	 *
	 * **来源成对性**：凡写入本数组的条目 MUST 带 `Source`，且该 `Source` MUST 能被
	 * `FTcsParamChainOps::RemoveParamModifiersBySource` 摘除（判据 = 不存在"摘不掉的条目"）。
	 */
	TArray<FTcsNumericParamModInstance> NumericParamModInstances;

#pragma endregion
};



/**
 * 技能账本层的 per-entity 桶（`FSkillBucket`）——**共享层模板的实例化**（2026-10-14 起）。
 *
 * **本文件不再是"实现"，只是"绑定"**：手写的槽位 / 代际 / 空闲栈 / 代际校验 / 稳定序遍历
 * 已于 2026-10-14 全部收敛到共享层的 `TTcsEntityRuntimeBucket`（见 `Runtime/TcsEntityRuntimeBucket.h`）。
 *
 * **桶载体 = `TTcsInstancePool`，且注入进程级发号器**（`FTcsStateGenerationIssuer`）：
 * 桶是 per-entity 的而句柄不带实体段 ⇒ 各桶各从代际 1 起发会让跨桶定位**命中错实体**
 * （本仓 2026-10-05 实测过的缺陷）⇒ **MUST NOT** 改回池内自增。
 *
 * **"按内容身份找条目"改由通用谓词口表达**（原 `FindByDefTag`）：共享层 MUST NOT 认识
 * 域词汇（`DefTag` 是技能域的词）⇒ 域侧用 `FindByPredicate` 传入自己的判据，
 * 既保住稳定序，又不引入第二个遍历实现。
 */
using FSkillBucket = TTcsEntityRuntimeBucket<FTcsLearnedSkillEntry, FTcsSkillEntryHandle>;



/**
 * per-entity 技能账本注册表：`实体 → 桶` 的登记表，持全部在册条目。
 *
 * **脏句柄一律拒绝（不 `ensure`）**：代际失配 / 下标越界 / 实体未注册都是**时序竞态**语义
 * （调用方手上的句柄过期了），不是配置错误——与"非法配置"的 `ensure` 口径刻意分开。
 * 拒绝 MUST NOT 影响同桶其它条目。
 */
using FTcsSkillRegistry = FTcsEntityRuntimeRegistry<FTcsLearnedSkillEntry, FTcsSkillEntryHandle>;
