// Copyright Tirefly. All Rights Reserved.


#include "Attribute/TcsAttributeModifier.h"
#include "Attribute/TcsAttributeModifierDefinitionAsset.h"


bool FTcsAttributeModifierInstance::operator<(const FTcsAttributeModifierInstance& Other) const
{
	// 直接使用硬指针获取优先级
	if (ModifierDefAsset && Other.ModifierDefAsset)
	{
		// Higher priority first.
		return ModifierDefAsset->Priority > Other.ModifierDefAsset->Priority;
	}

	// 如果无法获取定义，则按 ModifierId 排序
	return ModifierId.LexicalLess(Other.ModifierId);
}
