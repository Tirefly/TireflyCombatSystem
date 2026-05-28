// Copyright Tirefly. All Rights Reserved.

#include "Buff/TcsBuffComponent.h"

#include "GameFramework/Actor.h"
#include "Buff/BuffMerger/TcsBuffMerger.h"
#include "Buff/TcsBuffDefinition.h"
#include "Buff/TcsBuffInstance.h"
#include "Misc/ScopeExit.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateManagerSubsystem.h"
#include "State/TcsStateInstance.h"
#include "TcsLogChannels.h"

void FTcsBuffDurationTracker::Add(UTcsBuffInstance* BuffInstance)
{
	if (!IsValid(BuffInstance))
	{
		return;
	}

	TrackedInstances.Add(BuffInstance);
}

void FTcsBuffDurationTracker::Remove(UTcsBuffInstance* BuffInstance)
{
	if (!BuffInstance)
	{
		return;
	}

	TrackedInstances.Remove(BuffInstance);
}

void FTcsBuffDurationTracker::RefreshInstances()
{
	TArray<TObjectPtr<UTcsBuffInstance>> InvalidBuffs;
	for (const TObjectPtr<UTcsBuffInstance>& BuffInstance : TrackedInstances)
	{
		if (!IsValid(BuffInstance) || BuffInstance->GetCurrentStage() == ETcsStateStage::SS_Expired)
		{
			InvalidBuffs.Add(BuffInstance);
		}
	}

	for (const TObjectPtr<UTcsBuffInstance>& Buff : InvalidBuffs)
	{
		TrackedInstances.Remove(Buff);
	}
}

UTcsBuffComponent::UTcsBuffComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTcsBuffComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveOwnerStateComponent();
}

void UTcsBuffComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTcsStateComponent* StateComponent = OwnerStateComponent.Get())
	{
		UnbindOwnerStateEvents(StateComponent);
	}

	Super::EndPlay(EndPlayReason);
}

void UTcsBuffComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickBuffLifecycles(DeltaTime);
}

UTcsBuffComponent* UTcsBuffComponent::GetOrCreateForActor(AActor* OwnerActor)
{
	if (!IsValid(OwnerActor))
	{
		return nullptr;
	}

	UTcsBuffComponent* BuffComponent = OwnerActor->FindComponentByClass<UTcsBuffComponent>();
	if (!BuffComponent)
	{
		BuffComponent = NewObject<UTcsBuffComponent>(OwnerActor, TEXT("TcsBuffComponent"));
		if (BuffComponent)
		{
			OwnerActor->AddOwnedComponent(BuffComponent);
			BuffComponent->RegisterComponent();
		}
	}

	if (BuffComponent)
	{
		if (UTcsStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTcsStateComponent>())
		{
			BuffComponent->InitializeOwnerStateComponent(StateComponent);
		}
	}

	return BuffComponent;
}

UTcsStateComponent* UTcsBuffComponent::GetOwnerStateComponent() const
{
	return ResolveOwnerStateComponent();
}

void UTcsBuffComponent::InitializeOwnerStateComponent(UTcsStateComponent* InStateComponent)
{
	UTcsStateComponent* PreviousStateComponent = OwnerStateComponent.Get();
	if (IsValid(PreviousStateComponent))
	{
		if (PreviousStateComponent == InStateComponent)
		{
			return;
		}

		UnbindOwnerStateEvents(PreviousStateComponent);
	}

	OwnerStateComponent = InStateComponent;
	BindOwnerStateEvents(InStateComponent);
}

float UTcsBuffComponent::GetBuffRemainingDuration(const UTcsBuffInstance* BuffInstance) const
{
	if (IsValid(BuffInstance) && BuffInstance->HasInfiniteDuration())
	{
		return -1.0f;
	}

	if (IsValid(BuffInstance) && BuffInstance->HasFiniteDuration())
	{
		return BuffInstance->RemainingDuration;
	}

	return 0.0f;
}

void UTcsBuffComponent::RefreshBuffRemainingDuration(UTcsBuffInstance* BuffInstance)
{
	if (!IsValid(BuffInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] BuffInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	if (!BuffInstance->GetStateDef() || BuffInstance->HasInfiniteDuration() || !BuffInstance->HasFiniteDuration())
	{
		return;
	}

	BeginPublicEventBatch();
	ON_SCOPE_EXIT
	{
		EndPublicEventBatch();
	};

	const float NewRemaining = BuffInstance->GetTotalDuration();
	BuffInstance->RemainingDuration = NewRemaining;
	DurationTracker.Add(BuffInstance);

	NotifyBuffDurationRefreshed(BuffInstance, NewRemaining);
}

void UTcsBuffComponent::SetBuffRemainingDuration(UTcsBuffInstance* BuffInstance, float InDurationRemaining)
{
	if (!IsValid(BuffInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] BuffInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	if (!BuffInstance->GetStateDef() || BuffInstance->HasInfiniteDuration() || !BuffInstance->HasFiniteDuration())
	{
		return;
	}

	BeginPublicEventBatch();
	ON_SCOPE_EXIT
	{
		EndPublicEventBatch();
	};

	BuffInstance->RemainingDuration = FMath::Max(0.0f, InDurationRemaining);
	DurationTracker.Add(BuffInstance);

	NotifyBuffDurationRefreshed(BuffInstance, BuffInstance->RemainingDuration);
}

bool UTcsBuffComponent::GetBuffsInSlot(FGameplayTag SlotTag, TArray<UTcsBuffInstance*>& OutBuffs) const
{
	OutBuffs.Reset();

	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return false;
	}

	TArray<UTcsStateInstance*> StatesInSlot;
	if (!StateComponent->GetStatesInSlot(SlotTag, StatesInSlot))
	{
		return false;
	}

	for (UTcsStateInstance* StateInstance : StatesInSlot)
	{
		if (UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance))
		{
			OutBuffs.Add(BuffInstance);
		}
	}

	return OutBuffs.Num() > 0;
}

