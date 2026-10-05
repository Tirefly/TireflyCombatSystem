// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateOps.h"

#include "Def/TcsBuffDef.h"
#include "Engine/World.h"
#include "State/TcsStateInstance.h"
#include "TcsEffectSubsystem.h"
#include "TcsStateLogChannel.h"
#include "TcsStateSubsystem.h"
#include "Trigger/TcsEffectTriggerInstance.h"



// 内联触发行接线（`TRIG-4` 的行为半，R5 Task 6）
//
// **本文件是 TcsState 内唯一 include `TcsEffectSubsystem.h` 处**（与"属性解析点"同款单点纪律：
// 跨模块取用集中在一处，越界在编译期就不可达；将来若补触发行侧的注入契约，只换本文件）。
//
// 为什么按**级联锚点**登记：锚点每实例唯一（刷新不换）⇒ 撤销时一次摘净本实例的行、且不误伤
// 别条实例（"施加方来源句柄"可以一个来源挂多个定义，按它摘会误摘——见实例头注释的分工表）。

void FTcsStateOps::WireTriggerRows(
	UTcsStateSubsystem& Subsystem,
	const FTcsStateInstance& Instance,
	const FTcsBuffDef& Def)
{
	if (Def.Triggers.Num() == 0)
	{
		// 零内联行 ⇒ 零动作（连门面都不必取）
		return;
	}

	UTcsEffectSubsystem* EffectSubsystem = Subsystem.GetWorld()
		? Subsystem.GetWorld()->GetSubsystem<UTcsEffectSubsystem>()
		: nullptr;
	if (!EffectSubsystem)
	{
		// 世界拆解期（或该世界无效果门面）——时序/配置状态，不是契约违规：状态本身照常生效
		UE_LOG(LogTcsState, Warning, TEXT("状态内联触发行未登记：效果门面不可得（单位=%lld 定义=%s 行数=%d）"),
			Instance.Unit.Id, *Instance.DefTag.ToString(), Def.Triggers.Num());
		return;
	}

	for (const FTcsEffectTriggerDef& Row : Def.Triggers)
	{
		FTcsEffectTriggerInstance RowInstance;
		RowInstance.Def = Row;
		RowInstance.Source = Instance.CascadeAnchor;
		EffectSubsystem->RegisterTriggerRow(RowInstance);
	}

	UE_LOG(LogTcsState, Log, TEXT("状态内联触发行登记：单位=%lld 定义=%s 句柄=%d/%d 锚点=%llu 行数=%d 登记表总数=%d"),
		Instance.Unit.Id, *Instance.DefTag.ToString(), Instance.Handle.Index, Instance.Handle.Generation,
		Instance.CascadeAnchor.Id, Def.Triggers.Num(), EffectSubsystem->GetTriggerRowCount());
}

void FTcsStateOps::UnwireTriggerRows(
	UTcsStateSubsystem& Subsystem,
	const FTcsStateInstance& Instance)
{
	UTcsEffectSubsystem* EffectSubsystem = Subsystem.GetWorld()
		? Subsystem.GetWorld()->GetSubsystem<UTcsEffectSubsystem>()
		: nullptr;
	if (!EffectSubsystem)
	{
		return;
	}

	const int32 RemovedCount = EffectSubsystem->UnregisterTriggerRowsBySource(Instance.CascadeAnchor);
	if (RemovedCount > 0)
	{
		UE_LOG(LogTcsState, Log, TEXT("状态内联触发行退订：单位=%lld 定义=%s 锚点=%llu 条数=%d 登记表总数=%d"),
			Instance.Unit.Id, *Instance.DefTag.ToString(), Instance.CascadeAnchor.Id,
			RemovedCount, EffectSubsystem->GetTriggerRowCount());
	}
}
