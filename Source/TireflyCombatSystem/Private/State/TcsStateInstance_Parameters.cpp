// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateInstance.h"

#include "State/TcsStateComponent.h"
#include "State/StateParameter/TcsStateBoolParameter.h"
#include "State/StateParameter/TcsStateNumericParameter.h"
#include "State/StateParameter/TcsStateVectorParameter.h"
#include "TcsLogChannels.h"



void UTcsStateInstance::InitializeRuntimeParameters()
{
	// Base state instances have no specialized runtime parameters.
}

void UTcsStateInstance::SetLevel(int32 InLevel)
{
	if (Level == InLevel)
	{
		return;
	}

	int32 OldLevel = Level;
	Level = InLevel;

	// 通知状态组件等级变化
	if (OwnerStateCmp.IsValid())
	{
		OwnerStateCmp->NotifyStateLevelChanged(this, OldLevel, InLevel);
	}
}

void UTcsStateInstance::InitParameterValues()
{
	if (!StateDef || StateDef->Parameters.IsEmpty())
	{
		return;
	}

	for (const TPair<FName, FTcsStateParameter>& ParamPair : StateDef->Parameters)
	{
		switch (ParamPair.Value.ParameterType)
		{
		case ETcsStateParameterType::SPT_Numeric:
			{
				if (!ParamPair.Value.NumericParamEvaluator)
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] NumericParamEvaluator of state %s is invalid"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString());
					break;
				}

				float ParamValue;
				auto ParamEvaluator = ParamPair.Value.NumericParamEvaluator->GetDefaultObject<UTcsStateNumericParamEvaluator>();
				if (!ParamEvaluator->Evaluate(
					Instigator.Get(),
					Owner.Get(),
					this,
					ParamPair.Value.ParamValueContainer,
					ParamValue))
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to evaluate state %s 's numeric parameter: %s"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString(),
						*ParamPair.Key.ToString());
					break;
				}
				
				SetNumericParam(ParamPair.Key, ParamValue);
				break;
			}
		case ETcsStateParameterType::SPT_Bool:
			{
				if (!ParamPair.Value.BoolParamEvaluator)
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] BoolParamEvaluator of state %s is invalid"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString());
					break;
				}

				bool ParamValue;
				auto ParamEvaluator = ParamPair.Value.BoolParamEvaluator->GetDefaultObject<UTcsStateBoolParamEvaluator>();
				if (!ParamEvaluator->Evaluate(
					Instigator.Get(),
					Owner.Get(),
					this,
					ParamPair.Value.ParamValueContainer,
					ParamValue))
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to evaluate state %s 's  bool parameter: %s"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString(),
						*ParamPair.Key.ToString());
					break;
				}

				SetBoolParam(ParamPair.Key, ParamValue);
				break;
			}
		case ETcsStateParameterType::SPT_Vector:
			{
				if (!ParamPair.Value.VectorParamEvaluator)
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] VectorParamEvaluator of state %s is invalid"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString());
					break;
				}

				FVector ParamValue;
				auto ParamEvaluator = ParamPair.Value.VectorParamEvaluator->GetDefaultObject<UTcsStateVectorParamEvaluator>();
				if (!ParamEvaluator->Evaluate(
					Instigator.Get(),
					Owner.Get(),
					this,
					ParamPair.Value.ParamValueContainer,
					ParamValue))
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to evaluate state %s 's vector parameter: %s"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString(),
						*ParamPair.Key.ToString());
					break;
				}

				SetVectorParam(ParamPair.Key, ParamValue);
				break;
			}
		}
	}
}

