// Copyright Tirefly. All Rights Reserved.

#include "Entity/TcsPieEntityQuery.h"

#include "GameFramework/Actor.h"

#include "TcsIntegrationLogChannel.h"



void UTcsPieEntityQuery::RegisterEntity(FTcsCombatEntityHandle Handle, AActor* Actor)
{
	if (!Handle.IsValid() || !Actor)
	{
		UE_LOG(LogTcsIntegration, Warning, TEXT("UTcsPieEntityQuery::RegisterEntity: 句柄或 Actor 无效——忽略登记"));
		return;
	}

	// 登记序：首次登记才追加（重复登记同一句柄不改变其位置——保持"登记序"的稳定性）
	if (!EntityToActor.Contains(Handle))
	{
		RegistrationOrder.Add(Handle);
	}

	EntityToActor.Add(Handle, Actor);
}

void UTcsPieEntityQuery::UnregisterEntity(FTcsCombatEntityHandle Handle)
{
	// 注销路径的重复调用是正常时序（组件 EndPlay 与宿主清理可能都触发），静默返回
	if (EntityToActor.Remove(Handle) > 0)
	{
		RegistrationOrder.RemoveSingleSwap(Handle, EAllowShrinking::No);
	}
}



void UTcsPieEntityQuery::EnumerateEntities(TFunctionRef<void(FTcsCombatEntityHandle)> Visitor)
{
	// 稳定序 = 登记序（TMap 迭代序不稳定，故用 RegistrationOrder 驱动）；反向遍历以便就地移除失效项
	for (int32 Index = RegistrationOrder.Num() - 1; Index >= 0; --Index)
	{
		const FTcsCombatEntityHandle Handle = RegistrationOrder[Index];

		// 失效项就地清理（Actor 已 GC/销毁，但组件未走注销路径——如关卡切换的强制回收）
		const TWeakObjectPtr<AActor>* Found = EntityToActor.Find(Handle);
		if (!Found || !Found->IsValid())
		{
			EntityToActor.Remove(Handle);
			RegistrationOrder.RemoveAtSwap(Index, EAllowShrinking::No);
			continue;
		}

		Visitor(Handle);
	}
}

bool UTcsPieEntityQuery::GetLocation(FTcsCombatEntityHandle Entity, FVector& OutLocation)
{
	const TWeakObjectPtr<AActor>* Found = EntityToActor.Find(Entity);
	if (!Found || !Found->IsValid())
	{
		return false;
	}

	OutLocation = Found->Get()->GetActorLocation();
	return true;
}

bool UTcsPieEntityQuery::IsAlive(FTcsCombatEntityHandle Entity)
{
	// 宿主本体论：本实现取"组件仍注册且 Actor 有效"——死亡规则归项目（覆写本类或换实现）
	const TWeakObjectPtr<AActor>* Found = EntityToActor.Find(Entity);
	return Found && Found->IsValid();
}
