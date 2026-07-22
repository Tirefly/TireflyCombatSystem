// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateInstance.h"

#include "State/TcsStateComponent.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateParamInstance.h"
#include "State/StateParameter/TcsStateBoolParameter.h"
#include "State/StateParameter/TcsStateNumericParameter.h"
#include "State/StateParameter/TcsStateVectorParameter.h"
#include "TcsLogChannels.h"



void UTcsStateInstance::InitializeRuntimeParameters()
{
}


bool UTcsStateInstance::PopulateStateParamInstances(
	const UTcsStateDefinition* InStateDef,
	AActor* InInstigator,
	AActor* InTarget,
	TArray<FName>& OutFailedParams)
{
	OutFailedParams.Reset();

	if (!InStateDef)
	{
		return false;
	}

	bool bAllSuccess = true;

	for (const auto& ParamPair : InStateDef->Parameters)
	{
		const ETcsStateParameterType ParamType = ParamPair.Value.ParameterType;

		switch (ParamType)
		{
		case ETcsStateParameterType::SPT_Numeric:
			{
				FTcsNumericStateParamInstance Instance;
				Instance.BindEvaluationContext(nullptr, InInstigator);
				FString Error;
				if (!Instance.Initialize(ParamPair.Key, ParamPair.Value, Error))
				{
					UE_LOG(LogTcsState, Error, TEXT("[PopulateStateParamInstances] %s"), *Error);
					OutFailedParams.Add(ParamPair.Key.GetTagName());
					bAllSuccess = false;
					continue;
				}

				float OutValue = 0.0f;
				if (Instance.CachedEvaluator.Get()->Evaluate(InInstigator, InTarget, this, Instance.ParamData, OutValue))
				{
					Instance.NumericValue = OutValue;
					Instance.bHasEvaluated = Instance.bIsSnapshot;
				}
				else
				{
					OutFailedParams.Add(ParamPair.Key.GetTagName());
					bAllSuccess = false;
				}

				NumericParamInstances.Add(ParamPair.Key, Instance);
				break;
			}
		case ETcsStateParameterType::SPT_Bool:
			{
				FTcsBoolStateParamInstance Instance;
				Instance.BindEvaluationContext(nullptr, InInstigator);
				FString Error;
				if (!Instance.Initialize(ParamPair.Key, ParamPair.Value, Error))
				{
					UE_LOG(LogTcsState, Error, TEXT("[PopulateStateParamInstances] %s"), *Error);
					OutFailedParams.Add(ParamPair.Key.GetTagName());
					bAllSuccess = false;
					continue;
				}

				bool OutValue = false;
				if (Instance.CachedEvaluator.Get()->Evaluate(InInstigator, InTarget, this, Instance.ParamData, OutValue))
				{
					Instance.BoolValue = OutValue;
					Instance.bHasEvaluated = Instance.bIsSnapshot;
				}
				else
				{
					OutFailedParams.Add(ParamPair.Key.GetTagName());
					bAllSuccess = false;
				}

				BoolParamInstances.Add(ParamPair.Key, Instance);
				break;
			}
		case ETcsStateParameterType::SPT_Vector:
			{
				FTcsVectorStateParamInstance Instance;
				Instance.BindEvaluationContext(nullptr, InInstigator);
				FString Error;
				if (!Instance.Initialize(ParamPair.Key, ParamPair.Value, Error))
				{
					UE_LOG(LogTcsState, Error, TEXT("[PopulateStateParamInstances] %s"), *Error);
					OutFailedParams.Add(ParamPair.Key.GetTagName());
					bAllSuccess = false;
					continue;
				}

				FVector OutValue = FVector::ZeroVector;
				if (Instance.CachedEvaluator.Get()->Evaluate(InInstigator, InTarget, this, Instance.ParamData, OutValue))
				{
					Instance.VectorValue = OutValue;
					Instance.bHasEvaluated = Instance.bIsSnapshot;
				}
				else
				{
					OutFailedParams.Add(ParamPair.Key.GetTagName());
					bAllSuccess = false;
				}

				VectorParamInstances.Add(ParamPair.Key, Instance);
				break;
			}
		default:
			break;
		}
	}

	return bAllSuccess;
}


