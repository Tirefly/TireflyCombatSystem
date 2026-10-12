// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Parameter/TcsParamValue.h"

#include "TcsCooldownTypes.generated.h"



/**
 * 冷却时机（`D5-15` 原文的 Strategy 之外的另一维："**什么时候**起冷却"）。
 *
 * **四档，且刻意没有 `OnEvent`**：冷却是"施法这个动作**自己带来**的后果"，它的起因永远在本模块
 * 手里（激活成功 / 终结 / 宿主手动）——不需要"等别人的事件"。
 * ~~**Cost 侧才需要 `OnEvent`**（见 `ETcsCostTiming` 的判据）~~ —— **★ 2026-10-12 后半句作废**：
 * Cost 侧的 `OnEvent` 档**也已整体删除**（同一日用户裁定，理由 = "某个特定事件"是**项目词汇**，
 * 框架只认识"施法事务自身的边界"）⇒ **两套 Timing 枚举今天都没有 `OnEvent` 档**。
 * 但它们**仍然各自独立**，MUST NOT 合并成一个（判据见 `PLN-R6.5-1` §0.2 裁决 ③：
 * 两枚举的**档位集本就不同**——Cost 侧无 `OnCastInterrupted`，冷却侧无 `None`）。
 *
 * **★ 改名留痕（2026-10-12，`PLN-R6.5-CC` 裁定 R12）**：本类型原名 **`ETcsCooldownTiming`**，
 * 与 Cost 侧（`ETcsSkillCostTiming`）**同批加域前缀**以保持这一对姊妹枚举的对称。
 * **只改类型名，枚举值与语义一律不动**；归档提案与历史日志里的旧字面保留作现场记录，
 * **MUST NOT 改写**（本仓既有"换根注记"纪律同款）。
 *
 * **为什么没有 `Custom` 档**（2026-10-11 用户裁定，MUST NOT 补回）：自定义时机 = "**不自动执行，
 * 让宿主在自己的确定逻辑处手动调用**"⇒ 那正是本枚举的 `ECT_Manual` 档，**不需要任何 Payload /
 * 策略片段载体**。批次表 `c` 原列的第四档 `ITcsCooldownTimingFragment` 因此**整体删除**
 * （判据：时机是"何时做"的问题，"何时"只有两种表达——能被事件名说出来，或只能由代码在具体位置
 * 说出来；**中间没有第三种形态**）。
 *
 * **枚举值上方 MUST NOT 放 `//` 行注释**：UHT 会把该注释转成 `ToolTip` 元数据，与显式
 * `UMETA(ToolTip=...)` 冲突并**直接编译失败**（本仓实测）。语义一律写进 `UMETA`。
 */
UENUM(BlueprintType)
enum class ETcsSkillCooldownTiming : uint8
{
	ECT_OnCastStarted = 0		UMETA(DisplayName = "施法开始", ToolTip = "激活成功后立即起冷却（默认档）"),

	ECT_OnCastCompleted = 1		UMETA(DisplayName = "施法完成", ToolTip = "施法走完全部时段、或瞬发当刻完成时起冷却"),

	ECT_OnCastInterrupted = 2	UMETA(DisplayName = "施法打断", ToolTip = "施法被打断或被顶替时起冷却（不消耗的施法反而进冷却）"),

	ECT_Manual = 3				UMETA(DisplayName = "手动", ToolTip = "框架不自动起冷却，由宿主在自己的逻辑处调 StartCooldown"),
};



