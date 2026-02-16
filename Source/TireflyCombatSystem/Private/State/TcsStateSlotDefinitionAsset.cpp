// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateSlotDefinitionAsset.h"
#include "State/SamePriorityPolicy/TcsStateSamePriorityPolicy_UseNewest.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

// 定义 PrimaryAssetType 静态变量
const FPrimaryAssetType UTcsStateSlotDefinitionAsset::PrimaryAssetType = FPrimaryAssetType("TcsStateSlotDef");

UTcsStateSlotDefinitionAsset::UTcsStateSlotDefinitionAsset()
{
	// 设置默认的同优先级排序策略
	SamePriorityPolicy = UTcsStateSamePriorityPolicy_UseNewest::StaticClass();
}

FPrimaryAssetId UTcsStateSlotDefinitionAsset::GetPrimaryAssetId() const
{
	// 使用 StateSlotDefId 作为 PrimaryAssetName
	return FPrimaryAssetId(PrimaryAssetType, StateSlotDefId);
}

#if WITH_EDITOR
void UTcsStateSlotDefinitionAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	// 验证 SamePriorityPolicy
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTcsStateSlotDefinitionAsset, SamePriorityPolicy))
	{
		// 如果 SamePriorityPolicy 为空，设置为默认值
		if (!SamePriorityPolicy)
		{
			SamePriorityPolicy = UTcsStateSamePriorityPolicy_UseNewest::StaticClass();
		}
	}
}

EDataValidationResult UTcsStateSlotDefinitionAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 验证 StateSlotDefId
	if (StateSlotDefId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("StateSlotDefId cannot be empty")));
		Result = EDataValidationResult::Invalid;
	}

	// 验证 SlotTag
	if (!SlotTag.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("SlotTag cannot be empty")));
		Result = EDataValidationResult::Invalid;
	}

	// 验证 SamePriorityPolicy（如果 ActivationMode 为 PriorityOnly）
	if (ActivationMode == ETcsStateSlotActivationMode::SSAM_PriorityOnly && !SamePriorityPolicy)
	{
		Context.AddError(FText::FromString(TEXT("ActivationMode is PriorityOnly, but SamePriorityPolicy is empty")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
