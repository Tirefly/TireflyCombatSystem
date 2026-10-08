// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillOps.h"

#include "Parameter/TcsParamValueSource.h"
#include "TcsValueConvention.h"

#include "Def/TcsBoolSwitchRow.h"
#include "Def/TcsParamRow.h"
#include "Def/TcsSkillDefData.h"

#include "TcsSkillSubsystem.h"



// 等级
int32 FTcsSkillOps::GetLevel(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle Handle,
	const TArray<double>& LevelModifiers)
{
	const FTcsLearnedSkillEntry* Entry = Subsystem.Registry.Find(Unit, Handle);
	if (!Entry)
	{
		// 脏句柄 = 时序竞态（不 ensure）；返回 0 而不是条目等级的"合理猜测"——调用方应先用句柄有效性把关
		return 0;
	}

	// `LevelBase` 取自**已登记定义**（未登记说明条目是在注销前建的；按 0 处理，不猜）
	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(Entry->DefTag);
	const int32 LevelBase = Def ? Def->LevelBase : 0;

	// `EffectiveLevel = clamp(0, LevelBase + Σ参数账本 Level 键修正)`
	// 修正器列表由形参传入——**本轮 `Level` 键的写入面由参数链轮提供**，
	// 故本函数 MUST NOT 依赖条目结构体持该字段（见 TcsSkillRegistry.h 的字段边界说明）。
	double Sum = static_cast<double>(LevelBase);
	for (const double Modifier : LevelModifiers)
	{
		Sum += Modifier;
	}

	// clamp 下界到 0：负等级不是合法语义（同设计 D3-11 的 `clamp(0, …)` 原文）
	return FMath::Max(0, FMath::FloorToInt(Sum));
}



// 数值参数表
bool FTcsSkillOps::GetNumericParam(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle Handle,
	FGameplayTag Key,
	double& OutValue)
{
	const FTcsLearnedSkillEntry* Entry = Subsystem.Registry.Find(Unit, Handle);
	if (!Entry)
	{
		return false;
	}

	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(Entry->DefTag);
	if (!Def)
	{
		return false;
	}

	for (const FTcsNumericParamRow& Row : Def->Params)
	{
		if (Row.Key != Key)
		{
			continue;
		}

		// 逐行求值：上下文主体 = 该条目所属单位，生效等级 = 本条目的持久等级
		// （本轮快照化归参数链轮；此处按"当前账本值"求值，语义 = Live 通道）
		FTcsParamEvaluateContext Ctx;
		Ctx.Subject = Entry->Unit;
		Ctx.Instigator = Entry->Unit;
		Ctx.EffectiveLevel = Entry->Level;

		const double RawValue = Row.Base.Evaluate(Ctx);

		// 值约定转换 = 写入点唯一一次（D5-18 v2：账本内永远规范值）。
		// **能力位为假的源不转**（判据由源自身声明，MUST NOT 建"源类型 × 可配约定"的中心名单）
		const bool bAllowConvention =
			Row.Base.Source.GetPtr<FTcsParamValueSource>()
			&& Row.Base.Source.GetPtr<FTcsParamValueSource>()->AllowsValueConvention();

		OutValue = bAllowConvention
			? FTcsValueConvention::ConvertToCanonical(RawValue, static_cast<int32>(Row.ValueConvention))
			: RawValue;
		return true;
	}

	// miss：**MUST NOT** 静默返回 0 当命中——调用方要能区分"命中且为 0"与"没配这个键"
	return false;
}



// 布尔开关表
bool FTcsSkillOps::IsSwitchSet(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle Handle,
	FGameplayTag Key,
	bool& OutValue)
{
	const FTcsLearnedSkillEntry* Entry = Subsystem.Registry.Find(Unit, Handle);
	if (!Entry)
	{
		return false;
	}

	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(Entry->DefTag);
	if (!Def)
	{
		return false;
	}

	for (const FTcsBoolSwitchRow& Row : Def->BoolSwitches)
	{
		if (Row.Key == Key)
		{
			OutValue = Row.Base;
			return true;
		}
	}

	// **两表互不兜底**：本函数只看布尔开关表，MUST NOT 回落到数值参数表（反之亦然）
	return false;
}
