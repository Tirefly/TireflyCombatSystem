// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Parameter/TcsParamValueSource.h"

#include "TcsParamSourceHost.generated.h"



// 参数源宿主插槽（PV-5 / R-1）：宿主定义"新的数值求值语义"的接入口
// （UINTERFACE + UFUNCTION(BlueprintNativeEvent) = UE 原生反射分发，脚本/蓝图/任意脚本语言均可实现）
UINTERFACE(MinimalAPI, Blueprintable)
class UTcsParamSourceHost : public UInterface
{
	GENERATED_BODY()
};

/**
 * 参数源宿主插槽（**宿主契约**，PV-5 / 台账 `R-1`）：让宿主（UnrealSharp / 蓝图 / 任意 UE 脚本语言）
 * 定义**新的数值求值语义**，而不是只能从既有源里挑一个。
 *
 * **为什么需要它**：`FTcsParamValueSource` 走 C++ 虚分派（`USTRUCT` 基类 + `virtual Evaluate`），
 * 脚本定义的 struct **物理不可达**（无 C++ 类型 ⇒ `CppStructOps == nullptr` ⇒ vtable 位为 0 ⇒ 野调用）。
 * 解法 = **两层结构**（同族先例 `ITcsTargetSelectorHost` / `ITcsTargetScorerHost`）：宿主实现本接口，
 * `FTcsParamSource_HostDelegate` 把它接进既有虚分派体系。**内置路径保持 C++ 快路径不变**。
 *
 * **与 `ITcsParamTableReader` 的需求边界**：那个接口覆盖"查表"（`ParamRef` 源按名读到自定义值）；
 * 本源覆盖"**新求值语义**"——"攻击力 = 基础 + 等级 × 成长 + 装备加成"这类要写逻辑的源。
 *
 * **形参可直接用 `FTcsParamEvaluateContext`**（`USTRUCT(BlueprintType)`、字段全反射、无 `TFunction`）——
 * 这是本插槽比选择器族简单的原因：选择器被迫"传句柄 + 门面访问器"是因为 `FTcsEffectContext`
 * 是非反射纯 C++ struct（作 `UFUNCTION` 形参会被 UHT 拒）。
 */
class ITcsParamSourceHost
{
	GENERATED_BODY()

// 求值契约
#pragma region Evaluate

public:
	/**
	 * 在给定上下文下求出规范值。
	 *
	 * **中性默认实现住接口声明处**（UHT 检测到声明即不生成 stub，`UhtFunction.cs:ImplFound`；
	 * 先例 = 引擎 `ISequencerAnimationOverride`）：C++ 侧实现方无需重复实现未覆写的方法。
	 *
	 * @param Context 参数求值上下文（只读语义：实现方 MUST NOT 反查实例或全局表）。
	 * @return 返回求得的规范值。
	 */
	UFUNCTION(BlueprintNativeEvent)
	double Evaluate(const FTcsParamEvaluateContext& Context);

	/**
	 * 本源是否允许配置行级"值约定"列（D5-18 v3 白名单的唯一真相）。
	 *
	 * **MUST 覆写为所需值**：默认真，与宿主新源的直觉（"我写的数就是结果"）一致；
	 * 若宿主的源读到的是**已是规范值**的东西（如某个内部计数器），须覆写为假。
	 *
	 * @return 返回本源是否允许配置值约定列。
	 */
	UFUNCTION(BlueprintNativeEvent)
	bool AllowsValueConvention();

#pragma endregion
};



/**
 * 参数源宿主转发器（PV-5 / `R-1` 的落地形态）：把宿主实现接进 `FTcsParamValueSource` 虚分派体系。
 *
 * **纯转发 + 空 Host 守卫**：两个虚函数都转发到 `Execute_*`（**漏掉能力位会让宿主新源在
 * `ValueConvention` 白名单校验上拿到错误的默认能力位**）；`Host` 为空时分别返回 0 / true
 * （空 Host 是配置缺口，不崩溃、不 ensure）。
 *
 * **载体与调用点零改动**：`FTcsParamValue.Source` 是裸 `FInstancedStruct`，照装本转发器；
 * 既有 `Evaluate` 调用点走 `GetPtr<FTcsParamValueSource>()` + 判空，本类作为子类自然命中。
 *
 * **本类是全插件第一个持 `TScriptInterface` 的\*参数源\***（`TScriptInterface` 此前只出现在
 * 求值上下文与选择器族转发器上）；glue 侧已证明该字段能真往返（读写代码非空壳）。
 */
USTRUCT(BlueprintType)
struct TCSCORE_API FTcsParamSource_HostDelegate : public FTcsParamValueSource
{
	GENERATED_BODY()

// 宿主
#pragma region Host

public:
	// 宿主实现（空 = 本源不可求值：`Evaluate` 返回 0、能力位按默认真）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value")
	TScriptInterface<ITcsParamSourceHost> Host;

#pragma endregion


// 求值
#pragma region Evaluate

public:
	// 求值转发：空 Host 守卫返回 0（配置缺口，不崩溃不 ensure）
	virtual double Evaluate(const FTcsParamEvaluateContext& Context) const override
	{
		UObject* HostObject = Host.GetObject();
		if (!HostObject)
		{
			return 0.0;
		}

		// `Execute_` 是必需的，不是风格选择：虚表直调会**静默跳过**脚本层实现
		return ITcsParamSourceHost::Execute_Evaluate(HostObject, Context);
	}

#pragma endregion


// 约定能力位
#pragma region ValueConvention

public:
	// 能力位转发：空 Host 守卫返回真（与基类默认一致——"我写的数就是结果"）
	virtual bool AllowsValueConvention() const override
	{
		UObject* HostObject = Host.GetObject();
		if (!HostObject)
		{
			return true;
		}

		return ITcsParamSourceHost::Execute_AllowsValueConvention(HostObject);
	}

#pragma endregion
};
