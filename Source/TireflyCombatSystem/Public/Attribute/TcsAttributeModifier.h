// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TcsSourceHandle.h"
#include "TcsAttributeModifier.generated.h"


class UTcsAttributeModifierDefinition;
class UTcsStateInstance;
class UTcsSkillEntry;



// 修改器修改属性的方式
UENUM(BlueprintType)
enum class ETcsAttributeModifierMode : uint8
{
	AMM_BaseValue			UMETA(ToolTip = "The base value of the attribute."),
	AMM_CurrentValue		UMETA(ToolTip = "The current value, modified by skill or buff, of the attribute."),
};



// 操作数到 StateParam 的运行时绑定描述
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsStateParamBinding
{
	GENERATED_BODY()

public:
	// 操作数标识（如 "Magnitude" — ModifierDef 内部约定）
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName OperandName;

	// 绑定的 StateParam（GameplayTag 标识）
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag StateParamTag;
};



// 属性修改器实例
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeModifierInstance
{
	GENERATED_BODY()

#pragma region Variables

public:
	// 修改器定义 DataAsset 硬引用
	UPROPERTY(BlueprintReadOnly)
	const UTcsAttributeModifierDefinition* ModifierDef = nullptr;

	// 修改器实例Id
	UPROPERTY(BlueprintReadOnly)
	int32 ModifierInstId = -1;

	// 修改器定义Id (用于合并分组和快速查询)
	UPROPERTY(BlueprintReadOnly)
	FName ModifierId = NAME_None;

	// 修改器来源句柄 (统一的来源追踪)
	UPROPERTY(BlueprintReadOnly)
	FTcsSourceHandle SourceHandle;

	// 修改器发起者
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> Instigator;

	// 修改器目标
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> Target;

	// 修改器操作数
	UPROPERTY(BlueprintReadOnly)
	TMap<FName, float> Operands;

	// 操作数动态绑定：运行时从 StateParam 读取 Operand 值
	UPROPERTY(BlueprintReadOnly)
	TArray<FTcsStateParamBinding> OperandBindings;

	// 直接引用——源 StateInstance（O(1) 最快路径）
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<UTcsStateInstance> SourceStateInstance;

	// 源 SkillEntry（AOE/投射物：技能已结束但 Entry 仍存活）
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<UTcsSkillEntry> SourceSkillEntry;

	// 修改器应用时间戳
	// NOTE: 当前单位为 UTC Ticks (FDateTime::GetTicks, 100ns)。这不是网络同步时间，仅用于排序/调试/本地归因。
	UPROPERTY(BlueprintReadOnly)
	int64 ApplyTimestamp = -1;

	// 修改器最新更新时间戳
	// NOTE: 当前单位为 UTC Ticks (FDateTime::GetTicks, 100ns)。这不是网络同步时间，仅用于排序/调试/本地归因。
	UPROPERTY(BlueprintReadOnly)
	int64 UpdateTimestamp = -1;

	// 本地变更批次号：用于把一次 Apply/Update 操作导致的变化归因到对应 SourceHandle。
	// NOTE: 这是"顺序/归因"序号，不是时间戳；未来网络同步不应直接依赖它。
	UPROPERTY(BlueprintReadOnly)
	int64 LastTouchedBatchId = -1;

#pragma endregion


#pragma region Constructors

public:
	FTcsAttributeModifierInstance() {}

#pragma endregion


#pragma region Functions

public:
	bool IsValid() const;

	bool operator==(const FTcsAttributeModifierInstance& Other) const
	{
		return ModifierId == Other.ModifierId
			&& ModifierInstId == Other.ModifierInstId;
	}

	bool operator!=(const FTcsAttributeModifierInstance& Other) const
	{
		return !(*this == Other);
	}

	bool operator<(const FTcsAttributeModifierInstance& Other) const;

#pragma endregion
};
