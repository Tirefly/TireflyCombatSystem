// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "Handle/TcsCombatEntityHandle.h"



/**
 * 效果链黑板（D4-3 "黑板即上下文"；纯运行态结构，非反射数据）：
 * 链执行期间步骤之间传递信息的唯一载体——起链时装配、随运行态（FTcsChainRun）自持。
 *
 * 归口说明（04 §2.1 的全量形态分流）：
 * - **属性捕获（CapturedAttrs）不住这里**——它归 TcsDamage 的流程 Context（09 文档 AttrCapture 节）；
 * - **注入能力引用（实体查询等）不住这里**——它归门面注入点（链构建时按需取用），黑板只持本次执行的数据。
 *
 * 目标集默认来源（D4-4 v2"Context 默认目标初始化 = 事件目标"）：R3 无事件触发源（手动触发），
 * 由调用方预填 `Targets`；事件载荷 → 目标 的通路随触发行轮落地（M3/M5）。
 */
struct FTcsEffectContext
{
// 主体（**一律实体身份句柄**——D3-1 Actor 无关性；无 Actor 实体（未来 Mass）同样可表达）
#pragma region Subjects

public:
	// 施法者（技能/效果的来源方）
	FTcsCombatEntityHandle Caster;

	// 发起者（连续技/连锁的上一环；表"谁发起的"，可与 Caster 不同）
	FTcsCombatEntityHandle Instigator;

	// 触发事件载荷（无事件触发时为默认构造——R3 手动触发的常态）
	FInstancedStruct EventPayload;

#pragma endregion


// 数据
#pragma region Data

public:
	// 目标集（实体句柄——句柄无生命周期语义："目标还在不在"经注入接口 `IsAlive` 询问宿主，
	// 框架不持 Actor 生命周期引用）
	TArray<FTcsCombatEntityHandle> Targets;

	// 链内变量（SetVar / Branch 类步骤的载体；R3 无写入方，留位）
	TMap<FGameplayTag, double> Variables;

#pragma endregion
};
