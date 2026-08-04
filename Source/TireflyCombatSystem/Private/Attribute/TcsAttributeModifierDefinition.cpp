// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Attribute/AttrModMerger/TcsAttrModMerger_NoMerge.h"

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

	// 验证 AttributeModifierDefId
	if (AttributeModifierDefId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("AttributeModifierDefId cannot be empty")));
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

	return Result;
}
#endif
