// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsParamChain.h"

#include "Attribute/TcsAttributeBandFold.h"
#include "Parameter/TcsParamValue.h"
#include "Parameter/TcsParamValueSource.h"
#include "TcsValueConvention.h"

#include "Def/TcsParamRow.h"
#include "Def/TcsSkillDefData.h"
#include "Param/TcsStateParamKeys.h"
#include "TcsSkillLogChannel.h"
#include "TcsSkillSubsystem.h"



namespace
{
	/**
	 * `Level` 折叠用的**专用运算带条目收集器**与普通参数键**共用**（见下）。
	 *
	 * 竞争组选优（D5-19 读侧竞争组）：组内解析值最大者进折叠，未分组者恒进。
	 */
	bool IsWinningParamChainCompetitor(const TArray<FTcsNumericParamModInstance>& Instances, int32 Index)
	{
		const FTcsNumericParamModInstance& Candidate = Instances[Index];
		if (!Candidate.CompeteGroup.IsValid())
		{
			// 未分组者恒进折叠（分组只在"同组互斥"语义下起作用）
			return true;
		}

		for (int32 Other = 0; Other < Instances.Num(); ++Other)
		{
			if (Other == Index)
			{
				continue;
			}

			const FTcsNumericParamModInstance& Rival = Instances[Other];
			if (Rival.CompeteGroup != Candidate.CompeteGroup)
			{
				continue;
			}

			// **组内解析值最大者进折叠**；数值打平时先出现者胜——这一步必须是**确定的全序**，
			// 否则同一账本两次求值可能选出不同的条（折叠本身顺序无关，但"谁进折叠"不由折叠器管）
			if (Rival.ResolvedValue > Candidate.ResolvedValue)
			{
				return false;
			}
		}

		return true;
	}

	/**
	 * **全模块唯一一处 `FoldTcsAttributeBands` 调用**（`STAT-1` 收束的落点）。
	 *
	 * 普通参数键与特殊键 `Level` 都经本函数折叠——**这正是"特殊键"的准确含义**：
	 * 特殊之处在于**它改的是等级而不是参数表读数**，而不在于它另有一套算术
	 * （那会造出第二份折叠语义，正是 `STAT-1` 要防的）。
	 *
	 * 摊平规则：同键条目 → 竞争组选优 → `FTcsAttributeBandEntry{Op, Value, OverridePriority}`。
	 */
	double FoldParamChainInstances(
		const TArray<FTcsNumericParamModInstance>& Instances,
		FGameplayTag Key,
		double BaseValue)
	{
		TArray<FTcsAttributeBandEntry> Entries;
		Entries.Reserve(Instances.Num());

		for (int32 Index = 0; Index < Instances.Num(); ++Index)
		{
			const FTcsNumericParamModInstance& Instance = Instances[Index];
			if (Instance.ParamKey != Key)
			{
				continue;
			}

			if (!IsWinningParamChainCompetitor(Instances, Index))
			{
				continue;
			}

			FTcsAttributeBandEntry BandEntry;
			BandEntry.Op = Instance.Op;
			BandEntry.Value = Instance.ResolvedValue;
			BandEntry.OverridePriority = Instance.OverridePriority;
			Entries.Add(BandEntry);
		}

		// 第三参取默认 `OTB_Max`：策略在 M2 住在**属性定义**（`FTcsAttributeDefTableRow::OverrideTieBreak`），
		// 而技能参数**没有"属性定义"这一层** ⇒ 取默认即正确处置，MUST NOT 为 M5 另造 tie-break 字段。
		// 先例 = 同族第三处消费者 TcsDamage 的 `TcsFlowAttributes.cpp:45` 亦以默认实参调用。
		return FoldTcsAttributeBands(BaseValue, Entries);
	}

