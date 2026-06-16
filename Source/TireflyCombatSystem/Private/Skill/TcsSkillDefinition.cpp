// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillDefinition.h"

#include "Skill/TcsSkillEntry.h"
#include "Skill/TcsSkillInstance.h"
#include "TcsDeveloperSettings.h"



UTcsSkillDefinition::UTcsSkillDefinition()
{
	// 从 DeveloperSettings 读取冷却参数默认 Tag
	if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	{
		if (Settings->DefaultSkillCooldownParamTag.IsValid())
		{
			CooldownParamTag = Settings->DefaultSkillCooldownParamTag;
		}
	}
}


UClass* UTcsSkillDefinition::ResolveSkillEntryClass() const
{
	return SkillEntryClass ? SkillEntryClass.Get() : UTcsSkillEntry::StaticClass();
}

UClass* UTcsSkillDefinition::ResolveStateInstanceClass() const
{
	return SkillInstanceClass ? SkillInstanceClass.Get() : UTcsSkillInstance::StaticClass();
}