FTcsNumericStateParamInstance* UTcsStateInstance::GetNumericParamInstance(FGameplayTag Tag)
{
	return NumericParamInstances.Find(Tag);
}

FTcsBoolStateParamInstance* UTcsStateInstance::GetBoolParamInstance(FGameplayTag Tag)
{
	return BoolParamInstances.Find(Tag);
}

FTcsVectorStateParamInstance* UTcsStateInstance::GetVectorParamInstance(FGameplayTag Tag)
{
	return VectorParamInstances.Find(Tag);
}


void UTcsStateInstance::SetLevel(int32 InLevel)
{
	if (Level == InLevel)
	{
		return;
	}

	int32 OldLevel = Level;
	Level = InLevel;

	if (OwnerStateCmp.IsValid())
	{
		OwnerStateCmp->NotifyStateLevelChanged(this, OldLevel, InLevel);
	}
}


bool UTcsStateInstance::GetNumericParamByTag(FGameplayTag ParameterTag, float& OutValue) const
{
	const FTcsNumericStateParamInstance* P =
		const_cast<UTcsStateInstance*>(this)->GetNumericParamInstance(ParameterTag);
	if (!P)
	{
		return false;
	}

	OutValue = P->GetModifiedValue();
	return true;
}

bool UTcsStateInstance::GetNumericBaseParamByTag(FGameplayTag ParameterTag, float& OutValue) const
{
	const FTcsNumericStateParamInstance* P =
		const_cast<UTcsStateInstance*>(this)->GetNumericParamInstance(ParameterTag);
	if (!P)
	{
		return false;
	}

	OutValue = P->GetBaseValue();
	return true;
}

void UTcsStateInstance::SetNumericParamByTag(FGameplayTag ParameterTag, float Value)
{
	if (!ParameterTag.IsValid())
	{
		return;
	}

	if (FTcsNumericStateParamInstance* P = GetNumericParamInstance(ParameterTag))
	{
		P->NumericValue = Value;
	}
}


bool UTcsStateInstance::GetBoolParamByTag(FGameplayTag ParameterTag, bool& OutValue) const
{
	const FTcsBoolStateParamInstance* P =
		const_cast<UTcsStateInstance*>(this)->GetBoolParamInstance(ParameterTag);
	if (!P)
	{
		return false;
	}

	OutValue = P->GetModifiedValue();
	return true;
}

bool UTcsStateInstance::GetBoolBaseParamByTag(FGameplayTag ParameterTag, bool& OutValue) const
{
	const FTcsBoolStateParamInstance* P =
		const_cast<UTcsStateInstance*>(this)->GetBoolParamInstance(ParameterTag);
	if (!P)
	{
		return false;
	}

	OutValue = P->GetBaseValue();
	return true;
}

void UTcsStateInstance::SetBoolParamByTag(FGameplayTag ParameterTag, bool Value)
{
	if (!ParameterTag.IsValid())
	{
		return;
	}

	if (FTcsBoolStateParamInstance* P = GetBoolParamInstance(ParameterTag))
	{
		P->BoolValue = Value;
	}
}


bool UTcsStateInstance::GetVectorParamByTag(FGameplayTag ParameterTag, FVector& OutValue) const
{
	const FTcsVectorStateParamInstance* P =
		const_cast<UTcsStateInstance*>(this)->GetVectorParamInstance(ParameterTag);
	if (!P)
	{
		return false;
	}

	OutValue = P->GetModifiedValue();
	return true;
}

bool UTcsStateInstance::GetVectorBaseParamByTag(FGameplayTag ParameterTag, FVector& OutValue) const
{
	const FTcsVectorStateParamInstance* P =
		const_cast<UTcsStateInstance*>(this)->GetVectorParamInstance(ParameterTag);
	if (!P)
	{
		return false;
	}

	OutValue = P->GetBaseValue();
	return true;
}

void UTcsStateInstance::SetVectorParamByTag(FGameplayTag ParameterTag, const FVector& Value)
{
	if (!ParameterTag.IsValid())
	{
		return;
	}

	if (FTcsVectorStateParamInstance* P = GetVectorParamInstance(ParameterTag))
	{
		P->VectorValue = Value;
	}
}
