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
		// **`Mode` 列分流（D5-12 v2 的"逐参数 Mode"）**：`EPM_Live` ⇒ **标记跳过**，
		// 不进快照数值面（该键的实时读取走账本求值通道）。
		//
		// **`EPM_Snapshot`（默认）⇒ 求值并冻结**。
		//
		// **业务上这一列在选什么**（策划在参数行上看到的「取值模式」下拉）：
		// - `快照` = **施法那一瞬定死**。技能伤害就该用它：按下技能时锁定伤害值，之后哪怕中途被
		//   上了"攻击力-50%"，**这一发伤害不变**（玩家预期如此）。
		// - `实时` = **每次读取重算**。引导型技能的持续伤害、随战况浮动的系数该用它：
		//   引导 5 秒里对手一直在给你上 debuff，每秒跳的伤害应跟着变。
		//
		// **为什么必须分流（不分的后果）**：无差别地把 `EPM_Live` 行也冻结，会让"实时通道"
		// **名存实亡**——策划把下拉改成 `实时`，运行时行为**和 `快照` 一模一样**，而且
		// **屏幕上不会有任何异常、不会有 warning**。这是最坏的一类缺陷：配置项看着生效了、
		// 实际被无视（本人 Task 3 交付时即漏了这一列，Task 4 补齐——见 `skill-cast-runtime`
		// 的缺口登记条款）。
		if (Row.Mode == ETcsParamMode::EPM_Live)
		{
			continue;
		}

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
