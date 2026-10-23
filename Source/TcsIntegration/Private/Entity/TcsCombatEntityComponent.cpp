// Copyright Tirefly. All Rights Reserved.

#include "Entity/TcsCombatEntityComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Entity/TcsPieEntityQuery.h"
#include "TcsAttributeSubsystem.h"
#include "TcsBuffSubsystem.h"
#include "TcsDefinitionSubsystem.h"
#include "TcsEffectSubsystem.h"
#include "TcsIntegrationLogChannel.h"
#include "TcsSkillSubsystem.h"



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

	EntityHandle = AttributeSubsystem->RegisterEntity(Owner->GetFName());

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
		// **跨门面注销级联：四段，顺序是契约**（`ARCH-11` U-2 的裁决落地）。
		//
		// 为什么必须由本组件承担：属性门面**不能**反向调 Buff/Skill——`TcsAttribute.Build.cs`
		// 只依赖 `Core` / `TcsNotation`，加边即成环。而本组件是**身份锚**（`EntityHandle` 的持有者），
		// 也是全仓唯一同时看得见四个门面的地方 ⇒ 只有它能编这个排。
		//
		// **顺序 = 查询映射 → Buff → Skill → 属性账本**，每一段都有理由：
		// ① **查询映射最先**：此后 `EnumerateEntities` 不再吐本句柄，订阅者无法再按句柄反查到
		//    这个即将消失的实体；
		// ② **Buff 必须早于属性账本**：它的移除链走 `StripModifiers` ⇒ 需要属性容器**还活着**
		//    才能按来源撤掉修正器。反过来（账本先销毁）不会崩——`StripModifiers` 有
		//    `IsLedgerReady` 守卫——但会**静默跳过**，"状态走了、属性先还"这条纪律就落空了；
		// ③ **Skill 与 Buff 无顺序耦合**（技能账本零外部资源，只有冷却条目），但它同样会广播
		//    `Removed` 一类事件 ⇒ 排在属性账本之前，保证订阅者读属性时账本仍在；
		// ④ **属性账本最后**：它是"实体存在"的唯一凭证，销毁它就是"实体已死"的最终宣告。
		//
		// ⚠ **②③ 两段此前缺失**（`ARCH-11` U-2）。后果不是"泄漏一点内存"，而是：
		// 实例留在桶里 ⇒ 周期回调仍能 `Find` 到它 ⇒ **每次周期广播一次 `Periodic` 并重新入堆**
		// （`TcsBuffOps_Lifetime.cpp:96-113`），而实体句柄**无代际段且永不复用**
		// （`TcsCombatEntityHandle.h:17`）⇒ 该循环**永不停止**，且每个泄漏桶永久抬高
		// `FTcsBuffOps::Find` 的全桶扫描成本（`TcsBuffOps_Query.cpp:22`）。
		if (UTcsPieEntityQuery* EntityQuery = ResolveEntityQuery().Get())
		{
			EntityQuery->UnregisterEntity(EntityHandle);
		}

		if (UWorld* World = GetWorld())
		{
			// ② 状态：逐条广播 `Removed`（撤修正器 → 退订触发行/行为 → 撤时间条目 → 广播 → 归还槽位）后删桶
			if (UTcsBuffSubsystem* BuffSubsystem = World->GetSubsystem<UTcsBuffSubsystem>())
			{
				const int32 RemovedStates = BuffSubsystem->UnregisterEntity(EntityHandle);
				if (RemovedStates > 0)
				{
					UE_LOG(LogTcsIntegration, Log,
						TEXT("UTcsCombatEntityComponent: 注销时移除状态 %d 条（句柄 %lld）"), RemovedStates, EntityHandle.Id);
				}
			}

			// ③ 技能：摘账本条目 + 撤冷却锚点后删桶
			if (UTcsSkillSubsystem* SkillSubsystem = World->GetSubsystem<UTcsSkillSubsystem>())
			{
				const int32 RevokedSkills = SkillSubsystem->UnregisterEntity(EntityHandle);
				if (RevokedSkills > 0)
				{
					UE_LOG(LogTcsIntegration, Log,
						TEXT("UTcsCombatEntityComponent: 注销时摘除技能 %d 条（句柄 %lld）"), RevokedSkills, EntityHandle.Id);
				}
			}

			// ④ 属性账本最后（它是"实体存在"的唯一凭证）
			if (UTcsAttributeSubsystem* AttributeSubsystem = World->GetSubsystem<UTcsAttributeSubsystem>())
			{
				AttributeSubsystem->UnregisterEntity(EntityHandle);
			}
		}

		UE_LOG(LogTcsIntegration, Log, TEXT("UTcsCombatEntityComponent: 已注销实体（句柄 %lld）"), EntityHandle.Id);
		EntityHandle = FTcsCombatEntityHandle();
	}

	Super::EndPlay(EndPlayReason);
}



double UTcsCombatEntityComponent::GetCurrent(const FGameplayTag& Attribute) const
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



FTcsChainRunHandle UTcsCombatEntityComponent::ExecuteChainById(FGameplayTag ChainId) const
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
