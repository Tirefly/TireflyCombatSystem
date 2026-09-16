// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Parameter/FTcsParamValueSource.h"
#include "FTcsParamSource_ParamRef.generated.h"



/**
 * 参数引用源（PV-2）：引用同域参数表的另一键（参数嵌套，链式允许 A→B→C）。
 * 自引用/成环 = 编辑期/加载期报错（编辑期 DAG 去重——M8 校验器职责，本文件不内置）；
 * 限同域参数表（跨域引用维持 D2-11 域不透明拒绝理由）。
 */
USTRUCT(BlueprintType)
struct TCSCORE_API FTcsParamSource_ParamRef : public FTcsParamValueSource
{
	GENERATED_BODY()

// 引用配置
#pragma region Reference

public:
	// 引用的参数键（限同域参数表——PV-2.b）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value")
	FName Key = NAME_None;

	// 兜底值（键 miss 或上下文无参数表时取用；必填——PV-2.c，编辑期校验兜底缺失）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value")
	double Fallback = 0.0;

#pragma endregion

// 求值
#pragma region Evaluate

public:
	// 经上下文参数表取键值；接口为空或 miss 落 Fallback（PV-2）
	virtual double Evaluate(const FTcsParamEvaluateContext& Context) const override
	{
		double OutValue = 0.0;
		UObject* TableObject = Context.ParamTable.GetObject();
		if (TableObject && ITcsParamTableReader::Execute_TryGetNumericParam(TableObject, Key, OutValue))
		{
			return OutValue;
		}

		return Fallback;
	}

#pragma endregion
};