bool UTcsBuffComponent::GetBuffsByDefId(FName BuffDefId, TArray<UTcsBuffInstance*>& OutBuffs) const
{
	OutBuffs.Reset();

	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return false;
	}

	TArray<UTcsStateInstance*> StatesByDefId;
	if (!StateComponent->GetStatesByDefId(BuffDefId, StatesByDefId))
	{
		return false;
	}

	for (UTcsStateInstance* StateInstance : StatesByDefId)
	{
		if (UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance))
		{
			OutBuffs.Add(BuffInstance);
		}
	}

	return OutBuffs.Num() > 0;
}

bool UTcsBuffComponent::GetAllActiveBuffs(TArray<UTcsBuffInstance*>& OutBuffs) const
{
	OutBuffs.Reset();

	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return false;
	}

	TArray<UTcsStateInstance*> ActiveStates;
	if (!StateComponent->GetAllActiveStates(ActiveStates))
	{
		return false;
	}

	for (UTcsStateInstance* StateInstance : ActiveStates)
	{
		if (UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance))
		{
			OutBuffs.Add(BuffInstance);
		}
	}

	return OutBuffs.Num() > 0;
}

bool UTcsBuffComponent::HasBuffWithDefId(FName BuffDefId) const
{
	TArray<UTcsBuffInstance*> Buffs;
	return GetBuffsByDefId(BuffDefId, Buffs);
}

bool UTcsBuffComponent::HasActiveBuffInSlot(FGameplayTag SlotTag) const
{
	TArray<UTcsBuffInstance*> Buffs;
	return GetBuffsInSlot(SlotTag, Buffs);
}

void UTcsBuffComponent::BroadcastBuffRuntimeDeltaBatchEvent(
	const TArray<FTcsBuffRuntimeDeltaEventPayload>& Payloads) const
{
	if (!Payloads.IsEmpty() && OnBuffRuntimeDelta.IsBound())
	{
		OnBuffRuntimeDelta.Broadcast(Payloads);
	}
}

void UTcsBuffComponent::BroadcastBuffRemovedBatchEvent(
	const TArray<FTcsBuffRemovedEventPayload>& Payloads) const
{
	if (!Payloads.IsEmpty() && OnBuffRemoved.IsBound())
	{
		OnBuffRemoved.Broadcast(Payloads);
	}
}

void UTcsBuffComponent::BeginPublicEventBatch()
{
	// 嵌套链路只累计深度，真正的提交动作留给最外层批次统一收口。
	++PublicEventBatchDepth;
}

void UTcsBuffComponent::EndPublicEventBatch()
{
	// 防守式收尾：如果外部成对调用失衡，直接清空挂起结果，避免脏事件跨帧残留。
	if (PublicEventBatchDepth <= 0)
	{
		PendingBuffRuntimeDeltaEvents.Reset();
		PendingBuffRemovedEvents.Reset();
		PublicEventBatchDepth = 0;
		return;
	}

	--PublicEventBatchDepth;
	if (PublicEventBatchDepth == 0)
	{
		// 销毁路径不再对外广播，避免在宿主回收阶段触发不稳定的重入访问。
		if (IsBeingDestroyed())
		{
			PendingBuffRuntimeDeltaEvents.Reset();
			PendingBuffRemovedEvents.Reset();
			return;
		}

		FlushPendingPublicEvents();
	}
}

void UTcsBuffComponent::FlushPendingPublicEvents()
{
	if (PendingBuffRuntimeDeltaEvents.IsEmpty() && PendingBuffRemovedEvents.IsEmpty())
	{
		return;
	}

	TArray<FTcsBuffRuntimeDeltaEventPayload> RuntimePayloads = MoveTemp(PendingBuffRuntimeDeltaEvents);
	TArray<FTcsBuffRemovedEventPayload> RemovedPayloads = MoveTemp(PendingBuffRemovedEvents);
	PendingBuffRuntimeDeltaEvents.Reset();
	PendingBuffRemovedEvents.Reset();

	// 过滤掉已经失效的 Buff，或者虽然占了一个槽位但最终没有留下任何可见变化的空载荷。
	RuntimePayloads.RemoveAll([](const FTcsBuffRuntimeDeltaEventPayload& Payload)
	{
		return !IsValid(Payload.BuffInstance.Get())
			|| (!Payload.bStackCountChanged
				&& !Payload.bMaxStackCountChanged
				&& !Payload.bPeriodChanged
				&& !Payload.bDurationRefreshed);
	});

	RemovedPayloads.RemoveAll([](const FTcsBuffRemovedEventPayload& Payload)
	{
		return !IsValid(Payload.BuffInstance.Get());
	});

	// 保持先 RuntimeDelta、后 Removed 的公共顺序，便于外部观察同批最终态。
	BroadcastBuffRuntimeDeltaBatchEvent(RuntimePayloads);
	BroadcastBuffRemovedBatchEvent(RemovedPayloads);
}

FTcsBuffRuntimeDeltaEventPayload* UTcsBuffComponent::FindPendingBuffRuntimeDeltaEvent(UTcsBuffInstance* BuffInstance)
{
	if (!IsValid(BuffInstance))
	{
		return nullptr;
	}

	return PendingBuffRuntimeDeltaEvents.FindByPredicate([BuffInstance](const FTcsBuffRuntimeDeltaEventPayload& Payload)
	{
		return Payload.BuffInstance.Get() == BuffInstance;
	});
}

FTcsBuffRemovedEventPayload* UTcsBuffComponent::FindPendingBuffRemovedEvent(UTcsBuffInstance* BuffInstance)
{
	if (!IsValid(BuffInstance))
	{
		return nullptr;
	}

	return PendingBuffRemovedEvents.FindByPredicate([BuffInstance](const FTcsBuffRemovedEventPayload& Payload)
	{
		return Payload.BuffInstance.Get() == BuffInstance;
	});
}

bool UTcsBuffComponent::HasPendingBuffRemovedEvent(UTcsBuffInstance* BuffInstance) const
{
	if (!IsValid(BuffInstance))
	{
		return false;
	}

	return PendingBuffRemovedEvents.ContainsByPredicate([BuffInstance](const FTcsBuffRemovedEventPayload& Payload)
	{
		return Payload.BuffInstance.Get() == BuffInstance;
	});
}

