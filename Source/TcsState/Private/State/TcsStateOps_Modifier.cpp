// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateOps.h"

#include "Attribute/TcsAttrModInstance.h"
#include "Def/TcsBuffDef.h"
#include "Engine/World.h"
#include "State/TcsStateAttributeAccess.h"
#include "State/TcsStateInstance.h"
#include "State/TcsStateModifierMaterializer.h"
#include "TcsAttributeSubsystem.h"
#include "TcsStateLogChannel.h"
#include "TcsStateSubsystem.h"



// 属性访问解析点
FTcsStateAttributeAccess FTcsStateAttributeAccess::Resolve(const UWorld* World)
{
	FTcsStateAttributeAccess Access;

	// "怎么找到门面"归 TcsAttribute 的单一查找点（2026-10-05：将来补注入契约只换那一处）
	Access.AttributeSubsystem = UTcsAttributeSubsystem::Resolve(World);

	return Access;
}

bool FTcsStateAttributeAccess::IsLedgerReady(FTcsCombatEntityHandle Unit) const
{
	// `ResolveStore` 是**非确保**查询口（账本侧的读取路径用它）——未注册返回 nullptr，正是本判据要的
	return AttributeSubsystem && AttributeSubsystem->ResolveStore(Unit) != nullptr;
}

bool FTcsStateAttributeAccess::ApplyModifier(FTcsCombatEntityHandle Unit, const FTcsAttrModInstance& Modifier) const
{
	return AttributeSubsystem ? AttributeSubsystem->ApplyModifier(Unit, Modifier) : false;
}

int32 FTcsStateAttributeAccess::RemoveBySource(FTcsCombatEntityHandle Unit, const FTcsSourceHandle& Source) const
{
	return AttributeSubsystem ? AttributeSubsystem->RemoveBySource(Unit, Source) : 0;
}

void FTcsStateAttributeAccess::BeginBatch(FTcsCombatEntityHandle Unit) const
{
	if (AttributeSubsystem)
	{
		AttributeSubsystem->BeginBatch(Unit);
	}
}

void FTcsStateAttributeAccess::Commit(FTcsCombatEntityHandle Unit) const
{
	if (AttributeSubsystem)
	{
		AttributeSubsystem->Commit(Unit);
	}
}



// 修正器挂点
void FTcsStateOps::MountModifiers(
	UTcsStateSubsystem& Subsystem,
	const FTcsBuffDef& Def,
	const FTcsStateInstance& Instance,
	bool bStripFirst)
{
	const bool bHasTemplates = Def.ModifierRows.Num() > 0;

	// 零模板且非刷新：连批都不开——零条既不是错误，也不该在属性账本上留下任何痕迹
	if (!bHasTemplates && !bStripFirst)
	{
		return;
	}

	const FTcsStateAttributeAccess Access = FTcsStateAttributeAccess::Resolve(Subsystem.GetWorld());
	if (!Access.IsValid() || !Access.IsLedgerReady(Instance.Unit))
	{
		// 该单位没有属性账本 ⇒ 修正器无处可挂。状态本身照常生效（状态与属性是两套登记），
		// 故这是**配置状态**不是错误：Log 级、不 ensure、不留 Warning。
		UE_LOG(LogTcsState, Log, TEXT("状态修正器未挂载：单位无属性账本（单位=%lld 定义=%s）"),
			Instance.Unit.Id, *Instance.DefTag.ToString());
		return;
	}

	// 物化（只读；快照经反射壳作为"本次求值的参数表"装进上下文）
	TArray<FTcsAttrModInstance> Modifiers;
	if (bHasTemplates)
	{
		FTcsStateModifierMaterializer::Materialize(Subsystem, Def, Instance, Modifiers);
	}

	// 摘旧与挂新 MUST 在同一批内（批内只标脏、最外层提交才重算 + 广播）：
	// 多个修正器只触发一次重算与一次广播，且"摘了一半"的中间态不会被任何订阅者看到。
	Access.BeginBatch(Instance.Unit);

	if (bStripFirst)
	{
		// 刷新路径：按**级联锚点**摘除旧条目——同一锚点，摘掉的正是本实例上次挂的那些
		Access.RemoveBySource(Instance.Unit, Instance.CascadeAnchor);
	}

	for (const FTcsAttrModInstance& Modifier : Modifiers)
	{
		Access.ApplyModifier(Instance.Unit, Modifier);
	}

	Access.Commit(Instance.Unit);

	UE_LOG(LogTcsState, Log, TEXT("状态修正器挂载：单位=%lld 定义=%s 来源=%llu 条目=%d 摘旧=%s"),
		Instance.Unit.Id, *Instance.DefTag.ToString(), Instance.Source.Id, Modifiers.Num(),
		bStripFirst ? TEXT("是") : TEXT("否"));
}

void FTcsStateOps::StripModifiers(
	UTcsStateSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSourceHandle Source)
{
	const FTcsStateAttributeAccess Access = FTcsStateAttributeAccess::Resolve(Subsystem.GetWorld());
	if (!Access.IsValid() || !Access.IsLedgerReady(Unit))
	{
		// 无属性账本 ⇒ 本来就没挂过（移除路径上的正常分支，静默返回）
		return;
	}

	Access.BeginBatch(Unit);
	const int32 RemovedCount = Access.RemoveBySource(Unit, Source);
	Access.Commit(Unit);

	if (RemovedCount > 0)
	{
		UE_LOG(LogTcsState, Log, TEXT("状态修正器摘除：单位=%lld 来源=%llu 条数=%d"),
			Unit.Id, Source.Id, RemovedCount);
	}
}
