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



// Gate关闭行为
UENUM(BlueprintType)
enum class ETcsStateSlotGateClosePolicy : uint8
{
	SSGCP_HangUp	UMETA(DisplayName = "Hang Up States",
		ToolTip = "Gate关闭时挂起槽位中的状态：\n- 持续时间：继续计时\n- 叠层合并：正常参与（保持叠层数准确）\n- 逻辑执行：暂停StateTree执行"),
	SSGCP_Pause		UMETA(DisplayName = "Pause States",
		ToolTip = "Gate关闭时暂停槽位中的状态：\n- 持续时间：完全冻结\n- 叠层合并：正常参与（保持叠层数准确）\n- 逻辑执行：暂停StateTree执行"),
	SSGCP_Cancel	UMETA(DisplayName = "Cancel States",
		ToolTip = "Gate关闭时直接取消槽位中的状态")
};



// 优先级抢占策略
UENUM(BlueprintType)
enum class ETcsStatePreemptionPolicy : uint8
{
	SPP_HangUpLowerPriority		UMETA(DisplayName = "Hang Up Lower Priority",
		ToolTip = "高优先级状态抢占时，低优先级状态进入挂起：\n- 持续时间：继续计时\n- 叠层合并：正常参与（保持叠层数准确）\n- 逻辑执行：暂停StateTree执行"),
	SPP_PauseLowerPriority		UMETA(DisplayName = "Pause Lower Priority",
		ToolTip = "高优先级状态抢占时，低优先级状态进入暂停：\n- 持续时间：完全冻结\n- 叠层合并：正常参与（保持叠层数准确）\n- 逻辑执行：暂停StateTree执行"),
	SPP_CancelLowerPriority		UMETA(DisplayName = "Cancel Lower Priority",
		ToolTip = "高优先级状态抢占时，低优先级状态被取消")
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

	// 同优先级排序策略
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration",
		meta = (ToolTip = "在 PriorityOnly 模式下，多个状态具有相同优先级时的排序策略"))
	TSubclassOf<class UTcsStateSamePriorityPolicy> SamePriorityPolicy;

	// 构造函数
	FTcsStateSlotDefinition();

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
