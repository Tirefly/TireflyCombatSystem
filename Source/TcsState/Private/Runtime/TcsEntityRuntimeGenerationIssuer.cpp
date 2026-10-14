// Copyright Tirefly. All Rights Reserved.

#include "Runtime/TcsEntityRuntimeGenerationIssuer.h"

#include "TcsStateLogChannel.h"



namespace
{
	// 出线到 TcsState DLL：跨桶、跨世界共用，世界拆解不复位。奇数为在册，偶数为释放态。
	// **文件前缀防 unity 合并撞名**（同模块多个 .cpp 的匿名 namespace 会被合并进同一 TU）。
	uint32 GTcsEntityRuntimeRegistryNextGeneration = 1;
}

uint32 FTcsStateGenerationIssuer::IssueAllocation(uint32 /*PriorGeneration*/)
{
	check(IsInGameThread());

	if (GTcsEntityRuntimeRegistryNextGeneration > static_cast<uint32>(MAX_int32))
	{
		// 反射句柄的 int32 上限不能回绕：即便 Shipping 也必须拒绝身份复用。
		UE_LOG(LogTcsState, Fatal, TEXT("运行态实例代际空间已耗尽，不能复用旧身份"));
	}

	const uint32 Generation = GTcsEntityRuntimeRegistryNextGeneration;
	GTcsEntityRuntimeRegistryNextGeneration += 2;
	return Generation;
}
