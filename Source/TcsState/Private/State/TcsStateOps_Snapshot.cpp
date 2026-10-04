// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateOps.h"

#include "Clock/TcsClockSubsystem.h"
#include "Def/TcsBuffDef.h"
#include "Host/TcsEntityLevelProvider.h"
#include "Parameter/TcsParamValue.h"
#include "State/TcsStateSnapshot.h"
#include "TcsStateLogChannel.h"
#include "TcsStateSubsystem.h"
#include "TcsValueConvention.h"



// 上下文
void FTcsStateOps::MakeContext(
	FTcsStateEvaluateContext& OutContext,
	UTcsStateSubsystem& Subsystem,
	FTcsCombatEntityHandle Target,
	FTcsCombatEntityHandle Instigator,
	int32 EffectiveLevel,
	const TScriptInterface<ITcsParamTableReader>& ParamTable)
{
	OutContext.ParamTable = ParamTable;

	// 被施加方 = 实例所依附的单位；发起者无效时取被施加方（"谁施加的"缺省即被施加方）
	OutContext.Subject = Target;
	OutContext.Instigator = Instigator.IsValid() ? Instigator : Target;

	// 生效等级由调用方给出（本轮 = `Def.LevelBase`；等级增长策略归宿主业务侧）
	OutContext.EffectiveLevel = EffectiveLevel;

	// 等级读口来自门面登记的宿主实现（未注入 = 空接口 ⇒ 等级类源落兜底）
	OutContext.LevelProvider = Subsystem.GetEntityLevelProvider();
}



// 快照
void FTcsStateOps::BuildSnapshot(
	FTcsParamSnapshot& OutSnapshot,
	const FTcsBuffDef& Def,
	const FTcsParamEvaluateContext& Ctx,
	const TMap<FGameplayTag, double>& Overrides)
{
	// 重建语义：先清空（刷新路径也走本函数——"新 payload 覆盖旧"靠这一句成立）
	OutSnapshot.Reset();
	OutSnapshot.Entries.Reserve(Def.Params.Num());

	for (const FTcsNumericParamRow& Row : Def.Params)
	{
		FTcsParamSnapshotEntry Entry;
		Entry.Key = Row.Key;
		Entry.SourceRef = Row.Base.Source;

		if (const double* Override = Overrides.Find(Row.Key))
		{
			// 覆盖值是调用方给出的**规范值**——不再过一次值约定（只转定义侧书写的数值）
			Entry.Value = *Override;
		}
		else
		{
			const double RawValue = Row.Base.Evaluate(Ctx);

			// 值约定转换 = 写入点唯一一次（D5-18 v2；快照内永远规范值）。
			// 能力位为假的行本不该配约定列（资产 `IsDataValid` 报错），此处按"不转换"降级——
			// 运行期不拦（静默错配在作者期暴露）
			const bool bAllowConvention =
				Row.Base.Source.GetPtr<FTcsParamValueSource>()
				&& Row.Base.Source.GetPtr<FTcsParamValueSource>()->AllowsValueConvention();

			Entry.Value = bAllowConvention
				? FTcsValueConvention::ConvertToCanonical(RawValue, static_cast<int32>(Row.ValueConvention))
				: RawValue;
		}

		OutSnapshot.Entries.Add(MoveTemp(Entry));
	}
}

double FTcsStateOps::EvaluateTotalDuration(
	UTcsStateSubsystem& Subsystem,
	const FTcsBuffDef& Def,
	const FTcsParamSnapshot& Snapshot,
	FTcsCombatEntityHandle Target,
	FTcsCombatEntityHandle Instigator,
	const TScriptInterface<ITcsParamTableReader>& ParamTable)
{
	// **刻意实时求值而不是回查快照**：`DurationTime` 是**时值字段**（不在 `Def.Params` 里），
	// 快照只装参数行；把时值也塞进快照会引入一个"时值的参数键"——那是插件自造参数表键
	// （键空间归宿主声明，见 `gameplay-tag-governance`）。求值上下文与快照构建同一份装配，
	// 故"时值与参数行不同档"的双口径不会出现。快照形参保留在签名里是为了调用点形态统一
	// （将来若要改"时值也冻结"只需改本函数体，不动调用点）。
	(void)Snapshot;

	// `Infinite` 无总时长语义（字段不使用）——调用方据此不入堆
	if (Def.DurationPolicy == EDurationPolicy::EDP_Infinite)
	{
		return 0.0;
	}

	FTcsStateEvaluateContext Ctx;
	MakeContext(Ctx, Subsystem, Target, Instigator, Def.LevelBase, ParamTable);

	const double TotalSeconds = Def.DurationTime.Evaluate(Ctx);
	if (TotalSeconds <= 0.0)
	{
		UE_LOG(LogTcsState, Warning, TEXT("状态时值非正：按 0 处理（定义=%s 策略=%d 求值=%.3f）"),
			*Def.StatusTag.ToString(), static_cast<int32>(Def.DurationPolicy), TotalSeconds);
		return 0.0;
	}

	return TotalSeconds;
}
