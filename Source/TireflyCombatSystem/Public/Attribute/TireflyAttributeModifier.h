// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TireflyAttributeModifier.generated.h"



// 修改器修改属性的方式
UENUM(BlueprintType)
enum class ETireflyAttributeModifierMode : uint8
{
	BaseValue			UMETA(ToolTip = "The base value of the attribute."),
	CurrentValue		UMETA(ToolTip = "The current value, modified by skill or buff, of the attribute."),
};



// 属性修改器定义
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTireflyAttributeModifierDefinition : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 修改器名称
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition")
	FName ModifierName = NAME_None;

	// 修改器标签
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition")
	FGameplayTagContainer Tags;

	// 修改器优先级（值越小，优先级越高）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	int32 Priority = 0;

	// 修改器要修改的属性
	UPROPERTY(Meta = (GetOptions = "TireflyCombatSystemLibrary.GetAttributeNames"),
		EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	FName AttributeName = NAME_None;

	// 修改器修改属性的方式
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	ETireflyAttributeModifierMode ModifierMode = ETireflyAttributeModifierMode::CurrentValue;

	// 修改器操作数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation")
	TArray<FName> OperandNames = { TEXT("Magnitude") };

	// 修改器执行器
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation")
	TSubclassOf<class UTireflyAttributeModifierExecution> ModifierType;

	// 修改器合并器
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation")
	TSubclassOf<class UTireflyAttributeModifierMerger> MergerType;
};



// 属性修改器实例
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTireflyAttributeModifierInstance
{
	GENERATED_BODY()

#pragma region Variables

public:
	// 修改器定义
	UPROPERTY(BlueprintReadOnly)
	FTireflyAttributeModifierDefinition ModifierDef;

	// 修改器实例Id
	UPROPERTY(BlueprintReadOnly)
	int32 ModifierInstId = -1;

	// 修改器来源
	UPROPERTY(BlueprintReadOnly)
	FName SourceName = NAME_None;

	// 修改器发起者
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> Instigator;

	// 修改器目标
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> Target;

	// 修改器操作数
	UPROPERTY(BlueprintReadOnly)
	TMap<FName, float> Operands;

	// 修改器应用时间戳
	UPROPERTY(BlueprintReadOnly)
	int64 ApplyTimestamp = -1;

	// 修改器最新更新时间戳
	UPROPERTY(BlueprintReadOnly)
	int64 UpdateTimestamp = -1;

#pragma endregion


#pragma region Constructors
	
public:
	FTireflyAttributeModifierInstance(){}

#pragma endregion


#pragma region Functions

public:
	bool IsValid() const
	{
		return ModifierDef.ModifierName != NAME_None
			&& ModifierInstId >= 0;
	}

	bool operator==(const FTireflyAttributeModifierInstance& Other) const
	{
		return ModifierDef.ModifierName == Other.ModifierDef.ModifierName
			&& ModifierInstId == Other.ModifierInstId;
	}

	bool operator!=(const FTireflyAttributeModifierInstance& Other) const
	{
		return !(*this == Other);
	}

	bool operator<(const FTireflyAttributeModifierInstance& Other) const
	{
		return ModifierDef.Priority < Other.ModifierDef.Priority;
	}

#pragma endregion
};
