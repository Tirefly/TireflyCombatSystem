// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TcsSourceHandle.h"
#include "TcsAttributeModifier.generated.h"



// 修改器修改属性的方式
UENUM(BlueprintType)
enum class ETcsAttributeModifierMode : uint8
{
	AMM_BaseValue			UMETA(ToolTip = "The base value of the attribute."),
	AMM_CurrentValue		UMETA(ToolTip = "The current value, modified by skill or buff, of the attribute."),
};



// 属性修改器定义
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeModifierDefinition : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 修改器名称
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition")
	FName ModifierName = NAME_None;

	// 修改器标签
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Definition")
	FGameplayTagContainer Tags;

	// 修改器优先级（值越大，优先级越高，越优先执行，默认优先级为0）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	int32 Priority = 0;

	// 修改器要修改的属性
	UPROPERTY(Meta = (GetOptions = "TcsGenericLibrary.GetAttributeNames"),
		EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	FName AttributeName = NAME_None;

	// 修改器修改属性的方式
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	ETcsAttributeModifierMode ModifierMode = ETcsAttributeModifierMode::AMM_CurrentValue;

	// 修改器操作数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation")
	TMap<FName, float> Operands;

	// 修改器执行器
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation")
	TSubclassOf<class UTcsAttributeModifierExecution> ModifierType;

	// 修改器合并器
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operation")
	TSubclassOf<class UTcsAttributeModifierMerger> MergerType;

	FTcsAttributeModifierDefinition()
	{
		Operands.Add(FName("Magnitude"), 0.f);
	}
};



// 属性修改器实例
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeModifierInstance
{
	GENERATED_BODY()

#pragma region Variables

public:
	// 修改器定义
	UPROPERTY(BlueprintReadOnly)
	FTcsAttributeModifierDefinition ModifierDef;

	// 修改器实例Id
	UPROPERTY(BlueprintReadOnly)
	int32 ModifierInstId = -1;

	// 修改器定义Id (DataTable RowName, 用于合并分组)
	UPROPERTY(BlueprintReadOnly)
	FName ModifierId = NAME_None;

	// 修改器来源句柄 (统一的来源追踪)
	UPROPERTY(BlueprintReadOnly)
	FTcsSourceHandle SourceHandle;

	// 修改器来源 (冗余字段, 与 SourceHandle.SourceName 保持同步, 用于向后兼容)
	UPROPERTY(BlueprintReadOnly)
	FName SourceName = NAME_None;

	// 修改器发起者
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> Instigator;

	// 修改器目标
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> Target;

	// 修改器操作数
	UPROPERTY(BlueprintReadOnly)
	TMap<FName, float> Operands;

	// 修改器应用时间戳
	// NOTE: 当前单位为 UTC Ticks (FDateTime::GetTicks, 100ns)。这不是网络同步时间，仅用于排序/调试/本地归因。
	UPROPERTY(BlueprintReadOnly)
	int64 ApplyTimestamp = -1;

	// 修改器最新更新时间戳
	// NOTE: 当前单位为 UTC Ticks (FDateTime::GetTicks, 100ns)。这不是网络同步时间，仅用于排序/调试/本地归因。
	UPROPERTY(BlueprintReadOnly)
	int64 UpdateTimestamp = -1;

	// 本地变更批次号：用于把一次 Apply/Update 操作导致的变化归因到对应 SourceHandle。
	// NOTE: 这是“顺序/归因”序号，不是时间戳；未来网络同步不应直接依赖它。
	UPROPERTY(BlueprintReadOnly)
	int64 LastTouchedBatchId = -1;

#pragma endregion


#pragma region Constructors
	
public:
	FTcsAttributeModifierInstance(){}

#pragma endregion


#pragma region Functions

public:
	bool IsValid() const
	{
		return ModifierDef.ModifierName != NAME_None
			&& ModifierInstId >= 0;
	}

	bool operator==(const FTcsAttributeModifierInstance& Other) const
	{
		return ModifierDef.ModifierName == Other.ModifierDef.ModifierName
			&& ModifierInstId == Other.ModifierInstId;
	}

	bool operator!=(const FTcsAttributeModifierInstance& Other) const
	{
		return !(*this == Other);
	}

	bool operator<(const FTcsAttributeModifierInstance& Other) const
	{
		// Higher priority first.
		return ModifierDef.Priority > Other.ModifierDef.Priority;
	}

#pragma endregion
};
