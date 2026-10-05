// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TcsStateEnums.generated.h"



// 时值策略（D3-13）：把"时长 ≤0 即永久"的魔法值拆成显式两态
// 枚举值前缀 EDP_ 是 DurationPolicy 的缩写
UENUM(BlueprintType)
enum class EDurationPolicy : uint8
{
	EDP_Finite = 0		UMETA(DisplayName = "有限", ToolTip = "按 DurationTime 到期：注册到期条目，到点发 Expired"),
	EDP_Infinite = 1	UMETA(DisplayName = "无限", ToolTip = "无到期条目、永不过期——由 Remove / Cancel 显式结束"),
};



// 周期刷新策略（D3-13）：堆叠刷新对周期计时器的作用
// 枚举值前缀 EPR_ 是 PeriodRefresh 的缩写
UENUM(BlueprintType)
enum class ETcsPeriodRefresh : uint8
{
	EPR_Keep = 0		UMETA(DisplayName = "保持", ToolTip = "刷新不动周期计时（默认）——下一个周期仍按原剩余时间到点"),
	EPR_Reset = 1		UMETA(DisplayName = "重置", ToolTip = "刷新时把周期计时重置为满额（下一个周期从零开始计）"),
	EPR_Immediate = 2	UMETA(DisplayName = "立即", ToolTip = "刷新时立刻执行一次周期效果并重置计时"),
};



// 状态阶段（D3-7）：实例在册期间的可观测状态
// 枚举值前缀 ESP_ 是 StatePhase 的缩写
//
// **三态的由来**：`Expiring` 是"已进入移除流程、槽位尚未归还"的窗口——它是"先广播后释放槽位"
// 这条纪律（订阅者在移除回调里仍能 GetState 读到实例）能成立的前提，不是为对称凑的第三态。
// `Inactive` 在正常路径上**不可观测**（槽位归还后句柄即凭代际失配失效、查不到实例），
// 保留它是非法迁移判据（`ensure`）需要的"迁移矩阵起点"。
UENUM(BlueprintType)
enum class EStatePhase : uint8
{
	ESP_Inactive = 0	UMETA(DisplayName = "未激活", ToolTip = "未在册 / 槽位已归还（正常路径不可观测——句柄已失配）"),
	ESP_Active = 1		UMETA(DisplayName = "在册", ToolTip = "实例生效中（施加成功后的稳态）"),
	ESP_Expiring = 2	UMETA(DisplayName = "移除中", ToolTip = "已进入移除流程、槽位尚未归还（移除广播在此窗口内发出）"),
};



// 状态移除原因（D3-7 生命周期事件全集）：到期之外的两种由调用方显式给定
// 枚举值前缀 ESRC_ 是 StateRemoveCause 的缩写
UENUM(BlueprintType)
enum class EStateRemoveCause : uint8
{
	ESRC_Expired = 0	UMETA(DisplayName = "到期", ToolTip = "时值走完由到期路径结束（ExpireState 或到期回调）"),
	ESRC_Removed = 1	UMETA(DisplayName = "被移除", ToolTip = "外部显式驱散（RemoveState / 单位注销批量移除）"),
	ESRC_Cancelled = 2	UMETA(DisplayName = "被取消", ToolTip = "被更高优先级的共存规则取消（关系表族归 R5.5-e；本轮无内建产生者，由调用方显式传入）"),
};



// 施加结果（D3-7 ApplyState 回执）：**四档全部可达**（R5 Task 5 起）
// 枚举值前缀 EAR_ 是 ApplyResult 的缩写
//
// **判据归堆叠策略**（`state-stacking-policies` 能力）：无同组实例 ⇒ `Applied`；同组**同来源**
// （续杯）⇒ `Refreshed`；同组**异来源**且未满仓 ⇒ `Stacked`；满仓 ⇒ `Rejected` 或替换后 `Applied`。
// **拒绝原因的具名细分**仍归 R5.5-e（关系表族）——今天只有 `Rejected` 一档 + 日志。
UENUM(BlueprintType)
enum class EApplyResult : uint8
{
	EAR_Applied = 0		UMETA(DisplayName = "已施加", ToolTip = "建出新实例（组内无实例，或满仓按替换政策摘旧建新）"),
	EAR_Refreshed = 1	UMETA(DisplayName = "已刷新", ToolTip = "命中同组**同来源**实例（续杯）：在原实例上按刷新语义更新，层数不变"),
	EAR_Stacked = 2		UMETA(DisplayName = "已叠层", ToolTip = "同组**异来源**加入且未满仓：层数 +1（R5 Task 5 起可达）"),
	EAR_Rejected = 3	UMETA(DisplayName = "被拒绝", ToolTip = "施加未成立（定义未登记 / 目标无效 / 满仓拒绝 / 自定义决策不接受）"),
};
