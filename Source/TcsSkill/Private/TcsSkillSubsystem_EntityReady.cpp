// Copyright Tirefly. All Rights Reserved.

#include "TcsSkillSubsystem.h"

#include "TcsSkillLogChannel.h"



// 门禁第一道：实体可操作
void UTcsSkillSubsystem::SetEntityQuery(TScriptInterface<ITcsEntityQuery> InEntityQuery)
{
	EntityQuery = InEntityQuery;

	UE_LOG(LogTcsSkill, Log, TEXT("技能门面注入实体查询：%s"),
		InEntityQuery.GetObject() ? *InEntityQuery.GetObject()->GetName() : TEXT("（已清空，门禁第一道降级为只判句柄有效）"));
}

bool UTcsSkillSubsystem::IsEntityReady(FTcsCombatEntityHandle Unit) const
{
	if (!Unit.IsValid())
	{
		return false;
	}

	// **未注入 = 配置状态不是错误**（同 ITcsParamTableReader 可空的口径）：
	// 降级为"只判句柄有效"并放行——MUST NOT 因"能力未注入"把一切施法拦死、MUST NOT ensure。
	ITcsEntityQuery* Query = EntityQuery.GetInterface();
	if (!Query)
	{
		return true;
	}

	// 判据 = 实体的**可操作性**（"框架能不能在它身上操作"），**不是** IsAlive 的"宿主认为它活着吗"。
	// 两者是正交轴：死亡触发的被动 / 复活技能应当 IsAlive == false 但可放；
	// 待销毁尸体应当 IsAlive == true 但不可放。IsAlive 的 PIE 实现是"映射里还有这个句柄"而非"活着"
	// （TcsPieEntityQuery.cpp:71，其类注释自认"框架不认识死亡"）⇒ 拿它当门禁会双向误判。
	return Query->IsEntityReady(Unit);
}
