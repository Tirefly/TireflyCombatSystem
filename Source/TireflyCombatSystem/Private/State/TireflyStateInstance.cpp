// Copyright Tirefly. All Rights Reserved.

#include "State/TireflyStateInstance.h"

void UTireflyStateInstance::Initialize(const FTireflyStateDefinition& InStateDef, AActor* InOwner, int32 InLevel)
{
	StateDef = InStateDef;
	Owner = InOwner;
	Level = InLevel;
	InstanceId = FMath::RandRange(0, INT32_MAX);
	ApplyTimestamp = FDateTime::UtcNow().ToUnixTimestamp();
	UpdateTimestamp = ApplyTimestamp;
	Stage = ETireflyStateStage::Active;
	StackCount = 1;

	// 初始化持续时间
	if (StateDef.DurationType == ETireflyStateDurationType::SDT_Duration)
	{
		DurationRemaining = StateDef.Duration;
	}
	else if (StateDef.DurationType == ETireflyStateDurationType::SDT_Infinite)
	{
		DurationRemaining = -1.0f;
	}
	else
	{
		DurationRemaining = 0.0f;
	}
}

void UTireflyStateInstance::Update(float DeltaTime)
{
	if (Stage != ETireflyStateStage::Active)
	{
		return;
	}

	UpdateTimestamp = FDateTime::UtcNow().ToUnixTimestamp();

	// 更新持续时间
	if (StateDef.DurationType == ETireflyStateDurationType::SDT_Duration && DurationRemaining > 0.0f)
	{
		DurationRemaining -= DeltaTime;
		if (DurationRemaining <= 0.0f)
		{
			Stage = ETireflyStateStage::Expired;
		}
	}
}

bool UTireflyStateInstance::IsExpired() const
{
	return Stage == ETireflyStateStage::Expired;
}

bool UTireflyStateInstance::CanStack() const
{
	return StackCount < StateDef.MaxStackCount;
}

float UTireflyStateInstance::GetRemainingTime() const
{
	return DurationRemaining;
}

float UTireflyStateInstance::GetTotalDuration() const
{
	return StateDef.Duration;
}

void UTireflyStateInstance::SetStackCount(int32 InStackCount)
{
	StackCount = FMath::Clamp(InStackCount, 1, StateDef.MaxStackCount);
}

void UTireflyStateInstance::AddStack(int32 Count)
{
	SetStackCount(StackCount + Count);
}

void UTireflyStateInstance::RemoveStack(int32 Count)
{
	SetStackCount(StackCount - Count);
}

float UTireflyStateInstance::GetRuntimeParameter(FName ParameterName) const
{
	if (const float* Value = RuntimeParameters.Find(ParameterName))
	{
		return *Value;
	}
	return 0.0f;
}

void UTireflyStateInstance::SetRuntimeParameter(FName ParameterName, float Value)
{
	RuntimeParameters.Add(ParameterName, Value);
}

bool UTireflyStateInstance::HasRequiredState(FName StateName) const
{
	return RequiredStates.Contains(StateName);
}

bool UTireflyStateInstance::HasImmunityState(FName StateName) const
{
	return ImmunityStates.Contains(StateName);
}

void UTireflyStateInstance::AddRequiredState(FName StateName)
{
	RequiredStates.Add(StateName);
}

void UTireflyStateInstance::AddImmunityState(FName StateName)
{
	ImmunityStates.Add(StateName);
}

void UTireflyStateInstance::RemoveRequiredState(FName StateName)
{
	RequiredStates.Remove(StateName);
}

void UTireflyStateInstance::RemoveImmunityState(FName StateName)
{
	ImmunityStates.Remove(StateName);
} 