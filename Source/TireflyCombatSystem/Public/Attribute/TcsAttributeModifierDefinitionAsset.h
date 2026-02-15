// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TcsAttributeModifier.h"
#include "TcsAttributeModifierDefinitionAsset.generated.h"



/**
 * 属性修改器定义资产
 *
 * 用途: 定义单个属性修改器的所有配置信息
 * 继承: UPrimaryDataAsset（支持 Asset Manager）
 * 命名约定: DA_AttrMod_<ModifierName> (例如: DA_AttrMod_HealthRegen)
 */
UCLASS(BlueprintType, Const)
class TIREFLYCOMBATSYSTEM_API UTcsAttributeModifierDefinitionAsset : public UPrimaryDataAsset
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
	 * 修改器的唯一标识符
	 * 对应原 DataTable 的 RowName
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName AttributeModifierDefId;

#pragma endregion


#pragma region Definition

public:
	/**
	 * 修改器名称
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition")
	FName ModifierName = NAME_None;

	/**
	 * 修改器标签
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition")
	FGameplayTagContainer Tags;

#pragma endregion


#pragma region Modifier

public:
	/**
	 * 修改器优先级（值越大，优先级越高，越优先执行，默认优先级为0）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	int32 Priority = 0;

	/**
	 * 修改器要修改的属性
	 */
	UPROPERTY(Meta = (GetOptions = "TcsGenericLibrary.GetAttributeNames"),
		EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	FName AttributeName = NAME_None;

	/**
	 * 修改器修改属性的方式
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	ETcsAttributeModifierMode ModifierMode = ETcsAttributeModifierMode::AMM_CurrentValue;

#pragma endregion


#pragma region Operation

public:
	/**
	 * 修改器操作数
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation")
	TMap<FName, float> Operands;

	/**
	 * 修改器执行器
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation")
	TSubclassOf<class UTcsAttributeModifierExecution> ModifierType;

	/**
	 * 修改器合并器
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation")
	TSubclassOf<class UTcsAttributeModifierMerger> MergerType;

#pragma endregion


public:
	// 构造函数
	UTcsAttributeModifierDefinitionAsset();

	// 覆写 GetPrimaryAssetId
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
