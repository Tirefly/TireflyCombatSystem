// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateDefinitionAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif


// 定义 PrimaryAssetType 静态变量
const FPrimaryAssetType UTcsStateDefinitionAsset::PrimaryAssetType = FPrimaryAssetType("TcsStateDef");

FPrimaryAssetId UTcsStateDefinitionAsset::GetPrimaryAssetId() const
{
	// 使用 StateDefId 作为 PrimaryAssetName
	return FPrimaryAssetId(PrimaryAssetType, StateDefId);
}

#if WITH_EDITOR
void UTcsStateDefinitionAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	// 验证 Duration
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTcsStateDefinitionAsset, Duration))
	{
		// 确保 Duration >= 0
		if (Duration < 0.f)
		{
			Duration = 0.f;
		}
	}

	// 验证 MaxStackCount
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTcsStateDefinitionAsset, MaxStackCount))
	{
		// 确保 MaxStackCount >= 1
		if (MaxStackCount < 1)
		{
			MaxStackCount = 1;
		}
	}
}

EDataValidationResult UTcsStateDefinitionAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 验证 StateDefId
	if (StateDefId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("StateDefId cannot be empty")));
		Result = EDataValidationResult::Invalid;
	}

	// 验证 StateTag（推荐但不强制）
	if (!StateTag.IsValid())
	{
		Context.AddWarning(FText::FromString(TEXT("StateTag is empty, recommend setting to TCS.State.<StateDefId>")));
		if (Result == EDataValidationResult::Valid)
		{
			Result = EDataValidationResult::NotValidated;
		}
	}

	// 验证 StateSlotType
	if (!StateSlotType.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("StateSlotType cannot be empty")));
		Result = EDataValidationResult::Invalid;
	}

	// 验证 Duration（如果 DurationType 为 Duration）
	if (DurationType == ETcsStateDurationType::SDT_Duration)
	{
		if (Duration <= 0.f)
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("DurationType is Duration, but Duration (%.2f) <= 0"),
				Duration)));
			Result = EDataValidationResult::Invalid;
		}
	}

	// 验证 MaxStackCount
	if (MaxStackCount < 1)
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("MaxStackCount (%d) must be >= 1"),
			MaxStackCount)));
		Result = EDataValidationResult::Invalid;
	}

	// 验证 MergerType（如果 MaxStackCount > 1）
	if (MaxStackCount > 1 && !MergerType)
	{
		Context.AddWarning(FText::FromString(TEXT("MaxStackCount > 1, but MergerType is empty, state merging may not work properly")));
		if (Result == EDataValidationResult::Valid)
		{
			Result = EDataValidationResult::NotValidated;
		}
	}

	return Result;
}
#endif