void UTcsBuffComponent::DiscardPendingBuffRuntimeDeltaEvent(UTcsBuffInstance* BuffInstance)
{
	if (!IsValid(BuffInstance))
	{
		return;
	}

	PendingBuffRuntimeDeltaEvents.RemoveAll([BuffInstance](const FTcsBuffRuntimeDeltaEventPayload& Payload)
	{
		return Payload.BuffInstance.Get() == BuffInstance;
	});
}

void UTcsBuffComponent::QueueBuffStackChangedEvent(
	UTcsBuffInstance* BuffInstance,
	int32 OldStackCount,
	int32 NewStackCount)
{
	// 一旦同批次已经确定移除，这个 Buff 的运行时变化对外就不再有意义。
	if (!IsValid(BuffInstance) || OldStackCount == NewStackCount || HasPendingBuffRemovedEvent(BuffInstance))
	{
		return;
	}

	FTcsBuffRuntimeDeltaEventPayload* PendingPayload = FindPendingBuffRuntimeDeltaEvent(BuffInstance);
	if (!PendingPayload)
	{
		PendingPayload = &PendingBuffRuntimeDeltaEvents.Add_GetRef(FTcsBuffRuntimeDeltaEventPayload(BuffInstance));
	}

	if (!PendingPayload->bStackCountChanged)
	{
		// 只保留本批次第一次看到的旧值，后续更新继续覆盖 New 值即可形成 Old -> Final。
		PendingPayload->bStackCountChanged = true;
		PendingPayload->OldStackCount = OldStackCount;
	}

	PendingPayload->NewStackCount = NewStackCount;
}

void UTcsBuffComponent::QueueBuffMaxStackCountChangedEvent(
	UTcsBuffInstance* BuffInstance,
	int32 OldMaxStackCount,
	int32 NewMaxStackCount)
{
	// 同批移除优先级高于运行时变化，避免对外暴露已失效的中间态。
	if (!IsValid(BuffInstance) || OldMaxStackCount == NewMaxStackCount || HasPendingBuffRemovedEvent(BuffInstance))
	{
		return;
	}

	FTcsBuffRuntimeDeltaEventPayload* PendingPayload = FindPendingBuffRuntimeDeltaEvent(BuffInstance);
	if (!PendingPayload)
	{
		PendingPayload = &PendingBuffRuntimeDeltaEvents.Add_GetRef(FTcsBuffRuntimeDeltaEventPayload(BuffInstance));
	}

	if (!PendingPayload->bMaxStackCountChanged)
	{
		// 最大叠层同样只记录第一次旧值，最后一次新值由后续写入覆盖。
		PendingPayload->bMaxStackCountChanged = true;
		PendingPayload->OldMaxStackCount = OldMaxStackCount;
	}

	PendingPayload->NewMaxStackCount = NewMaxStackCount;
}

void UTcsBuffComponent::QueueBuffPeriodChangedEvent(
	UTcsBuffInstance* BuffInstance,
	float OldPeriod,
	float NewPeriod)
{
	// Removed 已经挂起时，不再为该 Buff 累积周期变化结果。
	if (!IsValid(BuffInstance) || OldPeriod == NewPeriod || HasPendingBuffRemovedEvent(BuffInstance))
	{
		return;
	}

	FTcsBuffRuntimeDeltaEventPayload* PendingPayload = FindPendingBuffRuntimeDeltaEvent(BuffInstance);
	if (!PendingPayload)
	{
		PendingPayload = &PendingBuffRuntimeDeltaEvents.Add_GetRef(FTcsBuffRuntimeDeltaEventPayload(BuffInstance));
	}

	if (!PendingPayload->bPeriodChanged)
	{
		// 周期变化也按 Old -> Final 收敛，外部无需感知中间多次改写。
		PendingPayload->bPeriodChanged = true;
		PendingPayload->OldPeriod = OldPeriod;
	}

	PendingPayload->NewPeriod = NewPeriod;
}

void UTcsBuffComponent::QueueBuffDurationRefreshedEvent(
	UTcsBuffInstance* BuffInstance,
	float NewDuration)
{
	// 持续时间只关心本批次对外可见的最终剩余值，因此每次直接覆盖即可。
	if (!IsValid(BuffInstance) || HasPendingBuffRemovedEvent(BuffInstance))
	{
		return;
	}

	FTcsBuffRuntimeDeltaEventPayload* PendingPayload = FindPendingBuffRuntimeDeltaEvent(BuffInstance);
	if (!PendingPayload)
	{
		PendingPayload = &PendingBuffRuntimeDeltaEvents.Add_GetRef(FTcsBuffRuntimeDeltaEventPayload(BuffInstance));
	}

	PendingPayload->bDurationRefreshed = true;
	PendingPayload->NewDuration = NewDuration;
}

void UTcsBuffComponent::QueueBuffRemovedEvent(UTcsBuffInstance* BuffInstance, FName RemovalReason)
{
	if (!IsValid(BuffInstance))
	{
		return;
	}

	// Removed 是该 Buff 在本批次里的终态结果，之前累计的 RuntimeDelta 全部失效。
	DiscardPendingBuffRuntimeDeltaEvent(BuffInstance);

	FTcsBuffRemovedEventPayload* PendingPayload = FindPendingBuffRemovedEvent(BuffInstance);
	if (!PendingPayload)
	{
		PendingPayload = &PendingBuffRemovedEvents.Add_GetRef(FTcsBuffRemovedEventPayload(BuffInstance, RemovalReason));
		return;
	}

	if (PendingPayload->RemovalReason.IsNone() || !RemovalReason.IsNone())
	{
		// 后续补来的有效 RemovalReason 可以覆盖之前的空原因，保留更完整的终态语义。
		PendingPayload->RemovalReason = RemovalReason;
	}
}

