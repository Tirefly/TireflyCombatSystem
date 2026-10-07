// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Parameter/TcsParamValue.h"
#include "TcsPhaseSpan.generated.h"



// 施法时段配置：任意段数，时段名称由宿主指定；瞬发技能使用空时段表。
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsPhaseSpan
{
	GENERATED_BODY()

// 时长
#pragma region Duration

public:
	// 时段时长的参数来源（秒；激活时从参数快照求值）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cast Phase|Duration")
	FTcsParamValue Duration;

#pragma endregion


// 时段状态
#pragma region State

public:
	// 当前时段是否允许打断（PhaseTable 查询档读取）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cast Phase|State")
	bool bInterruptible = false;

	// 当前时段是否允许移动（PhaseTable 查询档读取）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cast Phase|State")
	bool bCanMove = false;

	// 宿主定义的时段标识，不内置前摇、后摇等内容词汇。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cast Phase|State")
	FGameplayTag Tag;

#pragma endregion
};
