// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Def/TcsParamRow.h"
#include "TcsBoolSwitchRow.generated.h"



/**
 * 技能布尔开关行：与继承的数值参数行构成参数双表。
 * 当前消费者归技能层，故类型住 TcsSkill；取值模式复用 TcsState，不另立同义枚举。
 * Key 使用宿主声明的 TcsStateParam 参数键；GateCheck 与布尔修正器归 R6.5。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsBoolSwitchRow
{
	GENERATED_BODY()

// 参数键
#pragma region Key

public:
	// 布尔参数键（与数值参数表独立判重复）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bool Switch")
	FGameplayTag Key;

#pragma endregion


// 值与模式
#pragma region Value

public:
	// 开关基础值，宿主可按行配置。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bool Switch")
	bool Base = false;

	// 快照 = 激活时冻结；实时 = 每次读取账本。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bool Switch")
	ETcsParamMode Mode = ETcsParamMode::EPM_Snapshot;

#pragma endregion
};