bool UTcsBuffComponent::ApplyBuff(
	FName BuffDefId,
	AActor* Instigator,
	int32 BuffLevel,
	const FTcsSourceHandle& ParentSourceHandle)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	auto ReportApplyFailure = [&](ETcsStateApplyFailReason FailureReason, const FString& FailureMessage)
	{
		if (IsValid(StateComponent))
		{
			if (AActor* OwnerActor = StateComponent->GetOwner())
			{
				StateComponent->NotifyStateApplyFailed(OwnerActor, BuffDefId, FailureReason, FailureMessage);
			}
		}

		return false;
	};

	if (!IsValid(StateComponent))
	{
		return false;
	}

	if (BuffDefId.IsNone())
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidInput,
			TEXT("BuffDefId is None."));
	}

	if (!IsValid(Instigator))
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidInput,
			TEXT("Instigator is invalid."));
	}

	UTcsStateManagerSubsystem* StateManager = StateComponent->GetStateManager();
	if (!StateManager)
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidInput,
			TEXT("Failed to resolve StateManagerSubsystem."));
	}

	const UTcsStateDefinition* StateDef = StateManager->GetStateDefinition(BuffDefId);
	if (!StateDef)
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidStateDefinition,
			TEXT("Invalid buff definition."));
	}

	if (!StateDef->IsA<UTcsBuffDefinition>())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] State definition %s is not a BuffDefinition."),
			*FString(__FUNCTION__),
			*BuffDefId.ToString());

		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidStateDefinition,
			TEXT("Target definition is not a BuffDefinition."));
	}

	return StateComponent->TryApplyState(BuffDefId, Instigator, BuffLevel, ParentSourceHandle);
}

bool UTcsBuffComponent::RemoveBuff(UTcsBuffInstance* BuffInstance, FName RemovalReason)
{
	if (!IsValid(BuffInstance))
	{
		return false;
	}

	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return false;
	}

	if (RemovalReason.IsNone())
	{
		RemovalReason = TcsStateRemovalReasons::Removed;
	}

	BeginPublicEventBatch();
	ON_SCOPE_EXIT
	{
		EndPublicEventBatch();
	};

	return StateComponent->RequestStateRemoval(BuffInstance, RemovalReason);
}

void UTcsBuffComponent::RegisterBuffInstance(UTcsStateInstance* StateInstance)
{
	UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance);
	if (!BuffInstance)
	{
		return;
	}

	if (BuffInstance->GetDurationType() == ETcsBuffDurationType::SDT_Duration)
	{
		DurationTracker.Add(BuffInstance);
	}
}

void UTcsBuffComponent::UnregisterBuffInstance(UTcsStateInstance* StateInstance)
{
	if (UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance))
	{
		DurationTracker.Remove(BuffInstance);
	}
}

void UTcsBuffComponent::RemoveBuffInstance(UTcsStateInstance* StateInstance, FName RemovalReason)
{
	if (UTcsStateComponent* StateComponent = ResolveOwnerStateComponent())
	{
		BeginPublicEventBatch();
		ON_SCOPE_EXIT
		{
			EndPublicEventBatch();
		};

		StateComponent->RequestStateRemoval(StateInstance, RemovalReason);
	}
}

void UTcsBuffComponent::ExpireBuffInstance(UTcsStateInstance* StateInstance)
{
	RemoveBuffInstance(StateInstance, TcsBuffRemovalReasons::Expired);
}

void UTcsBuffComponent::TickBuffLifecycles(float DeltaTime)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return;
	}

	TArray<UTcsStateInstance*> ExpiredStates;
	TArray<UTcsStateInstance*> InvalidStates;

	for (const TObjectPtr<UTcsBuffInstance>& TrackedBuffInstance : DurationTracker.TrackedInstances)
	{
		UTcsBuffInstance* BuffInstance = TrackedBuffInstance.Get();
		UTcsStateInstance* StateInstance = BuffInstance;

		if (!IsValid(StateInstance))
		{
			InvalidStates.Add(StateInstance);
			continue;
		}

		const ETcsStateStage CurrentStage = StateInstance->GetCurrentStage();
		if (CurrentStage == ETcsStateStage::SS_Expired)
		{
			InvalidStates.Add(StateInstance);
			continue;
		}

		if (CurrentStage != ETcsStateStage::SS_Active && CurrentStage != ETcsStateStage::SS_HangUp)
		{
			continue;
		}

		if (BuffInstance->RemainingDuration <= 0.0f)
		{
			ExpiredStates.Add(StateInstance);
			continue;
		}

		BuffInstance->RemainingDuration = FMath::Max(0.0f, BuffInstance->RemainingDuration - DeltaTime);
		if (BuffInstance->RemainingDuration <= 0.0f)
		{
			ExpiredStates.Add(StateInstance);
		}
	}

	// 同一轮生命周期 Tick 里可能会命中多个到期 Buff，而且它们可能共享同一个槽位。
	// 如果逐个直接移除，每次移除都会在状态侧请求一次槽位刷新，最终把同一槽位的收敛逻辑重复跑多次。
	// 这里先进入一次槽位刷新批处理，把这一轮到期移除压缩成批次末的一次最终结算。
	const bool bBatchExpiredRemovals = ExpiredStates.Num() > 1;
	const bool bBatchExpiredPublicEvents = ExpiredStates.Num() > 1;
	if (bBatchExpiredPublicEvents)
	{
		BeginPublicEventBatch();
	}

	if (bBatchExpiredRemovals)
	{
		StateComponent->BeginStateSlotActivationBatch();
	}

	// 到期处理过程中可能级联触发状态移除、Buff 注销，甚至把宿主组件或 Owner 一起带进销毁流程。
	// 这里不能在检测到销毁后直接 return，否则前面已经打开的批处理作用域无法对称关闭，
	// 最终会把批处理深度和待排空请求都留在不一致状态。
	bool bAbortExpiredProcessing = false;
	for (UTcsStateInstance* ExpiredState : ExpiredStates)
	{
		if (UTcsBuffInstance* ExpiredBuff = ResolveBuffInstance(ExpiredState))
		{
			HandleBuffDurationExpired(ExpiredBuff);
			if (StateComponent->IsBeingDestroyed() || !IsValid(StateComponent->GetOwner()))
			{
				bAbortExpiredProcessing = true;
				break;
			}
		}
		else
		{
			InvalidStates.Add(ExpiredState);
		}
	}

	// 只要前面进入过批处理，这里就必须对称结束。
	// 正常情况下会在最外层统一排空待处理槽位；销毁路径则交给状态组件内部做安全收尾。
	if (bBatchExpiredRemovals)
	{
		StateComponent->EndStateSlotActivationBatch();
	}

	if (bBatchExpiredPublicEvents)
	{
		EndPublicEventBatch();
	}

	if (bAbortExpiredProcessing)
	{
		return;
	}

	for (UTcsStateInstance* InvalidState : InvalidStates)
	{
		UnregisterBuffInstance(InvalidState);
	}
}

