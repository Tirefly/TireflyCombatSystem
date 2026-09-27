// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsStepExecutor.h"

#include "TcsEffectLogChannel.h"



ETcsStepResult UTcsStepExecutor::Execute_Implementation(FGameplayTag ChainId, FTcsChainRunHandle Run, const FInstancedStruct& StepData)
{
	// 中性默认实现：按完成处理。
	// 本类是 Abstract（不可实例化）——运行侧取到本实现属代码缺陷（派生类忘了覆写），故留 Warning 而非静默。
	UE_LOG(LogTcsEffect, Warning, TEXT("UTcsStepExecutor: 链 %s 的脚本执行器未覆写 Execute（基类 %s）——本步按完成处理"),
		*ChainId.ToString(), *GetClass()->GetName());
	return ETcsStepResult::TSR_Completed;
}
