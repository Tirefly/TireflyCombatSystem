// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateParamInstance.h"
#include "TcsSkillDefinition.generated.h"



class UTcsSkillEntry;
class UTcsSkillInstance;



/**
 * Skill 定义资产。
 *
 * 负责统一声明 Skill 的 learned 数据对象类型、激活执行态类型与冷却参数。
 */
UCLASS(BlueprintType, Const)
class TIREFLYCOMBATSYSTEM_API UTcsSkillDefinition : public UTcsStateDefinition
{
	GENERATED_BODY()

public:
	UTcsSkillDefinition();

	/** Skill Definition 的 PrimaryAssetType 标识符。 */
	static const FPrimaryAssetType PrimaryAssetType;

	/** @return Skill Definition 的 PrimaryAssetId。 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#pragma region Runtime

public:
	/** Skill 激活时创建的运行时实例类。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime")
	TSubclassOf<UTcsSkillInstance> SkillInstanceClass;

	/** SkillComponent 持有的 learned-skill 数据对象类。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime")
	TSubclassOf<UTcsSkillEntry> SkillEntryClass;

	/** @return 当前 Skill 定义解析出的 learned-skill 数据对象类。 */
	virtual UClass* ResolveSkillEntryClass() const;

	/** @return 当前 Skill 定义解析出的激活执行态类。 */
	virtual UClass* ResolveStateInstanceClass() const override;

#pragma endregion


#pragma region Cooldown

public:
	/**
	 * 冷却参数的标识 GameplayTag。
	 * 仅在 Tag 有效时，下方的 CooldownParam 才可编辑。
	 * 默认值从 UTcsDeveloperSettings::DefaultSkillCooldownParamTag 读取。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooldown")
	FGameplayTag CooldownParamTag;

	/**
	 * 冷却参数配置（Numeric 类型，支持 LevelArray 等求值器）。
	 * 不指定 Evaluator 或求值结果为 0 表示无冷却。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooldown",
		Meta = (EditCondition = "CooldownParamTag.IsValid()", EditConditionHides))
	FTcsStateParameter CooldownParam;

#pragma endregion


#if WITH_EDITOR
	/**
	 * 编辑器属性变更后的默认值归一化。
	 *
	 * @param PropertyChangedEvent 变更事件。
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/**
	 * 编辑器数据有效性检查。
	 *
	 * @param Context 验证上下文。
	 * @return 数据验证结果。
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
