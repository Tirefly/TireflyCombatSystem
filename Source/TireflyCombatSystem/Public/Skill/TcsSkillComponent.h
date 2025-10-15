// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "InstancedStruct.h"
#include "Skill/Modifiers/TcsSkillModifierEffect.h"
#include "Skill/Modifiers/TcsSkillModifierInstance.h"
#include "Skill/Modifiers/TcsSkillModifierDefinition.h"
#include "TcsSkillComponent.generated.h"

class UTcsStateInstance;
class UTcsStateComponent;
class UTcsSkillManagerSubsystem;
class UTcsStateManagerSubsystem;
class UTcsSkillFilter;
class UTcsSkillModifierCondition;
class UTcsSkillInstance;
struct FTcsStateDefinition;

/**
 * 技能查询配置结构
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSkillQuery
{
	GENERATED_BODY()

public:
	// 是否按状态类型过滤
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
	bool bFilterByStateType = true;
	
	// 状态类型过滤值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query", meta = (EditCondition = "bFilterByStateType"))
	uint8 StateType = 1; // ST_Skill
	
	// 是否按槽位类型过滤
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
	bool bFilterBySlotType = false;
	
	// 槽位类型过滤
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query", meta = (EditCondition = "bFilterBySlotType"))
	FGameplayTag StateSlotType;
	
	// 是否按标签过滤
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query")
	bool bFilterByTags = false;
	
	// 必须包含的标签
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query", meta = (EditCondition = "bFilterByTags"))
	FGameplayTagContainer RequiredTags;
	
	// 排除的标签
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query", meta = (EditCondition = "bFilterByTags"))
	FGameplayTagContainer BlockedTags;
};

UCLASS(ClassGroup = (TcsCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tirefly Skill Comp"))
class TIREFLYCOMBATSYSTEM_API UTcsSkillComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region ActorComponent

public:
	UTcsSkillComponent();

protected:
	virtual void BeginPlay() override;
	
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

#pragma endregion


#pragma region SkillLearning

public:
	// 技能学习系统
	UFUNCTION(BlueprintCallable, Category = "Skill|Learning")
	UTcsSkillInstance* LearnSkill(FName SkillDefId, int32 InitialLevel = 1);
	
	UFUNCTION(BlueprintCallable, Category = "Skill|Learning")
	void ForgetSkill(FName SkillDefId);
	
	UFUNCTION(BlueprintPure, Category = "Skill|Learning")
	bool HasLearnedSkill(FName SkillDefId) const;
	
	UFUNCTION(BlueprintPure, Category = "Skill|Learning")
	UTcsSkillInstance* GetSkillInstance(FName SkillDefId) const;
	
	UFUNCTION(BlueprintPure, Category = "Skill|Learning")
	int32 GetSkillLevel(FName SkillDefId) const;
	
	UFUNCTION(BlueprintCallable, Category = "Skill|Learning")
	bool UpgradeSkillInstance(FName SkillDefId, int32 LevelIncrease = 1);

protected:
	// 已学会的技能实例 (SkillDefId -> SkillInstance)
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Learned Skills")
	TMap<FName, UTcsSkillInstance*> LearnedSkillInstances;

	// 活跃技能状态跟踪 (SkillDefId -> ActiveStateInstance)
	UPROPERTY(BlueprintReadOnly, Category = "Active Skills")
	TMap<FName, UTcsStateInstance*> ActiveSkillStateInstances;

	// 技能实例创建辅助
	virtual UTcsSkillInstance* CreateSkillInstance(FName SkillDefId, int32 InitialLevel);

#pragma endregion

	
#pragma region SkillCasting

public:
	// 技能释放系统
	UFUNCTION(BlueprintCallable, Category = "Skill|Casting")
	bool TryCastSkill(FName SkillDefId, AActor* TargetActor = nullptr, const FInstancedStruct& CastParameters = FInstancedStruct());
	
	UFUNCTION(BlueprintCallable, Category = "Skill|Casting")
	void CancelSkill(FName SkillDefId);
	
	UFUNCTION(BlueprintCallable, Category = "Skill|Casting")
	void CancelAllSkills();
	
	UFUNCTION(BlueprintPure, Category = "Skill|Casting")
	bool CanCastSkill(FName SkillDefId, AActor* TargetActor = nullptr) const;

protected:
	// 技能释放逻辑
	virtual bool ValidateSkillCast(FName SkillDefId, AActor* TargetActor, FText& FailureReason) const;
	
	virtual UTcsStateInstance* CreateSkillStateInstance(
		FName SkillDefId,
		AActor* TargetActor,
		const FInstancedStruct& CastParameters);

#pragma endregion

	
#pragma region SkillQuery

public:
	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	TArray<FName> GetLearnedSkills() const;

	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	TArray<UTcsSkillInstance*> GetAllSkillInstances() const;
	
	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	TArray<UTcsStateInstance*> GetActiveSkillStateInstances() const;
	
	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	UTcsStateInstance* GetActiveSkillStateInstance(FName SkillDefId) const;
	
	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	bool IsSkillActive(FName SkillDefId) const;

#pragma endregion

	
#pragma region SkillCooldown

public:
	// 技能冷却管理
	UFUNCTION(BlueprintPure, Category = "Skill|Cooldown")
	bool IsSkillOnCooldown(FName SkillDefId) const;
	
	UFUNCTION(BlueprintPure, Category = "Skill|Cooldown")
	float GetSkillCooldownRemaining(FName SkillDefId) const;
	
	UFUNCTION(BlueprintPure, Category = "Skill|Cooldown")
	float GetSkillCooldownTotal(FName SkillDefId) const;
	
	UFUNCTION(BlueprintCallable, Category = "Skill|Cooldown")
	void SetSkillCooldown(FName SkillDefId, float CooldownDuration);
	
	UFUNCTION(BlueprintCallable, Category = "Skill|Cooldown")
	void ReduceSkillCooldown(FName SkillDefId, float CooldownReduction);
	
	UFUNCTION(BlueprintCallable, Category = "Skill|Cooldown")
	void ClearSkillCooldown(FName SkillDefId);

protected:
	// 技能冷却管理 (SkillDefId -> CooldownEndTime)
	UPROPERTY(BlueprintReadOnly, Category = "Cooldowns")
	TMap<FName, float> SkillCooldowns;
	
	// 技能冷却持续时间缓存 (SkillDefId -> TotalCooldownDuration)
	UPROPERTY(BlueprintReadOnly, Category = "Cooldowns")
	TMap<FName, float> SkillCooldownDurations;
	
	// 更新技能冷却
	virtual void UpdateSkillCooldowns(float DeltaTime);
	
	// 更新活跃技能的实时参数
	virtual void UpdateActiveSkillRealTimeParameters();

#pragma endregion

	
#pragma region SkillModifiers

public:
	UFUNCTION(BlueprintCallable, Category = "Skill|Modifiers")
	bool ApplySkillModifiers(const TArray<FTcsSkillModifierDefinition>& Modifiers, TArray<int32>& OutInstanceIds);

	UFUNCTION(BlueprintCallable, Category = "Skill|Modifiers")
	bool RemoveSkillModifierById(int32 InstanceId);

	UFUNCTION(BlueprintCallable, Category = "Skill|Modifiers")
	void UpdateSkillModifiers();

	UFUNCTION(BlueprintPure, Category = "Skill|Modifiers")
	bool GetAggregatedParamEffect(const UTcsSkillInstance* Skill, FName ParamName, FTcsAggregatedParamEffect& OutEffect) const;

	UFUNCTION(BlueprintPure, Category = "Skill|Modifiers")
	bool GetAggregatedParamEffectByTag(const UTcsSkillInstance* Skill, FGameplayTag ParamTag, FTcsAggregatedParamEffect& OutEffect) const;

protected:
	void MarkSkillParamDirty(UTcsSkillInstance* Skill, FName ParamName);
	void MarkSkillParamDirtyByTag(UTcsSkillInstance* Skill, FGameplayTag ParamTag);
	void RebuildAggregatedForSkill(UTcsSkillInstance* Skill);
	void RebuildAggregatedForSkillParam(UTcsSkillInstance* Skill, FName ParamName);
	void RebuildAggregatedForSkillParamByTag(UTcsSkillInstance* Skill, FGameplayTag ParamTag);
	void RunFilter(const FTcsSkillModifierDefinition& Definition, TArray<UTcsSkillInstance*>& OutSkills) const;
	bool EvaluateConditions(AActor* Owner, UTcsSkillInstance* Skill, UTcsStateInstance* ActiveState, const FTcsSkillModifierInstance& Instance) const;
	void MergeAndSortModifiers(TArray<FTcsSkillModifierInstance>& InOutModifiers) const;
	FTcsAggregatedParamEffect BuildAggregatedEffect(const UTcsSkillInstance* Skill, const FName& ParamName) const;
	FTcsAggregatedParamEffect BuildAggregatedEffectByTag(const UTcsSkillInstance* Skill, const FGameplayTag& ParamTag) const;

protected:
	UPROPERTY()
	TArray<FTcsSkillModifierInstance> ActiveSkillModifiers;

	// 基于FName的技能参数效果缓存 (SkillInstance -> ParamEffectByName)
	UPROPERTY()
	mutable TMap<const UTcsSkillInstance*, FTcsSkillParamEffectByName> SkillParamEffectsByName;

	// 基于GameplayTag的技能参数效果缓存 (SkillInstance -> ParamEffectByTag)
	UPROPERTY()
	mutable TMap<const UTcsSkillInstance*, FTcsSkillParamEffectByTag> SkillParamEffectsByTag;

	UPROPERTY()
	int32 SkillModifierInstanceIdMgr = 0;

#pragma endregion

#pragma region Components

protected:
	// 获取组件引用
	UTcsStateComponent* GetStateComponent() const;

protected:
	// 子系统引用
	UPROPERTY()
	TObjectPtr<UTcsSkillManagerSubsystem> SkillManagerSubsystem;

	UPROPERTY()
	TObjectPtr<UTcsStateManagerSubsystem> StateManagerSubsystem;

#pragma endregion

	
#pragma region SkillParameters

public:
	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	float GetSkillNumericParameter(FName SkillDefId, FName ParamName) const;

	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	bool GetSkillBoolParameter(FName SkillDefId, FName ParamName, bool DefaultValue = false) const;

	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	FVector GetSkillVectorParameter(FName SkillDefId, FName ParamName, const FVector& DefaultValue = FVector::ZeroVector) const;

	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	float GetSkillNumericParameterByTag(FName SkillDefId, FGameplayTag ParamTag) const;

	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	bool GetSkillBoolParameterByTag(FName SkillDefId, FGameplayTag ParamTag, bool DefaultValue = false) const;

	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	FVector GetSkillVectorParameterByTag(FName SkillDefId, FGameplayTag ParamTag, const FVector& DefaultValue = FVector::ZeroVector) const;

protected:
	// 技能参数计算 (使用现有的StateParameter系统)
	virtual void CalculateSkillParameters(const FTcsStateDefinition& SkillDef, UTcsStateInstance* SkillInstance, const FInstancedStruct& CastParameters);
	
	// 技能冷却时间计算
	virtual float CalculateSkillCooldown(const FTcsStateDefinition& SkillDef, int32 SkillLevel) const;

#pragma endregion
};