void UTcsStateInstance::InitParameterTagValues()
{
	if (!StateDef || StateDef->TagParameters.IsEmpty())
	{
		return;
	}

	for (const TPair<FGameplayTag, FTcsStateParameter>& ParamPair : StateDef->TagParameters)
	{
		switch (ParamPair.Value.ParameterType)
		{
		case ETcsStateParameterType::SPT_Numeric:
			{
				if (!ParamPair.Value.NumericParamEvaluator)
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] NumericParamEvaluator of state %s is invalid"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString());
					break;
				}

				float ParamValue;
				auto ParamEvaluator = ParamPair.Value.NumericParamEvaluator->GetDefaultObject<UTcsStateNumericParamEvaluator>();
				if (!ParamEvaluator->Evaluate(
					Instigator.Get(),
					Owner.Get(),
					this,
					ParamPair.Value.ParamValueContainer,
					ParamValue))
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to evaluate state %s 's numeric parameter: %s"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString(),
						*ParamPair.Key.ToString());
					break;
				}
				
				SetNumericParamByTag(ParamPair.Key, ParamValue);
				break;
			}
		case ETcsStateParameterType::SPT_Bool:
			{
				if (!ParamPair.Value.BoolParamEvaluator)
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] BoolParamEvaluator of state %s is invalid"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString());
					break;
				}

				bool ParamValue;
				auto ParamEvaluator = ParamPair.Value.BoolParamEvaluator->GetDefaultObject<UTcsStateBoolParamEvaluator>();
				if (!ParamEvaluator->Evaluate(
					Instigator.Get(),
					Owner.Get(),
					this,
					ParamPair.Value.ParamValueContainer,
					ParamValue))
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to evaluate state %s 's  bool parameter: %s"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString(),
						*ParamPair.Key.ToString());
					break;
				}

				SetBoolParamByTag(ParamPair.Key, ParamValue);
				break;
			}
		case ETcsStateParameterType::SPT_Vector:
			{
				if (!ParamPair.Value.VectorParamEvaluator)
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] VectorParamEvaluator of state %s is invalid"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString());
					break;
				}

				FVector ParamValue;
				auto ParamEvaluator = ParamPair.Value.VectorParamEvaluator->GetDefaultObject<UTcsStateVectorParamEvaluator>();
				if (!ParamEvaluator->Evaluate(
					Instigator.Get(),
					Owner.Get(),
					this,
					ParamPair.Value.ParamValueContainer,
					ParamValue))
				{
					UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to evaluate state %s 's vector parameter: %s"),
						*FString(__FUNCTION__),
						*GetStateDefId().ToString(),
						*ParamPair.Key.ToString());
					break;
				}

				SetVectorParamByTag(ParamPair.Key, ParamValue);
				break;
			}
		}
	}
}

bool UTcsStateInstance::GetNumericParam(FName ParameterName, float& OutValue) const
{
	if (const float* Value = NumericParameters.Find(ParameterName))
	{
		OutValue = *Value;
		return true;
	}
	return false;
}

void UTcsStateInstance::SetNumericParam(FName ParameterName, float Value)
{
	float* ExistingValue = NumericParameters.Find(ParameterName);
	bool bIsNewValue = (ExistingValue == nullptr);
	bool bValueChanged = bIsNewValue || (*ExistingValue != Value);

	NumericParameters.FindOrAdd(ParameterName) = Value;

	// 仅在值发生变化时通知（排除初始化阶段的大量调用）
	if (bValueChanged && OwnerStateCmp.IsValid() && Stage != ETcsStateStage::SS_Inactive)
	{
		OwnerStateCmp->NotifyStateParameterChanged(
			this,
			ETcsStateParameterKeyType::Name,
			ParameterName,
			FGameplayTag(),
			ETcsStateParameterType::SPT_Numeric);
	}
}

bool UTcsStateInstance::GetNumericParamByTag(FGameplayTag ParameterTag, float& OutValue) const
{
	if (!ParameterTag.IsValid())
	{
		return false;
	}

	if (const float* Value = NumericParametersTag.Find(ParameterTag))
	{
		OutValue = *Value;
		return true;
	}

	return false;
}

void UTcsStateInstance::SetNumericParamByTag(FGameplayTag ParameterTag, float Value)
{
	if (!ParameterTag.IsValid())
	{
		return;
	}

	float* ExistingValue = NumericParametersTag.Find(ParameterTag);
	bool bIsNewValue = (ExistingValue == nullptr);
	bool bValueChanged = bIsNewValue || (*ExistingValue != Value);

	NumericParametersTag.FindOrAdd(ParameterTag) = Value;

	if (bValueChanged && OwnerStateCmp.IsValid() && Stage != ETcsStateStage::SS_Inactive)
	{
		OwnerStateCmp->NotifyStateParameterChanged(
			this,
			ETcsStateParameterKeyType::Tag,
			NAME_None,
			ParameterTag,
			ETcsStateParameterType::SPT_Numeric);
	}
}

bool UTcsStateInstance::GetBoolParam(FName ParameterName, bool& OutValue) const
{
	if (const bool* Value = BoolParameters.Find(ParameterName))
	{
		OutValue = *Value;
		return true;
	}
	return false;
}

void UTcsStateInstance::SetBoolParam(FName ParameterName, bool Value)
{
	bool* ExistingValue = BoolParameters.Find(ParameterName);
	bool bIsNewValue = (ExistingValue == nullptr);
	bool bValueChanged = bIsNewValue || (*ExistingValue != Value);

	BoolParameters.FindOrAdd(ParameterName) = Value;

	// 仅在值发生变化时通知
	if (bValueChanged && OwnerStateCmp.IsValid() && Stage != ETcsStateStage::SS_Inactive)
	{
		OwnerStateCmp->NotifyStateParameterChanged(
			this,
			ETcsStateParameterKeyType::Name,
			ParameterName,
			FGameplayTag(),
			ETcsStateParameterType::SPT_Bool);
	}
}

