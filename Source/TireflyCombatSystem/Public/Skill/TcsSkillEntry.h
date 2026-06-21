// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "State/TcsStateParamInstance.h"
#include "TcsSkillEntry.generated.h"



class UTcsSkillDefinition;
class UTcsSkillInstance;



/**
 * 已学会技能的数据对象。
 *
 * 负责表达 SkillComponent 持有的 learned-skill 身份，管理等级、冷却、StateParamInstances，并记录活跃技能实例。
 */
UCLASS(BlueprintType, Blueprintable)
class TIREFLYCOMBATSYSTEM_API UTcsSkillEntry : public UObject
{
	GENERATED_BODY()

#pragma region UObject

public:
	/** 构造默认的 learned-skill 数据对象。 */
	UTcsSkillEntry();

	/** @return 当前对象所在的世界；无有效外层时返回 nullptr。 */
	virtual UWorld* GetWorld() const override;

#pragma endregion


#pragma region Identity

public:
	/** @return 当前 learned-skill 绑定的 Skill 定义资产。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Entry")
	UTcsSkillDefinition* GetSkillDefinition() const { return SkillDefinition.Get(); }

	/** 设置当前 learned-skill 绑定的 Skill 定义资产。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Entry")
	void SetSkillDefinition(UTcsSkillDefinition* InSkillDefinition) { SkillDefinition = InSkillDefinition; }

protected:
	/** 当前 learned-skill 绑定的 Skill 定义资产。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Entry", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTcsSkillDefinition> SkillDefinition = nullptr;

#pragma endregion


#pragma region Level

public:
	/** @return 当前技能等级（含 SkillModifier 修正）。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Entry")
	int32 GetLevel() const;

	/** 设置当前技能基础等级（写入 NumericValue，不经过 Modifier）。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Entry")
	void SetLevel(int32 InLevel);

#pragma endregion


#pragma region Cooldown

public:
	/** 开始冷却（在技能成功激活时调用）。对 SkillDefinition 中 CooldownParamTag 对应的 StateParamInstance 进行求值。 */
	bool StartCooldown(UTcsSkillInstance* SkillInstance);

	/** Tick 递减冷却剩余时间。 */
	void TickCooldown(float DeltaTime);

	/** @return 当前是否在冷却中。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Cooldown")
	bool IsOnCooldown() const;

	/** @return 剩余冷却时间占最大冷却时间的比率（0~1）。冷却时长为 0 时返回 0。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Cooldown")
	float GetRemainingCooldownRatio() const;

protected:
	/** 剩余冷却时间（秒）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Skill|Cooldown", Meta = (AllowPrivateAccess = "true"))
	float RemainingCooldown = 0.0f;

#pragma endregion


#pragma region Parameters

public:
	/** 运行时 Numeric 参数实例表（权威持有者）。 */
	TMap<FGameplayTag, FTcsNumericStateParamInstance> NumericParamInstances;

	/** 运行时 Bool 参数实例表。 */
	TMap<FGameplayTag, FTcsBoolStateParamInstance> BoolParamInstances;

	/** 运行时 Vector 参数实例表。 */
	TMap<FGameplayTag, FTcsVectorStateParamInstance> VectorParamInstances;

	/** 从 Def 初始化技能参数。 */
	void InitializeFromDef(UTcsSkillDefinition* Def);

	/** @return 命中的 Numeric 参数实例；未命中时返回 nullptr。 */
	FTcsNumericStateParamInstance* FindNumericParamInstance(FGameplayTag ParamTag);

	/** @return 命中的 Bool 参数实例；未命中时返回 nullptr。 */
	FTcsBoolStateParamInstance* FindBoolParamInstance(FGameplayTag ParamTag);

	/** @return 命中的 Vector 参数实例；未命中时返回 nullptr。 */
	FTcsVectorStateParamInstance* FindVectorParamInstance(FGameplayTag ParamTag);

#pragma endregion


#pragma region Active

public:
	/** 当前激活中的 SkillInstance（单实例）。 */
	TWeakObjectPtr<UTcsSkillInstance> ActiveInstance;

#pragma endregion
};