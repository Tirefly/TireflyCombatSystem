// Copyright Tirefly. All Rights Reserved.

#include "DefinitionManager/BlueprintAsyncActions/TcsAsyncAction_LoadDefinitionsBatch.h"

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"



namespace
{
	/**
	 * 从 WorldContext 获取 DefinitionManagerSubsystem。
	 *
	 * @param WorldContext 蓝图调用上下文。
	 * @return DefinitionManagerSubsystem 指针；获取失败时返回 nullptr。
	 */
	UTcsDefinitionManagerSubsystem* ResolveDefinitionManagerForBatch(const UObject* WorldContext)
	{
		const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
		if (!World)
		{
			return nullptr;
		}

		UGameInstance* GameInst = World->GetGameInstance();
		if (!GameInst)
		{
			return nullptr;
		}

		return GameInst->GetSubsystem<UTcsDefinitionManagerSubsystem>();
	}

	/**
	 * 异步批量加载失败时的统一处理：广播空结果并销毁 Action。
	 */
	template <typename ActionClass>
	void BroadcastBatchFailureAndDestroy(ActionClass* Action, const TArray<FName>& DefIds)
	{
		TArray<UPrimaryDataAsset*> EmptyResult;
		Action->OnLoaded.Broadcast(DefIds, EmptyResult);
		Action->SetReadyToDestroy();
	}

	/**
	 * 发起批量异步加载并在回调中广播。
	 */
	template <typename ActionClass>
	void StartBatchDefinitionAsyncLoad(
		ActionClass* Action,
		const TArray<FName>& DefIds,
		UTcsDefinitionManagerSubsystem* DefinitionManager,
		TFunctionRef<void(UTcsDefinitionManagerSubsystem*, const TArray<FName>&, const FOnTcsDefinitionsBatchLoaded&)> InvokeLoad)
	{
		TWeakObjectPtr<ActionClass> WeakAction(Action);

		InvokeLoad(DefinitionManager, DefIds,
			FOnTcsDefinitionsBatchLoaded::CreateLambda(
				[WeakAction](const TArray<FName>& RequestedDefIds, const TArray<UPrimaryDataAsset*>& LoadedAssets)
				{
					if (ActionClass* StrongAction = WeakAction.Get())
					{
						StrongAction->OnLoaded.Broadcast(RequestedDefIds, LoadedAssets);
						StrongAction->SetReadyToDestroy();
					}
				}));
	}
}



