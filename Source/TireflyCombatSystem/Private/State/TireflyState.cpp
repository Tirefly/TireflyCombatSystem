// Copyright Tirefly. All Rights Reserved.


#include "State/TireflyState.h"

#include "State/TireflyStateComponent.h"


UTireflyStateInstance::UTireflyStateInstance()
{
}

UWorld* UTireflyStateInstance::GetWorld() const
{
	if (UWorldSubsystem* WorldSubsystem = Cast<UWorldSubsystem>(GetOuter()))
	{
		return WorldSubsystem->GetWorld();
	}
	
	return nullptr;
}

void UTireflyStateInstance::Initialize(
	const FTireflyStateDefinition& InStateDef,
	AActor* InOwner,
	AActor* InInstigator,
	int32 InInstanceId,
	int32 InLevel)
{
	StateDef = InStateDef;
	StateInstanceId = InInstanceId;
	Level = InLevel;
	
	Owner = InOwner;
	Instigator = InInstigator;

	Parameters.Add("MaxStackCount", StateDef.MaxStackCount);
	Parameters.Add("TotalDuration", StateDef.Duration);
}

void UTireflyStateInstance::SetCurrentStage(ETireflyStateStage InStage)
{
	Stage = InStage;
}

bool UTireflyStateInstance::CanStack() const
{
	int32 MaxStackCount = GetMaxStackCount();
	if (MaxStackCount <= 0)
	{
		return false;
	}
	
	return StackCount < MaxStackCount;
}

float UTireflyStateInstance::GetDurationRemaining() const
{
	// 从TireflyStateComponent获取剩余时间
	if (AActor* OwnerActor = Owner.Get())
	{
		if (UTireflyStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTireflyStateComponent>())
		{
			return StateComponent->GetStateInstanceDurationRemaining(const_cast<UTireflyStateInstance*>(this));
		}
	}
	
	return 0.0f;
}

void UTireflyStateInstance::RefreshDurationRemaining()
{
	// 通知TireflyStateComponent刷新剩余时间
	if (AActor* OwnerActor = Owner.Get())
	{
		if (UTireflyStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTireflyStateComponent>())
		{
			StateComponent->RefreshStateInstanceDurationRemaining(this);
		}
	}
}

void UTireflyStateInstance::SetDurationRemaining(float InDurationRemaining)
{
	// 通知TireflyStateComponent设置剩余时间
	if (AActor* OwnerActor = Owner.Get())
	{
		if (UTireflyStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTireflyStateComponent>())
		{
			StateComponent->SetStateInstanceDurationRemaining(this, InDurationRemaining);
		}
	}
}

float UTireflyStateInstance::GetTotalDuration() const
{
	switch (StateDef.DurationType)
	{
	default:
	case SDT_None:
		return 0.0f;
	case SDT_Infinite:
		return -1.f;
	case SDT_Duration:
		break;
	}
	
	float TotalDuration = StateDef.Duration;
	if (const float* DurationParam = Parameters.Find("TotalDuration"))
	{
		TotalDuration = FMath::Max(TotalDuration, *DurationParam);
	}
	
	return TotalDuration;
}

int32 UTireflyStateInstance::GetMaxStackCount() const
{
	int32 MaxStackCount = StateDef.MaxStackCount;
	if (const float* MaxStackParam = Parameters.Find("MaxStackCount"))
	{
		MaxStackCount = static_cast<int32>(*MaxStackParam);
	}

	return MaxStackCount;
}

void UTireflyStateInstance::SetStackCount(int32 InStackCount)
{
	int32 MaxStackCount = GetMaxStackCount();
	if (MaxStackCount <= 0)
	{
		StackCount = 0;
		return;
	}
	
	StackCount = FMath::Clamp(InStackCount, 1, MaxStackCount);
}

void UTireflyStateInstance::AddStack(int32 Count)
{
	SetStackCount(StackCount + Count);
}

void UTireflyStateInstance::RemoveStack(int32 Count)
{
	SetStackCount(StackCount - Count);
}

void UTireflyStateInstance::SetLevel(int32 InLevel)
{
	Level = InLevel;
}

float UTireflyStateInstance::GetParamValue(FName ParameterName) const
{
	if (const float* Value = Parameters.Find(ParameterName))
	{
		return *Value;
	}
	return 0.0f;
}

void UTireflyStateInstance::SetParamValue(FName ParameterName, float Value)
{
	Parameters.FindOrAdd(ParameterName) = Value;
}
