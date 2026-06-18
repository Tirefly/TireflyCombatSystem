// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateParamInstance.h"

#include "Skill/SkillModExecution/TcsSkillModifierExecution.h"
#include "Skill/TcsSkillModifierInstance.h"
#include "Skill/TcsSkillEntry.h"
#include "State/TcsStateInstance.h"
#include "State/StateParameter/TcsStateBoolParameter.h"
#include "State/StateParameter/TcsStateNumericParameter.h"
#include "State/StateParameter/TcsStateVectorParameter.h"
#include "TcsLogChannels.h"



bool FTcsNumericStateParamInstance::Initialize(const FGameplayTag& InTag, const FTcsStateParameter& ParamDef, FString& OutError)
{
	ParamTag     = InTag;
	bIsSnapshot  = ParamDef.bIsSnapshot;
	ParamData    = ParamDef.ParamValueContainer;

	if (!ParamDef.NumericParamEvaluator)
	{
		OutError = FString::Printf(TEXT("NumericParamEvaluator is null for StateParam '%s'"), *InTag.ToString());
		return false;
	}
	NumericEvaluatorClass = ParamDef.NumericParamEvaluator;
	CachedEvaluator = NumericEvaluatorClass->GetDefaultObject<UTcsStateNumericParamEvaluator>();
	if (!CachedEvaluator)
	{
		OutError = FString::Printf(TEXT("Failed to get CDO for StateParam '%s'"), *InTag.ToString());
		return false;
	}

	return true;
}


bool FTcsBoolStateParamInstance::Initialize(const FGameplayTag& InTag, const FTcsStateParameter& ParamDef, FString& OutError)
{
	ParamTag     = InTag;
	bIsSnapshot  = ParamDef.bIsSnapshot;
	ParamData    = ParamDef.ParamValueContainer;

	if (!ParamDef.BoolParamEvaluator)
	{
		OutError = FString::Printf(TEXT("BoolParamEvaluator is null for StateParam '%s'"), *InTag.ToString());
		return false;
	}
	BoolEvaluatorClass = ParamDef.BoolParamEvaluator;
	CachedEvaluator = BoolEvaluatorClass->GetDefaultObject<UTcsStateBoolParamEvaluator>();
	if (!CachedEvaluator)
	{
		OutError = FString::Printf(TEXT("Failed to get CDO for StateParam '%s'"), *InTag.ToString());
		return false;
	}

	return true;
}


bool FTcsVectorStateParamInstance::Initialize(const FGameplayTag& InTag, const FTcsStateParameter& ParamDef, FString& OutError)
{
	ParamTag     = InTag;
	bIsSnapshot  = ParamDef.bIsSnapshot;
	ParamData    = ParamDef.ParamValueContainer;

	if (!ParamDef.VectorParamEvaluator)
	{
		OutError = FString::Printf(TEXT("VectorParamEvaluator is null for StateParam '%s'"), *InTag.ToString());
		return false;
	}
	VectorEvaluatorClass = ParamDef.VectorParamEvaluator;
	CachedEvaluator = VectorEvaluatorClass->GetDefaultObject<UTcsStateVectorParamEvaluator>();
	if (!CachedEvaluator)
	{
		OutError = FString::Printf(TEXT("Failed to get CDO for StateParam '%s'"), *InTag.ToString());
		return false;
	}

	return true;
}


void FTcsNumericStateParamInstance::AssignModifier(const FStateParamNumericModifierInstance& Instance)
{
	if (Instance.MergePolicy == ETcsSkillModifierMergePolicy::Exclusive)
	{
		for (FStateParamNumericModifierInstance& Existing : ModifierInstances)
		{
			if (Existing.ModifierId == Instance.ModifierId && Existing.bActive)
			{
				if (Existing.Priority < Instance.Priority)
				{
					Existing.bActive = false;
				}
				else
				{
					FStateParamNumericModifierInstance InactiveCopy = Instance;
					InactiveCopy.bActive = false;
					ModifierInstances.Add(InactiveCopy);
					return;
				}
			}
		}
	}

	ModifierInstances.Add(Instance);
	ModifierInstances.Sort([](const FStateParamNumericModifierInstance& A, const FStateParamNumericModifierInstance& B)
	{
		return A.Priority > B.Priority;
	});
}


void FTcsNumericStateParamInstance::RemoveModifiersBySourceHandle(const struct FTcsSourceHandle& SourceHandle)
{
	TArray<FName> RemovedIds;

	ModifierInstances.RemoveAll([&](const FStateParamNumericModifierInstance& Inst)
	{
		if (Inst.SourceHandle == SourceHandle)
		{
			RemovedIds.AddUnique(Inst.ModifierId);
			return true;
		}
		return false;
	});

	for (const FName& Id : RemovedIds)
	{
		FStateParamNumericModifierInstance* HighestInactive = nullptr;
		for (FStateParamNumericModifierInstance& Inst : ModifierInstances)
		{
			if (Inst.ModifierId == Id && !Inst.bActive)
			{
				if (!HighestInactive || Inst.Priority > HighestInactive->Priority)
				{
					HighestInactive = &Inst;
				}
			}
		}
		if (HighestInactive)
		{
			HighestInactive->bActive = true;
		}
	}
}


