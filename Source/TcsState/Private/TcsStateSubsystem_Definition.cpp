// Copyright Tirefly. All Rights Reserved.

#include "TcsStateSubsystem.h"

#include "TcsStateLogChannel.h"



// 定义登记
bool UTcsStateSubsystem::RegisterStateDef(FGameplayTag DefTag, const FTcsBuffDef& Def)
{
	// 身份来自资产（`UTcsStateDef::DefTag`）——由调用方传入，不从内容里读（理由见头文件）
	if (!DefTag.IsValid())
	{
		UE_LOG(LogTcsState, Error, TEXT("状态定义登记被拒：DefTag 无效"));
		return false;
	}

	if (RegisteredDefs.Contains(DefTag))
	{
		// 不静默覆写（口径同链 / 触发定义登记）——同一身份两个定义是加载期错误
		UE_LOG(LogTcsState, Error, TEXT("状态定义登记被拒：%s 在本世界已登记（不静默覆写）"),
			*DefTag.ToString());
		return false;
	}

	// 按值拷入（TUniquePtr 持有使解析返回的指针地址稳定，且内容不与定义库共享生命周期）
	RegisteredDefs.Add(DefTag, MakeUnique<FTcsBuffDef>(Def));

	return true;
}

bool UTcsStateSubsystem::UnregisterStateDef(FGameplayTag DefTag)
{
	// 已建的实例持有的是 `DefTag`（身份）而非定义指针 ⇒ 注销只影响"新施加能否解析到定义"
	if (RegisteredDefs.Remove(DefTag) > 0)
	{
		return true;
	}

	UE_LOG(LogTcsState, Warning, TEXT("状态定义注销：%s 未登记（正常清理路径）"), *DefTag.ToString());
	return false;
}

const FTcsBuffDef* UTcsStateSubsystem::GetRegisteredStateDef(FGameplayTag DefTag) const
{
	// 未登记返回 nullptr——正常查询路径，不 ensure（同族的 ResolveStateDef / FindChain 同款）
	const TUniquePtr<FTcsBuffDef>* Found = RegisteredDefs.Find(DefTag);
	return Found ? Found->Get() : nullptr;
}

int32 UTcsStateSubsystem::GetRegisteredStateDefCount() const
{
	return RegisteredDefs.Num();
}
