// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TcsStateSlot.generated.h"



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
	DT_ActiveOnly        UMETA(DisplayName = "Active Only", ToolTip = "仅在状态处于Active阶段时递减持续时间"),
	DT_Always            UMETA(DisplayName = "Always", ToolTip = "无论状态是否激活都持续计时"),
	DT_OnlyWhenGateOpen  UMETA(DisplayName = "Only When Gate Open", ToolTip = "仅当槽位Gate开启时计时"),
	DT_ActiveOrGateOpen  UMETA(DisplayName = "Active Or Gate Open", ToolTip = "状态处于Active阶段或Gate开启时计时"),
	DT_ActiveAndGateOpen UMETA(DisplayName = "Active And Gate Open", ToolTip = "仅当状态激活且Gate开启时计时")
};



// Gate关闭行为
UENUM(BlueprintType)
enum class ETcsSlotGateCloseBehavior : uint8
{
	GCB_Pause   UMETA(DisplayName = "Pause States", ToolTip = "Gate关闭时暂停槽位中的状态（进入挂起阶段）"),
	GCB_Cancel  UMETA(DisplayName = "Cancel States", ToolTip = "Gate关闭时直接取消槽位中的状态")
};



// 优先级抢占策略
UENUM(BlueprintType)
enum class ETcsStatePreemptionPolicy : uint8
{
	SPP_PauseLowerPriority   UMETA(DisplayName = "Pause Lower Priority", ToolTip = "高优先级状态抢占时，低优先级状态进入挂起"),
	SPP_CancelLowerPriority  UMETA(DisplayName = "Cancel Lower Priority", ToolTip = "高优先级状态抢占时，低优先级状态被取消")
};



// 状态槽配置数据表行结构
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsStateSlotDefinition : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 槽位标签
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration",
		meta = (ToolTip = "状态槽的GameplayTag标识"))
	FGameplayTag SlotTag;

	// 激活模式
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration",
		meta = (ToolTip = "该槽位的状态激活模式"))
	ETcsStateSlotActivationMode ActivationMode = ETcsStateSlotActivationMode::SSAM_AllActive;

	// Gate关闭时的处理策略
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration",
		meta = (ToolTip = "当Gate关闭时对槽位内状态采取的处理策略"))
	ETcsSlotGateCloseBehavior GateCloseBehavior = ETcsSlotGateCloseBehavior::GCB_Pause;

	// 优先级抢占策略
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration",
		meta = (ToolTip = "高优先级状态进入槽位时，对低优先级状态的处理策略"))
	ETcsStatePreemptionPolicy PreemptionPolicy = ETcsStatePreemptionPolicy::SPP_PauseLowerPriority;

	// 持续时间计时策略
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration",
		meta = (ToolTip = "控制槽位内状态剩余时间递减的策略"))
	ETcsDurationTickPolicy DurationTickPolicy = ETcsDurationTickPolicy::DT_ActiveOnly;

	// 是否启用排队
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration",
		meta = (ToolTip = "当Gate关闭或策略不允许立即激活时，是否将状态加入排队等待"))
	bool bEnableQueue = false;

	// 排队元素最大存活时间（秒），0表示无限期等待
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration",
		meta = (EditCondition = "bEnableQueue", ClampMin = "0.0",
			ToolTip = "排队状态的最大存活时间（秒），0表示无限期等待"))
	float QueueTimeToLive = 0.f;

	// 对应的StateTree状态名（可选，用于槽位 <-> 状态映射）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StateTree Integration")
	FName StateTreeStateName = NAME_None;

	// 构造函数
	FTcsStateSlotDefinition()
		: ActivationMode(ETcsStateSlotActivationMode::SSAM_AllActive)
	{}

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
	TArray<class UTcsStateInstance*> States;

	// Gate状态 (用于StateTree联动,控制槽位是否允许激活状态)
	UPROPERTY()
	bool bIsGateOpen = true;

	FTcsStateSlot()
		: bIsGateOpen(true)
	{}
};
