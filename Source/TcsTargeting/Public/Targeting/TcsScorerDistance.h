// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Targeting/TcsTargetScorerStrategy.h"

#include "TcsScorerDistance.generated.h"



/**
 * 内置评分器：**距离**（P-A 排序相位**唯一**的框架内置评分器——用户 2026-09-30 拍板
 * "先只内置一个距离最近，因为这是 LAC 一定会使用到的"）。
 *
 * 语义：候选到**施法者**的**欧氏距离**（非负）；配 `ETSD_Ascending` 即"最近优先"、
 * `ETSD_Descending` 即"最远优先"（同一评分器，方向由排序项给定）。
 *
 * **它为什么可以内置**（"框架零宿主词汇"判据）：它引用的词汇只有"**实体有位置**"这一**框架自身
 * 契约**（`ITcsEntityQuery::GetLocation`），不认识任何宿主属性名 / Tag 词表 / 阵营关系。
 * 凡引用后者的一律归宿主（C++ 策略子类或宿主脚本插槽）。
 *
 * **分数 MUST 是可解释的真实距离**（MUST NOT 改成平方距离等未声明变换）：它是**可观测值**，
 * 后续的诊断/预览面会展示"首个排序项分数"。
 *
 * **非 `Hidden`**：与两个策略基类不同，本类型是**可选实现**而非抽象基类——必须出现在编辑器
 * 类型 picker 里（策划在链资产上直接选它）。
 */
USTRUCT()
struct TCSTARGETING_API FTcsScorerDistance : public FTcsTargetScorerStrategy
{
	GENERATED_BODY()

// 评分
#pragma region Score

public:
	/**
	 * 求候选到施法者的欧氏距离。
	 *
	 * 降级口径（**"无法评分"一律用 NaN 表达 ⇒ 该候选被排除**）：`EntityQuery` 未注入、
	 * 施法者句柄无效、施法者无定位、候选无定位——四种情形都返回 NaN 并留痕；
	 * **MUST NOT 返回 `-Inf`**（升序下会变成"排最前"）、**MUST NOT 返回 0**
	 * （0 是合法距离，"取不到位置"与"就在脚下"必须可区分）。
	 *
	 * @param Candidate 候选目标实体句柄。
	 * @param Context 链黑板（本评分器只读 `Caster`）。
	 * @param EntityQuery 宿主实体查询能力（本评分器**消费**它；为 `nullptr` 时无法评分）。
	 * @return 返回到施法者的欧氏距离；无法评分时返回 NaN。
	 */
	virtual double Score(FTcsCombatEntityHandle Candidate, const FTcsEffectContext& Context, ITcsEntityQuery* EntityQuery) const override;

#pragma endregion
};
