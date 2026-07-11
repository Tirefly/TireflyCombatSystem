// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "TcsAsyncAction_LoadDefinitionsBatch.generated.h"



// 前向声明
class UTcsDefinitionManagerSubsystem;



/**
 * 批量异步加载全部完成回调委托（蓝图侧）。
 *
 * @param RequestedDefIds 本次请求加载的全部 Definition ID。
 * @param LoadedDefinitions 成功加载的 Definition 指针列表。
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnTcsDefinitionsBatchLoadedDynamic, const TArray<FName>&, RequestedDefIds,
	const TArray<UPrimaryDataAsset*>&, LoadedDefinitions);



/**
 * 蓝图批量异步加载 BuffDefinition 的 Action 节点。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAsyncAction_LoadBuffDefinitionsBatch : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

// BlueprintAsyncAction
#pragma region BlueprintAsyncAction

public:
	/**
	 * 批量异步加载多个 BuffDefinition。
	 *
	 * @param WorldContext 蓝图调用上下文（自动隐藏）。
	 * @param BuffDefIds 要加载的 Buff 定义 ID 列表。
	 * @return 异步 Action 对象；蓝图侧表现为 latent 节点。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition",
		meta = (BlueprintInternalUseOnly = "true"))
	static UTcsAsyncAction_LoadBuffDefinitionsBatch* AsyncLoadBuffDefinitionsBatch(
		const UObject* WorldContext, const TArray<FName>& BuffDefIds);

	/** 批量加载完成委托。 */
	UPROPERTY(BlueprintAssignable, Category = "TireflyCombatSystem|Definition")
	FOnTcsDefinitionsBatchLoadedDynamic OnLoaded;

	/** 触发异步加载。 */
	virtual void Activate() override;

private:
	/** 要加载的 Definition ID 列表。 */
	TArray<FName> TargetDefIds;

#pragma endregion
};



/**
 * 蓝图批量异步加载 SkillDefinition 的 Action 节点。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAsyncAction_LoadSkillDefinitionsBatch : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

// BlueprintAsyncAction
#pragma region BlueprintAsyncAction

public:
	/**
	 * 批量异步加载多个 SkillDefinition。
	 *
	 * @param WorldContext 蓝图调用上下文（自动隐藏）。
	 * @param SkillDefIds 要加载的 Skill 定义 ID 列表。
	 * @return 异步 Action 对象；蓝图侧表现为 latent 节点。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition",
		meta = (BlueprintInternalUseOnly = "true"))
	static UTcsAsyncAction_LoadSkillDefinitionsBatch* AsyncLoadSkillDefinitionsBatch(
		const UObject* WorldContext, const TArray<FName>& SkillDefIds);

	/** 批量加载完成委托。 */
	UPROPERTY(BlueprintAssignable, Category = "TireflyCombatSystem|Definition")
	FOnTcsDefinitionsBatchLoadedDynamic OnLoaded;

	/** 触发异步加载。 */
	virtual void Activate() override;

private:
	/** 要加载的 Definition ID 列表。 */
	TArray<FName> TargetDefIds;

#pragma endregion
};



/**
 * 蓝图批量异步加载 StateSlotDefinition 的 Action 节点。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAsyncAction_LoadStateSlotDefinitionsBatch : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

// BlueprintAsyncAction
#pragma region BlueprintAsyncAction

public:
	/**
	 * 批量异步加载多个 StateSlotDefinition。
	 *
	 * @param WorldContext 蓝图调用上下文（自动隐藏）。
	 * @param StateSlotDefIds 要加载的 StateSlot 定义 ID 列表。
	 * @return 异步 Action 对象；蓝图侧表现为 latent 节点。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition",
		meta = (BlueprintInternalUseOnly = "true"))
	static UTcsAsyncAction_LoadStateSlotDefinitionsBatch* AsyncLoadStateSlotDefinitionsBatch(
		const UObject* WorldContext, const TArray<FName>& StateSlotDefIds);

	/** 批量加载完成委托。 */
	UPROPERTY(BlueprintAssignable, Category = "TireflyCombatSystem|Definition")
	FOnTcsDefinitionsBatchLoadedDynamic OnLoaded;

	/** 触发异步加载。 */
	virtual void Activate() override;

