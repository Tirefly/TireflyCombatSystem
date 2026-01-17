// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TcsStateSlot.generated.h"



class UTcsStateInstance;



// 状态槽激活模式枚举
UENUM(BlueprintType)
enum class ETcsStateSlotActivationMode : uint8
{
	SSAM_PriorityOnly    UMETA(DisplayName = "Priority Only",
		ToolTip = "优先级激活模式：只有最高优先级状态激活，适用于互斥行为如Action"),
	SSAM_AllActive       UMETA(DisplayName = "All Active",
		ToolTip = "全部激活模式：所有状态都可同时激活，适用于可共存效果如Buff/Debuff")
};



// 持续时间计时策略
UENUM(BlueprintType)
enum class ETcsDurationTickPolicy : uint8
{
	DTP_ActiveOnly			UMETA(DisplayName = "Active Only", ToolTip = "仅在状态处于Active阶段时递减持续时间"),
	DTP_Always				UMETA(DisplayName = "Always", ToolTip = "无论状态是否激活都持续计时"),
	DTP_OnlyWhenGateOpen	UMETA(DisplayName = "Only When Gate Open", ToolTip = "仅当槽位Gate开启时计时"),
	DTP_ActiveOrGateOpen	UMETA(DisplayName = "Active Or Gate Open", ToolTip = "状态处于Active阶段或Gate开启时计时"),
	DTP_ActiveAndGateOpen	UMETA(DisplayName = "Active And Gate Open", ToolTip = "仅当状态激活且Gate开启时计时")
};



// Gate关闭行为
UENUM(BlueprintType)
enum class ETcsStateSlotGateClosePolicy : uint8
{
	SSGCP_HangUp	UMETA(DisplayName = "Hang Up States", ToolTip = "Gate关闭时挂起槽位中的状态（仍然计算剩余持续时间和叠层，但不执行逻辑）"),
	SSGCP_Pause		UMETA(DisplayName = "Pause States", ToolTip = "Gate关闭时暂停槽位中的状态（剩余持续时间计算，叠层计算和逻辑执行都暂停）"),
	SSGCP_Cancel	UMETA(DisplayName = "Cancel States", ToolTip = "Gate关闭时直接取消槽位中的状态")
};



// 优先级抢占策略
UENUM(BlueprintType)
enum class ETcsStatePreemptionPolicy : uint8
{
	SPP_HangUpLowerPriority		UMETA(DisplayName = "Hang Up Lower Priority", ToolTip = "高优先级状态抢占时，低优先级状态进入挂起（仍然计算剩余持续时间和叠层，但不执行逻辑）"),
	SPP_PauseLowerPriority		UMETA(DisplayName = "Pause Lower Priority", ToolTip = "高优先级状态抢占时，低优先级状态进入暂停（剩余持续时间计算，叠层计算和逻辑执行都暂停）"),
	SPP_CancelLowerPriority		UMETA(DisplayName = "Cancel Lower Priority", ToolTip = "高优先级状态抢占时，低优先级状态被取消")
};



// 状态槽配置数据表行结构
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsStateSlotDefinition : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 槽位标签
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State Slot",
		meta = (ToolTip = "状态槽的GameplayTag标识"))
	FGameplayTag SlotTag;

	// 对应的StateTree状态名（可选，用于槽位 <-> 状态映射）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State Slot")
	FName StateTreeStateName = NAME_None;

	// 激活模式
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State Slot Gate",
		meta = (ToolTip = "该槽位的状态激活模式"))
	ETcsStateSlotActivationMode ActivationMode = ETcsStateSlotActivationMode::SSAM_PriorityOnly;

	// Gate关闭时的处理策略
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State Slot Gate",
		meta = (ToolTip = "当Gate关闭时对槽位内状态采取的处理策略"))
	ETcsStateSlotGateClosePolicy GateCloseBehavior = ETcsStateSlotGateClosePolicy::SSGCP_Pause;

	// 优先级抢占策略
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration",
		meta = (ToolTip = "高优先级状态进入槽位时，对低优先级状态的处理策略"))
	ETcsStatePreemptionPolicy PreemptionPolicy = ETcsStatePreemptionPolicy::SPP_PauseLowerPriority;

	// 持续时间计时策略
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration",
		meta = (ToolTip = "控制槽位内状态剩余时间递减的策略"))
	ETcsDurationTickPolicy DurationTickPolicy = ETcsDurationTickPolicy::DTP_ActiveOnly;

	// Pause 阶段是否冻结持续时间（默认冻结；若关闭，则 Pause 状态也会遵循 DurationTickPolicy 的判定逻辑）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration",
		meta = (ToolTip = "是否在状态进入 Pause 阶段时强制冻结持续时间。关闭后，Pause 阶段会遵循 DurationTickPolicy。"))
	bool bFreezeDurationWhenPaused = true;

	// 构造函数
	FTcsStateSlotDefinition() {}

	FTcsStateSlotDefinition(const FGameplayTag& InSlotTag, ETcsStateSlotActivationMode InActivationMode)
		: SlotTag(InSlotTag)
		, ActivationMode(InActivationMode)
	{}
};



// 状态槽数据容器
USTRUCT()
struct FTcsStateSlot
{
	GENERATED_BODY()

public:
	// 槽位中的状态实例数组
	UPROPERTY()
	TArray<UTcsStateInstance*> States;

	// Gate状态 (用于StateTree联动,控制槽位是否允许激活状态)
	UPROPERTY()
	bool bIsGateOpen = true;

	FTcsStateSlot()
		: bIsGateOpen(true)
	{}
};
