#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TcsSkillModifierCondition.generated.h"

class UTcsSkillInstance;
class UTcsStateInstance;
struct FTcsSkillModifierInstance;

/**
 * 技能修改器条件：判断是否生效
 */
UCLASS(Abstract, Blueprintable, ClassGroup = (TcsCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsSkillModifierCondition : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "TcsCombatSystem|SkillModifier")
	bool Evaluate(AActor* Owner, UTcsSkillInstance* SkillInstance, UTcsStateInstance* ActiveStateInstance, const FTcsSkillModifierInstance& ModInst) const;
	virtual bool Evaluate_Implementation(AActor* Owner, UTcsSkillInstance* SkillInstance, UTcsStateInstance* ActiveStateInstance, const FTcsSkillModifierInstance& ModInst) const;
};