void UTcsBuffComponent::HandleBuffStackCountChangedInternal(
	UTcsBuffInstance* BuffInstance,
	int32 OldStackCount,
	int32 NewStackCount)
{
	if (!IsValid(BuffInstance) || NewStackCount <= OldStackCount)
	{
		return;
	}

	const UTcsBuffDefinition* BuffDef = BuffInstance->GetBuffDef();
	if (!BuffDef || BuffDef->MaxStackCount <= 1)
	{
		return;
	}

	switch (BuffDef->OnStackIncrease.DurationPolicy)
	{
	case ETcsBuffDurationRefreshPolicy::RefreshRemainingToTotal:
		RefreshBuffRemainingDuration(BuffInstance);
		break;

	case ETcsBuffDurationRefreshPolicy::None:
	default:
		break;
	}
}

void UTcsBuffComponent::HandleBuffDurationExpired(UTcsBuffInstance* BuffInstance)
{
	if (!IsValid(BuffInstance))
	{
		return;
	}

	BeginPublicEventBatch();
	ON_SCOPE_EXIT
	{
		EndPublicEventBatch();
	};

	const UTcsBuffDefinition* BuffDef = BuffInstance->GetBuffDef();
	const bool bUsesReactiveExpiration = BuffDef
		&& BuffDef->DurationType == ETcsBuffDurationType::SDT_Duration
		&& BuffDef->MaxStackCount > 1;
	const ETcsBuffStackExpirationPolicy ExpirationPolicy = bUsesReactiveExpiration
		? BuffDef->OnDurationExpired.ExpirationPolicy
		: ETcsBuffStackExpirationPolicy::ClearEntireBuff;
	const int32 CurrentStackCount = BuffInstance->GetStackCount();

	if (ExpirationPolicy == ETcsBuffStackExpirationPolicy::ClearEntireBuff || CurrentStackCount <= 1)
	{
		RemoveBuffInstance(BuffInstance, TcsBuffRemovalReasons::Expired);
		return;
	}

	BuffInstance->SetStackCount(CurrentStackCount - 1);
	if (!IsValid(BuffInstance) || BuffInstance->GetCurrentStage() == ETcsStateStage::SS_Expired)
	{
		return;
	}

	if (ExpirationPolicy == ETcsBuffStackExpirationPolicy::RemoveSingleStackAndRefreshDuration)
	{
		RefreshBuffRemainingDuration(BuffInstance);
	}
}

void UTcsBuffComponent::RefreshTrackedBuffs()
{
	DurationTracker.RefreshInstances();
}

void UTcsBuffComponent::RebuildBuffMergeGroups(FTcsStateSlot* StateSlot)
{
	if (!StateSlot)
	{
		return;
	}

	TMap<FName, ETcsBuffMergeDirtyReason> ExistingDirtyReasons;
	for (const TPair<FName, FTcsBuffMergeGroupRuntime>& Pair : StateSlot->BuffMergeGroups)
	{
		if (Pair.Value.HasDirty())
		{
			ExistingDirtyReasons.Add(Pair.Key, Pair.Value.DirtyReasons);
		}
	}

	StateSlot->BuffMergeGroups.Empty();

	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (UTcsBuffInstance* BuffInstance = ResolveBuffInstance(State))
		{
			FTcsBuffMergeGroupRuntime& GroupRuntime = StateSlot->BuffMergeGroups.FindOrAdd(BuffInstance->GetStateDefId());
			GroupRuntime.StateDefId = BuffInstance->GetStateDefId();
			GroupRuntime.AddMember(BuffInstance);
		}
	}

	for (TPair<FName, FTcsBuffMergeGroupRuntime>& Pair : StateSlot->BuffMergeGroups)
	{
		if (const ETcsBuffMergeDirtyReason* ExistingReasons = ExistingDirtyReasons.Find(Pair.Key))
		{
			Pair.Value.MarkDirty(*ExistingReasons);
			StateSlot->DirtyBuffMergeStateDefIds.Add(Pair.Key);
		}

		if (StateSlot->bBuffMergeRequiresFullRebuild)
		{
			Pair.Value.MarkDirty(ETcsBuffMergeDirtyReason::ForceRebuild);
			StateSlot->DirtyBuffMergeStateDefIds.Add(Pair.Key);
		}
	}

	StateSlot->bBuffMergeRequiresFullRebuild = false;
}

bool UTcsBuffComponent::ShouldProcessBuffMergeGroup(
	ETcsBuffMergeDirtyReason DirtyReasons,
	ETcsBuffMergeDependencyFlags DependencyFlags) const
{
	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::ForceRebuild))
	{
		return true;
	}

	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::MembershipChanged)
		&& EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::MemberSet))
	{
		return true;
	}

	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::RuntimeValueChanged)
		&& EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::RuntimeStack))
	{
		return true;
	}

	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::ExecutionStageChanged)
		&& EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::ExecutionStage))
	{
		return true;
	}

	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::SlotGateChanged)
		&& EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::SlotGateState))
	{
		return true;
	}

	return false;
}