/**
 * 单条冷却轨道 —— 一次冷却的**一条计时线**。
 *
 * **为什么是"轨道数组"而不是一个时长**：同一个技能常常要同时进多个冷却——
 * 典型 = "自己 8 秒不能再放" **且** "所有技能共享的 1 秒公共冷却（GCD）"。
 * 前者是**自身轨**，后者是**组轨**（同组的全部技能共用一条线）。两者形状完全相同，
 * 区别只在 `GroupTag` 是否有效 ⇒ 不需要两套类型（`D5-15`："Strategy 三档**溶解进轨道**"）。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsCooldownTrack
{
	GENERATED_BODY()

// 时长
#pragma region Duration

public:
	/**
	 * 本轨时长的**基数**（秒）。
	 *
	 * **★ 2026-10-12 用户裁定：由静态 `double` 换型为 `FTcsParamValue`（参数源载体）**——
	 * 判据与 Cost 侧 `FTcsSkillCostPolicy_ResourceAttr::Amount` **逐字同构**（台账 `COOLDOWN-1` 与 `COST-1`
	 * **同一技术根因**：技能侧数值字段缺参数化载体 ⇒ MUST 同批裁）：
	 * ① 按等级配时长（"1 级 8s / 2 级 6s"）靠等级表参数源；
	 * ② 按来源配时长（`HostDelegate` / `ParamRef` 等既有源）；
	 * ③ 缩减靠**契约键** `Tag_TcsSkillParam_CooldownPct` 的系数
	 *   （**注意与 Cost 的差别**：CDR 是**纯系数** `Duration × (1 − CDR)`，
	 *   而 Cost 走**带式折叠**——因为 Cost 要支持"减固定值"，纯系数表达不了。见各自的键注释）。
	 *
	 * **求值时点 = 起冷那一刻，且全场只求一次**（`D5-15`："CDR 快照于冷却触发时刻，
	 * 中途变化不追溯"）⇒ 本字段的求值同样只做一次，作用于**全部**轨道
	 * （逐轨各求一次会让"同一次冷却内部不自洽"）。
	 *
	 * **默认值**：`FTcsParamValue` 默认构造 = `Literal(0)` ⇒ **默认时长 0 秒**。
	 * 时长 0 在今天会被 `EffectiveDuration = Max(0, …)` 钳成"立刻到期"，
	 * **且起草期实测**：空 `Tracks` 数组才是"无冷却"的正确表达（`Cooldown.Tracks.Num() == 0`
	 * 在 `StartCooldownOnEntry` 开头即返回 0）——故"配了轨道但时长 0"= 起一条立刻到期的轨，
	 * 那是**作者的明确声明**，不是缺陷。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooldown Track")
	FTcsParamValue Duration;

#pragma endregion


// 归属
#pragma region Group

public:
	/**
	 * 组轨身份：**无效 = 自身轨**（只算在本条目上）；**有效 = 单位级组槽**
	 * （同组的所有条目共用一条冷却线 ⇒ 这就是**真公共冷却（GCD）的纯数据表达**）。
	 *
	 * **批次表 `:198` 的问号 `GroupTag?` 就此关闭**（2026-10-11 裁定）：字段**恒存在**，
	 * 用"tag 是否有效"承担两个语义——而不是用一个可选的"有没有组"开关。
	 * 判据：无效 tag 已经是"没有组"的天然表达，再加一个开关就是**同一件事两个入口**。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooldown Track")
	FGameplayTag GroupTag;
};



/**
 * 冷却策略 —— 一个技能的完整冷却配置（`D5-15` 的 `FCooldownPolicy`）。
 *
 * **空 `Tracks` = 永远可放**：这是 `D5-15` "默认无冷却"的数据表达，也是"Strategy 三档溶解进轨道"
 * 之后剩下的唯一档——不需要一个 `None` 枚举值。
 *
 * **无 `CustomFragment` 字段**（2026-10-11 用户裁定，MUST NOT 补回）：`D5-15` 字段表里原本
 * 有一个自定义时机片段位，它随 `ECT_Manual` 档的确立而**整体作废**——手动时机是"宿主调一个函数"，
 * 不需要把任何策略对象塞进 Def。**判据**：载体存在的意义是"框架要按它替你决定做什么"；
 * 手动档恰恰是"框架什么都不替你决定"⇒ 零载体。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsCooldownPolicy
{
	GENERATED_BODY()

// 轨道
#pragma region Tracks

public:
	// 全部冷却轨道（空 = 永远可放；自身轨与组轨同形，靠 GroupTag 区分）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooldown Policy")
	TArray<FTcsCooldownTrack> Tracks;

#pragma endregion


// 时机
#pragma region Timing

public:
	/**
	 * 起冷却的时点（**策略级**，不是每轨一个——`D5-15` 明文）。
	 *
	 * 判据：时机回答的是"这次施法什么时候进冷却"，而"进冷却"对全部轨道是**同一个瞬间**；
	 * 每轨各配一个时机只会造出"自身轨已起、组轨还没起"这种没有语义的中间态。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooldown Policy")
	ETcsSkillCooldownTiming Timing = ETcsSkillCooldownTiming::ECT_OnCastStarted;

#pragma endregion
};
