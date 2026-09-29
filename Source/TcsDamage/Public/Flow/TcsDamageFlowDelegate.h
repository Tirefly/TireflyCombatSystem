// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Flow/TcsDamageFlowContextView.h"
#include "Handle/TcsCombatEntityHandle.h"

#include "TcsDamageFlowDelegate.generated.h"



// 伤害流程委托契约（宿主实现；机制层零公式——PV-7/D7-2"插件零公式"）
UINTERFACE(MinimalAPI, Blueprintable)
class UTcsDamageFlowDelegate : public UInterface
{
	GENERATED_BODY()
};

/**
 * 伤害流程的宿主委托契约（09 §2.4）：**公式与宿主本体论全在宿主侧**，插件只提供挂点。
 *
 * **全部函数带中性默认实现**——"普通项目零 delegate"（PV-7 / D7-2 收窄）要求宿主什么都不实现也能跑通
 * 官方默认模板；`CalculateBaseDamage` 是**降级逃生口**（仅宿主有特殊公式时才实现），其默认实现
 * 即"不改动传入值"。
 *
 * 参与者一律实体身份句柄（承接 2026-09-20 句柄化——无 Actor 依赖，Mass 实体同样可委托）。
 *
 * **反射面 = 宿主脚本插槽（2026-09-24，台账 SCRIPT-8）**：5 方法全带 `UFUNCTION(BlueprintNativeEvent)`
 * ⇒ 宿主可用**任意 UE 脚本语言**实现本契约（C# / AS / Luau / TS / 蓝图——UE 原生反射分发，
 * 天然语言无关，兑现 D4-17"语言无关执行器"）。
 *
 * **为什么形参是 `FTcsDamageFlowContextView` 而不是 `FTcsDamageFlowContext`**：
 * 后者是**纯 C++ struct（无 `USTRUCT`）**——出现在 `UFUNCTION` 签名里会让 UHT 报
 * `Unable to find 'struct' with name ...`（同款既有实证：`UTcsEffectSubsystem::ExecuteChain`
 * 因此无法标记，只能另开 `ExecuteChainForCaster`）。视图是它的**可反射数据面投影**（单向）。
 *
 * **中性默认实现住接口声明处**：UHT 检测到接口类内的 `_Implementation` 声明则**不生成默认 stub**
 * （`UhtFunction.cs:681` 的 `ImplFound` 分支），故默认体可直接写在声明里——同款先例 = 引擎
 * `ISequencerAnimationOverride`（`SequencerAnimationOverride.h:31-44`）。
 *
 * **C++ 调用方 MUST 走 `Execute_<名>`**（MUST NOT 虚表直调）：虚表直调会**静默跳过**脚本层实现
 * （脚本覆写走 `ProcessEvent`），表现为"公式不生效"而非崩溃。`Execute_` 内部先查 `UFunction`
 * 走反射、查不到才回落原生 `_Implementation`（`TcsAttributeProvider.gen.cpp:141-157` 同款生成代码）
 * ⇒ **C++ 实现与脚本实现双轨并存**，宿主无须为插槽改动既有 C++ 实现。
 */
class ITcsDamageFlowDelegate
{
	GENERATED_BODY()

// 判定与解析
#pragma region Evaluate

public:
	/**
	 * 基础命中率（[0,1]）。默认 1.0（必中）。
	 *
	 * @param Attacker 攻击者实体句柄。
	 * @param Target 目标实体句柄。
	 * @param Context 流程上下文反射视图（宿主可读参与者/参数/分类 Tag）。
	 * @return 返回基础命中率。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Tcs|Damage|Flow")
	double GetBaseHitRate(FTcsCombatEntityHandle Attacker, FTcsCombatEntityHandle Target, const FTcsDamageFlowContextView& Context);
	virtual double GetBaseHitRate_Implementation(FTcsCombatEntityHandle Attacker, FTcsCombatEntityHandle Target, const FTcsDamageFlowContextView& Context)
	{
		return 1.0;
	}

	/**
	 * 基础暴击率（[0,1]）。默认 0.0（不暴击）。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Tcs|Damage|Flow")
	double GetBaseCritRate(FTcsCombatEntityHandle Attacker, FTcsCombatEntityHandle Target, const FTcsDamageFlowContextView& Context);
	virtual double GetBaseCritRate_Implementation(FTcsCombatEntityHandle Attacker, FTcsCombatEntityHandle Target, const FTcsDamageFlowContextView& Context)
	{
		return 0.0;
	}

	/**
	 * 解析元素 Tag（写入分类 Tag 集，供修改器匹配）。默认空 Tag（无元素）。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Tcs|Damage|Flow")
	FGameplayTag ResolveElement(FTcsCombatEntityHandle Attacker, FTcsCombatEntityHandle Target, const FTcsDamageFlowContextView& Context);
	virtual FGameplayTag ResolveElement_Implementation(FTcsCombatEntityHandle Attacker, FTcsCombatEntityHandle Target, const FTcsDamageFlowContextView& Context)
	{
		return FGameplayTag();
	}

	/**
	 * **降级逃生口**：宿主特殊公式。契约 = 接收输入值、返回基础伤害值——默认实现**原样返回输入**
	 * （即"不改动"，普通项目不实现本函数）。
	 *
	 * @param IncomingBase 调用方传入的基础值（链步骤 `DamageBase` 的解算结果）。
	 * @return 返回基础伤害值。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Tcs|Damage|Flow")
	double CalculateBaseDamage(double IncomingBase, FTcsCombatEntityHandle Attacker, FTcsCombatEntityHandle Target, const FTcsDamageFlowContextView& Context);
	virtual double CalculateBaseDamage_Implementation(double IncomingBase, FTcsCombatEntityHandle Attacker, FTcsCombatEntityHandle Target, const FTcsDamageFlowContextView& Context)
	{
		return IncomingBase;
	}

#pragma endregion


// 护盾 hook
#pragma region Shield

public:
	/**
	 * 护盾吸收（宿主 hook；不做护盾系统）。返回本次可吸收的伤害量（默认 0 = 无护盾）。
	 *
	 * @param Target 目标实体句柄。
	 * @param IncomingDamage 本次伤害量。
	 * @param Context 流程上下文反射视图。
	 * @return 返回被吸收的伤害量（从执行量中扣除）。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Tcs|Damage|Flow")
	double ModifyShield(FTcsCombatEntityHandle Target, double IncomingDamage, const FTcsDamageFlowContextView& Context);
	virtual double ModifyShield_Implementation(FTcsCombatEntityHandle Target, double IncomingDamage, const FTcsDamageFlowContextView& Context)
	{
		return 0.0;
	}

#pragma endregion
};
