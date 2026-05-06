// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeModifierDefinition.h"
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

	// 验证 AttributeName
	if (AttributeName.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("AttributeName cannot be empty")));
		Result = EDataValidationResult::Invalid;
	}

	// 验证 ModifierType
	if (!ModifierType)
	{
		Context.AddError(FText::FromString(TEXT("ModifierType cannot be empty")));
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
