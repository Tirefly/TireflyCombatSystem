// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"

#include "TcsStateStackPolicy.generated.h"



// 堆叠分组轴（D3-4）：只有同组实例才互相计数与刷新
// 枚举值前缀 EGB_ 是 GroupByPolicy 的缩写；**值 0 = 默认（不分组）、Custom 走高位置位**
//
// **值 3 已裁撤**（2026-10-05 R5 Task 5，原"按分组词分组"档）：组键基座含 `DefTag`，而分组词取自
// 定义自身（同一 `DefTag` 内恒定）⇒ 该档与 `EGB_None` 完全同义。它唯一有意义的读法是"不同定义
// 共享一个组"（层数上限跨定义累计），那要求**组内允许多条实例**——那是另一套层模型
// （"层 = 独立实例"），本轮明确不采用；"跨定义共享层数组"作为未落地面登记台账。
UENUM(BlueprintType)
enum class EGroupByPolicy : uint8
{
	EGB_None = 0			UMETA(DisplayName = "不分组", ToolTip = "同一 DefTag 的全部实例共处一组（默认）"),
	EGB_PerSource = 1		UMETA(DisplayName = "按来源", ToolTip = "组键附加来源句柄（调用方未声明来源时视为同来源）——不同来源各自独立计层"),
	EGB_PerInstigator = 2	UMETA(DisplayName = "按发起者", ToolTip = "组键附加发起者实体——只有同一发起者的重复施加才互相影响"),
	EGB_Custom = 1 << 4		UMETA(DisplayName = "自定义", ToolTip = "逃逸位：分组与接受/层数决策交给决策 Fragment（值 3 已裁撤，低位留给将来的组键附加项）"),
};



// 溢出轴（D3-4）：组内满仓时新施加的处置
// 枚举值前缀 EOP_ 是 OverflowPolicy 的缩写；值 0 = 默认
//
// **只有两档**（2026-10-05 R5 Task 5 裁撤"替换最旧 / 替换最新"两档）：本模型下**组内至多一条实例**
// （层数记在该实例上）⇒ "移除最旧的"与"移除最新的"指的是同一条，属死档。
UENUM(BlueprintType)
enum class EOverflowPolicy : uint8
{
	EOP_RejectNew = 0		UMETA(DisplayName = "拒绝新施加", ToolTip = "满仓即拒绝（返回 Rejected），旧实例不动（默认）"),
	EOP_ReplaceExisting = 1	UMETA(DisplayName = "替换现有", ToolTip = "移除组内现有实例，再接受新施加（层数回到 1，旧句柄悬空）"),
};



// 数值叠加轴（D3-4）：层数如何作用于数值
// 枚举值前缀 EVS_ 是 ValueStackPolicy 的缩写；值 0 = 默认
UENUM(BlueprintType)
enum class EValueStackPolicy : uint8
{
	EVS_KeepMax = 0			UMETA(DisplayName = "取最大", ToolTip = "层数不参与取值（数值与单层一致），层数只作计数（默认）"),
	EVS_AddValues = 1		UMETA(DisplayName = "累加", ToolTip = "值随层数成倍（DoT 叠层即此语义；在物化边界乘层数）"),
	EVS_PerStackValue = 2	UMETA(DisplayName = "按层取值", ToolTip = "**本轮零行为**（与取最大一致）：合规实现需\"层数读口 + 按层参数源\"，否则会变成设计明文撤回的\"等级跟随层数\"——未落地面入台账"),
};



// 时长刷新轴（D3-17 终定）：刷新对剩余时长的作用
// 枚举值前缀 ESD_ 是 StackDurationPolicy 的缩写；值 0 = 默认
UENUM(BlueprintType)
enum class EStackDurationPolicy : uint8
{
	ESD_None = 0						UMETA(DisplayName = "不动时长", ToolTip = "刷新只更新数值与层数，剩余时长照原计时走（默认）"),
	ESD_RefreshRemainingToTotal = 1		UMETA(DisplayName = "回满额", ToolTip = "刷新把剩余时长重置为满额"),
};



/**
 * 堆叠与刷新策略（D3-4 / D3-17 五轴，`FTcsBuffDef` 字段）：
 * 同一 buff 的重复施加行为由五轴确定性决定——分组、层数上限、溢出处置、数值叠加、时长刷新。
 *
 * **模型口径（2026-10-05 R5 Task 5 定）**：**层数记在一条实例内**（`FTcsStateInstance.Stacks`），
 * 组内至多一条实例；"续杯 / 叠层"的判据是**施加方换没换**（同来源 ⇒ 刷新、异来源 ⇒ 叠一层，
 * 未声明来源按同来源处理）。完整决策树与事件面见 `state-stacking-policies` 能力。
 *
 * **默认值即"最不介入"**：全 0 的组合 = 同 `DefTag` 一组、层数无限、层数不参与取值、
 * 刷新不动时长；`MaxStacks <= 0` 使 `Overflow` 不可达（无限层永不判满）。
 *
 * **`CustomDecision` 只被 `EGB_Custom` 读取**：策略实例归 **Def 资产**持有（单字段暴露，
 * 类/载荷配对 bug 类消失）；载体是**裸 `FInstancedStruct`**（2026-09-24 换型口径——
 * `TInstancedStruct<T>` 字段在宿主脚本层导出为空壳），类型收窄由手写 `BaseStruct` 元数据提供。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FStateStackPolicy
{
	GENERATED_BODY()

// 分组
#pragma region Grouping

public:
	// 分组轴（同组才互相计数与刷新；值 0 = 不分组）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack Policy")
	EGroupByPolicy GroupBy = EGroupByPolicy::EGB_None;

#pragma endregion


// 层数
#pragma region Stacks

public:
	// 组内最大层数（≤0 = 无限）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack Policy")
	int32 MaxStacks = 0;

#pragma endregion


// 溢出
#pragma region Overflow

public:
	// 满仓时新施加的处置（值 0 = 拒绝新施加；`MaxStacks <= 0` 时本轴不可达）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack Policy")
	EOverflowPolicy Overflow = EOverflowPolicy::EOP_RejectNew;

#pragma endregion


// 数值叠加
#pragma region ValueStack

public:
	// 层数对数值的作用方式（值 0 = 层数不参与取值）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack Policy")
	EValueStackPolicy ValueStack = EValueStackPolicy::EVS_KeepMax;

#pragma endregion


// 时长刷新
#pragma region StackDuration

public:
	// 刷新对剩余时长的作用（值 0 = 不动时长）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack Policy")
	EStackDurationPolicy StackDurationPolicy = EStackDurationPolicy::ESD_None;

#pragma endregion


// 自定义决策
#pragma region Custom

public:
	// 自定义决策载荷（**仅 `GroupBy == EGB_Custom` 时读取**；picker 经 BaseStruct metadata 限定到决策 Fragment 族；
	// 载荷缺失或类型不符 ⇒ 退化为内置"不分组"语义 + Warning——配置错误语义）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack Policy",
		meta = (BaseStruct = "/Script/TcsState.TcsStateStackDecisionFragment"))
	FInstancedStruct CustomDecision;

#pragma endregion
};