bool UTcsStateInstance::GetBoolParamByTag(FGameplayTag ParameterTag, bool& OutValue) const
{
	if (!ParameterTag.IsValid())
	{
		return false;
	}

	if (const bool* Value = BoolParametersTag.Find(ParameterTag))
	{
		OutValue = *Value;
		return true;
	}

	return false;
}

void UTcsStateInstance::SetBoolParamByTag(FGameplayTag ParameterTag, bool Value)
{
	if (!ParameterTag.IsValid())
	{
		return;
	}

	bool* ExistingValue = BoolParametersTag.Find(ParameterTag);
	bool bIsNewValue = (ExistingValue == nullptr);
	bool bValueChanged = bIsNewValue || (*ExistingValue != Value);

	BoolParametersTag.FindOrAdd(ParameterTag) = Value;

	if (bValueChanged && OwnerStateCmp.IsValid() && Stage != ETcsStateStage::SS_Inactive)
	{
		OwnerStateCmp->NotifyStateParameterChanged(
			this,
			ETcsStateParameterKeyType::Tag,
			NAME_None,
			ParameterTag,
			ETcsStateParameterType::SPT_Bool);
	}
}

bool UTcsStateInstance::GetVectorParam(FName ParameterName, FVector& OutValue) const
{
	if (const FVector* Value = VectorParameters.Find(ParameterName))
	{
		OutValue = *Value;
		return true;
	}
	return false;
}

void UTcsStateInstance::SetVectorParam(FName ParameterName, const FVector& Value)
{
	FVector* ExistingValue = VectorParameters.Find(ParameterName);
	bool bIsNewValue = (ExistingValue == nullptr);
	bool bValueChanged = bIsNewValue || (*ExistingValue != Value);

	VectorParameters.FindOrAdd(ParameterName) = Value;

	// 仅在值发生变化时通知
	if (bValueChanged && OwnerStateCmp.IsValid() && Stage != ETcsStateStage::SS_Inactive)
	{
		OwnerStateCmp->NotifyStateParameterChanged(
			this,
			ETcsStateParameterKeyType::Name,
			ParameterName,
			FGameplayTag(),
			ETcsStateParameterType::SPT_Vector);
	}
}

bool UTcsStateInstance::GetVectorParamByTag(FGameplayTag ParameterTag, FVector& OutValue) const
{
	if (!ParameterTag.IsValid())
	{
		return false;
	}

	if (const FVector* Value = VectorParametersTag.Find(ParameterTag))
	{
		OutValue = *Value;
		return true;
	}

	return false;
}

void UTcsStateInstance::SetVectorParamByTag(FGameplayTag ParameterTag, const FVector& Value)
{
	if (!ParameterTag.IsValid())
	{
		return;
	}

	FVector* ExistingValue = VectorParametersTag.Find(ParameterTag);
	bool bIsNewValue = (ExistingValue == nullptr);
	bool bValueChanged = bIsNewValue || (*ExistingValue != Value);

	VectorParametersTag.FindOrAdd(ParameterTag) = Value;

	if (bValueChanged && OwnerStateCmp.IsValid() && Stage != ETcsStateStage::SS_Inactive)
	{
		OwnerStateCmp->NotifyStateParameterChanged(
			this,
			ETcsStateParameterKeyType::Tag,
			NAME_None,
			ParameterTag,
			ETcsStateParameterType::SPT_Vector);
	}
}

TArray<FName> UTcsStateInstance::GetAllNumericParamNames() const
{
	TArray<FName> ParamNames;
	NumericParameters.GetKeys(ParamNames);
	return ParamNames;
}

TArray<FGameplayTag> UTcsStateInstance::GetAllNumericParamTags() const
{
	TArray<FGameplayTag> ParamTags;
	NumericParametersTag.GetKeys(ParamTags);
	return ParamTags;
}

TArray<FName> UTcsStateInstance::GetAllBoolParamNames() const
{
	TArray<FName> ParamNames;
	BoolParameters.GetKeys(ParamNames);
	return ParamNames;
}

TArray<FGameplayTag> UTcsStateInstance::GetAllBoolParamTags() const
{
	TArray<FGameplayTag> ParamTags;
	BoolParametersTag.GetKeys(ParamTags);
	return ParamTags;
}

TArray<FName> UTcsStateInstance::GetAllVectorParamNames() const
{
	TArray<FName> ParamNames;
	VectorParameters.GetKeys(ParamNames);
	return ParamNames;
}

TArray<FGameplayTag> UTcsStateInstance::GetAllVectorParamTags() const
{
	TArray<FGameplayTag> ParamTags;
	VectorParametersTag.GetKeys(ParamTags);
	return ParamTags;
}