	// 参数行求值（与物化边界同一套规则：求值一次 + 按行值约定转一次规范值）。
	// 为什么必须同规则：修正器值进账本时已转规范值，若初值仍按书写值参与折叠，
	// 就会出现"修正器按规范值折叠、初值还是书写值"的双口径（差 100 倍的那类静默错误）。
	void EvaluateParamChainRow(
		const FTcsNumericParamRow& Row,
		FTcsCombatEntityHandle Unit,
		int32 Level,
		double& OutValue)
	{
		FTcsParamEvaluateContext Ctx;
		Ctx.Subject = Unit;
		Ctx.Instigator = Unit;
		Ctx.EffectiveLevel = Level;
		Ctx.ParamTable = TScriptInterface<ITcsParamTableReader>();

		const double RawValue = Row.Base.Evaluate(Ctx);

		// **能力位为假的源不转**（判据由源自身 `AllowsValueConvention()` 声明，
		// MUST NOT 建"源类型 × 可配约定"的中心名单——同 `TcsCastOps_Snapshot.cpp` 的既有写法）
		const FTcsParamValueSource* ValueSource = Row.Base.Source.GetPtr<FTcsParamValueSource>();
		const bool bAllowConvention = ValueSource && ValueSource->AllowsValueConvention();

		OutValue = bAllowConvention
			? FTcsValueConvention::ConvertToCanonical(RawValue, static_cast<int32>(Row.ValueConvention))
			: RawValue;
	}
}



// 求值
bool FTcsParamChainOps::EvaluateParamKey(
	UTcsSkillSubsystem& Subsystem,
	const FTcsLearnedSkillEntry& Entry,
	const FTcsSkillDefData* Def,
	FGameplayTag Key,
	double& OutValue)
{
	// 初值 = 该键参数行的求值结果；该键无参数行则 0（9.6）
	double BaseValue = 0.0;
	bool bHasParamRow = false;

	if (Def)
	{
		for (const FTcsNumericParamRow& Row : Def->Params)
		{
			if (Row.Key == Key)
			{
				EvaluateParamChainRow(Row, Entry.Unit, Entry.Level, BaseValue);
				bHasParamRow = true;
				break;
			}
		}
	}

	// 命中判据：参数行存在**或**至少有一条同键条目。两者皆无 = miss
	// （**MUST NOT** 静默返回 0 当命中——调用方要能区分"命中且为 0"与"没配这个键"）
	bool bHasSlot = false;
	for (const FTcsNumericParamModInstance& Instance : Entry.NumericParamModInstances)
	{
		if (Instance.ParamKey == Key)
		{
			bHasSlot = true;
			break;
		}
	}

	if (!bHasParamRow && !bHasSlot)
	{
		return false;
	}

	OutValue = FoldParamChainInstances(Entry.NumericParamModInstances, Key, BaseValue);
	return true;
}

int32 FTcsParamChainOps::EvaluateEffectiveLevel(
	UTcsSkillSubsystem& Subsystem,
	const FTcsLearnedSkillEntry& Entry,
	const FTcsSkillDefData* Def)
{
	const int32 LevelBase = Def ? Def->LevelBase : 0;

	// `Level` 键走**同一条**五带折叠（经唯一的折叠调用点）：初值取 `LevelBase`，
	// 于是 `Add +2` ⇒ 3、`Mul 2` ⇒ 6 —— 与普通参数键**逐字同式**。
	const double Folded = FoldParamChainInstances(
		Entry.NumericParamModInstances, Tag_TcsStateParam_Level, static_cast<double>(LevelBase));

	// 取整口径与既有 `GetLevel` 一致（`FloorToInt`）+ 钳下界到 0
	// （负等级不是合法语义——设计 D3-11 原文 `clamp(0, …)`）。
	// **`Level` 键不出现在普通参数折叠结果里**：`GetNumericParam(Level)` 是另一条独立路径，
	// 两边各自按同键条目折叠、互不写回（等级不进参数表，参数表不动等级）。
	return FMath::Max(0, FMath::FloorToInt(Folded));
}
