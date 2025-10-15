#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TcsSkillModifierUtils.generated.h"

struct FTcsSkillModifierDefinition;

/**
 * 技能修改器工具方法
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsSkillModifierUtils : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TcsCombatSystem|SkillModifier")
	static bool ValidateDefinitionRow(const FTcsSkillModifierDefinition& Definition, FString& OutError);
};
