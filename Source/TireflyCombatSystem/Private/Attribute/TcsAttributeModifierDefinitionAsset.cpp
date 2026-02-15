// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeModifierDefinitionAsset.h"
#include "Attribute/TcsAttributeModifier.h"


// 定义 PrimaryAssetType 静态变量
const FPrimaryAssetType UTcsAttributeModifierDefinitionAsset::PrimaryAssetType = FPrimaryAssetType("TcsAttributeModifierDef");

UTcsAttributeModifierDefinitionAsset::UTcsAttributeModifierDefinitionAsset()
{
	// 设置默认操作数（与 FTcsAttributeModifierDefinition 保持一致）
	Operands.Add(FName("Magnitude"), 0.f);
}
FPrimaryAssetId UTcsAttributeModifierDefinitionAsset::GetPrimaryAssetId() const
{
	// 使用 AttributeModifierDefId 作为 PrimaryAssetName
	return FPrimaryAssetId(PrimaryAssetType, AttributeModifierDefId);
}