void UTcsBuffComponent::ProcessBuffMerging(FTcsStateSlot* StateSlot)
{
	if (!StateSlot)
	{
		return;
	}

	if (StateSlot->bBuffMergeRequiresFullRebuild)
	{
		RebuildBuffMergeGroups(StateSlot);
	}

	if (StateSlot->DirtyBuffMergeStateDefIds.IsEmpty())
	{
		return;
	}

	const TSet<FName> DirtyStateDefIds = StateSlot->DirtyBuffMergeStateDefIds;
	StateSlot->DirtyBuffMergeStateDefIds.Empty();

	TArray<UTcsBuffInstance*> AllMergedOutBuffs;
	for (const FName& StateDefId : DirtyStateDefIds)
	{
		FTcsBuffMergeGroupRuntime* GroupRuntime = StateSlot->BuffMergeGroups.Find(StateDefId);
		if (!GroupRuntime)
		{
			continue;
		}

		TArray<UTcsBuffInstance*> GroupMembers;
		GroupRuntime->GatherValidMembers(GroupMembers);
		if (GroupMembers.IsEmpty())
		{
			StateSlot->BuffMergeGroups.Remove(StateDefId);
			continue;
		}

		UTcsBuffInstance* MergeSource = GroupMembers[0];
		TSubclassOf<UTcsBuffMerger> MergerClass = MergeSource ? MergeSource->GetMergerType() : nullptr;
		UTcsBuffMerger* Merger = MergerClass ? MergerClass->GetDefaultObject<UTcsBuffMerger>() : nullptr;
		const ETcsBuffMergeDependencyFlags DependencyFlags = Merger
			? Merger->GetDependencyFlags()
			: ETcsBuffMergeDependencyFlags::None;
		GroupRuntime->DependencyFlags = DependencyFlags;
		const ETcsBuffMergeDirtyReason PendingDirtyReasons = GroupRuntime->DirtyReasons;

		if (!ShouldProcessBuffMergeGroup(PendingDirtyReasons, DependencyFlags))
		{
			GroupRuntime->LastProcessedDirtyReasons = PendingDirtyReasons;
			GroupRuntime->ClearDirty(PendingDirtyReasons);
			continue;
		}

		TArray<UTcsBuffInstance*> MergedGroup;
		TArray<UTcsBuffInstance*> MergedOutGroup;
		MergeBuffStateGroup(GroupMembers, MergedGroup, MergedOutGroup);
		GroupRuntime->SetMembers(MergedGroup);
		GroupRuntime->LastProcessedDirtyReasons = PendingDirtyReasons;
		GroupRuntime->ClearDirty(PendingDirtyReasons);
		AllMergedOutBuffs.Append(MergedOutGroup);
	}

	RemoveMergedOutBuffs(StateSlot, AllMergedOutBuffs);
}

void UTcsBuffComponent::MergeBuffStateGroup(
	TArray<UTcsBuffInstance*>& BuffsToMerge,
	TArray<UTcsBuffInstance*>& OutMergedBuffs,
	TArray<UTcsBuffInstance*>& OutMergedOutBuffs)
{
	if (BuffsToMerge.Num() == 0)
	{
		return;
	}

	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	UTcsStateManagerSubsystem* LocalStateMgr = StateComponent->GetStateManager();
	if (!LocalStateMgr)
	{
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	const UTcsStateDefinition* StateDef = LocalStateMgr->GetStateDefinition(BuffsToMerge[0]->GetStateDefId());
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to get state definition for %s"),
			*FString(__FUNCTION__),
			*BuffsToMerge[0]->GetStateDefId().ToString());
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	UTcsBuffInstance* MergeSource = BuffsToMerge[0];
	if (!MergeSource)
	{
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	TSubclassOf<UTcsBuffMerger> MergerClass = MergeSource->GetMergerType();
	if (!MergerClass)
	{
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	UTcsBuffMerger* Merger = MergerClass->GetDefaultObject<UTcsBuffMerger>();
	if (!IsValid(Merger))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to get merger instance for %s"),
			*FString(__FUNCTION__),
			*MergerClass->GetName());
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	Merger->Merge(BuffsToMerge, OutMergedBuffs, OutMergedOutBuffs);
}

void UTcsBuffComponent::RemoveMergedOutBuffs(
	FTcsStateSlot* StateSlot,
	const TArray<UTcsBuffInstance*>& MergedOutBuffs)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !StateSlot)
	{
		return;
	}

	// 一次合并可能会把同槽位里的多个旧 Buff 一起淘汰。
	// 这些 Buff 的移除最终都会汇聚到同一个槽位刷新链路里，因此先包一层批处理，
	// 避免每淘汰一个 Buff 就对同一个槽位重跑一次完整结算。
	const bool bBatchMergedOutRemovals = MergedOutBuffs.Num() > 1;
	const bool bBatchMergedOutPublicEvents = MergedOutBuffs.Num() > 1;
	if (bBatchMergedOutPublicEvents)
	{
		BeginPublicEventBatch();
	}

	if (bBatchMergedOutRemovals)
	{
		StateComponent->BeginStateSlotActivationBatch();
	}

	for (UTcsBuffInstance* BuffInstance : MergedOutBuffs)
	{
		// 这里只处理当前仍然挂在这个槽位上的实例。
		// 如果某个 Buff 已经在前面的链路里被回收，再次进入移除主链只会制造重复工作。
		if (!IsValid(BuffInstance) || !StateSlot->States.Contains(BuffInstance))
		{
			continue;
		}

		StateComponent->RequestStateRemoval(BuffInstance, TcsBuffRemovalReasons::MergedOut);
	}

	// 统一在批次尾部结束，让状态组件按槽位去重后再做最终刷新。
	if (bBatchMergedOutRemovals)
	{
		StateComponent->EndStateSlotActivationBatch();
	}

	if (bBatchMergedOutPublicEvents)
	{
		EndPublicEventBatch();
	}
}

void UTcsBuffComponent::MarkBuffMergeGroupDirty(UTcsBuffInstance* BuffInstance, ETcsBuffMergeDirtyReason DirtyReason)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	const UTcsStateDefinition* StateDef = IsValid(BuffInstance) ? BuffInstance->GetStateDef() : nullptr;
	if (!IsValid(StateComponent) || !IsValid(BuffInstance) || !StateDef || !StateDef->StateSlotType.IsValid())
	{
		return;
	}

	FTcsStateSlot* StateSlot = StateComponent->FindRuntimeStateSlot(StateDef->StateSlotType);
	if (!StateSlot)
	{
		return;
	}

	StateSlot->MarkBuffMergeGroupDirty(BuffInstance->GetStateDefId(), DirtyReason);
	StateComponent->RequestStateSlotRefresh(StateDef->StateSlotType);
}