float FTcsNumericStateParamInstance::GetModifiedValue(UTcsSkillEntry* SkillEntry, AActor* Instigator) const
{
	float Value = NumericValue;
	for (const FStateParamNumericModifierInstance& Inst : ModifierInstances)
	{
		if (!Inst.bActive || !Inst.Evaluator)
		{
			continue;
		}
		Value = Inst.Evaluator->Evaluate(Value, Inst, SkillEntry, Instigator);
	}
	return Value;
}


void FTcsBoolStateParamInstance::AssignModifier(const FStateParamBoolModifierInstance& Instance)
{
	if (Instance.MergePolicy == ETcsSkillModifierMergePolicy::Exclusive)
	{
		for (FStateParamBoolModifierInstance& Existing : ModifierInstances)
		{
			if (Existing.ModifierId == Instance.ModifierId && Existing.bActive)
			{
				if (Existing.Priority < Instance.Priority)
				{
					Existing.bActive = false;
				}
				else
				{
					FStateParamBoolModifierInstance InactiveCopy = Instance;
					InactiveCopy.bActive = false;
					ModifierInstances.Add(InactiveCopy);
					return;
				}
			}
		}
	}

	ModifierInstances.Add(Instance);
	ModifierInstances.Sort([](const FStateParamBoolModifierInstance& A, const FStateParamBoolModifierInstance& B)
	{
		return A.Priority > B.Priority;
	});
}


void FTcsBoolStateParamInstance::RemoveModifiersBySourceHandle(const struct FTcsSourceHandle& SourceHandle)
{
	TArray<FName> RemovedIds;

	ModifierInstances.RemoveAll([&](const FStateParamBoolModifierInstance& Inst)
	{
		if (Inst.SourceHandle == SourceHandle)
		{
			RemovedIds.AddUnique(Inst.ModifierId);
			return true;
		}
		return false;
	});

	for (const FName& Id : RemovedIds)
	{
		FStateParamBoolModifierInstance* HighestInactive = nullptr;
		for (FStateParamBoolModifierInstance& Inst : ModifierInstances)
		{
			if (Inst.ModifierId == Id && !Inst.bActive)
			{
				if (!HighestInactive || Inst.Priority > HighestInactive->Priority)
				{
					HighestInactive = &Inst;
				}
			}
		}
		if (HighestInactive)
		{
			HighestInactive->bActive = true;
		}
	}
}


bool FTcsBoolStateParamInstance::GetModifiedValue(UTcsSkillEntry* SkillEntry, AActor* Instigator) const
{
	bool Value = BoolValue;
	for (const FStateParamBoolModifierInstance& Inst : ModifierInstances)
	{
		if (!Inst.bActive || !Inst.Evaluator)
		{
			continue;
		}
		Value = Inst.Evaluator->Evaluate(Value, Inst, SkillEntry, Instigator);
	}
	return Value;
}


void FTcsVectorStateParamInstance::AssignModifier(const FStateParamVectorModifierInstance& Instance)
{
	if (Instance.MergePolicy == ETcsSkillModifierMergePolicy::Exclusive)
	{
		for (FStateParamVectorModifierInstance& Existing : ModifierInstances)
		{
			if (Existing.ModifierId == Instance.ModifierId && Existing.bActive)
			{
				if (Existing.Priority < Instance.Priority)
				{
					Existing.bActive = false;
				}
				else
				{
					FStateParamVectorModifierInstance InactiveCopy = Instance;
					InactiveCopy.bActive = false;
					ModifierInstances.Add(InactiveCopy);
					return;
				}
			}
		}
	}

	ModifierInstances.Add(Instance);
	ModifierInstances.Sort([](const FStateParamVectorModifierInstance& A, const FStateParamVectorModifierInstance& B)
	{
		return A.Priority > B.Priority;
	});
}


void FTcsVectorStateParamInstance::RemoveModifiersBySourceHandle(const struct FTcsSourceHandle& SourceHandle)
{
	TArray<FName> RemovedIds;

	ModifierInstances.RemoveAll([&](const FStateParamVectorModifierInstance& Inst)
	{
		if (Inst.SourceHandle == SourceHandle)
		{
			RemovedIds.AddUnique(Inst.ModifierId);
			return true;
		}
		return false;
	});

	for (const FName& Id : RemovedIds)
	{
		FStateParamVectorModifierInstance* HighestInactive = nullptr;
		for (FStateParamVectorModifierInstance& Inst : ModifierInstances)
		{
			if (Inst.ModifierId == Id && !Inst.bActive)
			{
				if (!HighestInactive || Inst.Priority > HighestInactive->Priority)
				{
					HighestInactive = &Inst;
				}
			}
		}
		if (HighestInactive)
		{
			HighestInactive->bActive = true;
		}
	}
}


FVector FTcsVectorStateParamInstance::GetModifiedValue(UTcsSkillEntry* SkillEntry, AActor* Instigator) const
{
	FVector Value = VectorValue;
	for (const FStateParamVectorModifierInstance& Inst : ModifierInstances)
	{
		if (!Inst.bActive || !Inst.Evaluator)
		{
			continue;
		}
		Value = Inst.Evaluator->Evaluate(Value, Inst, SkillEntry, Instigator);
	}
	return Value;
}
