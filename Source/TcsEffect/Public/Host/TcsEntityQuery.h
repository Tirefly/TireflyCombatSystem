// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Handle/TcsCombatEntityHandle.h"

#include "TcsEntityQuery.generated.h"



// 实体查询契约（TcsEffect 反射面；宿主/上层实现以承接实体能力——D4-14 反向依赖击穿）
UINTERFACE(MinimalAPI)
class UTcsEntityQuery : public UInterface
{
	GENERATED_BODY()
};

/**
 * 实体查询契约（机制层定义、宿主实现，D4-14 / 04 §2.1 / 10 §2.3）：TcsEffect 不持任何"世界有哪些
 * 战斗实体"的知识——遍历、定位、存活都是宿主本体论，由宿主/上层经本接口注入。
 *
 * **一切以实体身份句柄（`FTcsCombatEntityHandle`）为准**（三支能力见下）：链与流程只流动句柄，
 * **MUST NOT 依赖 Actor**——无 Actor 的实体（未来 Mass）因此天然可被表达（06 §33"Mass 适配核心零改动"
 * 的前提）。**句柄 → Actor 的映射归宿主**（谁注册实体谁掌握映射）；机制层需要定位时问 `GetLocation`，
 * 需要存活时问 `IsAlive`，不自行持有 Actor 生命周期引用。
 *
 * 哪些东西算战斗实体（单位 / 场景物 / 陷阱）由宿主实现决定——框架不裁决。
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
	 * 稳定序遍历当前全部战斗实体的**身份句柄**（顺序由宿主定义——调用方若需确定性，宿主须给稳定序）。
	 * 遍历期间不得增删实体；遍历所得句柄在后续使用时可能已失效，消费方自行用 `IsAlive` 核对。
	 *
	 * @param Visitor 访问者（逐实体回调，参数为实体句柄）。
	 */
	virtual void EnumerateEntities(TFunctionRef<void(FTcsCombatEntityHandle)> Visitor) = 0;

	/**
	 * 取实体定位（范围选择 / 表现 / 距离判定用）。
	 *
	 * @param Entity 实体句柄。
	 * @param OutLocation 实体定位出参。
	 * @return 返回是否取到定位（实体未知 / 无定位概念时返回 false）。
	 */
	virtual bool GetLocation(FTcsCombatEntityHandle Entity, FVector& OutLocation) = 0;

	/**
	 * 存活判定（**宿主本体论**：死亡规则归宿主——框架不认识"存活"）。
	 *
	 * @param Entity 实体句柄。
	 * @return 返回实体是否存活（未知句柄返回 false）。
	 */
	virtual bool IsAlive(FTcsCombatEntityHandle Entity) = 0;

#pragma endregion
};
