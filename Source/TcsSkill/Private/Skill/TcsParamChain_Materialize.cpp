// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsParamChain.h"

#include "Parameter/TcsParamValue.h"
#include "Parameter/TcsParamValueSource.h"
#include "TcsValueConvention.h"

#include "Def/TcsParamRow.h"
#include "Def/TcsSkillDefData.h"
#include "TcsSkillLogChannel.h"
#include "TcsSkillSubsystem.h"



// 物化边界
double FTcsParamChainOps::ResolveModifierValue(
	const FTcsNumericParamModifier& Row,
	const FTcsParamEvaluateContext& Ctx)
{
	const double RawValue = Row.Operand.Evaluate(Ctx);

	// 值约定转换 = 物化边界**唯一一次**（D5-18 v2：账本内永远规范值）。
	// **能力位为假的源不转**（判据由源自身 `AllowsValueConvention()` 声明——
	// MUST NOT 建"源类型 × 可配约定"的中心名单；同 `TcsCastOps_Snapshot.cpp` 的既有写法）
	const FTcsParamValueSource* ValueSource = Row.Operand.Source.GetPtr<FTcsParamValueSource>();
	const bool bAllowConvention = ValueSource && ValueSource->AllowsValueConvention();

	return bAllowConvention
		? FTcsValueConvention::ConvertToCanonical(RawValue, static_cast<int32>(Row.ValueConvention))
		: RawValue;
}



// 定义侧物化
int32 FTcsParamChainOps::MaterializeParamChainRows(
	UTcsSkillSubsystem& Subsystem,
	const FTcsSkillDefData& Def,
	FTcsLearnedSkillEntry& Entry,
	const TScriptInterface<ITcsParamTableReader>& ParamTable,
	FTcsSourceHandle RunSource)
{
	if (Def.ParamChainRows.Num() == 0)
	{
		return 0;
	}

	// 求值上下文：等级取**本次激活冻结的生效等级**（`Run.Level` 与 `Entry.Level` 在激活时同值；
	// 用 `Entry.Level` 是"激活时写回条目"的既有口径）；参数表 = 本次 run 的快照（引用类源经它取值——
	// 这是技能侧唯一的参数表载体：**它是求值期的上下文装填，不是"冻结读取面"**）
	FTcsParamEvaluateContext Ctx;
	Ctx.Subject = Entry.Unit;
	Ctx.Instigator = Entry.Unit;
	Ctx.EffectiveLevel = Entry.Level;
	Ctx.ParamTable = ParamTable;

	Entry.NumericParamModInstances.Reserve(Entry.NumericParamModInstances.Num() + Def.ParamChainRows.Num());

	for (const FTcsNumericParamModifier& Row : Def.ParamChainRows)
	{
		// `Source` 一律取**本施法运行态的来源句柄**（定义侧行自带的 `Source` 在此不作数——
		// 定义是共享模板，逐次施法各挂各的来源，才能"一次施法终结 ⇒ 一次摘净"）
		FTcsNumericParamModInstance Instance = FTcsNumericParamModInstance::MakeFromModifier(
			Row, ResolveModifierValue(Row, Ctx), RunSource);

		Entry.NumericParamModInstances.Add(MoveTemp(Instance));
	}

	UE_LOG(LogTcsSkill, Log,
		TEXT("参数链行物化：定义=%s 单位=%lld 条目=%d/%d 行=%d 来源=%llu 累计条目=%d"),
		*Entry.DefTag.ToString(), Entry.Unit.Id, Entry.Handle.Index, Entry.Handle.Generation,
		Def.ParamChainRows.Num(), RunSource.Id, Entry.NumericParamModInstances.Num());

	return Def.ParamChainRows.Num();
}
