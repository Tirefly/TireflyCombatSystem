// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TcsStateSlot.h"
#include "TcsStateSlotDefinitionAsset.generated.h"



/**
 * 状态槽定义资产
 *
 * 用途: 定义单个状态槽的所有配置信息
 * 继承: UPrimaryDataAsset（支持 Asset Manager）
 * 命名约定: DA_StateSlot_<SlotName> (例如: DA_StateSlot_Action)
 */
UCLASS(BlueprintType, Const)
class TIREFLYCOMBATSYSTEM_API UTcsStateSlotDefinitionAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * PrimaryAssetType 标识符
	 */
	static const FPrimaryAssetType PrimaryAssetType;


#pragma region Identity

public:
	/**
	 * 状态槽的唯一标识符
	 * 对应原 DataTable 的 RowName
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName StateSlotDefId;

#pragma endregion


#pragma region StateSlot

public:
	/**
	 * 槽位标签
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State Slot",
		meta = (ToolTip = "状态槽的GameplayTag标识"))
	FGameplayTag SlotTag;

	/**
	 * 对应的StateTree状态名（可选，用于槽位 <-> 状态映射）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State Slot")
	FName StateTreeStateName = NAME_None;

#pragma endregion


#pragma region StateSlotGate

public:
	/**
	 * 激活模式
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State Slot Gate",
		meta = (ToolTip = "该槽位的状态激活模式"))
	ETcsStateSlotActivationMode ActivationMode = ETcsStateSlotActivationMode::SSAM_PriorityOnly;

	/**
	 * Gate关闭时的处理策略
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State Slot Gate",
		meta = (ToolTip = "当Gate关闭时对槽位内状态采取的处理策略"))
	ETcsStateSlotGateClosePolicy GateCloseBehavior = ETcsStateSlotGateClosePolicy::SSGCP_Pause;

#pragma endregion


#pragma region SlotConfiguration

public:
	/**
	 * 优先级抢占策略
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration",
		meta = (ToolTip = "高优先级状态进入槽位时，对低优先级状态的处理策略"))
	ETcsStatePreemptionPolicy PreemptionPolicy = ETcsStatePreemptionPolicy::SPP_PauseLowerPriority;

	/**
	 * 同优先级排序策略
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration",
		meta = (ToolTip = "在 PriorityOnly 模式下，多个状态具有相同优先级时的排序策略"))
	TSubclassOf<class UTcsStateSamePriorityPolicy> SamePriorityPolicy;

#pragma endregion


public:
	// 构造函数
	UTcsStateSlotDefinitionAsset();

	// 覆写 GetPrimaryAssetId
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
