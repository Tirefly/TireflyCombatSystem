// Copyright Tirefly. All Rights Reserved.

#include "Buff/TcsBuffDefinition.h"

#include "Buff/BuffMerger/TcsBuffMerger_NoMerge.h"
#include "Buff/TcsBuffInstance.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif



const FPrimaryAssetType UTcsBuffDefinition::PrimaryAssetType = FPrimaryAssetType("TcsBuffDef");




UTcsBuffDefinition::UTcsBuffDefinition()
{
	MergerType = UTcsBuffMerger_NoMerge::StaticClass();
}


FPrimaryAssetId UTcsBuffDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, StateDefId);
}


UClass* UTcsBuffDefinition::ResolveStateInstanceClass() const
{
	return BuffInstanceClass ? BuffInstanceClass.Get() : UTcsBuffInstance::StaticClass();
}


#if WITH_EDITOR
void UTcsBuffDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTcsBuffDefinition, Duration))
	{
		if (Duration < 0.f)
		{
			Duration = 0.f;
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTcsBuffDefinition, Period))
	{
		if (Period < 0.f)
		{
			Period = 0.f;
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTcsBuffDefinition, MaxStackCount))
	{
		if (MaxStackCount < 1)
		{
			MaxStackCount = 1;
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTcsBuffDefinition, MergerType) && !MergerType)
	{
		MergerType = UTcsBuffMerger_NoMerge::StaticClass();
	}
}

EDataValidationResult UTcsBuffDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (DurationType == ETcsBuffDurationType::SDT_Duration)
	{
		if (Duration <= 0.f)
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("DurationType is Duration, but Duration (%.2f) <= 0"),
				Duration)));
			Result = EDataValidationResult::Invalid;
		}
	}

	if (Period < 0.f)
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("Period (%.2f) must be >= 0"),
			Period)));
		Result = EDataValidationResult::Invalid;
	}

	if (MaxStackCount < 1)
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("MaxStackCount (%d) must be >= 1"),
			MaxStackCount)));
		Result = EDataValidationResult::Invalid;
	}

	if (!MergerType)
	{
		Context.AddError(FText::FromString(TEXT("MergerType cannot be empty")));
		Result = EDataValidationResult::Invalid;
	}
	else if (MergerType->HasAnyClassFlags(CLASS_Abstract))
	{
		Context.AddError(FText::FromString(TEXT("MergerType cannot reference an abstract class")));
		Result = EDataValidationResult::Invalid;
	}

	if (MaxStackCount > 1 && !MergerType)
	{
		Context.AddWarning(FText::FromString(TEXT("MaxStackCount > 1, but MergerType is empty, buff merging may not work properly")));
		if (Result == EDataValidationResult::Valid)
		{
			Result = EDataValidationResult::NotValidated;
		}
	}

	if (MaxStackCount <= 1)
	{
		if (OnStackIncrease.DurationPolicy != ETcsBuffDurationRefreshPolicy::None)
		{
			Context.AddWarning(FText::FromString(TEXT("OnStackIncrease is only meaningful when MaxStackCount > 1; current values will be ignored at runtime")));
			if (Result == EDataValidationResult::Valid)
			{
				Result = EDataValidationResult::NotValidated;
			}
		}

		if (OnDurationExpired.ExpirationPolicy != ETcsBuffStackExpirationPolicy::ClearEntireBuff)
		{
			Context.AddWarning(FText::FromString(TEXT("OnDurationExpired is only meaningful when MaxStackCount > 1; current values will be ignored at runtime")));
			if (Result == EDataValidationResult::Valid)
			{
				Result = EDataValidationResult::NotValidated;
			}
		}
	}

	if (DurationType != ETcsBuffDurationType::SDT_Duration
		&& OnDurationExpired.ExpirationPolicy != ETcsBuffStackExpirationPolicy::ClearEntireBuff)
	{
		Context.AddWarning(FText::FromString(TEXT("OnDurationExpired is only meaningful for finite-duration buffs; current values will be ignored at runtime")));
		if (Result == EDataValidationResult::Valid)
		{
			Result = EDataValidationResult::NotValidated;
		}
	}

	return Result;
}
#endif
