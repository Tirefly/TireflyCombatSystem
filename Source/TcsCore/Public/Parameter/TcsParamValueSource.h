// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Parameter/TcsParamTableReader.h"
#include "TcsParamValueSource.generated.h"



/**
 * 参数求值上下文（PV-1）：参数源求值时的最小数据面，零领域词汇。
 * MUST 反射可见（宿主脚本扩展通道）——禁止 TFunction/std::function 等不可反射成员；
 * 领域扩展 = 结构体继承 + 源内 checked cast（PV-1）；Subject 句柄/级别位字段随 M2/M5 轮补齐。
 */
USTRUCT(BlueprintType)
struct TCSCORE_API FTcsParamEvaluateContext
{
	GENERATED_BODY()

// 参数表只读访问
#pragma region ParamTable

public:
	// 参数表只读访问接口（可空——空时 ParamRef 源落 Fallback）
	UPROPERTY(BlueprintReadWrite, Category = "Param Evaluate")
	TScriptInterface<ITcsParamTableReader> ParamTable;

#pragma endregion
};



/**
 * 参数值来源策略抽象基类（PV-1，D3-7 v3）：USTRUCT 反射基类 + C++ 虚函数分派（StateTree 同构）。
 * "抽象"由约定达成（StateTree FStateTreeConditionBase 同款）：UHT 为 USTRUCT 无条件生成
 * TCppStructOps 需要可默认构造——纯虚 (=0) 无法编译，故默认实现 + meta=(Hidden) 让编辑器
 * 类型 picker 不可选基类；运行侧消费方不应取到默认值（加载期校验兜底）。
 * 纯函数性纪律（D0-1）：同上下文同结果，随机/时间禁入。
 */
USTRUCT(meta = (Hidden))
struct TCSCORE_API FTcsParamValueSource
{
	GENERATED_BODY()

// 求值契约
#pragma region Evaluate

public:
	/**
	 * 在给定上下文下求出规范值。
	 *
	 * @param Context 参数求值上下文。
	 * @return 返回求得的规范值。
	 */
	virtual double Evaluate(const FTcsParamEvaluateContext& Context) const
	{
		return 0.0;
	}

#pragma endregion
};
