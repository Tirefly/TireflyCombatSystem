// Copyright Tirefly. All Rights Reserved.

#include "Flow/TcsFlowStepExecutorObject.h"

#include "TcsDamageLogChannel.h"



bool UTcsFlowStepExecutor::Execute_Implementation(const FInstancedStruct& StepData, const FTcsDamageFlowContextView& Context)
{
	// 中性默认实现：流程继续（本类是 Abstract，运行侧取到本实现属"派生类忘了覆写"）
	UE_LOG(LogTcsDamage, Warning, TEXT("UTcsFlowStepExecutor: 脚本流程步骤执行器未覆写 Execute（基类 %s）——本步按成功处理"),
		*GetClass()->GetName());
	return true;
}
