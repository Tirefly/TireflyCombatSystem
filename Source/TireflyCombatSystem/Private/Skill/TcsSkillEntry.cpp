// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillEntry.h"

#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillInstance.h"
#include "State/StateParameter/TcsStateNumericParameter.h"
#include "TcsLogChannels.h"



UTcsSkillEntry::UTcsSkillEntry()
{
}

UWorld* UTcsSkillEntry::GetWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}


int32 UTcsSkillEntry::GetLevel() const
{
	if (!SkillDefinition)
	{
		return 1;
	}

	const FTcsNumericStateParamInstance* Inst = NumericParamInstances.Find(SkillDefinition->LevelParamTag);
	if (!Inst)
	{
		return 1;
	}

	return FMath::RoundToInt(Inst->GetModifiedValue(const_cast<UTcsSkillEntry*>(this), nullptr));
}


void UTcsSkillEntry::SetLevel(int32 InLevel)
{
	if (!SkillDefinition)
	{
		return;
	}

	FTcsNumericStateParamInstance* Inst = NumericParamInstances.Find(SkillDefinition->LevelParamTag);
	if (Inst)
	{
		Inst->NumericValue = static_cast<float>(InLevel);
	}
}


void UTcsSkillEntry::InitializeFromDef(UTcsSkillDefinition* Def)
{
	if (!Def)
	{
		UE_LOG(LogTcsState, Error, TEXT("[SkillEntry::InitializeFromDef] SkillDefinition is null"));
		return;
	}

	for (const auto& ParamPair : Def->Parameters)
	{
		switch (ParamPair.Value.ParameterType)
		{
		case ETcsStateParameterType::SPT_Numeric:
			{
				FTcsNumericStateParamInstance Instance;
				FString Error;
				if (Instance.Initialize(ParamPair.Key, ParamPair.Value, Error))
				{
					NumericParamInstances.Add(ParamPair.Key, Instance);
				}
				else
				{
					UE_LOG(LogTcsState, Error, TEXT("[SkillEntry::InitializeFromDef] %s"), *Error);
				}
				break;
			}
		case ETcsStateParameterType::SPT_Bool:
			{
				FTcsBoolStateParamInstance Instance;
				FString Error;
				if (Instance.Initialize(ParamPair.Key, ParamPair.Value, Error))
				{
					BoolParamInstances.Add(ParamPair.Key, Instance);
				}
				else
				{
					UE_LOG(LogTcsState, Error, TEXT("[SkillEntry::InitializeFromDef] %s"), *Error);
				}
				break;
			}
		case ETcsStateParameterType::SPT_Vector:
			{
				FTcsVectorStateParamInstance Instance;
				FString Error;
				if (Instance.Initialize(ParamPair.Key, ParamPair.Value, Error))
				{
					VectorParamInstances.Add(ParamPair.Key, Instance);
				}
				else
				{
					UE_LOG(LogTcsState, Error, TEXT("[SkillEntry::InitializeFromDef] %s"), *Error);
				}
				break;
			}
		default:
			break;
		}
	}
}


FTcsNumericStateParamInstance* UTcsSkillEntry::FindNumericParamInstance(FGameplayTag ParamTag)
{
	return NumericParamInstances.Find(ParamTag);
}


FTcsBoolStateParamInstance* UTcsSkillEntry::FindBoolParamInstance(FGameplayTag ParamTag)
{
	return BoolParamInstances.Find(ParamTag);
}


FTcsVectorStateParamInstance* UTcsSkillEntry::FindVectorParamInstance(FGameplayTag ParamTag)
{
	return VectorParamInstances.Find(ParamTag);
}


bool UTcsSkillEntry::StartCooldown(UTcsSkillInstance* SkillInstance)
{
	if (!SkillInstance)
	{
		return false;
	}

	FTcsNumericStateParamInstance* CI = NumericParamInstances.Find(SkillDefinition->CooldownParamTag);
	if (!CI || !CI->CachedEvaluator)
	{
		return true;
	}

	float Duration = 0.0f;
	if (!CI->CachedEvaluator.Get()->Evaluate(
		SkillInstance->GetInstigator(),
		SkillInstance->GetOwner(),
		SkillInstance,
		CI->ParamData,
		Duration))
	{
		return false;
	}

	CI->NumericValue = Duration;
	CI->bHasEvaluated = CI->bIsSnapshot;

	if (Duration > 0.0f)
	{
		RemainingCooldown = CI->GetModifiedValue(this, SkillInstance->GetInstigator());
		return true;
	}

	return false;
}


void UTcsSkillEntry::TickCooldown(float DeltaTime)
{
	if (RemainingCooldown > 0.0f)
	{
		RemainingCooldown = FMath::Max(0.0f, RemainingCooldown - DeltaTime);
	}
}


bool UTcsSkillEntry::IsOnCooldown() const
{
	return RemainingCooldown > 0.0f;
}


float UTcsSkillEntry::GetRemainingCooldownRatio() const
{
	const FTcsNumericStateParamInstance* CI = NumericParamInstances.Find(SkillDefinition->CooldownParamTag);
	if (CI && CI->GetValue() > 0.0f)
	{
		return RemainingCooldown / CI->GetValue();
	}
	return 0.0f;
}
