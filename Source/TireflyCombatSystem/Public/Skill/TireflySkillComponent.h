// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TireflySkillComponent.generated.h"

class UTireflyStateInstance;
class UTireflyStateComponent;
class UTireflySkillManagerSubsystem;
class UTireflySkillInstance;
struct FTireflyStateDefinition;

/**
 * 技能查询配置结构
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTireflySkillQuery
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

UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tirefly Skill Comp"))
class TIREFLYCOMBATSYSTEM_API UTireflySkillComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region ActorComponent

public:
	UTireflySkillComponent();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#pragma endregion

#pragma region SkillLearning

public:
	// 技能学习系统
	UFUNCTION(BlueprintCallable, Category = "Skill|Learning")
	UTireflySkillInstance* LearnSkill(FName SkillDefId, int32 InitialLevel = 1);
	
	UFUNCTION(BlueprintCallable, Category = "Skill|Learning")
	void ForgetSkill(FName SkillDefId);
	
	UFUNCTION(BlueprintPure, Category = "Skill|Learning")
	bool HasLearnedSkill(FName SkillDefId) const;
	
	UFUNCTION(BlueprintPure, Category = "Skill|Learning")
	UTireflySkillInstance* GetSkillInstance(FName SkillDefId) const;
	
	UFUNCTION(BlueprintPure, Category = "Skill|Learning")
	int32 GetSkillLevel(FName SkillDefId) const;
	
	UFUNCTION(BlueprintCallable, Category = "Skill|Learning")
	bool UpgradeSkillInstance(FName SkillDefId, int32 LevelIncrease = 1);

	// 技能实例修正
	UFUNCTION(BlueprintCallable, Category = "Skill|Learning")
	void ModifySkillParameter(FName SkillDefId, FName ParamName, float Modifier, bool bIsMultiplier = false);

	UFUNCTION(BlueprintCallable, Category = "Skill|Learning")
	void SetSkillCooldownMultiplier(FName SkillDefId, float Multiplier);

	UFUNCTION(BlueprintCallable, Category = "Skill|Learning")
	void SetSkillCostMultiplier(FName SkillDefId, float Multiplier);

protected:
	// 已学会的技能实例 (SkillDefId -> SkillInstance)
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Learned Skills")
	TMap<FName, UTireflySkillInstance*> LearnedSkillInstances;

	// 活跃技能状态跟踪 (SkillDefId -> ActiveStateInstance)
	UPROPERTY(BlueprintReadOnly, Category = "Active Skills")
	TMap<FName, UTireflyStateInstance*> ActiveSkillStateInstances;

	// 技能实例创建辅助
	virtual UTireflySkillInstance* CreateSkillInstance(FName SkillDefId, int32 InitialLevel);

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
	virtual UTireflyStateInstance* CreateSkillStateInstance(FName SkillDefId, AActor* TargetActor, const FInstancedStruct& CastParameters);

#pragma endregion

#pragma region SkillQuery

public:
	// 技能查询系统
	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	TArray<FName> GetLearnedSkills() const;

	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	TArray<UTireflySkillInstance*> GetAllSkillInstances() const;
	
	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	TArray<UTireflyStateInstance*> GetActiveSkillStateInstances() const;
	
	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	UTireflyStateInstance* GetActiveSkillStateInstance(FName SkillDefId) const;
	
	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	bool IsSkillActive(FName SkillDefId) const;

	// 技能参数查询
	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	float GetSkillNumericParameter(FName SkillDefId, FName ParamName) const;

	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	bool GetSkillBoolParameter(FName SkillDefId, FName ParamName, bool DefaultValue = false) const;

	UFUNCTION(BlueprintPure, Category = "Skill|Query")
	FVector GetSkillVectorParameter(FName SkillDefId, FName ParamName, const FVector& DefaultValue = FVector::ZeroVector) const;

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

#pragma region Components

protected:
	// 获取组件引用
	UTireflyStateComponent* GetStateComponent() const;
	
	// 子系统引用
	UPROPERTY()
	TObjectPtr<UTireflySkillManagerSubsystem> SkillManagerSubsystem;

#pragma endregion

#pragma region SkillParameters

protected:
	// 技能参数计算 (使用现有的StateParameter系统)
	virtual void CalculateSkillParameters(const FTireflyStateDefinition& SkillDef, UTireflyStateInstance* SkillInstance, const FInstancedStruct& CastParameters);
	
	// 技能冷却时间计算
	virtual float CalculateSkillCooldown(const FTireflyStateDefinition& SkillDef, int32 SkillLevel) const;

#pragma endregion
};
