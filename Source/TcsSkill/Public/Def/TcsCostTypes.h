// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Parameter/TcsParamValue.h"

#include "TcsCostTypes.generated.h"



/**
 * 消耗策略 —— "**这个技能消耗什么**"（`D5-10` 的 `FCostConfig.Policy`）。
 *
 * 命名沿 R6 已立的短前缀惯例（`ECastInstancing` / `ECastQueryMode` / `EMainChainStart` 三处
 * 全部带前缀）⇒ `ECost*` 族的首词即域。
 */
UENUM(BlueprintType)
enum class ETcsCostPolicy : uint8
{
	ECP_None = 0			UMETA(DisplayName = "不消耗", ToolTip = "恒可付——正确声明'不消耗'，零日志（默认档）"),

	ECP_ResourceAttr = 1	UMETA(DisplayName = "属性资源", ToolTip = "走 M2 属性账本：按 ResourceAttr 键读基值并扣减 Amount"),

	ECP_CustomFragment = 2	UMETA(DisplayName = "自定义片段", ToolTip = "⚠ 本轮未实现：记 Warning 并按'不消耗'降级（阻塞 = 扩展点形状未裁，见台账 COST-3）"),
};



/**
 * 消耗时机 —— "**什么时候付**"（`D5-10` 的 Cost Timing）。
 *
 * **★ 三档（2026-10-12 用户裁定：`OnEvent` 档整体删除，MUST NOT 补回）**：
 * "由某个**特定事件**触发的消耗"属于**宿主项目自己的玩法逻辑**，不是框架该内置的档位——
 * 判据：框架只能认识"施法这个事务自身的边界"（开始 / 完成），而"某个特定事件"是
 * **项目词汇**（挨打时付费、连击到第 3 段付费、血低于 30% 付费……），
 * 内置等于让框架去猜项目的玩法。**宿主实现成本极低**：监听自己的事件 → 调 `PayCost`
 * （那正是 `ECT_Manual` 档 + 公开口 `PayCost` 的组合，**不需要新的框架面**）。
 *
 * **同批删除的伴随面（MUST 一起删，否则留下悬空契约）**：
 * ① `FTcsCostConfig::TriggerEventTag` 字段；② `UTcsCostEventRelay` 整个类（它**只**为该档服务）；
 * ③ 门面的 `GetCostEventRelay()` 与 `CostEventRelay` 成员；④ 三处撤销路径的中继退订。
 * **判据 = 本仓"零消费者不预建"**：删掉档位后，中继的消费者数 = 0 ⇒ MUST NOT 留着。
 *
 * **本枚举刻意没有 `OnCastInterrupted`**（2026-10-11 裁定，MUST NOT 补回）——两条硬判据：
 *
 * ① `D5-4` 已把 `bRefundCostOnInterrupt` 砍除，理由原文是"**返还是政策不是引擎语义**"
 *    ⇒ 框架层面"打断 ⇒ 退钱"这条路已被明确关闭；
 * ② 于是"打断时扣"这一档**没有任何独立含义**：说"施法开始时扣" ⇒ 被打断**也扣了**
 *    （那就是 `ECT_OnCastStarted`）；说"打断时扣" ⇒ 施法**成功完成就不扣**——**荒谬**。
 *
 * ⇒ 本枚举与 `ETcsCooldownTiming` **各有自己的档位集**，MUST NOT 共用、MUST NOT 用一个"覆盖"另一个。
 *
 * **枚举值上方 MUST NOT 放 `//` 行注释**（UHT 会转成 `ToolTip` 并与显式 `UMETA(ToolTip=...)` 冲突，
 * 直接编译失败——本仓实测）。语义一律写进 `UMETA`。
 */
UENUM(BlueprintType)
enum class ETcsCostTiming : uint8
{
	ECT_OnCastStarted = 0	UMETA(DisplayName = "施法开始", ToolTip = "激活事务内提交：门禁⑤通过后、建施法运行态之前（默认档）"),

	ECT_OnCastCompleted = 1	UMETA(DisplayName = "施法完成", ToolTip = "施法走完全部时段、或瞬发当刻完成时提交"),

	ECT_Manual = 2			UMETA(DisplayName = "手动", ToolTip = "框架不自动提交，由宿主在自己的逻辑处调 PayCost（含'某个特定事件触发'这类项目玩法）"),
};



