// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "State/TcsState.h"
#include "TcsSkillInstance.generated.h"



class UTcsStateInstance;
class UTcsStateParamEvaluator;



/**
 * 技能实例类
 * 代表战斗实体已学会的技能，持有技能的动态属性和运行时参数
 */
UCLASS(BlueprintType, Blueprintable)
class TIREFLYCOMBATSYSTEM_API UTcsSkillInstance : public UObject
{
	GENERATED_BODY()

#pragma region UObject

public:
	UTcsSkillInstance();

	virtual UWorld* GetWorld() const override;

#pragma endregion


#pragma region SkillInstance

public:
	// 初始化技能实例
	void Initialize(
		const FTcsStateDefinition& InSkillDef,
		FName InSkillDefId,
		AActor* InOwner,
		int32 InInitialLevel = 1);

	// 获取技能定义ID
	UFUNCTION(BlueprintPure, Category = "Skill Instance")
	FName GetSkillDefId() const { return SkillDefId; }

	// 获取技能定义数据
	UFUNCTION(BlueprintPure, Category = "Skill Instance")
	const FTcsStateDefinition& GetSkillDef() const { return SkillDef; }

	// 获取技能拥有者
	UFUNCTION(BlueprintPure, Category = "Skill Instance")
	AActor* GetOwner() const { return Owner.Get(); }

protected:
	// 技能静态信息
	UPROPERTY(BlueprintReadOnly, Category = "Skill Instance")
	FName SkillDefId;

	UPROPERTY(BlueprintReadOnly, Category = "Skill Instance")
	FTcsStateDefinition SkillDef;

	UPROPERTY(BlueprintReadOnly, Category = "Skill Instance")
	TWeakObjectPtr<AActor> Owner;

#pragma endregion

	
#pragma region SkillLevel

public:
	// 技能等级管理
	UFUNCTION(BlueprintPure, Category = "Skill Instance|Level")
	int32 GetCurrentLevel() const { return CurrentLevel; }

	UFUNCTION(BlueprintCallable, Category = "Skill Instance|Level")
	void SetCurrentLevel(int32 InLevel);

	UFUNCTION(BlueprintCallable, Category = "Skill Instance|Level")
	bool UpgradeLevel(int32 LevelIncrease = 1);

protected:
	// 技能动态属性
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Skill Instance")
	int32 CurrentLevel = 1;

#pragma endregion


#pragma region CostCooldown

public:
	// 冷却时间修正
	UFUNCTION(BlueprintPure, Category = "Skill Instance|Cooldown")
	float GetCooldownMultiplier() const { return CooldownMultiplier; }

	UFUNCTION(BlueprintCallable, Category = "Skill Instance|Cooldown")
	void SetCooldownMultiplier(float InMultiplier) { CooldownMultiplier = FMath::Max(0.0f, InMultiplier); }

	// 消耗修正
	UFUNCTION(BlueprintPure, Category = "Skill Instance|Cost") 
	float GetCostMultiplier() const { return CostMultiplier; }

	UFUNCTION(BlueprintCallable, Category = "Skill Instance|Cost")
	void SetCostMultiplier(float InMultiplier) { CostMultiplier = FMath::Max(0.0f, InMultiplier); }

protected:
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Skill Instance")
	float CooldownMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Skill Instance")
	float CostMultiplier = 1.0f;

#pragma endregion
	

#pragma region Parameters

public:
	// 布尔参数操作
	UFUNCTION(BlueprintPure, Category = "Skill Instance|Parameters")
	bool GetBoolParameter(FName ParamName, bool DefaultValue = false) const;

	UFUNCTION(BlueprintCallable, Category = "Skill Instance|Parameters")
	void SetBoolParameter(FName ParamName, bool Value);

	// 向量参数操作
	UFUNCTION(BlueprintPure, Category = "Skill Instance|Parameters")
	FVector GetVectorParameter(FName ParamName, const FVector& DefaultValue = FVector::ZeroVector) const;

	UFUNCTION(BlueprintCallable, Category = "Skill Instance|Parameters")
	void SetVectorParameter(FName ParamName, const FVector& Value);

	// 数值参数计算（实时计算，不存储值）
	UFUNCTION(BlueprintCallable, Category = "Skill Instance|Parameters")  
	float CalculateNumericParameter(FName ParamName, AActor* Instigator = nullptr, AActor* Target = nullptr) const;

	// 参数快照管理
	UFUNCTION(BlueprintCallable, Category = "Skill Instance|Parameters")
	void TakeParameterSnapshot(AActor* Instigator = nullptr, AActor* Target = nullptr);

