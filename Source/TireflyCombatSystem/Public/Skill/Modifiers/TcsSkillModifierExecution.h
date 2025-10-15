#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TcsSkillModifierExecution.generated.h"

class UTcsSkillInstance;
struct FTcsSkillModifierInstance;

/**
 * 技能修改器执行器：定义如何将载荷应用到技能参数
 */
UCLASS(Abstract, Blueprintable, ClassGroup = (TcsCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsSkillModifierExecution : public UObject
{
	GENERATED_BODY()

public:
	/** 执行修改器 */
	UFUNCTION(BlueprintNativeEvent, Category = "TcsCombatSystem|SkillModifier")
	void Execute(UTcsSkillInstance* SkillInstance, UPARAM(ref) FTcsSkillModifierInstance& SkillModInstance);
	virtual void Execute_Implementation(UTcsSkillInstance* SkillInstance, UPARAM(ref) FTcsSkillModifierInstance& SkillModInstance);
};
