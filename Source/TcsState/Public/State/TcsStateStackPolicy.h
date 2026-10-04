// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TcsStateStackPolicy.generated.h"



// 堆叠分组轴（D3-4）：只有同组实例才互相计数与刷新
// 枚举值前缀 EGB_ 是 GroupByPolicy 的缩写；**值 0 = 默认（不分组）、Custom 走逃逸位**
UENUM(BlueprintType)
enum class EGroupByPolicy : uint8
{
	EGB_None = 0			UMETA(DisplayName = "不分组", ToolTip = "同一 DefTag 的全部实例共处一组（默认）"),
	EGB_PerSource = 1		UMETA(DisplayName = "按来源", ToolTip = "同一来源句柄的实例共处一组（不同来源各自独立计层）"),
	EGB_PerInstigator = 2	UMETA(DisplayName = "按发起者", ToolTip = "同一发起者实体的实例共处一组"),
	EGB_PerTag = 3			UMETA(DisplayName = "按分组 Tag", ToolTip = "按策略指定的分组 tag 归组"),
	EGB_Custom = 1 << 4		UMETA(DisplayName = "自定义", ToolTip = "逃逸位：分组由决策 Fragment 接管（R5 Task 5 落地；本轮只落枚举值）"),
};



// 溢出轴（D3-4）：组内满仓时新施加的处置
// 枚举值前缀 EOP_ 是 OverflowPolicy 的缩写；值 0 = 默认
UENUM(BlueprintType)
enum class EOverflowPolicy : uint8
{
	EOP_RejectNew = 0		UMETA(DisplayName = "拒绝新施加", ToolTip = "满仓即拒绝（返回 Rejected），旧实例不动（默认）"),
	EOP_ReplaceOldest = 1	UMETA(DisplayName = "替换最旧", ToolTip = "移除组内最旧的实例，再接受新施加"),
	EOP_ReplaceNewest = 2	UMETA(DisplayName = "替换最新", ToolTip = "移除组内最新的实例，再接受新施加"),
};



// 数值叠加轴（D3-4）：层数如何作用于数值
// 枚举值前缀 EVS_ 是 ValueStackPolicy 的缩写；值 0 = 默认
UENUM(BlueprintType)
enum class EValueStackPolicy : uint8
{
	EVS_KeepMax = 0			UMETA(DisplayName = "取最大", ToolTip = "组内数值取最大者，层数只作计数（默认）"),
	EVS_AddValues = 1		UMETA(DisplayName = "累加", ToolTip = "层数累加数值（DoT 叠层即此语义）"),
	EVS_PerStackValue = 2	UMETA(DisplayName = "按层取值", ToolTip = "按当前层数取等级/档位表的值"),
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
 * **本轮只落形状**（PLN-R5 Task 1）：五轴字段与枚举值就位，**零行为**——共存决策与刷新语义
 * （`FStateOps` 侧）、以及 Custom 位的决策 Fragment 载荷归 Task 5；故本轮 MUST NOT 出现
 * `TInstancedStruct` 决策字段与任何策略基类（零消费者不预建）。
 *
 * **组契约（Task 5 消费点）**：组键由 `GroupBy` 决定，`MaxStacks ≤ 0` = 无限；
 * 四轴组合覆盖旧 TCS 四个 Merger 的行为（规格 §3.2 等价表即验收清单）。
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
	// 满仓时新施加的处置（值 0 = 拒绝新施加）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack Policy")
	EOverflowPolicy Overflow = EOverflowPolicy::EOP_RejectNew;

#pragma endregion


// 数值叠加
#pragma region ValueStack

public:
	// 层数对数值的作用方式（值 0 = 取最大）
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
};
