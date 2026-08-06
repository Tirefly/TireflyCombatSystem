// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeModifierDefinition.h"

#include "Attribute/AttrModMerger/TcsAttrModMerger_NoMerge.h"
#include "Attribute/TcsAttributeModifierCompatibility.h"
#include "TcsDeveloperSettings.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif



// 定义 PrimaryAssetType 静态变量
const FPrimaryAssetType UTcsAttributeModifierDefinition::PrimaryAssetType = FPrimaryAssetType("TcsAttributeModifierDef");


UTcsAttributeModifierDefinition::UTcsAttributeModifierDefinition()
{
	MergerType = UTcsAttrModMerger_NoMerge::StaticClass();
}

FPrimaryAssetId UTcsAttributeModifierDefinition::GetPrimaryAssetId() const
{
	// 使用 AttributeModifierDefId 作为 PrimaryAssetName
	return FPrimaryAssetId(PrimaryAssetType, AttributeModifierDefId);
}

#if WITH_EDITOR
void UTcsAttributeModifierDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTcsAttributeModifierDefinition, MergerType) && !MergerType)
	{
		MergerType = UTcsAttrModMerger_NoMerge::StaticClass();
	}
}

EDataValidationResult UTcsAttributeModifierDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (AttributeModifierDefId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("AttributeModifierDefId cannot be empty")));
		Result = EDataValidationResult::Invalid;
	}

	TArray<FText> Errors;
	TArray<FText> Warnings;
	const UTcsDeveloperSettings* const Settings = GetDefault<UTcsDeveloperSettings>();
	if (!FTcsAttributeModifierCompatibility::ValidateModifierDefinitionCompatibility(
		*this,
		Errors,
		Warnings,
		Settings))
	{
		for (const FText& Error : Errors)
		{
			Context.AddError(Error);
		}
		Result = EDataValidationResult::Invalid;
	}

	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
		if (Result == EDataValidationResult::Valid)
		{
			Result = EDataValidationResult::NotValidated;
		}
	}

	return Result;
}
#endif
