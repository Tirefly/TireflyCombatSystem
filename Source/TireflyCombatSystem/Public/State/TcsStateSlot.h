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



// 状态槽配置数据表行结构
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsStateSlotDefinition : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 槽位标签
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration",
		Meta = (ToolTip = "状态槽的GameplayTag标识"))
	FGameplayTag SlotTag;
    
	// 激活模式
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration",
		Meta = (ToolTip = "该槽位的状态激活模式"))
	ETcsStateSlotActivationMode ActivationMode = ETcsStateSlotActivationMode::SSAM_AllActive;

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