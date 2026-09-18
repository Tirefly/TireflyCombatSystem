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

// 类型标识
#pragma region Type

public:
	/**
	 * 具体上下文类型标识（PV-1 扩展机制"结构体继承 + 源内 checked cast"的锚点）。
	 * USTRUCT 无内建类型查询，故以虚函数承载（GAS `FGameplayEffectContext::GetScriptStruct` 同款机制）：
	 * 派生上下文覆写为自身类型，域侧源取值后做 `IsChildOf` 判定——**支持继续派生**（多层扩展上下文）。
	 * 不匹配 = 装配/配置错误（由源 × 上下文校验矩阵兜底），源侧只降级、不 ensure。
	 *
	 * @return 返回本上下文的具体 UScriptStruct。
	 */
	virtual const UScriptStruct* GetScriptStruct() const
	{
		return StaticStruct();
	}

#pragma endregion


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


// 约定能力位
#pragma region ValueConvention

public:
	/**
	 * 本源是否允许配置行级"值约定"列（D5-18 v3 白名单的唯一真相）。
	 * 判据 = "本源书写的数值是否就是结果"：Literal 与表型源为真（书写值直接成为结果，约定作用其上）；
	 * 引用源（ParamRef）为假（读到的已是规范值，再转 = 二次转换）；域侧换算源（如 AttributeScaled）
	 * 同样为假（约定作用在系数还是乘积上无定义）。
	 * 消费方（Def 资产数据校验等）一律走本虚函数——不得建立"源类型 × 可配约定"的中心名单或 switch
	 * （能力探测走虚分派，宿主自定义源自行声明、零公共代码改动——PV-10 同纪律）。
	 *
	 * @return 返回本源是否允许配置值约定列。
	 */
	virtual bool AllowsValueConvention() const
	{
		return true;
	}

#pragma endregion
};
