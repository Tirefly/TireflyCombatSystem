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
 * **两道占位档的现状（`R6.5-1` 起逐道转真）**：
 * - `SAR_OnCooldown`：**已转真**（`R6.5-1` Task 1——冷却多轨道落地，判据 = 全部自身轨清空 +
 *   所属组槽清空，实现单点在 `FTcsCooldownOps::IsEntryOnCooldown`）；
 * - `SAR_CannotAfford`：**已转真**（`R6.5-1` Task 2——Cost 策略 × 时机落地，
 *   实现单点在 `FTcsCostOps::CanAffordCost`；四类配置边界 fail-closed）。
 * ⇒ **六道门禁今天全部可拒**（①②③④⑤⑥）。**MUST NOT 再把这两道读成"占位/恒过"**
 * ——那会让宿主跳过 `SAR_CannotAfford` / `SAR_OnCooldown` 的 UI 分支。
 * **两者 MUST 保持可拒绝的结构**（各有独立枚举值），MUST NOT 合并进其它值——
 * 否则宿主面会出现"同一枚举值表示两件不同的事"。
 *
 * **枚举值上方 MUST NOT 再放 `//` 行注释**：UHT 会把该注释转成 `ToolTip` 元数据，
 * 与显式 `UMETA(ToolTip=...)` 冲突并**直接编译失败**（实测）。语义一律写进 `UMETA`。
 */
UENUM(BlueprintType)
enum class ESkillActivateResult : uint8
{
	SAR_EntityNotReady = 0		UMETA(DisplayName = "实体不可操作", ToolTip = "门禁①：实体尚未就绪（IsEntityReady 为假；未注入实现时本道降级放行）"),

	SAR_NotLearned = 1			UMETA(DisplayName = "未学该技能", ToolTip = "门禁②：该单位的账本里没有这条已学记录"),

	SAR_OnCooldown = 2			UMETA(DisplayName = "冷却中", ToolTip = "门禁③：技能仍在冷却（全部自身轨道 + 所属组槽均需清空；空 Tracks = 永远可放）"),

	SAR_AlreadyActive = 3		UMETA(DisplayName = "已在飞行中", ToolTip = "门禁④：该实体上已有在飞的同技能实例——含『不顶替而驳回』与『顶替被不可打断时段拒绝』两种来源"),

	SAR_CannotAfford = 4		UMETA(DisplayName = "代价不足", ToolTip = "门禁⑤：资源/代价不足，或消耗配置写错（无效属性键 / 负数量 / 未持有该属性）——四类边界一律 fail-closed"),

	SAR_DefInvalid = 5			UMETA(DisplayName = "定义无效", ToolTip = "门禁⑥：该 DefTag 的定义在本世界取不到（含未登记与空身份）"),

	SAR_Success = 6				UMETA(DisplayName = "成功", ToolTip = "六道门禁全过，施法运行态已建立")
};
