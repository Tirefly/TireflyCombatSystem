// Copyright Tirefly. All Rights Reserved.

#include "Buff/TcsBuffComponent.h"

#include "TcsDefinitionManagerSubsystem.h"
#include "GameFramework/Actor.h"
#include "Buff/TcsBuffDefinition.h"
#include "Buff/TcsBuffInstance.h"
#include "Misc/ScopeExit.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateManagerSubsystem.h"
#include "State/TcsStateInstance.h"
#include "TcsLogChannels.h"


namespace
{
	bool LogBuffRuntimeNotReady_Lifecycle(const UTcsBuffComponent* Component, const TCHAR* FunctionName)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Buff runtime is not ready for %s"), FunctionName, *GetPathNameSafe(Component));
		return false;
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

	if (!IsRuntimeReady())
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidInput,
			TEXT("Buff runtime is not ready yet."));
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

	UTcsDefinitionManagerSubsystem* DefinitionManager = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UTcsDefinitionManagerSubsystem>()
		: nullptr;
	if (!DefinitionManager)
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidInput,
			TEXT("Failed to resolve DefinitionManagerSubsystem."));
	}

	const UTcsBuffDefinition* BuffDef = DefinitionManager->GetBuffDefinition(BuffDefId);
	if (!BuffDef)
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidStateDefinition,
			TEXT("Invalid buff definition."));
	}

	return StateComponent->TryApplyState(BuffDefId, Instigator, BuffLevel, ParentSourceHandle);
}

bool UTcsBuffComponent::RemoveBuff(UTcsBuffInstance* BuffInstance, FName RemovalReason)
{
	if (!IsRuntimeReady())
	{
		return LogBuffRuntimeNotReady_Lifecycle(this, TEXT(__FUNCTION__));
	}

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

	// SDT_Duration   – 加入 DurationTracker，按剩余时长递减至 0 后过期。
	// SDT_None       – 加入 DurationTracker（RemainingDuration 已初始化为 0），
	//                  下一个 Tick 立即被检测为过期，确保 StateTree 至少执行一次。
	// SDT_Infinite   – 不加入 DurationTracker，永不自动过期。
	const ETcsBuffDurationType DurationType = BuffInstance->GetDurationType();
	if (DurationType == ETcsBuffDurationType::SDT_Duration || DurationType == ETcsBuffDurationType::SDT_None)
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

	for (const TObjectPtr<UTcsBuffInstance>& TrackedBuffInstance : DurationTracker.GetTrackedInstances())
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
