#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TcsSkillModifierExecution.generated.h"

class UTcsSkillInstance;
struct FTcsSkillModifierInstance;
struct FTcsAggregatedParamEffect;

/**
 * 技能修改器执行器：定义如何将载荷应用到技能参数
 */
UCLASS(Abstract, Blueprintable, ClassGroup = (TcsCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsSkillModifierExecution : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 执行修改器(新接口,直接操作聚合效果)
	 * @param SkillInstance 目标技能实例
	 * @param ModInst 修改器实例
	 * @param InOutEffect 聚合效果(输入输出参数)
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "TcsCombatSystem|SkillModifier")
	void ExecuteToEffect(
		UTcsSkillInstance* SkillInstance,
		const FTcsSkillModifierInstance& ModInst,
		UPARAM(ref) FTcsAggregatedParamEffect& InOutEffect
	);
	virtual void ExecuteToEffect_Implementation(
		UTcsSkillInstance* SkillInstance,
		const FTcsSkillModifierInstance& ModInst,
		FTcsAggregatedParamEffect& InOutEffect
	);

	/** 执行修改器(旧接口,保留蓝图兼容性) */
	UFUNCTION(BlueprintNativeEvent, Category = "TcsCombatSystem|SkillModifier")
	void Execute(UTcsSkillInstance* SkillInstance, UPARAM(ref) FTcsSkillModifierInstance& SkillModInstance);
	virtual void Execute_Implementation(UTcsSkillInstance* SkillInstance, UPARAM(ref) FTcsSkillModifierInstance& SkillModInstance);
};
