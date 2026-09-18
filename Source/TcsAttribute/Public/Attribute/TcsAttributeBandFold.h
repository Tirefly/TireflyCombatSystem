// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Attribute/TcsAttrModInstance.h"



/**
 * 带式折叠条目（(运算, 已求值数值) 的扁平对）：三处消费者（M2 属性聚合 / M5 参数链 /
 * TcsDamage 流程属性）各自的容器形状不同，摊平成这一对之后共用同一签名。
 */
struct FTcsAttributeBandEntry
{
	// 运算带（带序唯一真相；SortKey 不参与折叠）
	ETcsAttributeOp Op = ETcsAttributeOp::TAO_Add;

	// 已求值数值（M2 侧 = 收集期求值后的 modifier 值；参数链侧 = 该行求值结果）
	double Value = 0.0;

	// 覆盖带内的强弱排座次（**仅 `TAO_Override` 读**，其余带忽略）
	int32 OverridePriority = 0;
};



/**
 * 覆盖带的同优先级比较得分（"大者胜"统一成正向）：把四种策略折算成同一个方向，
 * 于是选优可以写成一条三级比较（优先级 → 得分 → 有符号值），比较关系天然是全序。
 *
 * @param Value 候选数值。
 * @param TieBreak 同优先级裁决策略。
 * @return 返回该数值在策略下的得分（越大越强）。
 */
FORCEINLINE double GetTcsOverrideTieScore(double Value, ETcsAttrOverrideTieBreak TieBreak)
{
	switch (TieBreak)
	{
	case ETcsAttrOverrideTieBreak::OTB_Min:
		return -Value;

	case ETcsAttrOverrideTieBreak::OTB_MaxAbs:
		return FMath::Abs(Value);

	case ETcsAttrOverrideTieBreak::OTB_MinAbs:
		return -FMath::Abs(Value);

	case ETcsAttrOverrideTieBreak::OTB_Max:
	default:
		return Value;
	}
}

/**
 * 候选覆盖条目是否强于当前最优（**全序**：优先级 → 同优先级策略得分 → 有符号值）。
 *
 * 第三级只用于"策略下不可区分"的情形（例：`OTB_MaxAbs` 下的 +5 与 -5）——补一级有符号值
 * 比较即得全序，结果与遍历顺序无关；否则同分打平会让赢家取决于谁先被遍历到。
 *
 * 比较一律用**精确关系**（`!=` / `>`）而非近似相等：模糊相等不满足传递性，会破坏"顺序无关"。
 *
 * @param CandidateValue 候选数值。
 * @param CandidatePriority 候选的覆盖优先级。
 * @param BestValue 当前最优数值。
 * @param BestPriority 当前最优的覆盖优先级。
 * @param TieBreak 同优先级裁决策略（属属性定义）。
 * @return 返回候选是否强于当前最优。
 */
FORCEINLINE bool IsStrongerTcsOverride(
	double CandidateValue, int32 CandidatePriority,
	double BestValue, int32 BestPriority,
	ETcsAttrOverrideTieBreak TieBreak)
{
	if (CandidatePriority != BestPriority)
	{
		return CandidatePriority > BestPriority;
	}

	const double CandidateScore = GetTcsOverrideTieScore(CandidateValue, TieBreak);
	const double BestScore = GetTcsOverrideTieScore(BestValue, TieBreak);
	if (CandidateScore != BestScore)
	{
		return CandidateScore > BestScore;
	}

	return CandidateValue > BestValue;
}



/**
 * 带式折叠（**唯一实现**，D5-5 v3：运算语义相同则实现必须单份）。
 *
 * 语义（顺序无关）：
 * - 存在 `TAO_Override` → 按"优先级 → 同优先级策略 → 有符号值"三级比较选出**一条**直接作为结果
 *   （其余带含 `TAO_FlatAdd` **一并被覆盖**）；
 * - 否则 `Final = ((BaseValue + ΣAdd) × (1 + ΣPercentAdd)) × ΠMul + ΣFlatAdd`；
 * - 条目为空 → 返回 `BaseValue`（参数链侧即"该键无参数行则 0"）。
 *
 * **覆盖带的强弱口径**（2026-09-18 用户拍板）：强弱**唯一由 `OverridePriority`（大者胜）决定**；
 * 优先级打平才落到属性定义的 `OverrideTieBreak` 策略比较数值。框架不再定义任何其它"谁盖谁"的
 * 规则（避免二次规则——"到底以哪个为准"对使用者是纯负担）。默认策略取最大值 = 引入优先级之前的行为。
 *
 * 纯函数（D0-1）：无副作用、无时间/随机依赖；`Entries` 的任意排列结果一致。
 * 消费者：`FTcsAttributePipeline`（M2 聚合）、M5 参数链、TcsDamage 流程属性容器——
 * MUST 一律调用本函数，不得另有并列的同义实现。
 *
 * @param BaseValue 折叠初值（M2 侧 = 属性基础值；参数链侧 = 该键参数行的求值结果）。
 * @param Entries 已求值的带式条目（顺序无关）。
 * @param OverrideTieBreak 覆盖带同优先级裁决策略（M2 侧来自属性定义；调用方无此概念时用默认值）。
 * @return 返回折叠结果（未经值域收口——收口由调用方按自己的值域语义执行）。
 */
inline double FoldTcsAttributeBands(
	double BaseValue,
	TConstArrayView<FTcsAttributeBandEntry> Entries,
	ETcsAttrOverrideTieBreak OverrideTieBreak = ETcsAttrOverrideTieBreak::OTB_Max)
{
	double SumAdd = 0.0;
	double SumPercentAdd = 0.0;
	double ProductMul = 1.0;
	double SumFlatAdd = 0.0;
	bool bHasOverride = false;
	double BestOverride = 0.0;
	int32 BestOverridePriority = 0;

	for (const FTcsAttributeBandEntry& Entry : Entries)
	{
		switch (Entry.Op)
		{
		case ETcsAttributeOp::TAO_Override:
			if (!bHasOverride ||
				IsStrongerTcsOverride(Entry.Value, Entry.OverridePriority, BestOverride, BestOverridePriority, OverrideTieBreak))
			{
				BestOverride = Entry.Value;
				BestOverridePriority = Entry.OverridePriority;
				bHasOverride = true;
			}
			break;

		case ETcsAttributeOp::TAO_Add:
			SumAdd += Entry.Value;
			break;

		case ETcsAttributeOp::TAO_PercentAdd:
			SumPercentAdd += Entry.Value;
			break;

		case ETcsAttributeOp::TAO_Mul:
			ProductMul *= Entry.Value;
			break;

		case ETcsAttributeOp::TAO_FlatAdd:
			SumFlatAdd += Entry.Value;
			break;

		default:
			break;
		}
	}

	// Override 组存在 → 覆盖一切（"最强覆盖生效"）
	if (bHasOverride)
	{
		return BestOverride;
	}

	return ((BaseValue + SumAdd) * (1.0 + SumPercentAdd) * ProductMul) + SumFlatAdd;
}
