// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeDefinitionAsset.h"
#include "Attribute/AttrClampStrategy/TcsAttrClampStrategy_Linear.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif


// 定义 PrimaryAssetType 静态变量
const FPrimaryAssetType UTcsAttributeDefinitionAsset::PrimaryAssetType = FPrimaryAssetType("TcsAttributeDef");

UTcsAttributeDefinitionAsset::UTcsAttributeDefinitionAsset()
{
	// 设置默认 Clamp 策略
	ClampStrategyClass = UTcsAttrClampStrategy_Linear::StaticClass();
}
FPrimaryAssetId UTcsAttributeDefinitionAsset::GetPrimaryAssetId() const
{
	// 使用 AttributeDefId 作为 PrimaryAssetName
	return FPrimaryAssetId(PrimaryAssetType, AttributeDefId);
}

#if WITH_EDITOR
void UTcsAttributeDefinitionAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	// 验证 AttributeRange（静态类型时，确保 MinValue <= MaxValue）
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTcsAttributeDefinitionAsset, AttributeRange))
	{
		if (AttributeRange.MinValueType == ETcsAttributeRangeType::ART_Static &&
			AttributeRange.MaxValueType == ETcsAttributeRangeType::ART_Static)
		{
			if (AttributeRange.MinValue > AttributeRange.MaxValue)
			{
				const float Temp = AttributeRange.MinValue;
				AttributeRange.MinValue = AttributeRange.MaxValue;
				AttributeRange.MaxValue = Temp;
			}
		}
	}

	// 验证 ClampStrategyClass
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTcsAttributeDefinitionAsset, ClampStrategyClass))
	{
		// 如果 ClampStrategyClass 为空，设置为默认值
		if (!ClampStrategyClass)
		{
			ClampStrategyClass = UTcsAttrClampStrategy_Linear::StaticClass();
		}
	}
}

EDataValidationResult UTcsAttributeDefinitionAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 验证 AttributeDefId
	if (AttributeDefId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("AttributeDefId cannot be empty")));
		Result = EDataValidationResult::Invalid;
	}

	// 验证 AttributeRange（静态类型时，确保 MinValue <= MaxValue）
	if (AttributeRange.MinValueType == ETcsAttributeRangeType::ART_Static &&
		AttributeRange.MaxValueType == ETcsAttributeRangeType::ART_Static)
	{
		if (AttributeRange.MinValue > AttributeRange.MaxValue)
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("AttributeRange invalid: MinValue (%.2f) > MaxValue (%.2f)"),
				AttributeRange.MinValue, AttributeRange.MaxValue)));
			Result = EDataValidationResult::Invalid;
		}
	}

	// 验证 ClampStrategyClass
	if (!ClampStrategyClass)
	{
		Context.AddError(FText::FromString(TEXT("ClampStrategyClass cannot be empty")));
		Result = EDataValidationResult::Invalid;
	}

	// 验证 AttributeName（如果显示在UI中，必须有名称）
	if (bShowInUI && AttributeName.IsEmpty())
	{
		Context.AddWarning(FText::FromString(TEXT("bShowInUI is true, but AttributeName is empty")));
		if (Result == EDataValidationResult::Valid)
		{
			Result = EDataValidationResult::NotValidated;
		}
	}

	return Result;
}
#endif
