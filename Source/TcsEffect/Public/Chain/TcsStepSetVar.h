// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Parameter/TcsParamValue.h"

#include "TcsStepSetVar.generated.h"



/**
 * 链内变量写入步骤（D4-3 元原语）：把 `FTcsParamValue` 的求值结果写进 `FTcsEffectContext::Variables`
 * ——**该字段的首个写入方**（此前"R3 无写入方，留位"的注记随之作废；门面按句柄访问器亦可读写同一张表）。
 *
 * **键空间（MUST 与门面共用）**：`VarKey` 与 `UTcsEffectSubsystem::SetRunVariable` / `TryGetRunVariable`
 * 读写的是**同一张表**，故 MUST 同住 `EffectChainRunVar` 根——分根会让"步骤写的变量脚本读不到"
 * （根段注册表与归属规则见 `gameplay-tag-governance` 能力）。
 *
 * **值 MUST 用 `FTcsParamValue`**（全插件统一数值配置载体）：可 Literal、可 ParamRef 等；
 * 退化成裸 `double` 字段会让"变量只能写字面量"成为隐式限制，与链上其它数值字段的书写能力不一致。
 *
 * 语义：**即时步骤**（恒 `TSR_Completed`，不挂起）；已存在的键**覆写**（顺序链的天然语义）。
 * 执行器住 `Private/Chain/TcsStepSetVar.cpp`（+ 一行宏自注册）。
 */
USTRUCT()
struct TCSEFFECT_API FTcsStepSetVar
{
	GENERATED_BODY()

// 变量写入
#pragma region Variable

public:
	// 变量键（`EffectChainRunVar` 根；无效时跳过写入 + Warning）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|SetVar")
	FGameplayTag VarKey;

	// 值（PV 载体——可 Literal / ParamRef，经参数求值上下文求出）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|SetVar")
	FTcsParamValue Value;

#pragma endregion
};
