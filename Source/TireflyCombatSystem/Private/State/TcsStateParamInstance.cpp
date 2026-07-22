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


namespace
{
	template <typename ModifierInstanceType>
	bool ReactivateHighestInactiveModifier(TArray<ModifierInstanceType>& ModifierInstances, FName ModifierId)
	{
		ModifierInstanceType* HighestInactive = nullptr;

		for (ModifierInstanceType& Instance : ModifierInstances)
		{
			if (Instance.ModifierId != ModifierId || Instance.bActive)
			{
				continue;
			}

			if (!HighestInactive || Instance.Priority > HighestInactive->Priority)
			{
				HighestInactive = &Instance;
			}
		}

		if (!HighestInactive)
		{
			return false;
		}

		HighestInactive->bActive = true;
		return true;
	}


	template <typename ModifierInstanceType>
	bool RemoveModifierByRuntimeIdInternal(
		TArray<ModifierInstanceType>& ModifierInstances,
		int32 RuntimeModifierId,
		FName& OutModifierId,
		bool& bOutRemovedActiveInstance)
	{
		for (int32 Index = 0; Index < ModifierInstances.Num(); ++Index)
		{
			const ModifierInstanceType RemovedInstance = ModifierInstances[Index];
			if (RemovedInstance.RuntimeModifierId != RuntimeModifierId)
			{
				continue;
			}

			OutModifierId = RemovedInstance.ModifierId;
			bOutRemovedActiveInstance = RemovedInstance.bActive;
			ModifierInstances.RemoveAt(Index);
			return true;
		}

		return false;
	}
}



FTcsStateParameter::FTcsStateParameter()
{
	NormalizeStateParameterStrategyDefaults(*this);
}


void NormalizeStateParameterStrategyDefaults(FTcsStateParameter& StateParameter)
{
	switch (StateParameter.ParameterType)
	{
	case ETcsStateParameterType::SPT_Numeric:
		if (!StateParameter.NumericParamEvaluator)
		{
			StateParameter.NumericParamEvaluator = UTcsStateNumericParamEvaluator::StaticClass();
		}
		break;

	case ETcsStateParameterType::SPT_Bool:
		if (!StateParameter.BoolParamEvaluator)
		{
			StateParameter.BoolParamEvaluator = UTcsStateBoolParamEvaluator::StaticClass();
		}
		break;

	case ETcsStateParameterType::SPT_Vector:
		if (!StateParameter.VectorParamEvaluator)
		{
			StateParameter.VectorParamEvaluator = UTcsStateVectorParamEvaluator::StaticClass();
		}
		break;

	default:
		break;
	}
}


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


