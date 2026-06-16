// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillModifierDefinition.h"



const FPrimaryAssetType UTcsSkillModifierDefinition::PrimaryAssetType = TEXT("TcsSkillModifierDef");


FPrimaryAssetId UTcsSkillModifierDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}