/**
 * 消耗配置 —— 一个技能的完整消耗定义（`D5-10` 的 `FCostConfig`）。
 *
 * **与冷却策略的关键差别（MUST 读，防按对称性误改）**：冷却侧的"无冷却"由**空数组**表达
 * （`FTcsCooldownPolicy::Tracks`），而消耗侧的"不消耗"由**本结构体的 `Policy` 枚举**表达。
 * 判据：冷却的形状天然是"零到多条轨道"（数组），消耗的形状是"零或一种资源扣减"（标量 + 键），
 * 后者没有可枚举的多个元素 ⇒ 只能用档位名表达"没有"。**这是一处刻意的不对称，不是遗漏。**
 *
 * **本轮只支持单资源**（`SPEC-04-skill` §7 非目标明文："v1 单资源，多资源 = 未来新原语"）。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsCostConfig
{
	GENERATED_BODY()

// 策略
#pragma region Policy

public:
	// 消耗什么（仅 ECP_ResourceAttr 档生效；ECP_None = 不消耗）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost Config")
	ETcsCostPolicy Policy = ETcsCostPolicy::ECP_None;

#pragma endregion


// 时机
#pragma region Timing

public:
	// 什么时候付（仅 Policy 非 None 时有意义；见 ETcsCostTiming 的四档判据）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost Config")
	ETcsCostTiming Timing = ETcsCostTiming::ECT_OnCastStarted;

#pragma endregion


// 资源
#pragma region Resource

public:
	/**
	 * 消耗哪个属性（**仅 `ECP_ResourceAttr` 档有效**；取值是**宿主本体论**——
	 * "蓝条 / 怒气 / 弹药"都是项目词，故框架只给空槽位、不填默认值）。
	 *
	 * **无效 tag 的处置 = 恒不可付 + `Warning`（fail-closed）**，MUST NOT 静默按"不消耗"放过。
	 * 判据 = 本仓既有惯例（`TcsFlowStepConditions.h:56` / `TcsStepModifyFlow.cpp:36` /
	 * `TcsEffectAttributeAccess.h:72` 三处措辞几乎逐字相同）："**配置写错**"与"**正确声明不消耗**"
	 * 是两件事，混成一类会让前者**表现成一切正常**、日志还清白。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost Config")
	FGameplayTag ResourceAttr;

	/**
	 * 消耗数量的**基数**（仅 `ECP_ResourceAttr` 档有效）。
	 *
	 * **★ 2026-10-12 用户裁定：由静态 `double` 换型为 `FTcsParamValue`（参数源载体）**——
	 * 判据 = **一条 `double` 同时承担了三件事，而三者都需要"可配置"**：
	 * ① **按等级配数值**（"1 级 10 蓝 / 2 级 15 蓝"）——靠等级表参数源
	 *   （`FTcsParamSource_StateLevelArray` / `_StateLevelMap`，它们读**基类**的
	 *   `Context.EffectiveLevel`，而技能侧三处求值点都已填该字段 ⇒ 直接可用）；
	 * ② **按来源配数值**（`HostDelegate` / `ParamRef` 等既有源）；
	 * ③ **减免**——靠**契约键** `Tag_TcsSkillParam_CostAmount` 上的修正器折叠
	 *   （**初值 = 本字段的求值结果**，见 `FTcsCostOps::ResolveEffectiveCost`）。
	 *
	 * **为什么减免不直接挂在本字段上、而要经一个契约键**（MUST 读，防误改）：
	 * 修正器住在**键**上（`FTcsNumericParamModInstance::ParamKey`），而本字段是**字段**不是键。
	 * ⇒ 修正是"往某个键挂一条 `{Op, Value}`"，再由折叠把它作用到该键的初值上。
	 * **这同时是"两种减免形态"的落点**：`TAO_PercentAdd` = 百分比减免、
	 * `TAO_FlatAdd` = 固定值减免（见 `Tag_TcsSkillParam_CostAmount` 的注释）。
	 *
	 * **取值的求值时点**：**门禁⑤ 与每个提交点各自求值一次**——
	 * MUST NOT 只求一次就跨"门禁 → 提交"复用（那会让"门禁判的数"与"真扣的数"漂移，
	 * 正是本仓 TOCTOU 防护要防的形态）。
	 *
	 * **取值边界（三条，与换型前同义，只是数字改为"求值结果"）**：
	 * - `0` = 正确声明"消耗 0 点" ⇒ 恒可付、**零日志**（不是错误）；
	 * - `< 0` = **恒不可付 + `Warning`**。判据：扣减式是 `Base - Amount`，负值会让技能
	 *   **倒给**资源（`Base - (-50)` = `Base + 50`）——这是**可被当作漏洞利用**的形态，
	 *   不是"配置不生效"；
	 * - `> 0` = 正常判定 `基值 >= Amount`，**且**该单位 MUST 持有该属性。
	 *
	 * **命名（为何带 `Amount` 而非就叫 `Value`）**：同族参数源字段一律以**它语义上的角色**命名
	 * （`FTcsPhaseSpan::Duration` / `FTcsCooldownTrack::Duration`），保留 `Amount` 让
	 * "这个载体的语义是消耗量"在字段名上仍然可读。
	 *
	 * **默认值**：`FTcsParamValue` 的默认构造是 **`Literal(0)`** ⇒ 默认 = 消耗 0 点 = 不消耗
	 * （与换型前的 `double Amount = 0.0` **逐字同义**，故已存资产的语义不变；
	 * 且全仓 `.uasset` 扫 `Cost` 的 `Amount` **零命中**，无存量资产需迁移）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost Config")
	FTcsParamValue Amount;

#pragma endregion
};
