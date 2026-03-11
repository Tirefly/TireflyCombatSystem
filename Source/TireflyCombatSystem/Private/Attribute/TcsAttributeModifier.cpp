// Copyright Tirefly. All Rights Reserved.


#include "Attribute/TcsAttributeModifier.h"
#include "Attribute/TcsAttributeModifierDefinitionAsset.h"


bool FTcsAttributeModifierInstance::IsValid() const
{
	bool bHasInvalidOperand = false;
	for (const auto& Pair : Operands)
	{
		if (Pair.Key == NAME_None)
		{
			bHasInvalidOperand = true;
			break;
		}
	}
	return ModifierId != NAME_None
		&& ModifierInstId >= 0
		&& !bHasInvalidOperand;
}

bool FTcsAttributeModifierInstance::operator<(const FTcsAttributeModifierInstance& Other) const
{
	// 直接使用硬指针获取优先级
	if (ModifierDefAsset && Other.ModifierDefAsset)
	{
		// Higher priority first.
		int32 thisPriority = FMath::Max<int32>(0, ModifierDefAsset->Priority);
		int32 OtherPriority = FMath::Max<int32>(0, Other.ModifierDefAsset->Priority);
		return thisPriority > OtherPriority;
	}

	// 如果无法获取定义，则按 ModifierId 排序
	return ModifierId.LexicalLess(Other.ModifierId);
}
