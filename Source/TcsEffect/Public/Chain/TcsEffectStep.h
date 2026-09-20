// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"

#include "TcsEffectStep.generated.h"



/**
 * 步骤结果（D4-17 步内挂起协议）：执行器向解释器表达"本步是否完成"。
 * 与 PC 挂起（WaitEvent 类）、事件订阅并列为三种挂起形态的一种——挂起状态住运行态（FTcsChainRun），
 * 不活在调用栈里（D4-3 异步语义）。
 */
enum class ETcsStepResult : uint8
{
	// 本步已完成——解释器 PC 前进
	TSR_Completed = 0,

	// 本步未完成——解释器停驻原 PC，等唤醒源按运行态句柄重入（首入/被唤醒由步骤自行分辨）
	TSR_Running = 1
};



/**
 * 步骤容器（D4-3 / D4-16）：步骤类型 = **数据 struct**（15 原语按归属分住各模块），
 * 由 `FInstancedStruct` 承载——链的步骤数组直接存 `FInstancedStruct`，本 struct 供
 * "把步骤当字段"的场合（内联挂载 / 传递 / 子链引用一类）复用。
 *
 * **无公共基类（D4-16 有意）**：步骤类型之间不存在继承关系，故链的步骤数组不设 `BaseStruct`
 * 编辑器限定——编辑器 picker 可挂任意 struct，**类型合法性由执行器注册表在执行期判定**
 * （未注册即断链 + Error 日志，见 TcsEffectStepExecutor.h）。
 */
USTRUCT()
struct TCSEFFECT_API FTcsEffectStep
{
	GENERATED_BODY()

	// 步骤数据（具体类型由执行器注册表按反射类型分派）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect")
	FInstancedStruct StepData;
};
