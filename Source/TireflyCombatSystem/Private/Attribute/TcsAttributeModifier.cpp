// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeModifier.h"
#include "Attribute/TcsAttributeModifierDefinition.h"



bool FTcsAttributeModifierInstance::IsValid() const
{
	return ModifierDef
		&& ModifierDefId != NAME_None
		&& ModifierInstId >= 0
		&& SourceHandle.IsValid()
		&& OwningStateInstance.IsValid();
}

bool FTcsAttributeModifierInstance::operator<(const FTcsAttributeModifierInstance& Other) const
{
	// 直接使用硬指针获取优先级
	if (ModifierDef && Other.ModifierDef)
	{
		const int32 ThisPriority = FMath::Max<int32>(0, ModifierDef->Priority);
		const int32 OtherPriority = FMath::Max<int32>(0, Other.ModifierDef->Priority);
		if (ThisPriority != OtherPriority)
		{
			return ThisPriority > OtherPriority;
		}
	}

	if (ModifierDefId != Other.ModifierDefId)
	{
		return ModifierDefId.LexicalLess(Other.ModifierDefId);
	}

	return ModifierInstId < Other.ModifierInstId;
}