void UTcsBuffComponent::GetDebugStateOverlay(const UTcsStateInstance* StateInstance, int32& OutStackCount, FString& OutDurationText) const
{
	OutStackCount = -1;
	OutDurationText = TEXT("0.00");

	const UTcsBuffInstance* BuffInstance = Cast<UTcsBuffInstance>(StateInstance);
	if (!BuffInstance)
	{
		return;
	}

	OutStackCount = BuffInstance->GetStackCount();
	const float RemainingDuration = GetBuffRemainingDuration(BuffInstance);
	OutDurationText = (BuffInstance->HasInfiniteDuration() || RemainingDuration < 0.0f)
		? TEXT("Inf")
		: FString::Printf(TEXT("%.2f"), RemainingDuration);
}

bool UTcsBuffComponent::GetBuffMergeDebugLines(FGameplayTag SlotTag, TArray<FString>& OutDebugLines) const
{
	OutDebugLines.Reset();

	if (!SlotTag.IsValid())
	{
		return false;
	}

	const UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return false;
	}

	const FTcsStateSlot* StateSlot = StateComponent->FindRuntimeStateSlot(SlotTag);
	if (!StateSlot)
	{
		return false;
	}

	OutDebugLines.Add(FString::Printf(
		TEXT("Slot=%s Gate=%s FullRebuild=%s DirtyGroupCount=%d GroupCount=%d"),
		*SlotTag.ToString(),
		StateSlot->bIsGateOpen ? TEXT("Open") : TEXT("Closed"),
		StateSlot->bBuffMergeRequiresFullRebuild ? TEXT("true") : TEXT("false"),
		StateSlot->DirtyBuffMergeStateDefIds.Num(),
		StateSlot->BuffMergeGroups.Num()));

	TArray<FName> StateDefIds;
	StateSlot->BuffMergeGroups.GetKeys(StateDefIds);
	StateDefIds.Sort([](const FName& A, const FName& B)
	{
		return A.ToString() < B.ToString();
	});

	for (const FName& StateDefId : StateDefIds)
	{
		const FTcsBuffMergeGroupRuntime* GroupRuntime = StateSlot->BuffMergeGroups.Find(StateDefId);
		if (!GroupRuntime)
		{
			continue;
		}

		OutDebugLines.Add(FString::Printf(
			TEXT("Group=%s Members=%d Pending=%s Dirty=%s LastProcessed=%s Dependencies=%s"),
			*StateDefId.ToString(),
			TcsBuffMergeRuntime::CountValidMembers(*GroupRuntime),
			StateSlot->DirtyBuffMergeStateDefIds.Contains(StateDefId) ? TEXT("true") : TEXT("false"),
			*TcsBuffMergeRuntime::FormatDirtyReasons(GroupRuntime->DirtyReasons),
			*TcsBuffMergeRuntime::FormatDirtyReasons(GroupRuntime->LastProcessedDirtyReasons),
			*TcsBuffMergeRuntime::FormatDependencyFlags(GroupRuntime->DependencyFlags)));
	}

	return true;
}

void UTcsBuffComponent::NotifyBuffStackChanged(UTcsBuffInstance* BuffInstance, int32 OldStackCount, int32 NewStackCount)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !IsValid(BuffInstance) || OldStackCount == NewStackCount)
	{
		return;
	}

	QueueBuffStackChangedEvent(BuffInstance, OldStackCount, NewStackCount);
	MarkBuffMergeGroupDirty(BuffInstance, ETcsBuffMergeDirtyReason::RuntimeValueChanged);

	// 没有显式批次时，沿用“本次调用立即可见”的旧行为，直接提交当前结果。
	if (PublicEventBatchDepth == 0)
	{
		FlushPendingPublicEvents();
	}
}

void UTcsBuffComponent::NotifyBuffMaxStackCountChanged(UTcsBuffInstance* BuffInstance, int32 OldMaxStackCount, int32 NewMaxStackCount)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !IsValid(BuffInstance) || OldMaxStackCount == NewMaxStackCount)
	{
		return;
	}

	QueueBuffMaxStackCountChangedEvent(BuffInstance, OldMaxStackCount, NewMaxStackCount);
	MarkBuffMergeGroupDirty(BuffInstance, ETcsBuffMergeDirtyReason::RuntimeValueChanged);

	// 外层没有批处理包裹时，立刻把这一次变化对外提交出去。
	if (PublicEventBatchDepth == 0)
	{
		FlushPendingPublicEvents();
	}
}

void UTcsBuffComponent::NotifyBuffPeriodChanged(UTcsBuffInstance* BuffInstance, float OldPeriod, float NewPeriod)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !IsValid(BuffInstance) || OldPeriod == NewPeriod)
	{
		return;
	}

	QueueBuffPeriodChangedEvent(BuffInstance, OldPeriod, NewPeriod);
	// 周期变化本身不影响 merge group，但仍要在非批处理路径里立即提交公共结果。
	if (PublicEventBatchDepth == 0)
	{
		FlushPendingPublicEvents();
	}
}

void UTcsBuffComponent::NotifyBuffDurationRefreshed(UTcsBuffInstance* BuffInstance, float NewDuration)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !IsValid(BuffInstance))
	{
		return;
	}

	QueueBuffDurationRefreshedEvent(BuffInstance, NewDuration);
	// 持续时间刷新同样允许在无批次时直接向外暴露最终结果。
	if (PublicEventBatchDepth == 0)
	{
		FlushPendingPublicEvents();
	}
}

void UTcsBuffComponent::NotifyBuffRemoved(UTcsBuffInstance* BuffInstance, FName RemovalReason)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !IsValid(BuffInstance))
	{
		return;
	}

	QueueBuffRemovedEvent(BuffInstance, RemovalReason);
	// Removed 没有外层批次时也要立即提交，保持移除回调的现有时效性。
	if (PublicEventBatchDepth == 0)
	{
		FlushPendingPublicEvents();
	}
}

