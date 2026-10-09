// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Attribute/TcsAttrModDef.h"
#include "Trigger/TcsEffectTrigger.h"

#include "Def/TcsBoolSwitchRow.h"
#include "Def/TcsPhaseSpan.h"
#include "Def/TcsSkillEnums.h"
#include "Def/TcsStateDefBase.h"
#include "Skill/TcsNumericParamModifier.h"

#include "TcsSkillDefData.generated.h"



/**
 * 技能定义数据：继承状态族的身份词、等级、数值参数、描述与修正器引用行，只补施法语义。
 * 关系字段只落形状，检查器归 R5.5-e；内联触发行与查询片段的运行机制在后续任务接入。
 * 参数链行依赖 Task 4 的完整元素类型，故本批不声明；冷却与 Cost 配置归 R6.5。
 *
 * **属性捕获声明列表已整体删除（2026-10-08，用户裁定；MUST NOT 补回）**：三条判据同时成立——
 * ① **零消费者**（技能侧 `CapturedAttrs` 全仓无任何类型读取）；② **机制重叠**——设计给捕获写的用途
 * （"捕获命中 → 改 `CapturedAttrs`，随流程消失、零账本污染"）已由**流程属性黑板** `FTcsFlowAttributes`
 * （作用域 = 流程用完即弃）与 `FlowModify` 数据步骤覆盖 ⇒ 同一个"流程局部可变值空间"存在两份；
 * ③ **技能侧已被参数快照占满**——技能自己的参数由 `FTcsCastRun.ParamSnapshot` 在激活瞬间冻结
 * （`AttributeScaled` 类源在快照构建时即取值），流程内工作值由黑板承担，**剩下的空间说不出技能侧独有的
 * 业务场景**，设计语料也未给出该用例。**边界**：只涉技能侧——伤害流程侧的
 * `FTcsDamageFlowContext::CapturedAttrs` 归台账 `WAIT-7`，其触发条件**不因此改变**。
 * **连带**：删除后本模块对属性门面零依赖 ⇒ MUST NOT 新建属性访问白名单薄壳。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsSkillDefData : public FTcsStateDefBase
{
	GENERATED_BODY()

// 布尔参数表
#pragma region BoolSwitches

public:
	// 与继承的 Params 构成参数双表；本批只提供配置与作者期校验。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Bool Switches")
	TArray<FTcsBoolSwitchRow> BoolSwitches;

#pragma endregion


// 参数链行
#pragma region ParamChainRows

public:
	/**
	 * 参数链行 —— **技能自己带的"复合算式"表**。
	 *
	 * **它解决什么**：技能伤害常常不是一个写死的数，而是由别的值算出来的。典型 =
	 * **"攻击力 × 技能倍率"**。这两样东西分别在别处：攻击力是单位的属性、倍率是本技能的一个参数。
	 * 想表达这个算式，就得有一张能写"拿谁、怎么算"的表 —— 就是本字段。
	 *
	 * **怎么填**（一行 = 一步运算，可以写多行，**书写顺序无关**，引擎按运算带自动排）：
	 *
	 * | 想要的效果 | `Op`（运算） | `Operand`（值从哪来） |
	 * |---|---|---|
	 * | 攻击力 × 倍率 | 第 1 行 `乘` | 技能参数 `DamageRate`（即本技能参数表里那一行的键） |
	 * | 攻击力 × 倍率（含攻击力本身） | 第 1 行 `加` + 第 2 行 `乘` | 属性换算(攻击力) → 参数引用(`DamageRate`) |
	 * | 固定加 20 点 | `加` | 字面量 20 |
	 * | 高级：让某个天赋能覆盖它 | `覆盖` | 字面量（配合「覆盖优先级」列，大者胜） |
	 *
	 * **五个运算带**（与属性修正器**同一套**，故两边直觉一致）：`加`（先累加）/ `百分比加` /
	 * `乘`（后连乘）/ `平加`（最后加、不受前面的百分比与乘法影响）/ `覆盖`（整个算式被它替换掉）。
	 * 合成顺序固定为 `((初值 + Σ加) × (1 + Σ百分比加)) × Π乘 + Σ平加`，所以**先写乘法还是先写加法，结果都一样**。
	 *
	 * **值从哪来**（`Operand` 的四个常用来源）：
	 * `字面量` = 直接写个数；`参数引用` = 取**本技能参数表**里另一个键的值（键必须是本技能已配的参数）；
	 * `属性换算` = 取单位的某个属性当前值再乘一个系数；`等级表` = 按当前等级取档。
	 *
	 * **什么时候算**：**施法开始那一瞬算一次**（术语叫"物化"），算完就固定了 —— 本次施法期间
	 * 中途被上 debuff 也不会改。放第二次技能会**重新算一遍**，且**第一次的痕迹会自动清干净**，
	 * 不会两次叠加。
	 *
	 * **特殊键 `Level`**：`ParamKey` 填 `Level` 时语义不是普通参数，而是**改这个技能的生效等级**
	 * （"技能每 +1 级伤害多一档"这类需求用它），该键由引擎内置、策划无需声明。
	 *
	 * **注意**：本表**只作用于本技能自己**（不会影响别人的技能）；想"给别的技能挂修正"是另一条路
	 * （宿主外部施加），两者互不替代。
	 *
	 * ---
	 *
	 * **以下为实现与边界（策划可略）**：
	 *
	 * 定义自带参数修正行（PV-9）：**声明作用域恒为本条目自身**的修正器行——
	 * 激活期物化进该条目账本，`Source` = 本次施法运行句柄，随施法终结级联摘除
	 * （与继承的 `ModifierRows` 同生命周期语义）。
	 *
	 * **无 `FTcsSkillEntrySelector`**：该选择器保持专属"外部施加"场景——同一件事不许两个入口。
	 *
	 * **只落内联形态**：字段的元素是内联的 `FTcsNumericParamModifier`，**不含**任何
	 * `UTcsSkillModDef` / `TemplateTag` 引用——实测 `UTcsSkillModDef` 在全仓 `Source/` **零命中**
	 * （"引用目标"没有载体）⇒ 模板引用路径整体归 `R6.5-f`。
	 *
	 * **当前能力边界（如实，MUST NOT 被读成"任意算式都能跑"）**：`属性换算` 这一来源目前
	 * **全仓无实现者**（它要求宿主提供 `ITcsAttributeProvider`，今天为零）⇒ 配了它只会落兜底值；
	 * 本轮已实测可用的是 `字面量` / `参数引用` / `等级表` 三条。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Param Chain")
	TArray<FTcsNumericParamModifier> ParamChainRows;

#pragma endregion


// 施法时段
#pragma region Phases

public:
	// 任意段数，瞬发必须使用空表；时段时长在激活时快照化。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Phases")
	TArray<FTcsPhaseSpan> Phases;

#pragma endregion


// 施法状态查询
#pragma region CastQuery

public:
	// 查询选择：定义开关、当前时段或宿主片段。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Cast Query")
	ECastQueryMode CastQueryMode = ECastQueryMode::CQM_DefSwitches;

	// 定义开关档的可打断值（宿主可覆盖）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Cast Query")
	bool bInterruptibleDefault = false;

	// 定义开关档的可移动值（宿主可覆盖）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Cast Query")
	bool bCanMoveDefault = false;

	// 自定义查询载荷：契约实现归后续任务，本批不预建策略基类。
	UPROPERTY(EditAnywhere, Category = "Skill Def|Cast Query")
	FInstancedStruct CastQueryFragment;

#pragma endregion


// 主效果链
#pragma region MainChain

public:
	// 主效果链身份，由效果定义库解析。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Main Chain")
	FGameplayTag CastChainId;

	// 主效果链启动时点（默认在施法开始）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Main Chain")
	EMainChainStart MainChainStart = EMainChainStart::MCS_OnCastStarted;

	// 进入指定时段档所引用的时段标识。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Main Chain")
	FGameplayTag MainChainStartPhaseTag;

#pragma endregion


// 实例化
#pragma region Instancing

public:
	// 施法运行态的实例化方式。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Instancing")
	ECastInstancing Instancing = ECastInstancing::CI_InstancePerExecution;

	/**
	 * 在飞顶替位（仅 `CI_InstancePerEntity` 有意义）：false（默认）⇒ 在飞时**驳回**；true ⇒ **顶替**。
	 *
	 * **默认值 `false` 是判据不是偏好**：GAS 的对应位（`bRetriggerInstancedAbility`）无初值即 `false`，
	 * 且本仓"框架零默认"纪律下，默认档 MUST 取**行为最保守**的一档（驳回 > 顶替）——
	 * 顶替会终止一个正在进行的施法，是更强的副作用。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Instancing")
	bool bRetriggerOnActive = false;

#pragma endregion


// 关系
#pragma region Relations

public:
	// 互斥状态词列表（检查器归 R5.5-e）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Relations")
	TArray<FGameplayTag> Blocks;

	// 必须存在的状态词列表（检查器归 R5.5-e）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Relations")
	TArray<FGameplayTag> Requires;

	// 顶替优先级，与状态族同形（检查器归 R5.5-e）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Relations")
	int32 Priority = 0;

	// 激活时取消的状态词列表（检查器归 R5.5-e）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Relations")
	TArray<FGameplayTag> Cancels;

#pragma endregion


// 内联触发行
#pragma region Triggers

public:
	// 本技能的内联事件映射行；本批零内置运行期消费者。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Def|Triggers")
	TArray<FTcsEffectTriggerDef> Triggers;

#pragma endregion
};
