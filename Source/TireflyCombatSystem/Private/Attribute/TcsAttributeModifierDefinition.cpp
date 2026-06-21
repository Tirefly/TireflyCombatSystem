// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Attribute/AttrModMerger/TcsAttrModMerger_NoMerge.h"
#include "Attribute/TcsAttributeModifier.h"
#include "Attribute/AttrModExecution/TcsAttributeModifierExecution.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif



// 定义 PrimaryAssetType 静态变量
const FPrimaryAssetType UTcsAttributeModifierDefinition::PrimaryAssetType = FPrimaryAssetType("TcsAttributeModifierDef");


UTcsAttributeModifierDefinition::UTcsAttributeModifierDefinition()
{
	// 设置默认操作数
	Operands.Add(FName("Magnitude"), 0.f);
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

	// 验证 Operands（确保至少有 Magnitude）
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTcsAttributeModifierDefinition, Operands))
	{
		if (!Operands.Contains(FName("Magnitude")))
		{
			Operands.Add(FName("Magnitude"), 0.f);
		}
	}

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

	// 验证 AttributeId
	if (AttributeId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("AttributeId cannot be empty")));
		Result = EDataValidationResult::Invalid;
	}

	// 验证 ModifierType
	if (!ModifierType)
	{
		Context.AddError(FText::FromString(TEXT("ModifierType cannot be empty")));
		Result = EDataValidationResult::Invalid;
	}
	else if (ModifierType->HasAnyClassFlags(CLASS_Abstract))
	{
		Context.AddError(FText::FromString(TEXT("ModifierType cannot reference an abstract class")));
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

	// 验证 Operands（必须包含 Magnitude）
	if (!Operands.Contains(FName("Magnitude")))
	{
		Context.AddError(FText::FromString(TEXT("Operands must contain 'Magnitude' key")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
