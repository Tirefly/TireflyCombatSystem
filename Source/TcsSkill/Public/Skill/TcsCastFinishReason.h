// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TcsCastFinishReason.generated.h"



/**
 * 施法运行态的**终结原因**——四路汇聚到同一个终结例程（`FTcsCastOps::FinishSkillInstance`）后，
 * **唯一分岔处的输入**。
 *
 * **为什么必须是一个枚举而不是三个入口**：四路终结的回收动作**逐字相同**（取消时段锚点 /
 * 摘 M2 属性账本条目 / 摘 M5 参数链条目 / 摘账本 `RunHandles` / 归还池槽位），只有
 * "发哪一枚终止事件"不同。若把它拆成"完成入口 / 打断入口 / 顶替入口"三份，回收动作就会
 * 复制三份 ⇒ **必然漂移**，而漂移的后果是**静默泄漏**（条目留在账本里、属性不回退，且无人报错）。
 * ⇒ 纪律：**四路只在"原因"这一处分岔**，回收只有一处。
 *
 * **两枚事件的对应关系（MUST NOT 互相冒充）**：
 * - `ECFR_Completed` → `TcsEvent.Cast.Completed`（**只有它**）；
 * - `ECFR_Interrupted` / `ECFR_Replaced` → `TcsEvent.Cast.Interrupted`（**两者都不是完成**）。
 *
 * **为什么"顶替"要单列一档而不是并进"打断"**：两者对订阅者语义不同（一个是外部打断、
 * 一个是同类技能抢位），且顶替是**先终结旧的、再建新的**——订阅者要靠这一档区分"我被打断了"
 * 与"我被自己的下一次施法顶掉了"。它们**发同一枚事件**只是"载荷形状够用"的结论，不是"语义相同"。
 *
 * **枚举值上方 MUST NOT 放 `//` 行注释**：UHT 会把该注释转成 `ToolTip` 元数据，与显式
 * `UMETA(ToolTip=...)` 冲突并**直接编译失败**（本仓实测）。语义一律写进 `UMETA`。
 */
UENUM(BlueprintType)
enum class ECastFinishReason : uint8
{
	ECFR_Completed = 0		UMETA(DisplayName = "完成", ToolTip = "全时段走完，或瞬发（空时段表）在激活当刻完成——发 TcsEvent.Cast.Completed"),

	ECFR_Interrupted = 1	UMETA(DisplayName = "被打断", ToolTip = "经 CancelCast 打断（先过可打断性查询）——发 TcsEvent.Cast.Interrupted"),

	ECFR_Replaced = 2		UMETA(DisplayName = "被顶替", ToolTip = "同类技能再次激活、顶替本次施法——发 TcsEvent.Cast.Interrupted"),
};
