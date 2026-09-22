// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Parameter/TcsParamValueSource.h"

#include "GameplayTagContainer.h"
#include "Attribute/TcsAttributeProvider.h"

#include "TcsParamSource_AttributeScaled.generated.h"



/**
 * 属性域求值上下文（PV-1 扩展机制：结构体继承 + 源内 checked cast）：
 * 在 Core 求值上下文之外增持属性读口——Core 不得反向依赖域接口，故属性读口挂在派生上下文上。
 *
 * 上下文"单位"的落点说明：本契约的单位由实现者绑定（ITcsAttributeProvider 的签名不含单位，
 * 军官组件/Mass 桶适配器各绑自己的单位），故 R3 上下文以读口本身代表"对谁求值"；
 * PV-1 规划的 Subject/EffectiveLevel 字段随 TcsState 等级源同批进 Core 上下文。
 */
USTRUCT(BlueprintType)
struct TCSATTRIBUTE_API FTcsAttributeEvaluateContext : public FTcsParamEvaluateContext
{
	GENERATED_BODY()

// 类型标识
#pragma region Type

public:
	// 覆写类型标识（checked cast 锚点——派生上下文自报具体类型）
	virtual const UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}

#pragma endregion


// 属性读口
#pragma region Provider

public:
	// 属性读契约实现（该实现自身绑定单位；空 = 本上下文不提供属性读取，源落 Fallback）
	UPROPERTY(BlueprintReadWrite, Category = "Param Evaluate")
	TScriptInterface<ITcsAttributeProvider> Provider;

#pragma endregion
};



/**
 * 属性值来源（PV-3）：参数直接取属性值——`Value = Coefficient × Current(Attribute)`。
 *
 * Snapshot 语义（R3 唯一路径）：快照构建时求值一次并冻结，属性后续变化不追溯；
 * Live 模式与"读即登记依赖边"为已承诺基建，随 Live 化实现（R3 不做）。
 * 禁配值约定列（D5-18 v3）：约定作用在系数还是乘积上无定义——系数需要百分比语义直接写小数。
 */
USTRUCT(BlueprintType)
struct TCSATTRIBUTE_API FTcsParamSource_AttributeScaled : public FTcsParamValueSource
{
	GENERATED_BODY()

// 取值配置
#pragma region Value

public:
	// 被读取的属性
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value")
	FGameplayTag Attribute;

	// 换算系数（Value = Coefficient × Current(Attribute)）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value")
	double Coefficient = 1.0;

	// 兜底值（上下文不匹配或读口缺失时取用——可失败源必填，PV-1/V-3）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value")
	double Fallback = 0.0;

#pragma endregion


// 求值
#pragma region Evaluate

public:
	// 经属性域上下文读口取属性当前值；上下文不匹配（checked cast 失败）或读口为空落 Fallback
	virtual double Evaluate(const FTcsParamEvaluateContext& Context) const override
	{
		// 上下文不匹配 = 装配/配置错误（由校验矩阵兜底）——运行期只降级，不崩溃不 ensure
		if (!Context.GetScriptStruct()->IsChildOf(FTcsAttributeEvaluateContext::StaticStruct()))
		{
			return Fallback;
		}

		const FTcsAttributeEvaluateContext& AttributeContext = static_cast<const FTcsAttributeEvaluateContext&>(Context);
		UObject* ProviderObject = AttributeContext.Provider.GetObject();
		if (!ProviderObject)
		{
			return Fallback;
		}

		return Coefficient * ITcsAttributeProvider::Execute_GetCurrentValue(ProviderObject, Attribute);
	}

#pragma endregion


// 约定能力位
#pragma region ValueConvention

public:
	// 禁配值约定列：约定作用在系数还是乘积上无定义（D5-18 v3 白名单）
	virtual bool AllowsValueConvention() const override
	{
		return false;
	}

#pragma endregion
};
