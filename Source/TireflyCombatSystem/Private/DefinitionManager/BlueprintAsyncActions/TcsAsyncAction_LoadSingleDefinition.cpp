// Copyright Tirefly. All Rights Reserved.

#include "DefinitionManager/BlueprintAsyncActions/TcsAsyncAction_LoadSingleDefinition.h"

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
	UTcsDefinitionManagerSubsystem* ResolveDefinitionManager(const UObject* WorldContext)
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
	 * 异步加载失败时的统一处理：广播失败并销毁 Action。
	 */
	template <typename ActionClass>
	void BroadcastFailureAndDestroy(ActionClass* Action, FName DefId)
	{
		Action->OnLoaded.Broadcast(DefId, false, nullptr);
		Action->SetReadyToDestroy();
	}

	/**
	 * 发起异步加载并在回调中广播。
	 */
	template <typename ActionClass>
	void StartDefinitionAsyncLoad(
		ActionClass* Action,
		FName DefId,
		UTcsDefinitionManagerSubsystem* DefinitionManager,
		TFunctionRef<void(UTcsDefinitionManagerSubsystem*, FName, const FOnTcsDefinitionAsyncLoaded&)> InvokeLoad)
	{
		TWeakObjectPtr<ActionClass> WeakAction(Action);

		InvokeLoad(DefinitionManager, DefId,
			FOnTcsDefinitionAsyncLoaded::CreateLambda(
				[WeakAction](FName LoadedDefId, bool bSuccess, UPrimaryDataAsset* Asset)
				{
					if (ActionClass* StrongAction = WeakAction.Get())
					{
						StrongAction->OnLoaded.Broadcast(LoadedDefId, bSuccess, Asset);
						StrongAction->SetReadyToDestroy();
					}
				}));
	}
}



