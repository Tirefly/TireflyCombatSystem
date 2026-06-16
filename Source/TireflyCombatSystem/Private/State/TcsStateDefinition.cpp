// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateDefinition.h"

#include "TcsDeveloperSettings.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif



UTcsStateDefinition::UTcsStateDefinition()
{
	if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	{
		if (Settings->DefaultLevelParamTag.IsValid())
		{
			LevelParamTag = Settings->DefaultLevelParamTag;
		}
	}
}

// 定义 PrimaryAssetType 静态变量
const FPrimaryAssetType UTcsStateDefinition::PrimaryAssetType = FPrimaryAssetType("TcsStateDef");



FPrimaryAssetId UTcsStateDefinition::GetPrimaryAssetId() const
{
	// 使用 StateDefId 作为 PrimaryAssetName
	return FPrimaryAssetId(PrimaryAssetType, StateDefId);
}

UClass* UTcsStateDefinition::ResolveStateInstanceClass() const
{
	return UTcsStateInstance::StaticClass();
}


#if WITH_EDITOR
void UTcsStateDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

EDataValidationResult UTcsStateDefinition::IsDataValid(FDataValidationContext& Context) const
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

	return Result;
}
#endif
