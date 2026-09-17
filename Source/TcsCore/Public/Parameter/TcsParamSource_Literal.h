// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Parameter/TcsParamValueSource.h"
#include "TcsParamSource_Literal.generated.h"



/**
 * 字面量源（PV-2）：直接返回配置值，恒求值成功（无需 Fallback）。
 * 值为"配置态书写值"——无约定列（VCF_None）时即规范值（恒等）；配约定列时由内容模块
 * 在物化/注册边界调用 ConvertToCanonical 转规范值（D5-18 v3，2026-09-14）。
 * 命名沿用 Literal 不用 Constant——与全系统既有词汇一致（用户拍板）。
 */
USTRUCT(BlueprintType)
struct TCSCORE_API FTcsParamSource_Literal : public FTcsParamValueSource
{
	GENERATED_BODY()

// 字面量值
#pragma region Value

public:
	// 字面量值（配置态书写值；无约定列时即规范值——D5-18 v3）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value")
	double Value = 0.0;

public:
	// 恒返回 Value（纯函数性，与上下文无关）
	virtual double Evaluate(const FTcsParamEvaluateContext& /*Context*/) const override
	{
		return Value;
	}

#pragma endregion
};
