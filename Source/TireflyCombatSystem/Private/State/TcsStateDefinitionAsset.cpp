// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateDefinitionAsset.h"


// 定义 PrimaryAssetType 静态变量
const FPrimaryAssetType UTcsStateDefinitionAsset::PrimaryAssetType = FPrimaryAssetType("TcsStateDef");

FPrimaryAssetId UTcsStateDefinitionAsset::GetPrimaryAssetId() const
{
	// 使用 StateDefId 作为 PrimaryAssetName
	return FPrimaryAssetId(PrimaryAssetType, StateDefId);
}
