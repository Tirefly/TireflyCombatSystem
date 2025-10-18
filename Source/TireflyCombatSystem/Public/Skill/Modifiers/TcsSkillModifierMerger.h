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
	/**
	 * 合并修改器(显式输出版本)
	 * @param SourceModifiers 待合并的源修改器列表
	 * @param OutModifiers 合并后的输出列表
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "TcsCombatSystem|SkillModifier")
	void Merge(
		UPARAM(ref) TArray<FTcsSkillModifierInstance>& SourceModifiers,
		TArray<FTcsSkillModifierInstance>& OutModifiers
	);
	virtual void Merge_Implementation(
		TArray<FTcsSkillModifierInstance>& SourceModifiers,
		TArray<FTcsSkillModifierInstance>& OutModifiers
	);
};
