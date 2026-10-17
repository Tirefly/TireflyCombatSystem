// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Handle/TcsCombatEntityHandle.h"



class ITcsStateLiveQuery;
struct FTcsSkillDefData;

/**
 * **技能侧关系门**（2026-10-16 `R5.5-e1` 立）：施法态与状态族的共存冲突判定。
 *
 * **为什么与状态侧不共用同一个入口**：判定逻辑**共用**（都走 `FTcsStateRelation` 的静态函数族），
 * 但**取客体词面的方式不同**——状态侧的客体是该实体在跑的状态（自己域内，直接遍历桶）；
 * 技能侧的客体是**状态域**的在跑实例（跨域，须经 `ITcsStateLiveQuery` 契约）。
 * `TcsSkill` MUST NOT 依赖 `TcsBuff` ⇒ 跨域那一步只能由契约承担。
 *
 * **为什么不是 `FTcsStateRelation` 的成员**：共享层 MUST NOT 认识 `FTcsSkillDefData`
 * （技能域类型）⇒ 本类型住技能域。
 */
struct TCSSKILL_API FTcsSkillOps_Relation
{
public:
	/**
	 * **技能关系门（激活前）**——三道判定，任一不过 ⇒ 拒绝（`SAR_RelationBlocked`）。
	 *
	 * ① `RequiredTags` 未全满足；② `ForbiddenTags` 命中；
	 * ③（施动侧反向查找）**技能侧只落「对我的状态」这一面**：状态域的 `TagsToBlock`
	 *    是否覆盖我（经 `ITcsStateLiveQuery` 读在跑状态的类别词）。
	 *
	 * **如实登记的未落面**：技能↔技能的 `TagsToBlock` 阻断判定**本批未落**
	 * （需要遍历同实体全部在跑 run 的类别词，而技能在跑实例的词面今天没有读口）。
	 * MUST NOT 读成本条已覆盖两个方向。
	 *
	 * **契约未注册时的降级**：走「无在跑状态」路径 + 具名 Warning
	 * （MUST NOT 静默、MUST NOT 因此拒绝——缺注册不该变成随机的业务失败）。
	 *
	 * @param LiveQuery 共享层的在飞运行态查询契约（**MAY 为 nullptr**，见上）。
	 * @param Entity 施法实体。
	 * @param DefTag 技能定义身份。
	 * @param Def 该定义（调用方已解析且非空）。
	 * @return 通过返回 true。
	 */
	static bool CheckRelationGate(
		ITcsStateLiveQuery* LiveQuery,
		FTcsCombatEntityHandle Entity,
		FGameplayTag DefTag,
		const FTcsSkillDefData& Def);
};
