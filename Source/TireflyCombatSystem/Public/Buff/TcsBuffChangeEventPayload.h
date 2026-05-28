// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TcsBuffChangeEventPayload.generated.h"



class UTcsBuffInstance;



// Buff 运行时变化批量事件数据
USTRUCT(BlueprintType)
struct FTcsBuffRuntimeDeltaEventPayload
{
	GENERATED_BODY()

public:
	// 发生变化的 Buff 实例
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UTcsBuffInstance> BuffInstance = nullptr;

	// 本批次内是否发生叠层变化
	UPROPERTY(BlueprintReadOnly)
	bool bStackCountChanged = false;

	// 叠层变化前的值
	UPROPERTY(BlueprintReadOnly)
	int32 OldStackCount = 0;

	// 叠层变化后的最终值
	UPROPERTY(BlueprintReadOnly)
	int32 NewStackCount = 0;

	// 本批次内是否发生最大叠层变化
	UPROPERTY(BlueprintReadOnly)
	bool bMaxStackCountChanged = false;

	// 最大叠层变化前的值
	UPROPERTY(BlueprintReadOnly)
	int32 OldMaxStackCount = 0;

	// 最大叠层变化后的最终值
	UPROPERTY(BlueprintReadOnly)
	int32 NewMaxStackCount = 0;

	// 本批次内是否发生周期变化
	UPROPERTY(BlueprintReadOnly)
	bool bPeriodChanged = false;

	// 周期变化前的值
	UPROPERTY(BlueprintReadOnly)
	float OldPeriod = 0.0f;

	// 周期变化后的最终值
	UPROPERTY(BlueprintReadOnly)
	float NewPeriod = 0.0f;

	// 本批次内是否发生持续时间刷新
	UPROPERTY(BlueprintReadOnly)
	bool bDurationRefreshed = false;

	// 持续时间刷新的最终剩余时长
	UPROPERTY(BlueprintReadOnly)
	float NewDuration = 0.0f;

public:
	FTcsBuffRuntimeDeltaEventPayload() {}

	explicit FTcsBuffRuntimeDeltaEventPayload(UTcsBuffInstance* InBuffInstance)
	{
		BuffInstance = InBuffInstance;
	}
};



// Buff 移除批量事件数据
USTRUCT(BlueprintType)
struct FTcsBuffRemovedEventPayload
{
	GENERATED_BODY()

public:
	// 被移除的 Buff 实例
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UTcsBuffInstance> BuffInstance = nullptr;

	// 本次移除原因
	UPROPERTY(BlueprintReadOnly)
	FName RemovalReason = NAME_None;

public:
	FTcsBuffRemovedEventPayload() {}

	FTcsBuffRemovedEventPayload(UTcsBuffInstance* InBuffInstance, FName InRemovalReason)
	{
		BuffInstance = InBuffInstance;
		RemovalReason = InRemovalReason;
	}
};