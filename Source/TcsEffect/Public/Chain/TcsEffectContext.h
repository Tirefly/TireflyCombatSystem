// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"



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


// 来源（R5 Task 6：链运行态的"身份 + 因果边"两件事 MUST 分开——
// 口径与判据见 `effect-interpreter` 能力的「链运行态来源锚点与因果边」需求）
#pragma region Attribution

public:
	// 本次运行的**来源锚点**（身份）：由本次运行产生的东西（`ApplyState` / `ModifyAttribute`）继承它
	// ⇒ 同一运行内同源（续杯）、跨运行异源（可叠层）。子链另发新号，不沿用父链的。
	FTcsSourceHandle RunSource;

	// 本次运行的**因果边**（溯源读数）：谁启动了我——触发行起链 = 该行的 `Source`（定义期登记的行是该
	// 行的来源句柄、状态施加期登记的行是该状态实例的级联锚点）；子链 = 父链的 `RunSource`；宿主直调可无。
	// **MUST NOT 参与撤销或共存判定**（原始版既有结论：父来源生命周期可先于子级结束、域之间不应按来源
	// 互相影响生命周期）——撤销一律按各实例自己的锚点。
	FTcsSourceHandle CausedBy;

#pragma endregion


// 数据
#pragma region Data

public:
	// 目标集（实体句柄——句柄无生命周期语义："目标还在不在"经注入接口 `IsAlive` 询问宿主，
	// 框架不持 Actor 生命周期引用）
	TArray<FTcsCombatEntityHandle> Targets;

	// 链内变量（SetVar 写入 / Branch 读取；**首个写入方 = `FTcsStepSetVar`**——
	// 此前"R3 无写入方，留位"的注记已作废；脚本层经门面 `SetRunVariable` 读写的是同一张表）
	TMap<FGameplayTag, double> Variables;

#pragma endregion
};
