// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "TcsAsyncAction_LoadSingleDefinition.generated.h"



// 前向声明
class UTcsDefinitionManagerSubsystem;



/**
 * 蓝图侧单资产异步加载完成回调委托。
 *
 * @param DefId 加载的 Definition ID。
 * @param bSuccess 加载是否成功。
 * @param Definition 加载完成的 Definition 指针；失败时为 nullptr。
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnTcsDefinitionAsyncLoadedDynamic, FName, DefId, bool, bSuccess, UPrimaryDataAsset*, Definition);



/**
 * 蓝图异步加载 BuffDefinition 的 Action 节点。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAsyncAction_LoadBuffDefinition : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

// BlueprintAsyncAction
#pragma region BlueprintAsyncAction

public:
	/**
	 * 异步加载单个 BuffDefinition。
	 *
	 * @param WorldContext 蓝图调用上下文（自动隐藏）。
	 * @param BuffDefId 要加载的 Buff 定义 ID。
	 * @return 异步 Action 对象；蓝图侧表现为 latent 节点。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition",
		meta = (BlueprintInternalUseOnly = "true"))
	static UTcsAsyncAction_LoadBuffDefinition* AsyncLoadBuffDefinition(
		const UObject* WorldContext, FName BuffDefId);

	/** 加载完成委托。 */
	UPROPERTY(BlueprintAssignable, Category = "TireflyCombatSystem|Definition")
	FOnTcsDefinitionAsyncLoadedDynamic OnLoaded;

	/** 触发异步加载。 */
	virtual void Activate() override;

private:
	/** 要加载的 Definition ID。 */
	FName TargetDefId;

#pragma endregion
};



/**
 * 蓝图异步加载 SkillDefinition 的 Action 节点。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAsyncAction_LoadSkillDefinition : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

// BlueprintAsyncAction
#pragma region BlueprintAsyncAction

public:
	/**
	 * 异步加载单个 SkillDefinition。
	 *
	 * @param WorldContext 蓝图调用上下文（自动隐藏）。
	 * @param SkillDefId 要加载的 Skill 定义 ID。
	 * @return 异步 Action 对象；蓝图侧表现为 latent 节点。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition",
		meta = (BlueprintInternalUseOnly = "true"))
	static UTcsAsyncAction_LoadSkillDefinition* AsyncLoadSkillDefinition(
		const UObject* WorldContext, FName SkillDefId);

	/** 加载完成委托。 */
	UPROPERTY(BlueprintAssignable, Category = "TireflyCombatSystem|Definition")
	FOnTcsDefinitionAsyncLoadedDynamic OnLoaded;

	/** 触发异步加载。 */
	virtual void Activate() override;

private:
	/** 要加载的 Definition ID。 */
	FName TargetDefId;

#pragma endregion
};



/**
 * 蓝图异步加载 StateSlotDefinition 的 Action 节点。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAsyncAction_LoadStateSlotDefinition : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

// BlueprintAsyncAction
#pragma region BlueprintAsyncAction

public:
	/**
	 * 异步加载单个 StateSlotDefinition。
	 *
	 * @param WorldContext 蓝图调用上下文（自动隐藏）。
	 * @param StateSlotDefId 要加载的 StateSlot 定义 ID。
	 * @return 异步 Action 对象；蓝图侧表现为 latent 节点。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition",
		meta = (BlueprintInternalUseOnly = "true"))
	static UTcsAsyncAction_LoadStateSlotDefinition* AsyncLoadStateSlotDefinition(
		const UObject* WorldContext, FName StateSlotDefId);

	/** 加载完成委托。 */
	UPROPERTY(BlueprintAssignable, Category = "TireflyCombatSystem|Definition")
	FOnTcsDefinitionAsyncLoadedDynamic OnLoaded;

	/** 触发异步加载。 */
	virtual void Activate() override;

private:
	/** 要加载的 Definition ID。 */
	FName TargetDefId;

#pragma endregion
};



/**
 * 蓝图异步加载 AttributeDefinition 的 Action 节点。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAsyncAction_LoadAttributeDefinition : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

// BlueprintAsyncAction
#pragma region BlueprintAsyncAction

public:
	/**
	 * 异步加载单个 AttributeDefinition。
	 *
	 * @param WorldContext 蓝图调用上下文（自动隐藏）。
	 * @param AttributeDefId 要加载的 Attribute 定义 ID。
	 * @return 异步 Action 对象；蓝图侧表现为 latent 节点。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition",
		meta = (BlueprintInternalUseOnly = "true"))
	static UTcsAsyncAction_LoadAttributeDefinition* AsyncLoadAttributeDefinition(
		const UObject* WorldContext, FName AttributeDefId);

	/** 加载完成委托。 */
	UPROPERTY(BlueprintAssignable, Category = "TireflyCombatSystem|Definition")
	FOnTcsDefinitionAsyncLoadedDynamic OnLoaded;

	/** 触发异步加载。 */
	virtual void Activate() override;

private:
	/** 要加载的 Definition ID。 */
	FName TargetDefId;

#pragma endregion
};



/**
 * 蓝图异步加载 AttributeModifierDefinition 的 Action 节点。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAsyncAction_LoadAttributeModifierDefinition : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

// BlueprintAsyncAction
#pragma region BlueprintAsyncAction

public:
	/**
	 * 异步加载单个 AttributeModifierDefinition。
	 *
	 * @param WorldContext 蓝图调用上下文（自动隐藏）。
	 * @param AttributeModifierDefId 要加载的 AttributeModifier 定义 ID。
	 * @return 异步 Action 对象；蓝图侧表现为 latent 节点。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition",
		meta = (BlueprintInternalUseOnly = "true"))
	static UTcsAsyncAction_LoadAttributeModifierDefinition* AsyncLoadAttributeModifierDefinition(
		const UObject* WorldContext, FName AttributeModifierDefId);

	/** 加载完成委托。 */
	UPROPERTY(BlueprintAssignable, Category = "TireflyCombatSystem|Definition")
	FOnTcsDefinitionAsyncLoadedDynamic OnLoaded;

	/** 触发异步加载。 */
	virtual void Activate() override;

private:
	/** 要加载的 Definition ID。 */
	FName TargetDefId;

#pragma endregion
};



/**
 * 蓝图异步加载 SkillModifierDefinition 的 Action 节点。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAsyncAction_LoadSkillModifierDefinition : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

// BlueprintAsyncAction
#pragma region BlueprintAsyncAction

public:
	/**
	 * 异步加载单个 SkillModifierDefinition。
	 *
	 * @param WorldContext 蓝图调用上下文（自动隐藏）。
	 * @param SkillModifierDefId 要加载的 SkillModifier 定义 ID。
	 * @return 异步 Action 对象；蓝图侧表现为 latent 节点。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition",
		meta = (BlueprintInternalUseOnly = "true"))
	static UTcsAsyncAction_LoadSkillModifierDefinition* AsyncLoadSkillModifierDefinition(
		const UObject* WorldContext, FName SkillModifierDefId);

	/** 加载完成委托。 */
	UPROPERTY(BlueprintAssignable, Category = "TireflyCombatSystem|Definition")
	FOnTcsDefinitionAsyncLoadedDynamic OnLoaded;

	/** 触发异步加载。 */
	virtual void Activate() override;

private:
	/** 要加载的 Definition ID。 */
	FName TargetDefId;

#pragma endregion
};