private:
	/** 要加载的 Definition ID 列表。 */
	TArray<FName> TargetDefIds;

#pragma endregion
};



/**
 * 蓝图批量异步加载 AttributeDefinition 的 Action 节点。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAsyncAction_LoadAttributeDefinitionsBatch : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

// BlueprintAsyncAction
#pragma region BlueprintAsyncAction

public:
	/**
	 * 批量异步加载多个 AttributeDefinition。
	 *
	 * @param WorldContext 蓝图调用上下文（自动隐藏）。
	 * @param AttributeDefIds 要加载的 Attribute 定义 ID 列表。
	 * @return 异步 Action 对象；蓝图侧表现为 latent 节点。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition",
		meta = (BlueprintInternalUseOnly = "true"))
	static UTcsAsyncAction_LoadAttributeDefinitionsBatch* AsyncLoadAttributeDefinitionsBatch(
		const UObject* WorldContext, const TArray<FName>& AttributeDefIds);

	/** 批量加载完成委托。 */
	UPROPERTY(BlueprintAssignable, Category = "TireflyCombatSystem|Definition")
	FOnTcsDefinitionsBatchLoadedDynamic OnLoaded;

	/** 触发异步加载。 */
	virtual void Activate() override;

private:
	/** 要加载的 Definition ID 列表。 */
	TArray<FName> TargetDefIds;

#pragma endregion
};



/**
 * 蓝图批量异步加载 AttributeModifierDefinition 的 Action 节点。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAsyncAction_LoadAttributeModifierDefinitionsBatch : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

// BlueprintAsyncAction
#pragma region BlueprintAsyncAction

public:
	/**
	 * 批量异步加载多个 AttributeModifierDefinition。
	 *
	 * @param WorldContext 蓝图调用上下文（自动隐藏）。
	 * @param AttributeModifierDefIds 要加载的 AttributeModifier 定义 ID 列表。
	 * @return 异步 Action 对象；蓝图侧表现为 latent 节点。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition",
		meta = (BlueprintInternalUseOnly = "true"))
	static UTcsAsyncAction_LoadAttributeModifierDefinitionsBatch* AsyncLoadAttributeModifierDefinitionsBatch(
		const UObject* WorldContext, const TArray<FName>& AttributeModifierDefIds);

	/** 批量加载完成委托。 */
	UPROPERTY(BlueprintAssignable, Category = "TireflyCombatSystem|Definition")
	FOnTcsDefinitionsBatchLoadedDynamic OnLoaded;

	/** 触发异步加载。 */
	virtual void Activate() override;

private:
	/** 要加载的 Definition ID 列表。 */
	TArray<FName> TargetDefIds;

#pragma endregion
};



/**
 * 蓝图批量异步加载 SkillModifierDefinition 的 Action 节点。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAsyncAction_LoadSkillModifierDefinitionsBatch : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

// BlueprintAsyncAction
#pragma region BlueprintAsyncAction

public:
	/**
	 * 批量异步加载多个 SkillModifierDefinition。
	 *
	 * @param WorldContext 蓝图调用上下文（自动隐藏）。
	 * @param SkillModifierDefIds 要加载的 SkillModifier 定义 ID 列表。
	 * @return 异步 Action 对象；蓝图侧表现为 latent 节点。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition",
		meta = (BlueprintInternalUseOnly = "true"))
	static UTcsAsyncAction_LoadSkillModifierDefinitionsBatch* AsyncLoadSkillModifierDefinitionsBatch(
		const UObject* WorldContext, const TArray<FName>& SkillModifierDefIds);

	/** 批量加载完成委托。 */
	UPROPERTY(BlueprintAssignable, Category = "TireflyCombatSystem|Definition")
	FOnTcsDefinitionsBatchLoadedDynamic OnLoaded;

	/** 触发异步加载。 */
	virtual void Activate() override;

private:
	/** 要加载的 Definition ID 列表。 */
	TArray<FName> TargetDefIds;

#pragma endregion
};
