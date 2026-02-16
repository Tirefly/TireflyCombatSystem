// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StateTreeReference.h"
#include "TcsState.h"
#include "TcsStateDefinitionAsset.generated.h"



/**
 * 状态定义资产
 *
 * 用途: 定义单个状态的所有配置信息
 * 继承: UPrimaryDataAsset（支持 Asset Manager）
 * 命名约定: DA_State_<StateName> (例如: DA_State_Stunned)
 */
UCLASS(BlueprintType, Const)
class TIREFLYCOMBATSYSTEM_API UTcsStateDefinitionAsset : public UPrimaryDataAsset
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
	 * 状态的唯一标识符
	 * 对应原 DataTable 的 RowName
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName StateDefId;

	/**
	 * 状态的语义标识（新增字段）
	 * 用于父子 Tag 匹配、分类筛选、跨系统对齐
	 * 推荐命名约定：TCS.State.<StateDefId>
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", Meta = (Categories = "TCS.State"))
	FGameplayTag StateTag;

#pragma endregion


#pragma region Meta

public:
	/**
	 * 状态类型
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
	TEnumAsByte<ETcsStateType> StateType = ST_State;

	/**
	 * 状态槽类型
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta", Meta = (Categories = "StateSlot"))
	FGameplayTag StateSlotType;

	/**
	 * 状态优先级（值越大，优先级越高，越优先执行，默认优先级为0）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
	int32 Priority = 0;

#pragma endregion


#pragma region Tag

public:
	/**
	 * 状态类别标签
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tag")
	FGameplayTagContainer CategoryTags;

	/**
	 * 状态功能标签
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tag")
	FGameplayTagContainer FunctionTags;

#pragma endregion


#pragma region Duration

public:
	/**
	 * 持续时间类型
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duration")
	TEnumAsByte<ETcsStateDurationType> DurationType = SDT_None;

	/**
	 * 持续时间
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duration",
		Meta = (EditConditionHides, EditCondition = "DurationType == SDT_Duration"))
	float Duration = 0.f;

#pragma endregion


#pragma region Stack

public:
	/**
	 * 最大叠层数
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack")
	int32 MaxStackCount = 1;

	/**
	 * 状态合并策略
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack")
	TSubclassOf<class UTcsStateMerger> MergerType;

#pragma endregion


#pragma region StateTree

public:
	/**
	 * 状态树资产引用，作为状态的运行时脚本
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Tree")
	FStateTreeReference StateTreeRef;

	/**
	 * StateTree Tick 策略
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Tree")
	ETcsStateTreeTickPolicy TickPolicy = ETcsStateTreeTickPolicy::WhileActive;

#pragma endregion


#pragma region Condition

public:
	/**
	 * 状态的激活条件配置
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	TArray<FTcsStateConditionConfig> ActiveConditions;

#pragma endregion


#pragma region Parameter

public:
	/**
	 * 状态的参数集（FName 键）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter")
	TMap<FName, FTcsStateParameter> Parameters;

	/**
	 * 状态的参数集（GameplayTag 键）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter")
	TMap<FGameplayTag, FTcsStateParameter> TagParameters;

#pragma endregion


public:
	// 覆写 GetPrimaryAssetId
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	// 编辑器验证：属性值变更时的验证
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// 编辑器验证：数据有效性检查
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
