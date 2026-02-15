// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateSlotDefinitionAsset.h"
#include "State/SamePriorityPolicy/TcsStateSamePriorityPolicy_UseNewest.h"

// 定义 PrimaryAssetType 静态变量
const FPrimaryAssetType UTcsStateSlotDefinitionAsset::PrimaryAssetType = FPrimaryAssetType("TcsStateSlotDef");

UTcsStateSlotDefinitionAsset::UTcsStateSlotDefinitionAsset()
{
	// 设置默认的同优先级排序策略（与 FTcsStateSlotDefinition 保持一致）
	SamePriorityPolicy = UTcsStateSamePriorityPolicy_UseNewest::StaticClass();
}

FPrimaryAssetId UTcsStateSlotDefinitionAsset::GetPrimaryAssetId() const
{
	// 使用 StateSlotDefId 作为 PrimaryAssetName
	return FPrimaryAssetId(PrimaryAssetType, StateSlotDefId);
}
