// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/TcsStateInstance.h"
#include "TcsSkillInstance.generated.h"



class UTcsSkillEntry;
class UStateTree;



/**
 * 技能激活执行态。
 *
 * 负责表达一次技能激活进入 State 主链后的运行时对象，不承载 learned-skill 持有态。
 */
UCLASS(BlueprintType, Blueprintable)
class TIREFLYCOMBATSYSTEM_API UTcsSkillInstance : public UTcsStateInstance
{
	GENERATED_BODY()

#pragma region UObject

public:
	/** 构造默认的 Skill 激活执行态。 */
	UTcsSkillInstance();

	/** @return 当前运行时实例所在的世界。 */
	virtual UWorld* GetWorld() const override;

#pragma endregion


#pragma region Skill

public:
	/** @return 当前技能激活对应的 learned-skill 数据对象。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Runtime")
	UTcsSkillEntry* GetSkillEntry() const { return SkillEntry.Get(); }

	/** 设置当前技能激活对应的 learned-skill 数据对象。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Runtime")
	void SetSkillEntry(UTcsSkillEntry* InSkillEntry) { SkillEntry = InSkillEntry; }

protected:
	/** 当前技能激活关联的 learned-skill 数据对象。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Skill|Runtime", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTcsSkillEntry> SkillEntry = nullptr;

#pragma endregion


#pragma region Runtime

protected:
	/** 为 SkillStateTree 写入 Skill runtime 上下文；当前阶段由 Skill 专用 schema 接管。 */
	virtual bool SetContextRequirements(FStateTreeExecutionContext& Context) override;

	/** 为 SkillStateTree 收集外部数据；当前阶段由 Skill 专用 schema 接管。 */
	virtual bool CollectExternalData(
		const FStateTreeExecutionContext& Context,
		const UStateTree* StateTree,
		TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs,
		TArrayView<FStateTreeDataView> OutDataViews) override;

#pragma endregion
};
