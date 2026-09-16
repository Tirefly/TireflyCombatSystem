// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Parameter/FTcsParamValueSource.h"
#include "Parameter/FTcsParamSource_Literal.h"
#include "FTcsParamValue.generated.h"



/**
 * 全插件统一"数值配置"载体（PV-1，取代 D2-12 FTcsParamScalar）：
 * 以 TInstancedStruct 持有参数值来源策略实例，默认 Literal 字面量源。
 * 消费面：Def 参数行/时值字段/模板 Operand 等全量切换（PV-6）；
 * 运行侧账本仍恒为已解析规范值（D2-13 不变式不动）。
 */
USTRUCT(BlueprintType)
struct TCSCORE_API FTcsParamValue
{
	GENERATED_BODY()

// 值来源策略
#pragma region Source

public:
	// 默认构造：Source 初始化为 Literal 字面量源（默认值 0）
	FTcsParamValue()
	{
		Source.InitializeAs<FTcsParamSource_Literal>();
	}

public:
	// 参数值来源策略（默认 Literal）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value")
	TInstancedStruct<FTcsParamValueSource> Source;

#pragma endregion

// 求值便利转发
#pragma region Evaluate

public:
	// 求值便利转发（PV-1 增补：消解调用点每次 .Source.Get() 的噪音；空载体兜 0——防 Source 失效后误用）
	double Evaluate(const FTcsParamEvaluateContext& Ctx) const
	{
		return Source.IsValid() ? Source.Get().Evaluate(Ctx) : 0.0;
	}

#pragma endregion
};
