// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateOps.h"

#include "Def/TcsBuffDef.h"
#include "TcsStateLogChannel.h"
#include "TcsStateSubsystem.h"



// 查询
const FTcsStateInstance* FTcsStateOps::Find(UTcsStateSubsystem& Subsystem, FTcsStateHandle Handle)
{
	if (!Handle.IsValid())
	{
		return nullptr;
	}

	// 句柄不带单位段 ⇒ 先在全部桶里做一次代际校验式定位（脏句柄上 `Instance.Unit` 是陈旧值、
	// 定位必然失败）；桶数量级 = 在场单位数，且本函数不在热路径上。
	const FTcsStateInstance* Found = nullptr;
	Subsystem.Registry.ForEachBucket([&Handle, &Found](FTcsCombatEntityHandle Unit, const FStateBucket& Bucket)
	{
		const FTcsStateInstance* Instance = Bucket.Find(Handle);
		if (Instance && Instance->Unit == Unit)
		{
			Found = Instance;
			return false;
		}

		return true;
	});

	return Found;
}

const FTcsBuffDef* FTcsStateOps::GetDef(UTcsStateSubsystem& Subsystem, FGameplayTag DefTag)
{
	return Subsystem.GetRegisteredStateDef(DefTag);
}

void FTcsStateOps::Broadcast(
	UTcsStateSubsystem& Subsystem,
	FGameplayTag EventTag,
	const FTcsStateInstance& Instance,
	EStateRemoveCause Cause)
{
	Subsystem.BroadcastStateEvent(EventTag, Instance, Cause);
}

void FTcsStateOps::Transit(UTcsStateSubsystem& Subsystem, FTcsStateInstance* Instance, EStatePhase NewPhase)
{
	Subsystem.TransitionPhase(Instance, NewPhase);
}

int32 FTcsStateOps::CountStates(UTcsStateSubsystem& Subsystem, FTcsCombatEntityHandle Unit)
{
	const FStateBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	return Bucket ? Bucket->Num() : 0;
}



// 生命周期操作
bool FTcsStateOps::PrepareDurationOp(
	UTcsStateSubsystem& Subsystem,
	FTcsStateHandle Handle,
	FTcsStateInstance*& OutInstance)
{
	OutInstance = nullptr;

	if (!Handle.IsValid())
	{
		UE_LOG(LogTcsState, Warning, TEXT("时长操作被拒：句柄无效（%d/%d）"), Handle.Index, Handle.Generation);
		return false;
	}

	const FTcsStateInstance* Found = Find(Subsystem, Handle);
	if (!Found)
	{
		UE_LOG(LogTcsState, Warning, TEXT("时长操作被拒：句柄悬空（%d/%d）"), Handle.Index, Handle.Generation);
		return false;
	}

	// `Infinite` 没有到期条目可调 ⇒ **配置状态**不是错误（同"未注入 provider"的口径）：Warning + 无操作、不 ensure。
	// 定义未登记时同样无从判定时值策略——不猜，按无操作处理。
	const FTcsBuffDef* Def = Subsystem.GetRegisteredStateDef(Found->DefTag);
	if (!Def)
	{
		UE_LOG(LogTcsState, Warning, TEXT("时长操作被拒：定义未在本世界登记（DefTag=%s）"), *Found->DefTag.ToString());
		return false;
	}

	if (Def->DurationPolicy == EDurationPolicy::EDP_Infinite)
	{
		UE_LOG(LogTcsState, Warning, TEXT("时长操作被拒：无限时值无到期条目可调（定义=%s 句柄=%d/%d）"),
			*Found->DefTag.ToString(), Handle.Index, Handle.Generation);
		return false;
	}

	FStateBucket* Bucket = Subsystem.Registry.FindBucket(Found->Unit);
	if (!Bucket)
	{
		return false;
	}

	// 定位成功后的实例指针在本函数返回前有效（期间无增删——调用方用完即走，不再跨调用持有）
	OutInstance = Bucket->Find(Handle);
	return OutInstance != nullptr;
}