UTcsAsyncAction_LoadBuffDefinition* UTcsAsyncAction_LoadBuffDefinition::AsyncLoadBuffDefinition(
	const UObject* WorldContext, FName BuffDefId)
{
	UTcsAsyncAction_LoadBuffDefinition* Action = NewObject<UTcsAsyncAction_LoadBuffDefinition>();
	Action->TargetDefId = BuffDefId;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UTcsAsyncAction_LoadBuffDefinition::Activate()
{
	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManager(this);
	if (!DefinitionManager)
	{
		BroadcastFailureAndDestroy(this, TargetDefId);
		return;
	}

	StartDefinitionAsyncLoad(this, TargetDefId, DefinitionManager,
		[](UTcsDefinitionManagerSubsystem* DefMgr, FName DefId, const FOnTcsDefinitionAsyncLoaded& Callback)
		{
			DefMgr->LoadBuffDefinitionAsync(DefId, Callback);
		});
}

UTcsAsyncAction_LoadSkillDefinition* UTcsAsyncAction_LoadSkillDefinition::AsyncLoadSkillDefinition(
	const UObject* WorldContext, FName SkillDefId)
{
	UTcsAsyncAction_LoadSkillDefinition* Action = NewObject<UTcsAsyncAction_LoadSkillDefinition>();
	Action->TargetDefId = SkillDefId;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UTcsAsyncAction_LoadSkillDefinition::Activate()
{
	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManager(this);
	if (!DefinitionManager)
	{
		BroadcastFailureAndDestroy(this, TargetDefId);
		return;
	}

	StartDefinitionAsyncLoad(this, TargetDefId, DefinitionManager,
		[](UTcsDefinitionManagerSubsystem* DefMgr, FName DefId, const FOnTcsDefinitionAsyncLoaded& Callback)
		{
			DefMgr->LoadSkillDefinitionAsync(DefId, Callback);
		});
}

UTcsAsyncAction_LoadStateSlotDefinition* UTcsAsyncAction_LoadStateSlotDefinition::AsyncLoadStateSlotDefinition(
	const UObject* WorldContext, FName StateSlotDefId)
{
	UTcsAsyncAction_LoadStateSlotDefinition* Action = NewObject<UTcsAsyncAction_LoadStateSlotDefinition>();
	Action->TargetDefId = StateSlotDefId;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UTcsAsyncAction_LoadStateSlotDefinition::Activate()
{
	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManager(this);
	if (!DefinitionManager)
	{
		BroadcastFailureAndDestroy(this, TargetDefId);
		return;
	}

	StartDefinitionAsyncLoad(this, TargetDefId, DefinitionManager,
		[](UTcsDefinitionManagerSubsystem* DefMgr, FName DefId, const FOnTcsDefinitionAsyncLoaded& Callback)
		{
			DefMgr->LoadStateSlotDefinitionAsync(DefId, Callback);
		});
}

UTcsAsyncAction_LoadAttributeDefinition* UTcsAsyncAction_LoadAttributeDefinition::AsyncLoadAttributeDefinition(
	const UObject* WorldContext, FName AttributeDefId)
{
	UTcsAsyncAction_LoadAttributeDefinition* Action = NewObject<UTcsAsyncAction_LoadAttributeDefinition>();
	Action->TargetDefId = AttributeDefId;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UTcsAsyncAction_LoadAttributeDefinition::Activate()
{
	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManager(this);
	if (!DefinitionManager)
	{
		BroadcastFailureAndDestroy(this, TargetDefId);
		return;
	}

	StartDefinitionAsyncLoad(this, TargetDefId, DefinitionManager,
		[](UTcsDefinitionManagerSubsystem* DefMgr, FName DefId, const FOnTcsDefinitionAsyncLoaded& Callback)
		{
			DefMgr->LoadAttributeDefinitionAsync(DefId, Callback);
		});
}

UTcsAsyncAction_LoadAttributeModifierDefinition* UTcsAsyncAction_LoadAttributeModifierDefinition::AsyncLoadAttributeModifierDefinition(
	const UObject* WorldContext, FName AttributeModifierDefId)
{
	UTcsAsyncAction_LoadAttributeModifierDefinition* Action = NewObject<UTcsAsyncAction_LoadAttributeModifierDefinition>();
	Action->TargetDefId = AttributeModifierDefId;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UTcsAsyncAction_LoadAttributeModifierDefinition::Activate()
{
	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManager(this);
	if (!DefinitionManager)
	{
		BroadcastFailureAndDestroy(this, TargetDefId);
		return;
	}

	StartDefinitionAsyncLoad(this, TargetDefId, DefinitionManager,
		[](UTcsDefinitionManagerSubsystem* DefMgr, FName DefId, const FOnTcsDefinitionAsyncLoaded& Callback)
		{
			DefMgr->LoadAttributeModifierDefinitionAsync(DefId, Callback);
		});
}

UTcsAsyncAction_LoadSkillModifierDefinition* UTcsAsyncAction_LoadSkillModifierDefinition::AsyncLoadSkillModifierDefinition(
	const UObject* WorldContext, FName SkillModifierDefId)
{
	UTcsAsyncAction_LoadSkillModifierDefinition* Action = NewObject<UTcsAsyncAction_LoadSkillModifierDefinition>();
	Action->TargetDefId = SkillModifierDefId;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UTcsAsyncAction_LoadSkillModifierDefinition::Activate()
{
	UTcsDefinitionManagerSubsystem* DefinitionManager = ResolveDefinitionManager(this);
	if (!DefinitionManager)
	{
		BroadcastFailureAndDestroy(this, TargetDefId);
		return;
	}

	StartDefinitionAsyncLoad(this, TargetDefId, DefinitionManager,
		[](UTcsDefinitionManagerSubsystem* DefMgr, FName DefId, const FOnTcsDefinitionAsyncLoaded& Callback)
		{
			DefMgr->LoadSkillModifierDefinitionAsync(DefId, Callback);
		});
}
