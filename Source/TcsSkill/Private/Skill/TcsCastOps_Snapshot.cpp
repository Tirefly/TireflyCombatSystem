// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsCastOps.h"

#include "Parameter/TcsParamValueSource.h"
#include "TcsValueConvention.h"

#include "Def/TcsParamRow.h"
#include "Def/TcsSkillDefData.h"



// 快照
void FTcsCastOps::BuildSkillSnapshot(
	FTcsParamSnapshot& OutSnapshot,
	const FTcsSkillDefData& Def,
	const FTcsParamEvaluateContext& Ctx,
	const TMap<FGameplayTag, double>& Overrides)
{
	// 重建语义：先清空（同一次激活内只构建一次，但保持与状态侧逐字同款的"重建"形态）
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
			// **能力位为假的源不转**（判据由源自身声明，MUST NOT 建"源类型 × 可配约定"的中心名单）
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
