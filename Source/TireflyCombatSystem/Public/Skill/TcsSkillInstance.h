// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/TcsStateInstance.h"
#include "State/TcsStateParamInstance.h"
#include "TcsSkillInstance.generated.h"



class UTcsSkillEntry;
class UStateTree;



/**
 * 技能激活执行态。
 *
 * 负责表达一次技能激活进入 State 主链后的运行时对象，不承载 learned-skill 持有态。
 * 覆写 GetLevel / GetStateParamInstance / PopulateStateParamInstances 指向 SkillEntry。
 * 因此来源存活期间写入 `SkillEntry` 的临时 SkillModifier，会被当前 SkillInstance 与其他读取者共享看到。
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


#pragma region Overrides

protected:
	// 返回 SkillEntry->GetLevel()
	virtual int32 GetLevel() const override;

	// 指向 Entry->NumericParamInstances，共享读取 SkillEntry 上的同一条 SkillModifier 参数链。
	virtual FTcsNumericStateParamInstance* GetNumericParamInstance(FGameplayTag Tag) override;

	// 指向 Entry->BoolParamInstances，共享读取 SkillEntry 上的同一条 SkillModifier 参数链。
	virtual FTcsBoolStateParamInstance* GetBoolParamInstance(FGameplayTag Tag) override;

	// 指向 Entry->VectorParamInstances，共享读取 SkillEntry 上的同一条 SkillModifier 参数链。
	virtual FTcsVectorStateParamInstance* GetVectorParamInstance(FGameplayTag Tag) override;

	// 返回 Entry->NumericParamInstances（供 ResolveNumericParamInstances 使用）
	virtual TMap<FGameplayTag, FTcsNumericStateParamInstance>& GetNumericParamInstances() override;

	// 返回 Entry->BoolParamInstances
	virtual TMap<FGameplayTag, FTcsBoolStateParamInstance>& GetBoolParamInstances() override;

	// 返回 Entry->VectorParamInstances
	virtual TMap<FGameplayTag, FTcsVectorStateParamInstance>& GetVectorParamInstances() override;

	// Entry 已持有 StateParamInstances，无需从 Def 重新 populate
	virtual bool PopulateStateParamInstances(
		const UTcsStateDefinition* InStateDef,
		AActor* InInstigator,
		AActor* InTarget,
		TArray<FName>& OutFailedParams) override { return true; }

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