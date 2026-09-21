// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Flow/TcsDamageFlowContext.h"
#include "Handle/TcsCombatEntityHandle.h"

#include "TcsDamageFlowDelegate.generated.h"



// 伤害流程委托契约（宿主实现；机制层零公式——PV-7/D7-2"插件零公式"）
UINTERFACE(MinimalAPI)
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
	 * @param Context 流程上下文（宿主可读黑板/分类 Tag）。
	 * @return 返回基础命中率。
	 */
	virtual double GetBaseHitRate(FTcsCombatEntityHandle Attacker, FTcsCombatEntityHandle Target, const FTcsDamageFlowContext& Context)
	{
		return 1.0;
	}

	/**
	 * 基础暴击率（[0,1]）。默认 0.0（不暴击）。
	 */
	virtual double GetBaseCritRate(FTcsCombatEntityHandle Attacker, FTcsCombatEntityHandle Target, const FTcsDamageFlowContext& Context)
	{
		return 0.0;
	}

	/**
	 * 解析元素 Tag（写入分类 Tag 集，供修改器匹配）。默认空 Tag（无元素）。
	 */
	virtual FGameplayTag ResolveElement(FTcsCombatEntityHandle Attacker, FTcsCombatEntityHandle Target, const FTcsDamageFlowContext& Context)
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
	virtual double CalculateBaseDamage(double IncomingBase, FTcsCombatEntityHandle Attacker, FTcsCombatEntityHandle Target, const FTcsDamageFlowContext& Context)
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
	 * @param Context 流程上下文。
	 * @return 返回被吸收的伤害量（从执行量中扣除）。
	 */
	virtual double ModifyShield(FTcsCombatEntityHandle Target, double IncomingDamage, const FTcsDamageFlowContext& Context)
	{
		return 0.0;
	}

#pragma endregion
};
