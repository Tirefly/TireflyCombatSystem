// Copyright Tirefly. All Rights Reserved.

#include "TcsSkillSubsystem.h"

#include "TcsEffectSubsystem.h"
#include "TcsSkillLogChannel.h"



// 门禁第一道：实体可操作
//
// **本文件不再提供 `SetEntityQuery`**（2026-10-26 删除）：实体查询的注入面全仓唯一 =
// `UTcsEffectSubsystem::SetEntityQuery`。本类此前持有**第二份**槽位，与效果门面那份互不感知，
// 造成两个静默失效形态（台账 `WAIT-12`）：① 宿主只注入其一 ⇒ 门禁**恒为假**、施法一律被拒；
// ② 分别注入两个不同实例 ⇒ 组件登记走效果那份、门禁走本类那份 ⇒ 映射查不到句柄。
// 两者读数是同一句话——"实体明明好好的却报实体未就绪"。
// ⇒ 处置 = 把槽位数从 N 减到 1，让分歧**结构上不可能**，而不是给 N 处补同步逻辑。
bool UTcsSkillSubsystem::IsEntityReady(FTcsCombatEntityHandle Entity) const
{
	if (!Entity.IsValid())
	{
		return false;
	}

	// 取共享槽位（**全仓唯一注入面**）。世界拆解期可能拿不到门面 ⇒ 同样按"未注入"降级。
	const UWorld* World = GetWorld();
	UTcsEffectSubsystem* EffectSubsystem = World
		? World->GetSubsystem<UTcsEffectSubsystem>()
		: nullptr;

	// **降级条件 = 宿主整个未注入**（2026-10-26 改判，此前是"技能侧未注入"）。
	//
	// 语义差别是要紧的：降级的理由是"**宿主没有提供实体查询能力**"（配置状态，不是错误），
	// 而不是"技能模块忘了接线"——后者恰恰是 `WAIT-12` 那个静默错配，把它当合法降级会掩盖缺陷。
	// ⇒ 未注入时降级为"只判句柄有效"并放行：MUST NOT 因"能力未注入"把一切施法拦死、MUST NOT ensure。
	// `GetEntityQuery()` 是 const 方法、返回非 const 指针；这里只需读，故用 `const` 前置。
	// ⚠ 接口方法 `IsEntityReady` 非 const ⇒ 不能经 const 指针调用，故取非 const 指针。
	ITcsEntityQuery* Query = EffectSubsystem ? EffectSubsystem->GetEntityQuery() : nullptr;
	if (!Query)
	{
		return true;
	}

	// 判据 = 实体的**可操作性**（"框架能不能在它身上操作"），**不是** IsAlive 的"宿主认为它活着吗"。
	// 两者是正交轴：死亡触发的被动 / 复活技能应当 IsAlive == false 但可放；
	// 待销毁尸体应当 IsAlive == true 但不可放。IsAlive 的 PIE 实现是"映射里还有这个句柄"而非"活着"
	// （TcsPieEntityQuery.cpp:71，其类注释自认"框架不认识死亡"）⇒ 拿它当门禁会双向误判。
	return Query->IsEntityReady(Entity);
}