UTcsAsyncAction_LoadBuffDefinitionsBatch* UTcsAsyncAction_LoadBuffDefinitionsBatch::AsyncLoadBuffDefinitionsBatch(
	const UObject* WorldContext, const TArray<FName>& BuffDefIds)
{
	UTcsAsyncAction_LoadBuffDefinitionsBatch* Action = NewObject<UTcsAsyncAction_LoadBuffDefinitionsBatch>();
	Action->TargetDefIds = BuffDefIds;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UTcsAsyncAction_LoadBuffDefinitionsBatch::Activate()
{
	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManagerForBatch(this);
	if (!DefinitionManager)
	{
		BroadcastBatchFailureAndDestroy(this, TargetDefIds);
		return;
	}

	StartBatchDefinitionAsyncLoad(this, TargetDefIds, DefinitionManager,
		[](UTcsDefinitionManagerSubsystem* DefMgr, const TArray<FName>& DefIds, const FOnTcsDefinitionsBatchLoaded& Callback)
		{
			DefMgr->LoadBuffDefinitionsBatch(DefIds, Callback);
		});
}

UTcsAsyncAction_LoadSkillDefinitionsBatch* UTcsAsyncAction_LoadSkillDefinitionsBatch::AsyncLoadSkillDefinitionsBatch(
	const UObject* WorldContext, const TArray<FName>& SkillDefIds)
{
	UTcsAsyncAction_LoadSkillDefinitionsBatch* Action = NewObject<UTcsAsyncAction_LoadSkillDefinitionsBatch>();
	Action->TargetDefIds = SkillDefIds;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UTcsAsyncAction_LoadSkillDefinitionsBatch::Activate()
{
	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManagerForBatch(this);
	if (!DefinitionManager)
	{
		BroadcastBatchFailureAndDestroy(this, TargetDefIds);
		return;
	}

	StartBatchDefinitionAsyncLoad(this, TargetDefIds, DefinitionManager,
		[](UTcsDefinitionManagerSubsystem* DefMgr, const TArray<FName>& DefIds, const FOnTcsDefinitionsBatchLoaded& Callback)
		{
			DefMgr->LoadSkillDefinitionsBatch(DefIds, Callback);
		});
}

UTcsAsyncAction_LoadStateSlotDefinitionsBatch* UTcsAsyncAction_LoadStateSlotDefinitionsBatch::AsyncLoadStateSlotDefinitionsBatch(
	const UObject* WorldContext, const TArray<FName>& StateSlotDefIds)
{
	UTcsAsyncAction_LoadStateSlotDefinitionsBatch* Action = NewObject<UTcsAsyncAction_LoadStateSlotDefinitionsBatch>();
	Action->TargetDefIds = StateSlotDefIds;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UTcsAsyncAction_LoadStateSlotDefinitionsBatch::Activate()
{
	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManagerForBatch(this);
	if (!DefinitionManager)
	{
		BroadcastBatchFailureAndDestroy(this, TargetDefIds);
		return;
	}

	StartBatchDefinitionAsyncLoad(this, TargetDefIds, DefinitionManager,
		[](UTcsDefinitionManagerSubsystem* DefMgr, const TArray<FName>& DefIds, const FOnTcsDefinitionsBatchLoaded& Callback)
		{
			DefMgr->LoadStateSlotDefinitionsBatch(DefIds, Callback);
		});
}

UTcsAsyncAction_LoadAttributeDefinitionsBatch* UTcsAsyncAction_LoadAttributeDefinitionsBatch::AsyncLoadAttributeDefinitionsBatch(
	const UObject* WorldContext, const TArray<FName>& AttributeDefIds)
{
	UTcsAsyncAction_LoadAttributeDefinitionsBatch* Action = NewObject<UTcsAsyncAction_LoadAttributeDefinitionsBatch>();
	Action->TargetDefIds = AttributeDefIds;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UTcsAsyncAction_LoadAttributeDefinitionsBatch::Activate()
{
	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManagerForBatch(this);
	if (!DefinitionManager)
	{
		BroadcastBatchFailureAndDestroy(this, TargetDefIds);
		return;
	}

	StartBatchDefinitionAsyncLoad(this, TargetDefIds, DefinitionManager,
		[](UTcsDefinitionManagerSubsystem* DefMgr, const TArray<FName>& DefIds, const FOnTcsDefinitionsBatchLoaded& Callback)
		{
			DefMgr->LoadAttributeDefinitionsBatch(DefIds, Callback);
		});
}

UTcsAsyncAction_LoadAttributeModifierDefinitionsBatch* UTcsAsyncAction_LoadAttributeModifierDefinitionsBatch::AsyncLoadAttributeModifierDefinitionsBatch(
	const UObject* WorldContext, const TArray<FName>& AttributeModifierDefIds)
{
	UTcsAsyncAction_LoadAttributeModifierDefinitionsBatch* Action = NewObject<UTcsAsyncAction_LoadAttributeModifierDefinitionsBatch>();
	Action->TargetDefIds = AttributeModifierDefIds;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UTcsAsyncAction_LoadAttributeModifierDefinitionsBatch::Activate()
{
	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManagerForBatch(this);
	if (!DefinitionManager)
	{
		BroadcastBatchFailureAndDestroy(this, TargetDefIds);
		return;
	}

	StartBatchDefinitionAsyncLoad(this, TargetDefIds, DefinitionManager,
		[](UTcsDefinitionManagerSubsystem* DefMgr, const TArray<FName>& DefIds, const FOnTcsDefinitionsBatchLoaded& Callback)
		{
			DefMgr->LoadAttributeModifierDefinitionsBatch(DefIds, Callback);
		});
}

UTcsAsyncAction_LoadSkillModifierDefinitionsBatch* UTcsAsyncAction_LoadSkillModifierDefinitionsBatch::AsyncLoadSkillModifierDefinitionsBatch(
	const UObject* WorldContext, const TArray<FName>& SkillModifierDefIds)
{
	UTcsAsyncAction_LoadSkillModifierDefinitionsBatch* Action = NewObject<UTcsAsyncAction_LoadSkillModifierDefinitionsBatch>();
	Action->TargetDefIds = SkillModifierDefIds;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UTcsAsyncAction_LoadSkillModifierDefinitionsBatch::Activate()
{
	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManagerForBatch(this);
	if (!DefinitionManager)
	{
		BroadcastBatchFailureAndDestroy(this, TargetDefIds);
		return;
	}

	StartBatchDefinitionAsyncLoad(this, TargetDefIds, DefinitionManager,
		[](UTcsDefinitionManagerSubsystem* DefMgr, const TArray<FName>& DefIds, const FOnTcsDefinitionsBatchLoaded& Callback)
		{
			DefMgr->LoadSkillModifierDefinitionsBatch(DefIds, Callback);
		});
}
