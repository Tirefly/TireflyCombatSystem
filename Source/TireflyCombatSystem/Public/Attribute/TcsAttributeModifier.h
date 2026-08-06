// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/TcsAttributeModifierApplication.h"
#include "TcsSourceHandle.h"
#include "TcsAttributeModifier.generated.h"


class UTcsAttributeModifierDefinition;
class UTcsStateInstance;



/** 由 StateInstance 持有的 Ongoing AttributeModifier 父实例。 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeModifierInstance
{
	GENERATED_BODY()

// Definition 与身份
#pragma region Identity

public:
	// AttributeModifier Definition DataAsset 硬引用。
	UPROPERTY(BlueprintReadOnly)
	const UTcsAttributeModifierDefinition* ModifierDef = nullptr;

	// Ongoing 父实例的稳定 ID。
	UPROPERTY(BlueprintReadOnly)
	int32 ModifierInstId = -1;

	// AttributeModifier Definition Id。
	UPROPERTY(BlueprintReadOnly)
	FName ModifierDefId = NAME_None;

#pragma endregion


// 来源与宿主
#pragma region Source

public:
	// 当前 Ongoing 父实例的来源句柄。
	UPROPERTY(BlueprintReadOnly)
	FTcsSourceHandle SourceHandle;

	// 持有并施加当前 Ongoing 父实例的本地 StateInstance。
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<UTcsStateInstance> OwningStateInstance;

#pragma endregion


// Operation 记录
#pragma region Operations

public:
	// 本父实例当前参与聚合的已求值 Operation 记录。
	UPROPERTY(BlueprintReadOnly)
	TArray<FTcsEvaluatedAttributeOperation> AppliedOperations;

	// 为后续受控重算保留的 Evaluator / Payload 覆写配置。
	UPROPERTY(BlueprintReadOnly)
	TMap<FName, FTcsAttributeModifierOperationOverride> OperationOverrides;

#pragma endregion


// 时间信息
#pragma region Timestamps

public:
	// Ongoing 父实例首次提交的 UTC Ticks 时间戳。
	UPROPERTY(BlueprintReadOnly)
	int64 ApplyTimestamp = -1;

	// Ongoing 父实例最近一次 Operation 重算的 UTC Ticks 时间戳。
	UPROPERTY(BlueprintReadOnly)
	int64 UpdateTimestamp = -1;

#pragma endregion


// 构造函数
#pragma region Construction

public:
	// 默认构造 Ongoing 父实例。
	FTcsAttributeModifierInstance() {}

#pragma endregion


// 查询与排序
#pragma region Query

public:
	// 检查父实例是否具备有效 Definition、Id、SourceHandle 和 StateInstance 宿主。
	bool IsValid() const;

	bool operator==(const FTcsAttributeModifierInstance& Other) const
	{
		return ModifierDefId == Other.ModifierDefId
			&& ModifierInstId == Other.ModifierInstId;
	}

	bool operator!=(const FTcsAttributeModifierInstance& Other) const
	{
		return !(*this == Other);
	}

	// 按 Definition Priority、Definition Id、实例 ID 稳定排序。
	bool operator<(const FTcsAttributeModifierInstance& Other) const;

#pragma endregion
};
