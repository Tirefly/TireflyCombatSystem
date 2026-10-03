// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "TcsStepRunSubChain.generated.h"



/**
 * 子链步骤（控制流原语）：起一条**已登记**的链。执行器住 `Private/Chain/TcsStepRunSubChain.cpp`
 * （+ 一行宏自注册）。
 *
 * **`ChainId` 是宿主词汇**：插件不声明任何具体链 id——链由宿主登记（`RegisterChain`），本步骤只持 id
 * （与 `FTcsEffectTriggerDef::EffectChainId` 同款分工）。未登记 → Error + 本步按完成处理（不挂起）；
 * 为空 → Warning + 本步按完成处理——两者都**不挂起**（挂起需要一个会来的唤醒源）。
 *
 * **黑板是快照、不是共享**：`ExecuteChain` 的形参是**值语义** ⇒ 子链拿到父链黑板的**拷贝**
 * （`Caster` / `Instigator` / `Targets` / `Variables` / `EventPayload` 都读得到），但子链对
 * `Variables` / `Targets` 的改写**不回传**父链——子链是**独立运行态**，两边各自演进。
 * 需要写父链时走门面按句柄访问器（`SetRunVariable` / `SetRunTargets` 写的就是**父**运行态）。
 *
 * **`bWait` 语义**（`FTcsStepBranch::bWait` 与之一字不差——两个原语**共用同一套"子链完成唤醒"实现**，
 * 见 `UTcsEffectSubsystem::RunChildChainStep`，规格明文 MUST NOT 各写一套）：
 * - `true`：子链**挂起**则本步挂起（`TSR_Running`，零每帧成本），子链结束时被唤醒 → 本步完成；
 *   子链**同步走完**（全即时步骤）时本步**不挂起**（不产生挂起-恢复往返）；子链**异常结束**
 *   （熔断 / 定义不可解析 / 未知步骤类型）**同样唤醒**并按完成处理（不唤醒 = 父永久挂起 = 死链，
 *   其诊断由子链自己的结束路径留痕）；
 * - `false`：放支线——起链后父立即前进，不跟踪子链结局（子链照常走自己的结束路径）。
 *
 * **子链不额外获得预算**：子链步数计入父链**本次进入**的 `MaxStepsPerFrame` 预算，嵌套深度另有
 * 固定上限（两条纪律见 `TcsChainRun.h`）——`A→B→A→B` 自激链会被拦下，而不是无限吃调用栈。
 */
USTRUCT()
struct TCSEFFECT_API FTcsStepRunSubChain
{
	GENERATED_BODY()

// 子链
#pragma region SubChain

public:
	// 子链 id（未登记 → Error + 按完成处理；为空 → Warning + 按完成处理）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|RunSubChain")
	FGameplayTag ChainId;

	// 是否等子链走完（false = 放支线、不跟踪其结局）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|RunSubChain")
	bool bWait = true;

#pragma endregion
};
