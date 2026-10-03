// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Chain/TcsEffectContext.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Host/TcsEntityQuery.h"

#include "TcsTargetScorerStrategy.generated.h"



/**
 * 目标评分器策略抽象基类（P-A 排序相位；载体同选择器/过滤器：USTRUCT 反射基类 + C++ 虚函数分派。
 * 禁纯虚 + `meta=(Hidden)` 的抽象约定见 `TcsTargetSelectorStrategy.h` 的说明，本文件不重复）。
 *
 * **职责分工（MUST 遵守；规格与类型注释两处都写）**：本契约只表达"**谁更优**"——
 * **MUST NOT** 用极低分表达"死亡 / 满血 / 不可选中"这类**资格**判定，那些归
 * `FTcsTargetFilterStrategy`。把资格伪装成分数，会让"为什么这个目标没被打"在日志里彻底不可见
 * （执行序本就在过滤之后，被资格淘汰的候选根本不进评分）。
 *
 * **"框架零宿主词汇"（用户 2026-09-30 原则的推论）**：只有**只引用框架自身契约**的评分器可内置
 * （先例 = 距离，只依赖 `ITcsEntityQuery::GetLocation`）；引用宿主属性名 / Tag 词表 / 阵营关系
 * 的一律归宿主（C++ 策略子类，或本目录的宿主脚本插槽转发器）。
 *
 * **浮点边界（三条，规格同款）**：①任一排序项对某候选返回 **NaN** ⇒ 该候选被**排除**
 * （MUST NOT 排到末尾、MUST NOT 取任意位置）；②**"无法评分"MUST 用 NaN 表达**，
 * MUST NOT 用 `-Inf`（升序下会变成"排最前"，是外部参照实现里实测存在的陷阱）；
 * ③**±Inf MUST 保留**并按其方向正常参与排序。
 */
USTRUCT(meta = (Hidden))
struct TCSTARGETING_API FTcsTargetScorerStrategy
{
	GENERATED_BODY()

// 评分契约
#pragma region Score

public:
	/**
	 * 求候选的评分（数值大小与方向的组合含义由排序项的 `Direction` 给定，本方法只给分）。
	 *
	 * @param Candidate 候选目标（**实体句柄**——需要定位等语义时经 `EntityQuery` 询问宿主）。
	 * @param Context 链黑板（宿主语义可自取所需；框架不解释其内容）。
	 * @param EntityQuery 宿主注入的实体查询能力；**MAY 为 nullptr**（未注入）——此时 MUST 返回
	 *                    **NaN**（= 该候选被排除）并留 Warning，**MUST NOT 返回 0**：0 是合法分数，
	 *                    用它会把"取不到世界能力"表现成"所有候选并列第一"。
	 * @return 返回评分；**NaN = 排除该候选**。中性默认实现返回 `0.0` = 不区分候选
	 *         （基类不可被编辑器选中；"未配置"不等于"淘汰全部"）。
	 */
	virtual double Score(FTcsCombatEntityHandle Candidate, const FTcsEffectContext& Context, ITcsEntityQuery* EntityQuery) const
	{
		// 中性默认实现：0.0（不区分候选——淘汰全部会让配置遗漏表现成"打不到人"）
		return 0.0;
	}

#pragma endregion
};