void UTcsBuffComponent::BindOwnerStateEvents(UTcsStateComponent* InStateComponent)
{
	if (!IsValid(InStateComponent))
	{
		return;
	}

	InStateComponent->OnInternalStateApplySuccess().AddUObject(this, &UTcsBuffComponent::HandleOwnerStateApplySuccess);
	InStateComponent->OnInternalStateRemoved().AddUObject(this, &UTcsBuffComponent::HandleOwnerStateRemoved);
	InStateComponent->OnInternalStateStageChanged().AddUObject(this, &UTcsBuffComponent::HandleOwnerStateStageChanged);
	InStateComponent->OnInternalSlotGateStateChanged().AddUObject(this, &UTcsBuffComponent::HandleOwnerSlotGateStateChanged);
	if (!OwnerStateSlotActivationHandle.IsValid())
	{
		OwnerStateSlotActivationHandle = InStateComponent->OnPrepareStateSlotActivation().AddUObject(
			this,
			&UTcsBuffComponent::HandleOwnerStateSlotActivation);
	}
	if (!OwnerStateDebugOverlayHandle.IsValid())
	{
		OwnerStateDebugOverlayHandle = InStateComponent->OnBuildStateDebugOverlay().AddUObject(
			this,
			&UTcsBuffComponent::HandleOwnerStateDebugOverlay);
	}
}

void UTcsBuffComponent::UnbindOwnerStateEvents(UTcsStateComponent* InStateComponent)
{
	if (!IsValid(InStateComponent))
	{
		return;
	}

	InStateComponent->OnInternalStateApplySuccess().RemoveAll(this);
	InStateComponent->OnInternalStateRemoved().RemoveAll(this);
	InStateComponent->OnInternalStateStageChanged().RemoveAll(this);
	InStateComponent->OnInternalSlotGateStateChanged().RemoveAll(this);
	if (OwnerStateSlotActivationHandle.IsValid())
	{
		InStateComponent->OnPrepareStateSlotActivation().Remove(OwnerStateSlotActivationHandle);
		OwnerStateSlotActivationHandle.Reset();
	}
	if (OwnerStateDebugOverlayHandle.IsValid())
	{
		InStateComponent->OnBuildStateDebugOverlay().Remove(OwnerStateDebugOverlayHandle);
		OwnerStateDebugOverlayHandle.Reset();
	}
}

void UTcsBuffComponent::HandleOwnerStateApplySuccess(
	AActor* TargetActor,
	FName /*StateDefId*/,
	UTcsStateInstance* CreatedStateInstance,
	FGameplayTag /*TargetSlot*/,
	ETcsStateStage /*AppliedStage*/)
{
	if (TargetActor != GetOwner())
	{
		return;
	}

	RegisterBuffInstance(CreatedStateInstance);
}

void UTcsBuffComponent::HandleOwnerStateRemoved(
	UTcsStateComponent* StateComponent,
	UTcsStateInstance* StateInstance,
	FName RemovalReason)
{
	if (!IsValid(StateComponent) || StateComponent != ResolveOwnerStateComponent())
	{
		return;
	}

	UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance);
	if (!BuffInstance)
	{
		return;
	}

	UnregisterBuffInstance(BuffInstance);
	NotifyBuffRemoved(BuffInstance, RemovalReason);
}

void UTcsBuffComponent::HandleOwnerStateStageChanged(
	UTcsStateComponent* StateComponent,
	UTcsStateInstance* StateInstance,
	ETcsStateStage /*PreviousStage*/,
	ETcsStateStage NewStage)
{
	if (!IsValid(StateComponent) || StateComponent != ResolveOwnerStateComponent() || NewStage == ETcsStateStage::SS_Expired)
	{
		return;
	}

	UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance);
	if (!BuffInstance)
	{
		return;
	}

	MarkBuffMergeGroupDirty(BuffInstance, ETcsBuffMergeDirtyReason::ExecutionStageChanged);
}

void UTcsBuffComponent::HandleOwnerSlotGateStateChanged(
	UTcsStateComponent* StateComponent,
	FGameplayTag SlotTag,
	bool /*bIsOpen*/)
{
	if (!IsValid(StateComponent) || StateComponent != ResolveOwnerStateComponent() || !SlotTag.IsValid())
	{
		return;
	}

	FTcsStateSlot* StateSlot = StateComponent->FindRuntimeStateSlot(SlotTag);
	if (!StateSlot)
	{
		return;
	}

	StateSlot->MarkAllBuffMergeGroupsDirty(ETcsBuffMergeDirtyReason::SlotGateChanged);
}

void UTcsBuffComponent::HandleOwnerStateSlotActivation(
	UTcsStateComponent* StateComponent,
	FGameplayTag /*SlotTag*/,
	FTcsStateSlot* StateSlot)
{
	if (!IsValid(StateComponent) || StateComponent != ResolveOwnerStateComponent())
	{
		return;
	}

	ProcessBuffMerging(StateSlot);
}

void UTcsBuffComponent::HandleOwnerStateDebugOverlay(
	UTcsStateComponent* StateComponent,
	const UTcsStateInstance* StateInstance,
	int32& OutStackCount,
	FString& OutDurationText)
{
	if (!IsValid(StateComponent) || StateComponent != ResolveOwnerStateComponent())
	{
		return;
	}

	GetDebugStateOverlay(StateInstance, OutStackCount, OutDurationText);
}

UTcsBuffInstance* UTcsBuffComponent::ResolveBuffInstance(UTcsStateInstance* StateInstance) const
{
	return Cast<UTcsBuffInstance>(StateInstance);
}

const UTcsBuffInstance* UTcsBuffComponent::ResolveBuffInstance(const UTcsStateInstance* StateInstance) const
{
	return Cast<UTcsBuffInstance>(StateInstance);
}

UTcsStateComponent* UTcsBuffComponent::ResolveOwnerStateComponent() const
{
	UTcsStateComponent* StateComponent = OwnerStateComponent.Get();
	if (IsValid(StateComponent))
	{
		return StateComponent;
	}

	if (AActor* OwnerActor = GetOwner())
	{
		StateComponent = OwnerActor->FindComponentByClass<UTcsStateComponent>();
		if (IsValid(StateComponent))
		{
			UTcsBuffComponent* MutableThis = const_cast<UTcsBuffComponent*>(this);
			MutableThis->OwnerStateComponent = StateComponent;
			MutableThis->BindOwnerStateEvents(StateComponent);
		}
	}

	return StateComponent;
}
