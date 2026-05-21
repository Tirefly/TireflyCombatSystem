// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StateTreeReference.h"
#include "TcsStateInstance.h"
#include "StateCondition/TcsStateCondition.h"
#include "TcsStateDefinition.generated.h"



/**
 * 状态定义抽象基类
 *
 * 用途: 定义所有运行态共享的状态配置信息
 * 继承: UPrimaryDataAsset（支持 Asset Manager）
 * 命名约定: 由具体派生定义类型决定
 */
UCLASS(Abstract, BlueprintType, Const)
class TIREFLYCOMBATSYSTEM_API UTcsStateDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

#pragma region PrimaryAsset

public:
	/**
	 * PrimaryAssetType 标识符
	 */
	static const FPrimaryAssetType PrimaryAssetType;

public:
	// 覆写 GetPrimaryAssetId
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#pragma endregion


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
	 * 命名约定：StateTag.<StateDefId>
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", Meta = (Categories = "StateTag"))
	FGameplayTag StateTag;

#pragma endregion


#pragma region Meta

public:
	/**
	 * 状态槽类型
	 * 命名约定：StateSlotTag.<StateSlotId>
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Slot", Meta = (Categories = "StateSlotTag"))
	FGameplayTag StateSlotType;

	/**
	 * 状态优先级（值越大，优先级越高，越优先执行，默认优先级为0）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Slot")
	int32 Priority = 0;

#pragma endregion


#pragma region Tag

public:
	/**
	 * 状态类别标签
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay Tag")
	FGameplayTagContainer CategoryTags;

	/**
	 * 状态功能标签
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay Tag")
	FGameplayTagContainer FunctionTags;

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


#pragma region Runtime

public:
	/** @return 当前定义对应的运行时实例类；默认返回共享的 StateInstance。 */
	virtual UClass* ResolveStateInstanceClass() const;

#pragma endregion


#if WITH_EDITOR
	// 编辑器验证：属性值变更时的验证
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// 编辑器验证：数据有效性检查
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
