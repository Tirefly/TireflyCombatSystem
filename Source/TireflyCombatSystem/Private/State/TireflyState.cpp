// Copyright Tirefly. All Rights Reserved.


#include "State/TireflyState.h"



void UTireflyStateInstance::Initialize(
	const FTireflyStateDefinition& InStateDef,
	AActor* InOwner,
	int32 InInstanceId,
	int32 InLevel)
{
	StateDef = InStateDef;
	StateInstanceId = InInstanceId;
	Level = InLevel;
	
	Owner = InOwner;

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
	switch (Stage)
	{
	case ETireflyStateStage::Inactive:
		{
			// 未激活状态
			break;
		}
	case ETireflyStateStage::Active:
		{
			DurationRemaining -= DeltaTime;
			if (DurationRemaining <= 0.0f)
			{
				Stage = ETireflyStateStage::Expired;
			}
			break;
		}
	case ETireflyStateStage::HangUp:
		{
			// 挂起状态
			break;
		}
	case ETireflyStateStage::Expired:
		{
			// 过期状态
			break;
		}
	}
}

void UTireflyStateInstance::SetCurrentStage(ETireflyStateStage InStage)
{
	Stage = InStage;
}

bool UTireflyStateInstance::CanStack() const
{
	return StackCount < StateDef.MaxStackCount;
}

float UTireflyStateInstance::GetRemainingTime() const
{
	return DurationRemaining;
}

void UTireflyStateInstance::SetRemainingTime(float InRemainingTime)
{
	DurationRemaining = InRemainingTime;
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
	Parameters.Add(ParameterName, Value);
}
