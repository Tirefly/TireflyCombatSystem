// Copyright Tirefly. All Rights Reserved.


#include "Buff/TcsBuffInstance.h"

#include "Buff/TcsBuffComponent.h"
#include "Buff/BuffMerger/TcsBuffMerger.h"
#include "Buff/TcsBuffDefinition.h"
#include "Misc/ScopeExit.h"
#include "State/TcsStateComponent.h"
#include "StateTree/TcsStateSchema_Buff.h"
#include "TcsLogChannels.h"



const UTcsBuffDefinition* UTcsBuffInstance::GetBuffDef() const
{
	return Cast<UTcsBuffDefinition>(StateDef);
}

ETcsBuffDurationType UTcsBuffInstance::GetDurationType() const
{
	const UTcsBuffDefinition* BuffDef = GetBuffDef();
	return BuffDef ? static_cast<ETcsBuffDurationType>(BuffDef->DurationType.GetValue()) : ETcsBuffDurationType::SDT_None;
}

bool UTcsBuffInstance::HasFiniteDuration() const
{
	return GetDurationType() == ETcsBuffDurationType::SDT_Duration;
}

bool UTcsBuffInstance::HasInfiniteDuration() const
{
	return GetDurationType() == ETcsBuffDurationType::SDT_Infinite;
}

TSubclassOf<UTcsBuffMerger> UTcsBuffInstance::GetMergerType() const
{
	const UTcsBuffDefinition* BuffDef = GetBuffDef();
	return BuffDef ? BuffDef->MergerType : nullptr;
}

UTcsBuffComponent* UTcsBuffInstance::ResolveOwnerBuffComponent() const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] OwnerActor is invalid"), *FString(__FUNCTION__));
		return nullptr;
	}

	UTcsBuffComponent* BuffComponent = UTcsBuffComponent::GetOrCreateForActor(OwnerActor);
	if (!IsValid(BuffComponent))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to resolve BuffComponent for %s"),
			*FString(__FUNCTION__),
			*OwnerActor->GetPathName());
	}

	return BuffComponent;
}

float UTcsBuffInstance::GetDurationRemaining() const
{
	const UTcsBuffDefinition* BuffDef = GetBuffDef();
	if (!BuffDef)
	{
		return 0.f;
	}

	if (BuffDef->DurationType == ETcsBuffDurationType::SDT_Infinite)
	{
		return -1.f;
	}

	if (BuffDef->DurationType == ETcsBuffDurationType::SDT_Duration)
	{
		return RemainingDuration;
	}

	return 0.f;
}

void UTcsBuffInstance::ResetRemainingDuration()
{
	UTcsBuffComponent* BuffComponent = ResolveOwnerBuffComponent();
	if (!IsValid(BuffComponent))
	{
		return;
	}

	BuffComponent->RefreshBuffRemainingDuration(this);
}

void UTcsBuffInstance::SetDurationRemaining(float InDurationRemaining)
{
	UTcsBuffComponent* BuffComponent = ResolveOwnerBuffComponent();
	if (!IsValid(BuffComponent))
	{
		return;
	}

	BuffComponent->SetBuffRemainingDuration(this, InDurationRemaining);
}

float UTcsBuffInstance::GetTotalDuration() const
{
	const UTcsBuffDefinition* BuffDef = GetBuffDef();
	if (!BuffDef)
	{
		return 0.0f;
	}

	if (BuffDef->DurationType == ETcsBuffDurationType::SDT_Infinite)
	{
		return -1.f;
	}

	if (BuffDef->DurationType == ETcsBuffDurationType::SDT_Duration)
	{
		return TotalDuration;
	}

	return 0.0f;
}

void UTcsBuffInstance::SetTotalDuration(float InTotalDuration)
{
	if (!HasFiniteDuration())
	{
		return;
	}

	TotalDuration = FMath::Max(0.f, InTotalDuration);
}

bool UTcsBuffInstance::HasPeriod() const
{
	return Period > 0.f;
}

float UTcsBuffInstance::GetPeriod() const
{
	return Period;
}

void UTcsBuffInstance::SetPeriod(float InPeriod)
{
	const float NewPeriod = FMath::Max(0.f, InPeriod);
	if (Period == NewPeriod)
	{
		return;
	}

	const float OldPeriod = Period;
	Period = NewPeriod;

	if (UTcsBuffComponent* BuffComponent = ResolveOwnerBuffComponent())
	{
		BuffComponent->BeginPublicEventBatch();
		ON_SCOPE_EXIT
		{
			BuffComponent->EndPublicEventBatch();
		};

		BuffComponent->NotifyBuffPeriodChanged(this, OldPeriod, Period);
	}
}

bool UTcsBuffInstance::CanStack() const
{
	const int32 CurrentMaxStackCount = GetMaxStackCount();
	if (CurrentMaxStackCount <= 0)
	{
		return false;
	}

	return GetStackCount() < CurrentMaxStackCount;
}

int32 UTcsBuffInstance::GetStackCount() const
{
	return StackCount;
}

int32 UTcsBuffInstance::GetMaxStackCount() const
{
	return MaxStackCount;
}

