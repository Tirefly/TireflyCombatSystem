// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeDefinitionAsset.h"
#include "Attribute/AttrClampStrategy/TcsAttrClampStrategy_Linear.h"


// 定义 PrimaryAssetType 静态变量
const FPrimaryAssetType UTcsAttributeDefinitionAsset::PrimaryAssetType = FPrimaryAssetType("TcsAttributeDef");

UTcsAttributeDefinitionAsset::UTcsAttributeDefinitionAsset()
{
	// 设置默认 Clamp 策略（与 FTcsAttributeDefinition 保持一致）
	ClampStrategyClass = UTcsAttrClampStrategy_Linear::StaticClass();
}
FPrimaryAssetId UTcsAttributeDefinitionAsset::GetPrimaryAssetId() const
{
	// 使用 AttributeDefId 作为 PrimaryAssetName
	return FPrimaryAssetId(PrimaryAssetType, AttributeDefId);
}
