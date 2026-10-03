// Copyright Tirefly. All Rights Reserved.

#include "TcsEffectSubsystem.h"

#include "TcsEffectLogChannel.h"



// 运行态按句柄访问（脚本层插槽的"传句柄、不传上下文"手法，2026-09-24 台账 SCRIPT-8）
//
// **为什么需要这一组**：插槽接口的形参只能是反射类型，而 `FTcsEffectContext` 是非反射纯 C++
// struct（含 `FInstancedStruct` 事件载荷与 `TMap` 变量表）——脚本层拿不到它。故改为"传句柄 +
// 按句柄访问器读写"，**绕开上下文反射化（台账 SCRIPT-3）**。
//
// **悬空句柄语义统一**（口径同 `ResumeRun`，SCRIPT-7 先例）：代际失配/已释放是**正常时序竞态**，
// 不是配置错误 ⇒ 读口返回空值/无效句柄、写口返回 false + Warning，**一律不 ensure**。

TArray<FTcsCombatEntityHandle> UTcsEffectSubsystem::GetRunTargets(FTcsChainRunHandle Handle)
{
	ensure(IsInGameThread());

	// 代际校验先于解析（Resolve 对悬空句柄会 ensure——本组方法按"竞态不 ensure"口径自行前置校验）
	if (!RunPool.IsValid(Handle.GetInner()))
	{
		return TArray<FTcsCombatEntityHandle>();
	}

	const FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner());
	return Run ? Run->Context.Targets : TArray<FTcsCombatEntityHandle>();
}

bool UTcsEffectSubsystem::SetRunTargets(FTcsChainRunHandle Handle, const TArray<FTcsCombatEntityHandle>& InTargets)
{
	ensure(IsInGameThread());

	if (!RunPool.IsValid(Handle.GetInner()))
	{
		UE_LOG(LogTcsEffect, Warning, TEXT("UTcsEffectSubsystem::SetRunTargets: 句柄悬空或已释放（Index=%d, Generation=%d）——拒绝写入"),
			Handle.Index, Handle.Generation);
		return false;
	}

	if (FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner()))
	{
		Run->Context.Targets = InTargets;
		UE_LOG(LogTcsEffect, Verbose, TEXT("UTcsEffectSubsystem: 链 %s 目标集被改写为 %d 个（脚本层访问器）"),
			*Run->ChainId.ToString(), InTargets.Num());
		return true;
	}

	return false;
}

bool UTcsEffectSubsystem::TryGetRunVariable(FTcsChainRunHandle Handle, FGameplayTag Key, double& OutValue)
{
	ensure(IsInGameThread());

	if (!RunPool.IsValid(Handle.GetInner()))
	{
		return false;
	}

	const FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner());
	if (!Run)
	{
		return false;
	}

	// miss 时出参内容未定义（调用方不得使用——契约同 ITcsParamTableReader::TryGetNumericParam）
	if (const double* Found = Run->Context.Variables.Find(Key))
	{
		OutValue = *Found;
		return true;
	}

	return false;
}

bool UTcsEffectSubsystem::SetRunVariable(FTcsChainRunHandle Handle, FGameplayTag Key, double Value)
{
	ensure(IsInGameThread());

	if (!RunPool.IsValid(Handle.GetInner()))
	{
		UE_LOG(LogTcsEffect, Warning, TEXT("UTcsEffectSubsystem::SetRunVariable: 句柄悬空或已释放（Index=%d, Generation=%d）——拒绝写入"),
			Handle.Index, Handle.Generation);
		return false;
	}

	if (FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner()))
	{
		Run->Context.Variables.Add(Key, Value);
		return true;
	}

	return false;
}

FTcsCombatEntityHandle UTcsEffectSubsystem::GetRunCaster(FTcsChainRunHandle Handle)
{
	ensure(IsInGameThread());

	if (!RunPool.IsValid(Handle.GetInner()))
	{
		return FTcsCombatEntityHandle();
	}

	const FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner());
	return Run ? Run->Context.Caster : FTcsCombatEntityHandle();
}

FTcsCombatEntityHandle UTcsEffectSubsystem::GetRunInstigator(FTcsChainRunHandle Handle)
{
	ensure(IsInGameThread());

	if (!RunPool.IsValid(Handle.GetInner()))
	{
		return FTcsCombatEntityHandle();
	}

	const FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner());
	return Run ? Run->Context.Instigator : FTcsCombatEntityHandle();
}
