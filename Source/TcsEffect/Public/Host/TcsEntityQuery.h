// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "TcsEntityQuery.generated.h"



// 实体查询契约（TcsEffect 反射面；宿主/上层实现以承接实体遍历——D4-14 反向依赖击穿）
UINTERFACE(MinimalAPI)
class UTcsEntityQuery : public UInterface
{
	GENERATED_BODY()
};

/**
 * 实体查询契约（机制层定义、宿主实现，D4-14 / 04 §2.1）：TcsEffect 不持任何"世界有哪些战斗实体"
 * 的知识——遍历、存活、阵营都是宿主本体论，由宿主/上层经本接口注入（军官组件 / Mass 桶适配，
 * M6 世界注册表落地后接中央注册表）。
 *
 * R3 只声明遍历一个成员：竖切的两条默认策略（Self / EventTarget）都不需要枚举，
 * 设计名 `ICombatEntityQuery` 的其余成员（`GetLocation` / `IsAlive`）随"范围选择（RadiusArea）"
 * 消费者落地（后置，零消费者不预建）。
 *
 * C++ 专用面：参数含 `TFunctionRef`，蓝图不可表达（与 R0 §9"蓝图不承诺"一致）。
 */
class ITcsEntityQuery
{
	GENERATED_BODY()

// 查询
#pragma region Query

public:
	/**
	 * 稳定序遍历当前全部战斗实体（顺序由宿主定义——调用方若需确定性，宿主须给稳定序）。
	 *
	 * @param Visitor 访问者（逐实体回调；遍历期间不得增删实体）。
	 */
	virtual void EnumerateEntities(TFunctionRef<void(AActor*)> Visitor) = 0;

#pragma endregion
};
