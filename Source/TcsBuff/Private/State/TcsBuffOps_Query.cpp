// Copyright Tirefly. All Rights Reserved.

#include "State/TcsBuffOps.h"

#include "Def/TcsBuffDef.h"
#include "TcsBuffLogChannel.h"
#include "TcsBuffSubsystem.h"



// 查询
const FTcsBuffInstance* FTcsBuffOps::Find(UTcsBuffSubsystem& Subsystem, FTcsBuffInstanceHandle Handle)
{
	if (!Handle.IsValid())
	{
		return nullptr;
	}

	// 句柄不带实体段 ⇒ 先在全部桶里做一次代际校验式定位（脏句柄上 `Instance.Entity` 是陈旧值、
	// 定位必然失败）；桶数量级 = 在场实体数，且本函数不在热路径上。
	const FTcsBuffInstance* Found = nullptr;
	Subsystem.Registry.ForEachBucket([&Handle, &Found](FTcsCombatEntityHandle Entity, const FStateBucket& Bucket)
	{
		const FTcsBuffInstance* Instance = Bucket.Find(Handle);
		if (Instance && Instance->Entity == Entity)
		{
			Found = Instance;
			return false;
		}

		return true;
	});

	return Found;
}

const FTcsBuffDef* FTcsBuffOps::GetDef(UTcsBuffSubsystem& Subsystem, FGameplayTag DefTag)
{
	return Subsystem.GetRegisteredBuffDef(DefTag);
}

void FTcsBuffOps::Broadcast(
	UTcsBuffSubsystem& Subsystem,
	FGameplayTag EventTag,
	const FTcsBuffInstance& Instance,
	EBuffRemoveCause Cause)
{
	Subsystem.BroadcastStateEvent(EventTag, Instance, Cause);
}

void FTcsBuffOps::Transit(UTcsBuffSubsystem& Subsystem, FTcsBuffInstance* Instance, EBuffPhase NewPhase)
{
	Subsystem.TransitionPhase(Instance, NewPhase);
}

int32 FTcsBuffOps::CountStates(UTcsBuffSubsystem& Subsystem, FTcsCombatEntityHandle Entity)
{
	const FStateBucket* Bucket = Subsystem.Registry.FindBucket(Entity);
	return Bucket ? Bucket->Num() : 0;
}



// 生命周期操作
bool FTcsBuffOps::PrepareDurationOp(
	UTcsBuffSubsystem& Subsystem,
	FTcsBuffInstanceHandle Handle,
	FTcsBuffInstance*& OutInstance)
{
	OutInstance = nullptr;

	if (!Handle.IsValid())
	{
		UE_LOG(LogTcsBuff, Warning, TEXT("时长操作被拒：句柄无效（%d/%d）"), Handle.Index, Handle.Generation);
		return false;
	}

	const FTcsBuffInstance* Found = Find(Subsystem, Handle);
	if (!Found)
	{
		UE_LOG(LogTcsBuff, Warning, TEXT("时长操作被拒：句柄悬空（%d/%d）"), Handle.Index, Handle.Generation);
		return false;
	}

	// `Infinite` 没有到期条目可调 ⇒ **配置状态**不是错误（同"未注入 provider"的口径）：Warning + 无操作、不 ensure。
	// 定义未登记时同样无从判定时值策略——不猜，按无操作处理。
	const FTcsBuffDef* Def = Subsystem.GetRegisteredBuffDef(Found->DefTag);
	if (!Def)
	{
		UE_LOG(LogTcsBuff, Warning, TEXT("时长操作被拒：定义未在本世界登记（DefTag=%s）"), *Found->DefTag.ToString());
		return false;
	}

	if (Def->DurationPolicy == EBuffDurationPolicy::EBDP_Infinite)
	{
		UE_LOG(LogTcsBuff, Warning, TEXT("时长操作被拒：无限时值无到期条目可调（定义=%s 句柄=%d/%d）"),
			*Found->DefTag.ToString(), Handle.Index, Handle.Generation);
		return false;
	}

	FStateBucket* Bucket = Subsystem.Registry.FindBucket(Found->Entity);
	if (!Bucket)
	{
		return false;
	}

	// 定位成功后的实例指针在本函数返回前有效（期间无增删——调用方用完即走，不再跨调用持有）
	OutInstance = Bucket->Find(Handle);
	return OutInstance != nullptr;
}