void UTcsBuffInstance::SetMaxStackCount(int32 InMaxStackCount)
{
	const int32 NewMaxStackCount = FMath::Max(1, InMaxStackCount);
	if (MaxStackCount == NewMaxStackCount)
	{
		return;
	}

	const int32 OldMaxStackCount = MaxStackCount;
	MaxStackCount = NewMaxStackCount;

	if (UTcsBuffComponent* BuffComponent = ResolveOwnerBuffComponent())
	{
		BuffComponent->BeginPublicEventBatch();
		ON_SCOPE_EXIT
		{
			BuffComponent->EndPublicEventBatch();
		};

		const int32 CurrentStackCount = GetStackCount();
		if (CurrentStackCount > MaxStackCount)
		{
			SetStackCount(MaxStackCount);
		}

		BuffComponent->NotifyBuffMaxStackCountChanged(this, OldMaxStackCount, MaxStackCount);
		return;
	}

	const int32 CurrentStackCount = GetStackCount();
	if (CurrentStackCount > MaxStackCount)
	{
		SetStackCount(MaxStackCount);
	}
}

void UTcsBuffInstance::ResetMaxStackCount()
{
	const UTcsBuffDefinition* BuffDef = GetBuffDef();
	if (!BuffDef)
	{
		return;
	}

	SetMaxStackCount(BuffDef->MaxStackCount);
}

void UTcsBuffInstance::SetStackCount(int32 InStackCount)
{
	const int32 CurrentMaxStackCount = GetMaxStackCount();
	if (CurrentMaxStackCount <= 0)
	{
		return;
	}

	const int32 OldStackCount = GetStackCount();
	const int32 NewStackCount = FMath::Clamp(InStackCount, 0, CurrentMaxStackCount);
	if (OldStackCount == NewStackCount)
	{
		return;
	}

	if (NewStackCount == 0)
	{
		if (UTcsBuffComponent* BuffComponent = ResolveOwnerBuffComponent())
		{
			BuffComponent->BeginPublicEventBatch();
			ON_SCOPE_EXIT
			{
				BuffComponent->EndPublicEventBatch();
			};

			BuffComponent->RemoveBuffInstance(this, TcsBuffRemovalReasons::StackDepleted);
		}
		return;
	}

	StackCount = NewStackCount;
	if (UTcsBuffComponent* BuffComponent = ResolveOwnerBuffComponent())
	{
		BuffComponent->BeginPublicEventBatch();
		ON_SCOPE_EXIT
		{
			BuffComponent->EndPublicEventBatch();
		};

		BuffComponent->HandleBuffStackCountChangedInternal(this, OldStackCount, NewStackCount);
		BuffComponent->NotifyBuffStackChanged(this, OldStackCount, NewStackCount);
	}
}

void UTcsBuffInstance::AddStack(int32 Count)
{
	SetStackCount(GetStackCount() + Count);
}

void UTcsBuffInstance::RemoveStack(int32 Count)
{
	SetStackCount(GetStackCount() - Count);
}

bool UTcsBuffInstance::SetContextRequirements(FStateTreeExecutionContext& Context)
{
	if (!Context.IsValid())
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("Invalid BuffStateTree execution context"));
		return false;
	}

	Context.SetCollectExternalDataCallback(
		FOnCollectStateTreeExternalData::CreateUObject(
			this,
			&UTcsBuffInstance::CollectExternalData
		)
	);

	return UTcsStateSchema_Buff::SetContextRequirements(*this, Context);
}

bool UTcsBuffInstance::CollectExternalData(
	const FStateTreeExecutionContext& Context,
	const UStateTree* StateTree,
	TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs,
	TArrayView<FStateTreeDataView> OutDataViews)
{
	return UTcsStateSchema_Buff::CollectExternalData(
		Context,
		StateTree,
		this,
		ExternalDataDescs,
		OutDataViews);
}

void UTcsBuffInstance::InitializeRuntimeParameters()
{
	TotalDuration = 0.f;
	RemainingDuration = 0.f;
	Period = 0.f;
	MaxStackCount = 1;
	StackCount = -1;

	const UTcsBuffDefinition* BuffDef = GetBuffDef();
	if (!BuffDef)
	{
		return;
	}

	// 在共享 State 主流程把实例放入槽位之前，确保 Buff 模块已经接管宿主组件与事件绑定。
	ResolveOwnerBuffComponent();

	switch (BuffDef->DurationType)
	{
	default:
	case ETcsBuffDurationType::SDT_None:
		TotalDuration = 0.f;
		RemainingDuration = 0.f;
		break;
	case ETcsBuffDurationType::SDT_Infinite:
		TotalDuration = -1.f;
		RemainingDuration = -1.f;
		break;
	case ETcsBuffDurationType::SDT_Duration:
		TotalDuration = BuffDef->Duration;
		RemainingDuration = TotalDuration;
		break;
	}

	Period = FMath::Max(0.f, BuffDef->Period);
	MaxStackCount = FMath::Max(1, BuffDef->MaxStackCount);

	if (MaxStackCount > 0)
	{
		StackCount = 1;
	}
}