// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Attribute/TcsAttributeName.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"
#include "Parameter/TcsParamValue.h"

#include "Flow/TcsFlowAttributes.h"



/**
 * 瞬时流程上下文（09 §2.1 三层值空间；黑板即上下文——与 FEffectContext 同构）。
 * **纯运行态结构**（非反射）：由调用方构造、`RunTemplate` 就地使用，MUST NOT 跨帧持有。
 *
 * 三层值空间的落位：
 * 1. **技能参数**（账本，Entry 级持久）——不进流程（由 TcsSkill 侧解算后经下述第 2 层传入）；
 * 2. **公式参数初值** = `FormulaParams`（只读原料，**键 = 项目词表、永不用下标**；宿主公式按名取用）；
 * 3. **流程属性黑板** = `Blackboard`（伤害计算的工作值；流程用完即弃）。
 */
struct FTcsDamageFlowContext
{
// 参与者（**一律实体身份句柄**——D3-1 Actor 无关性；无 Actor 实体（未来 Mass）同样可跑流程）
#pragma region Subjects

public:
	// 攻击者
	FTcsCombatEntityHandle Attacker;

	// 发起者（表"谁发起的"，可与 Attacker 不同）
	FTcsCombatEntityHandle Instigator;

	// 目标集（句柄——"目标还在不在"经注入接口 `IsAlive` 询问宿主，框架不持 Actor 生命周期引用）
	TArray<FTcsCombatEntityHandle> Targets;

#pragma endregion


// 值空间
#pragma region Values

public:
	// 公式参数初值（只读原料；键 = 项目词表，永不用下标）
	TMap<FName, FTcsParamValue> FormulaParams;

	// 流程属性黑板（工作值；折叠走 TcsAttribute 共享纯函数）
	FTcsFlowAttributes Blackboard;

	// 分类 Tag 集（来源标签启动写入 / 元素标签由 Element 步骤写入——词表归项目，插件只搬运与匹配）
	TArray<FGameplayTag> ClassificationTags;

#pragma endregion


// 归属与捕获
#pragma region Scope

public:
	// 每流程唯一的来源锚点（`RunTemplate` 起流程时分配；作用域修改器挂 M2 账本时以它为 Source，
	// 流程结束 RemoveBySource 级联摘除——09 §2.1/§3）
	FTcsSourceHandle FlowSource;

	// 属性捕获快照（读默认 Live、捕获命中读快照——流程内读取一致性；
	// **快照填充归标准步骤（AttrCapture 配置）**，本层只落字段与语义）
	TMap<FTcsAttributeName, double> CapturedAttrs;

#pragma endregion
};
