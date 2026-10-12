// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"



/**
 * 技能域参数表的**框架契约键**（冷却缩减根）。
 *
 * **为什么本键住 `TcsSkill`**（判据三条，与 `Tag_TcsStateParam_Level` 是**同一判据的相反结论**
 * ——该对照 MUST 保留，防后人误以为两处不一致）：
 *
 * ① **语义归属**：CDR 的公式（"**作用于各轨 `Duration`**"）写在 `D5-15`、由**技能冷却域**解释；
 *    状态侧**无此概念**——状态实例没有"冷却"这件事，它的时长语义是"持续多久"（另一种东西）。
 *    ⇒ `CooldownPct` 的本体论在 M5；
 * ② **依赖方向（决定性，与 `Level` 恰好相反）**：`TcsSkill` 的 `Build.cs` 含 `TcsState`
 *    （**已依赖**），而 `TcsState` **不依赖** `TcsSkill` ⇒ 声明在 TcsSkill 时**零成环风险**。
 *    `Level` 反过来（M5 没有自己的等级定义层，只是复用状态侧语义）故住 `TcsState`。
 *    **同一个词被多域消费是常规情形**（M2/M5/状态三域共用同一套五带即先例），
 *    声明方 MUST 取**语义所有者 + 依赖上游**的那一侧；
 * ③ **声明方 = "谁解释那个词"**：宿主只是"往本键上挂修正器"（`ModifierRows` / `ApplyParamModifiers`
 *    都是在宿主手上），它**不解释**这个键——缩减公式由技能冷却域解释。
 *
 * **为什么 tag 文本仍在 `TcsStateParam` 根下**（与常量名住 `TcsSkill` 不矛盾）：
 * 本键是**技能参数表的键**，而技能 Def 的数值参数表**继承**自 `FTcsStateDefBase::Params`
 * （`Def/TcsStateDefBase.h:70`）⇒ 键的词根由**参数表本身**决定（宿主在同一个词根下声明自己的
 * 内容参数），而**声明方**由判据 ①②③ 决定。两者是**两个独立问题**——`gameplay-tag-governance`
 * 的「同一根共享」正是为这种"契约词 + 内容词并存"准备的。
 *
 * **导出宏（跨模块实证）**：`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**
 * （`NativeGameplayTags.h:31`，无 `__declspec(dllexport)`）⇒ 其它模块引用本变量会**链接失败**
 * （`LNK2001`）。本键的设计意图正是"供宿主与装置引用" ⇒ MUST 带模块导出宏。
 */
extern TCSSKILL_API FNativeGameplayTag Tag_TcsSkillParam_CooldownPct;

/**
 * 技能域参数表的**框架契约键**（消耗修正根）。
 *
 * **它是什么**：`FTcsCostConfig::Amount` 的**修正键**——本键上的修正经**五带折叠**
 * 作用于该字段的求值结果（**初值 = `Amount` 自己的求值**，不是 0）。
 *
 * **为什么需要它（2026-10-12 用户裁定）**：`Amount` 换成 `FTcsParamValue` 只解决了
 * "**基数**怎么来"（按等级配、按来源配）；而"**减免**"（"特定条件下只付一半 / 少付 10"）
 * 需要一个**能挂修正器的键**——因为修正是挂在**键**上的（`NumericParamModInstances.ParamKey`）。
 *
 * **两种减免各占一带（`FoldTcsAttributeBands` 的既有算术，不新增机制）**：
 * - **百分比减免** ⇒ `TAO_PercentAdd`（`(基数) × (1 + ΣPercentAdd)`）；
 * - **固定值减免** ⇒ `TAO_FlatAdd`（**全部缩放之后**的平坦加）。
 *
 * **与 `CooldownPct` 的刻意不对称（MUST 读，防按对称性误改）**：
 * `CooldownPct` 是**纯系数**（`EffectiveDuration = Duration × (1 − CDR)`），故它**不做折叠**、
 * 只按 `[0,1]` 钳制；而本键是**带式折叠的初值修正**——因为 Cost 要同时支持
 * "打折"与"减固定值"两种形态，纯系数表达不了后者。
 * ⇒ **两者形状不同是有意的**，判据 = "减免能不能用一条系数说清"。
 *
 * **一处能力边界（如实）**：`FlatAdd` 按公式排在 `PercentAdd` **之后**
 * ⇒ "先打七折再减 10"可表达，"先减 10 再打七折"不可表达。这是**与属性/伤害共用同一份算术**
 * 的必然结果（一致性优先于灵活性）——要反过来就得为 Cost 另造一套折叠，那是 `STAT-1` 明令禁止的。
 *
 * **为什么住 `TcsSkill`**：判据同 `CooldownPct`（语义所有者 + 依赖上游的那一侧）——
 * 消耗的语义由技能域解释，且 `TcsSkill` 已依赖 `TcsState` 而反向不成立。
 */
extern TCSSKILL_API FNativeGameplayTag Tag_TcsSkillParam_CostAmount;
