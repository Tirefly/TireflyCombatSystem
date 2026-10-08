// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TcsSkillActivateResult.generated.h"



/**
 * 激活结果（**六道具名门禁** + 成功）——修 TCS 库外报告 09 的"门禁内联无钩子"缺陷：
 * 宿主 MUST 能查出**为什么放不出来**，而不是只拿到一个笼统的失败。
 *
 * **为什么 MUST 反射（`BlueprintType`）**：本枚举是宿主面唯一的诊断读数来源 ⇒ 它若不可蓝图/脚本表达，
 * 门禁的"具名"就只对 C++ 内部有意义，等于没修那个缺陷。
 *
 * **值名 MUST 带 `SAR_` 前缀**（全仓 `UENUM(BlueprintType)` 的枚举值无一例外带 2–4 字母前缀：
 * `EAR_` / `ESRC_` / `TAO_` / `EDP_` / `CI_` / `MCS_` / `CQM_`；唯一的裸名族
 * `ETcsStateStackDecisionKind` **不是 UENUM** ⇒ 不能照抄它的字面）。
 *
 * **两道占位档**（`SAR_OnCooldown` / `SAR_CannotAfford`）：其**真分支归 R6.5**
 * （冷却多轨道 / Cost 策略×时机），本轮判据恒过（空轨道恒可放 / 不消耗档恒可付）。
 * **它们 MUST 保持可拒绝的结构**（各有独立枚举值），MUST NOT 合并进其它值——
 * 否则 R6.5 落地时宿主面会出现"同一枚举值表示两件不同的事"。
 *
 * **枚举值上方 MUST NOT 再放 `//` 行注释**：UHT 会把该注释转成 `ToolTip` 元数据，
 * 与显式 `UMETA(ToolTip=...)` 冲突并**直接编译失败**（实测）。语义一律写进 `UMETA`。
 */
UENUM(BlueprintType)
enum class ESkillActivateResult : uint8
{
	SAR_EntityNotReady = 0		UMETA(DisplayName = "实体不可操作", ToolTip = "门禁①：实体尚未就绪（IsEntityReady 为假；未注入实现时本道降级放行）"),

	SAR_NotLearned = 1			UMETA(DisplayName = "未学该技能", ToolTip = "门禁②：该单位的账本里没有这条已学记录"),

	SAR_OnCooldown = 2			UMETA(DisplayName = "冷却中", ToolTip = "门禁③：技能仍在冷却（真分支归 R6.5，本轮空轨道恒可放）"),

	SAR_AlreadyActive = 3		UMETA(DisplayName = "已在飞行中", ToolTip = "门禁④：该实体上已有在飞的同技能实例——含『不顶替而驳回』与『顶替被不可打断时段拒绝』两种来源"),

	SAR_CannotAfford = 4		UMETA(DisplayName = "代价不足", ToolTip = "门禁⑤：资源/代价不足（真分支归 R6.5，本轮不消耗档恒可付）"),

	SAR_DefInvalid = 5			UMETA(DisplayName = "定义无效", ToolTip = "门禁⑥：该 DefTag 的定义在本世界取不到（含未登记与空身份）"),

	SAR_Success = 6				UMETA(DisplayName = "成功", ToolTip = "六道门禁全过，施法运行态已建立")
};
