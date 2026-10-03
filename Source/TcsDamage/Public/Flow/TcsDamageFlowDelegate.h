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
 * **反射面 = 宿主脚本插槽（2026-09-24，台账 SCRIPT-8）**：7 方法全带 `UFUNCTION(BlueprintNativeEvent)`
 * ⇒ 宿主可用**任意 UE 脚本语言**实现本契约（C# / AS / Luau / TS / 蓝图——UE 原生反射分发，
 * 天然语言无关，兑现 D4-17"语言无关执行器"）。
 * （2026-10-02 边界整肃把 5 方法扩到 7——新增 `ResolveExecutedDamage` / `IsLethal`，
 * 处置的是"框架替宿主下的模型结论"，见「执行与致死」区。）
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


// 执行量与致死线（撤销框架内定 —— 边界整肃 2026-10-02）
#pragma region Resolution

public:
	/**
	 * **吸收 → 执行量的映射**（宿主可替换）。默认实现 = 减法模型 + 零下限，**与整肃前行为一致**。
	 *
	 * **为什么必须有覆盖点**：`FMath::Max(0.0, ·)` 的**零下限**只引用数值、方向收敛到"不介入"
	 * ⇒ 属合法的**契约默认**；但 `候选 − 吸收` 这个**减法模型本身**是框架内定——宿主只能供
	 * "吸收了多少"，无法换成按比例分摊 / 带溢出上限 / 先扣护盾再溢出到生命等任何别的模型。
	 * **框架内定的处置不是删，而是给它一个覆盖点**（判据 = "这个默认值能不能被宿主换掉？
	 * 覆盖点在哪一行 API？"——答不出来就不是契约默认）。
	 *
	 * @param Target 目标实体句柄（多目标时取首个——映射是**流程级**取值，见调用点注释）。
	 * @param CandidateDamage 候选伤害量（黑板 `DamageFlowKey.BaseDamage` 的折叠结果）。
	 * @param AbsorbedDamage 本次被吸收量（本流程各目标的 `ModifyShield` 之和）。
	 * @param Context 流程上下文反射视图（宿主可读参与者/参数/分类 Tag）。
	 * @return 返回本次执行量。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Tcs|Damage|Flow")
	double ResolveExecutedDamage(FTcsCombatEntityHandle Target, double CandidateDamage, double AbsorbedDamage, const FTcsDamageFlowContextView& Context);
	virtual double ResolveExecutedDamage_Implementation(FTcsCombatEntityHandle Target, double CandidateDamage, double AbsorbedDamage, const FTcsDamageFlowContextView& Context)
	{
		return FMath::Max(0.0, CandidateDamage - AbsorbedDamage);
	}

	/**
	 * **致死线判定**（宿主可替换）。默认实现 = 当前值 `<= 0.0`，**与整肃前行为一致**。
	 *
	 * **为什么必须有覆盖点**（`damage-primitive`「伤害记录与事件」明文）：框架自称"记账而非裁定"
	 * （只写 `DamageFlowKey.Kill` 标志、不杀实体），但 `<= 0.0` 这个**阈值本身就是一次裁定**——
	 * 它断言了"生命归零即死亡"这一玩法模型。改为委托后，框架只保留中性默认，裁定权
	 * （"≤ 1 即致死"、无敌帧……）归宿主；框架**仍只记账**、MUST NOT 杀实体。
	 *
	 * @param Target 目标实体句柄。
	 * @param CurrentValueAfterApply 本次 M2 事务提交后该目标的属性当前值。
	 * @param Context 流程上下文反射视图。
	 * @return 判定为致死返回 true。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Tcs|Damage|Flow")
	bool IsLethal(FTcsCombatEntityHandle Target, double CurrentValueAfterApply, const FTcsDamageFlowContextView& Context);
	virtual bool IsLethal_Implementation(FTcsCombatEntityHandle Target, double CurrentValueAfterApply, const FTcsDamageFlowContextView& Context)
	{
		return CurrentValueAfterApply <= 0.0;
	}

#pragma endregion
};
