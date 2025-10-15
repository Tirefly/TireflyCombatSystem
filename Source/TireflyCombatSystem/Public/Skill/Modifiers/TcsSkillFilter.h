#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TcsSkillFilter.generated.h"

class UTcsSkillInstance;

/**
 * 技能过滤器：确定修改器要作用的技能集合
 */
UCLASS(Abstract, Blueprintable, ClassGroup = (TcsCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsSkillFilter : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "TcsCombatSystem|SkillModifier")
	void Filter(AActor* SkillOwner, TArray<UTcsSkillInstance*>& OutSkills);
	virtual void Filter_Implementation(AActor* SkillOwner, TArray<UTcsSkillInstance*>& OutSkills);
};
