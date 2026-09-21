// Copyright Tirefly. All Rights Reserved.

#include "Entity/TcsCombatEntityComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Entity/TcsPieEntityQuery.h"
#include "TcsAttributeSubsystem.h"
#include "TcsDefinitionSubsystem.h"
#include "TcsEffectSubsystem.h"
#include "TcsIntegrationLogChannel.h"



TWeakObjectPtr<UTcsPieEntityQuery> UTcsCombatEntityComponent::ResolveEntityQuery() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UTcsEffectSubsystem* EffectSubsystem = World->GetSubsystem<UTcsEffectSubsystem>();
	if (!EffectSubsystem)
	{
		return nullptr;
	}

	// 门面注入点取实现（未注入时为 nullptr——"能力尚未注入"是配置状态，不 ensure）
	ITcsEntityQuery* Query = EffectSubsystem->GetEntityQuery();
	return Cast<UTcsPieEntityQuery>(Query);
}



void UTcsCombatEntityComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		UE_LOG(LogTcsIntegration, Warning, TEXT("UTcsCombatEntityComponent::BeginPlay: Owner 或 World 不可得——跳过注册"));
		return;
	}

	// 门禁：定义就绪之后才注册（06 §4 硬规则②；未就绪 = 宿主时序问题 → Warning + 跳过，不 ensure）
	const UGameInstance* GameInstance = World->GetGameInstance();
	UTcsDefinitionSubsystem* DefinitionSubsystem = GameInstance
		? GameInstance->GetSubsystem<UTcsDefinitionSubsystem>()
		: nullptr;

	if (!DefinitionSubsystem)
	{
		UE_LOG(LogTcsIntegration, Warning,
			TEXT("UTcsCombatEntityComponent::BeginPlay: 定义库不可得（%s）——跳过注册"), *Owner->GetName());
		return;
	}

	if (!DefinitionSubsystem->IsRuntimeReady())
	{
		UE_LOG(LogTcsIntegration, Warning,
			TEXT("UTcsCombatEntityComponent::BeginPlay: 定义库未就绪（%s）——跳过注册（时序是宿主责任）"),
			*Owner->GetName());
		return;
	}

	// 身份锚：注册为战斗实体并记录句柄
	UTcsAttributeSubsystem* AttributeSubsystem = World->GetSubsystem<UTcsAttributeSubsystem>();
	if (!AttributeSubsystem)
	{
		UE_LOG(LogTcsIntegration, Warning,
			TEXT("UTcsCombatEntityComponent::BeginPlay: 属性门面不可得（%s）——跳过注册"), *Owner->GetName());
		return;
	}

	EntityHandle = AttributeSubsystem->RegisterUnit(Owner->GetFName());

	// 登记进实体查询映射（宿主侧唯一"句柄 ↔ Actor"映射点）
	if (UTcsPieEntityQuery* EntityQuery = ResolveEntityQuery().Get())
	{
		EntityQuery->RegisterEntity(EntityHandle, Owner);
	}

	UE_LOG(LogTcsIntegration, Log, TEXT("UTcsCombatEntityComponent: 已注册实体 %s（句柄 %lld）"),
		*Owner->GetName(), EntityHandle.Id);
}

void UTcsCombatEntityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EntityHandle.IsValid())
	{
		// 先摘映射（此后查询不再吐本句柄），再注销单位（句柄随之失效）
		if (UTcsPieEntityQuery* EntityQuery = ResolveEntityQuery().Get())
		{
			EntityQuery->UnregisterEntity(EntityHandle);
		}

		if (UWorld* World = GetWorld())
		{
			if (UTcsAttributeSubsystem* AttributeSubsystem = World->GetSubsystem<UTcsAttributeSubsystem>())
			{
				AttributeSubsystem->UnregisterUnit(EntityHandle);
			}
		}

		UE_LOG(LogTcsIntegration, Log, TEXT("UTcsCombatEntityComponent: 已注销实体（句柄 %lld）"), EntityHandle.Id);
		EntityHandle = FTcsCombatEntityHandle();
	}

	Super::EndPlay(EndPlayReason);
}



double UTcsCombatEntityComponent::GetCurrent(const FTcsAttributeName& Attribute) const
{
	if (!EntityHandle.IsValid())
	{
		return 0.0;
	}

	UWorld* World = GetWorld();
	UTcsAttributeSubsystem* AttributeSubsystem = World ? World->GetSubsystem<UTcsAttributeSubsystem>() : nullptr;
	return AttributeSubsystem ? AttributeSubsystem->EvaluateCurrent(EntityHandle, Attribute) : 0.0;
}

bool UTcsCombatEntityComponent::ApplyModifier(const FTcsAttrModInstance& Modifier) const
{
	if (!EntityHandle.IsValid())
	{
		return false;
	}

	UWorld* World = GetWorld();
	UTcsAttributeSubsystem* AttributeSubsystem = World ? World->GetSubsystem<UTcsAttributeSubsystem>() : nullptr;
	return AttributeSubsystem && AttributeSubsystem->ApplyModifier(EntityHandle, Modifier);
}

int32 UTcsCombatEntityComponent::RemoveBySource(const FTcsSourceHandle& Source) const
{
	if (!EntityHandle.IsValid())
	{
		return 0;
	}

	UWorld* World = GetWorld();
	UTcsAttributeSubsystem* AttributeSubsystem = World ? World->GetSubsystem<UTcsAttributeSubsystem>() : nullptr;
	return AttributeSubsystem ? AttributeSubsystem->RemoveBySource(EntityHandle, Source) : 0;
}



FTcsChainRunHandle UTcsCombatEntityComponent::ExecuteChainById(FName ChainId) const
{
	if (!EntityHandle.IsValid())
	{
		UE_LOG(LogTcsIntegration, Warning, TEXT("UTcsCombatEntityComponent::ExecuteChainById: 实体未注册——拒绝起链"));
		return FTcsChainRunHandle();
	}

	UWorld* World = GetWorld();
	UTcsEffectSubsystem* EffectSubsystem = World ? World->GetSubsystem<UTcsEffectSubsystem>() : nullptr;
	if (!EffectSubsystem)
	{
		UE_LOG(LogTcsIntegration, Error, TEXT("UTcsCombatEntityComponent::ExecuteChainById: 效果门面不可得——拒绝起链"));
		return FTcsChainRunHandle();
	}

	// 上下文装配：Caster/Instigator = 自身，Targets = 仅自身
	// （D4-4 v2"默认目标 = 事件目标"；R3 无事件、无 FTcsSelSelf 选择器，故由组件预填）
	FTcsEffectContext Context;
	Context.Caster = EntityHandle;
	Context.Instigator = EntityHandle;
	Context.Targets.Add(EntityHandle);

	return EffectSubsystem->ExecuteChain(ChainId, MoveTemp(Context));
}