void FTcsNumericStateParamInstance::BindEvaluationContext(
	UTcsSkillEntry* InSkillEntry,
	AActor* InInstigator)
{
	OwningSkillEntry = InSkillEntry;
	EvaluationInstigator = InInstigator;
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


void FTcsBoolStateParamInstance::BindEvaluationContext(
	UTcsSkillEntry* InSkillEntry,
	AActor* InInstigator)
{
	OwningSkillEntry = InSkillEntry;
	EvaluationInstigator = InInstigator;
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


void FTcsVectorStateParamInstance::BindEvaluationContext(
	UTcsSkillEntry* InSkillEntry,
	AActor* InInstigator)
{
	OwningSkillEntry = InSkillEntry;
	EvaluationInstigator = InInstigator;
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


bool FTcsNumericStateParamInstance::RemoveModifierByRuntimeId(
	int32 RuntimeModifierId,
	FName& OutModifierId,
	bool& bOutRemovedActiveInstance)
{
	OutModifierId = NAME_None;
	bOutRemovedActiveInstance = false;
	return RemoveModifierByRuntimeIdInternal(ModifierInstances, RuntimeModifierId, OutModifierId, bOutRemovedActiveInstance);
}


bool FTcsNumericStateParamInstance::ReactivateHighestInactiveExclusive(FName ModifierId)
{
	return ReactivateHighestInactiveModifier(ModifierInstances, ModifierId);
}


void FTcsNumericStateParamInstance::RemoveModifiersBySourceHandle(const struct FTcsSourceHandle& SourceHandle)
{
	TArray<int32> RuntimeIdsToRemove;
	RuntimeIdsToRemove.Reserve(ModifierInstances.Num());

	for (const FStateParamNumericModifierInstance& Instance : ModifierInstances)
	{
		if (Instance.SourceHandle == SourceHandle)
		{
			RuntimeIdsToRemove.Add(Instance.RuntimeModifierId);
		}
	}

	for (const int32 RuntimeModifierId : RuntimeIdsToRemove)
	{
		FName RemovedModifierId;
		bool bRemovedActiveInstance = false;
		if (RemoveModifierByRuntimeId(RuntimeModifierId, RemovedModifierId, bRemovedActiveInstance) && bRemovedActiveInstance)
		{
			ReactivateHighestInactiveExclusive(RemovedModifierId);
		}
	}
}


float FTcsNumericStateParamInstance::GetModifiedValue() const
{
	float Value = NumericValue;
	for (const FStateParamNumericModifierInstance& Inst : ModifierInstances)
	{
		if (!Inst.bActive || !Inst.Evaluator)
		{
			continue;
		}
		Value = Inst.Evaluator->Evaluate(
			Value,
			Inst,
			OwningSkillEntry.Get(),
			EvaluationInstigator.Get());
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


bool FTcsBoolStateParamInstance::RemoveModifierByRuntimeId(
	int32 RuntimeModifierId,
	FName& OutModifierId,
	bool& bOutRemovedActiveInstance)
{
	OutModifierId = NAME_None;
	bOutRemovedActiveInstance = false;
	return RemoveModifierByRuntimeIdInternal(ModifierInstances, RuntimeModifierId, OutModifierId, bOutRemovedActiveInstance);
}


bool FTcsBoolStateParamInstance::ReactivateHighestInactiveExclusive(FName ModifierId)
{
	return ReactivateHighestInactiveModifier(ModifierInstances, ModifierId);
}


void FTcsBoolStateParamInstance::RemoveModifiersBySourceHandle(const struct FTcsSourceHandle& SourceHandle)
{
	TArray<int32> RuntimeIdsToRemove;
	RuntimeIdsToRemove.Reserve(ModifierInstances.Num());

	for (const FStateParamBoolModifierInstance& Instance : ModifierInstances)
	{
		if (Instance.SourceHandle == SourceHandle)
		{
			RuntimeIdsToRemove.Add(Instance.RuntimeModifierId);
		}
	}

	for (const int32 RuntimeModifierId : RuntimeIdsToRemove)
	{
		FName RemovedModifierId;
		bool bRemovedActiveInstance = false;
		if (RemoveModifierByRuntimeId(RuntimeModifierId, RemovedModifierId, bRemovedActiveInstance) && bRemovedActiveInstance)
		{
			ReactivateHighestInactiveExclusive(RemovedModifierId);
		}
	}
}


bool FTcsBoolStateParamInstance::GetModifiedValue() const
{
	bool Value = BoolValue;
	for (const FStateParamBoolModifierInstance& Inst : ModifierInstances)
	{
		if (!Inst.bActive || !Inst.Evaluator)
		{
			continue;
		}
		Value = Inst.Evaluator->Evaluate(
			Value,
			Inst,
			OwningSkillEntry.Get(),
			EvaluationInstigator.Get());
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


bool FTcsVectorStateParamInstance::RemoveModifierByRuntimeId(
	int32 RuntimeModifierId,
	FName& OutModifierId,
	bool& bOutRemovedActiveInstance)
{
	OutModifierId = NAME_None;
	bOutRemovedActiveInstance = false;
	return RemoveModifierByRuntimeIdInternal(ModifierInstances, RuntimeModifierId, OutModifierId, bOutRemovedActiveInstance);
}


bool FTcsVectorStateParamInstance::ReactivateHighestInactiveExclusive(FName ModifierId)
{
	return ReactivateHighestInactiveModifier(ModifierInstances, ModifierId);
}


void FTcsVectorStateParamInstance::RemoveModifiersBySourceHandle(const struct FTcsSourceHandle& SourceHandle)
{
	TArray<int32> RuntimeIdsToRemove;
	RuntimeIdsToRemove.Reserve(ModifierInstances.Num());

	for (const FStateParamVectorModifierInstance& Instance : ModifierInstances)
	{
		if (Instance.SourceHandle == SourceHandle)
		{
			RuntimeIdsToRemove.Add(Instance.RuntimeModifierId);
		}
	}

	for (const int32 RuntimeModifierId : RuntimeIdsToRemove)
	{
		FName RemovedModifierId;
		bool bRemovedActiveInstance = false;
		if (RemoveModifierByRuntimeId(RuntimeModifierId, RemovedModifierId, bRemovedActiveInstance) && bRemovedActiveInstance)
		{
			ReactivateHighestInactiveExclusive(RemovedModifierId);
		}
	}
}


FVector FTcsVectorStateParamInstance::GetModifiedValue() const
{
	FVector Value = VectorValue;
	for (const FStateParamVectorModifierInstance& Inst : ModifierInstances)
	{
		if (!Inst.bActive || !Inst.Evaluator)
		{
			continue;
		}
		Value = Inst.Evaluator->Evaluate(
			Value,
			Inst,
			OwningSkillEntry.Get(),
			EvaluationInstigator.Get());
	}
	return Value;
}
