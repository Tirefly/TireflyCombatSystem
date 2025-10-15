#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TcsSkillModifierMerger.generated.h"

struct FTcsSkillModifierInstance;

/**
 * 技能修改器合并器：统一合并同类修改器
 */
UCLASS(Abstract, Blueprintable, ClassGroup = (TcsCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsSkillModifierMerger : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "TcsCombatSystem|SkillModifier")
	void Merge(UPARAM(ref) TArray<FTcsSkillModifierInstance>& SourceModifiers, TArray<FTcsSkillModifierInstance>& OutModifiers);
	virtual void Merge_Implementation(UPARAM(ref) TArray<FTcsSkillModifierInstance>& SourceModifiers, TArray<FTcsSkillModifierInstance>& OutModifiers);
};