	UFUNCTION(BlueprintPure, Category = "Skill Instance|Parameters")
	float GetSnapshotParameter(FName ParamName, float DefaultValue = 0.0f) const;

	UFUNCTION(BlueprintCallable, Category = "Skill Instance|Parameters")
	void ClearParameterSnapshot();

	// 参数同步到StateInstance (同步所有参数或只同步快照参数)
	UFUNCTION(BlueprintCallable, Category = "Skill Instance|Parameters")
	void SyncParametersToStateInstance(UTcsStateInstance* StateInstance, bool bForceAll = false);

	// 刷新所有参数（重新计算非快照参数）
	UFUNCTION(BlueprintCallable, Category = "Skill Instance|Parameters")
	void RefreshAllParameters(AActor* Instigator = nullptr, AActor* Target = nullptr);

	// 实时同步参数到指定的StateInstance（只同步非快照参数）
	UFUNCTION(BlueprintCallable, Category = "Skill Instance|Parameters", Meta = (DisplayName = "Sync Realtime Parameters To State Instance"))
	void SyncRealtimeParametersToStateInstance(UTcsStateInstance* StateInstance, AActor* Instigator = nullptr, AActor* Target = nullptr);

	// 检查参数是否需要更新（性能优化）
	UFUNCTION(BlueprintPure, Category = "Skill Instance|Parameters")
	bool ShouldUpdateParameter(FName ParamName, AActor* Instigator = nullptr, AActor* Target = nullptr) const;

	// 批量检查是否有任何实时参数需要更新
	UFUNCTION(BlueprintPure, Category = "Skill Instance|Parameters")
	bool HasPendingParameterUpdates(AActor* Instigator = nullptr, AActor* Target = nullptr) const;

protected:
	// 布尔类型参数（直接存储）
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Skill Instance|Parameters")
	TMap<FName, bool> BoolParameters;

	// 向量类型参数（直接存储）
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Skill Instance|Parameters")
	TMap<FName, FVector> VectorParameters;

	// 数值参数配置（存储计算配置，不存储值）
	UPROPERTY(BlueprintReadOnly, Category = "Skill Instance|Parameters")
	TMap<FName, FTcsStateParameter> NumericParameterConfigs;

	// 快照参数缓存
	UPROPERTY(BlueprintReadOnly, Category = "Skill Instance|Parameters")
	TMap<FName, float> CachedSnapshotParameters;

	// 实时参数最近值缓存（用于变化检测）
	UPROPERTY(BlueprintReadOnly, Category = "Skill Instance|Parameters")
	TMap<FName, float> LastRealTimeParameterValues;

	// 参数更新时间戳（用于性能优化）
	UPROPERTY(BlueprintReadOnly, Category = "Skill Instance|Parameters")
	TMap<FName, float> ParameterUpdateTimestamps;

protected:
	// 内部参数计算辅助
	float CalculateBaseNumericParameter(FName ParamName, AActor* Instigator, AActor* Target) const;
	float ApplySkillModifiers(FName ParamName, float BaseValue) const;

#pragma endregion

	
#pragma region ParameterModifiers

public:
	UFUNCTION(BlueprintCallable, Category = "Skill Instance|Modifiers")
	void AddParameterModifier(FName ParamName, float ModifierValue, bool bIsMultiplier = false);

	UFUNCTION(BlueprintCallable, Category = "Skill Instance|Modifiers")
	void RemoveParameterModifier(FName ParamName, float ModifierValue, bool bIsMultiplier = false);

	UFUNCTION(BlueprintCallable, Category = "Skill Instance|Modifiers")
	void ClearParameterModifiers(FName ParamName);

	UFUNCTION(BlueprintPure, Category = "Skill Instance|Modifiers")
	float GetTotalParameterModifier(FName ParamName) const;

protected:
	// 参数修正器（加法修正）
	TMap<FName, TArray<float>> AdditiveModifiers;

	// 参数修正器（乘法修正）
	TMap<FName, TArray<float>> MultiplicativeModifiers;

#pragma endregion

	
#pragma region Events

public:
	// 技能实例事件
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillParameterChanged, FName, ParamName, float, NewValue);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillLevelChanged, int32, NewLevel);

	UPROPERTY(BlueprintAssignable, Category = "Skill Instance|Events")
	FOnSkillParameterChanged OnSkillParameterChanged;

	UPROPERTY(BlueprintAssignable, Category = "Skill Instance|Events")
	FOnSkillLevelChanged OnSkillLevelChanged;

protected:
	// 事件触发
	void BroadcastParameterChanged(FName ParamName, float NewValue);
	void BroadcastLevelChanged(int32 NewLevel);

#pragma endregion
};