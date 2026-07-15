// Copyright Tirefly. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"

#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Buff/TcsBuffDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillModifierDefinition.h"
#include "State/TcsStateInstance.h"
#include "State/TcsStateSlotDefinition.h"
#include "TcsDeveloperSettings.h"
#include "TcsGenericLibrary.h"
#include "UObject/UnrealType.h"



namespace
{
	/** 单个真实 DefinitionAsset 的异步加载观察结果。 */
	struct FTcsDefinitionAsyncLoadResult
	{
		/** 当前游戏世界中已初始化的 DefinitionManager。 */
		TObjectPtr<UTcsDefinitionManagerSubsystem> Manager;

		/** 当前请求的 Definition ID。 */
		FName DefId;

		/** 当前请求的 PrimaryAsset 类型。 */
		FPrimaryAssetType AssetType;

		/** 异步回调是否已执行。 */
		bool bCallbackCompleted = false;

		/** 异步回调报告的成功状态。 */
		bool bSuccess = false;

		/** 异步回调返回的 DefinitionAsset。 */
		TObjectPtr<UPrimaryDataAsset> Definition;
	};

	/**
	 * 获取当前游戏测试世界所属 GameInstance 中已初始化的 DefinitionManager。
	 *
	 * @return 可用于运行时加载测试的 DefinitionManager；未找到游戏世界或子系统时返回 nullptr。
	 */
	UTcsDefinitionManagerSubsystem* GetDefinitionManagerForAutomationTest()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			UWorld* World = WorldContext.World();
			if (!World || World->WorldType != EWorldType::Game)
			{
				continue;
			}

			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				return GameInstance->GetSubsystem<UTcsDefinitionManagerSubsystem>();
			}
		}

		return nullptr;
	}

	/** 多个异步请求共享的等待与断言状态。 */
	struct FTcsDefinitionAsyncLoadTestState
	{
		/** 当前 Automation Test，用于在 latent 命令中报告断言。 */
		FAutomationTestBase* Test = nullptr;

		/** 等待完成的异步加载结果。 */
		TArray<TSharedRef<FTcsDefinitionAsyncLoadResult>> Results;

		/** 启动加载后的超时绝对时间。 */
		double TimeoutAtSeconds = 0.0;
	};

	/**
	 * 返回对应测试 Definition 类型的 source cache。
	 *
	 * @param Manager 保存测试 source cache 的临时 DefinitionManager。
	 * @param AssetType 要访问的具体 Definition PrimaryAsset 类型。
	 * @return 对应具体 Definition 类型的可写 source cache。
	 */
	TMap<FName, FTcsDefinitionSourceEntry>& GetSourceCache(
		UTcsDefinitionManagerSubsystem& Manager,
		const FPrimaryAssetType& AssetType)
	{
		if (AssetType == UTcsAttributeDefinition::PrimaryAssetType)
		{
			return Manager.AttributeDefinitionSources;
		}

		if (AssetType == UTcsAttributeModifierDefinition::PrimaryAssetType)
		{
			return Manager.AttributeModifierDefinitionSources;
		}

		if (AssetType == UTcsBuffDefinition::PrimaryAssetType)
		{
			return Manager.BuffDefinitionSources;
		}

		return Manager.StateSlotDefinitionSources;
	}

	/**
	 * 检查异步加载是否把资产写入对应类型的 loaded cache。
	 *
	 * @param Manager 保存测试 loaded cache 的临时 DefinitionManager。
	 * @param AssetType 要检查的具体 Definition PrimaryAsset 类型。
	 * @param DefId 要检查的 Definition ID。
	 * @return 对应 typed loaded cache 已包含 DefId 时返回 true。
	 */
	bool IsWrittenToLoadedCache(
		const UTcsDefinitionManagerSubsystem& Manager,
		const FPrimaryAssetType& AssetType,
		const FName DefId)
	{
		if (AssetType == UTcsAttributeDefinition::PrimaryAssetType)
		{
			return Manager.AttributeDefinitions.Contains(DefId);
		}

		if (AssetType == UTcsAttributeModifierDefinition::PrimaryAssetType)
		{
			return Manager.AttributeModifierDefinitions.Contains(DefId);
		}

		if (AssetType == UTcsBuffDefinition::PrimaryAssetType)
		{
			return Manager.BuffDefinitions.Contains(DefId);
		}

		return Manager.StateSlotDefinitions.Contains(DefId);
	}

	/**
	 * 清除测试目标的 typed cache 条目，强制下一次请求经过单资产 async 加载分支。
	 *
	 * @param Manager 当前游戏世界中已初始化的 DefinitionManager。
	 * @param AssetType 要清除的具体 Definition PrimaryAsset 类型。
	 * @param DefId 要清除的 Definition ID。
	 */
	void RemoveFromLoadedCache(
		UTcsDefinitionManagerSubsystem& Manager,
		const FPrimaryAssetType& AssetType,
		const FName DefId)
	{
		if (AssetType == UTcsAttributeDefinition::PrimaryAssetType)
		{
			Manager.AttributeDefinitions.Remove(DefId);
			return;
		}

		if (AssetType == UTcsAttributeModifierDefinition::PrimaryAssetType)
		{
			Manager.AttributeModifierDefinitions.Remove(DefId);
			return;
		}

		if (AssetType == UTcsBuffDefinition::PrimaryAssetType)
		{
			Manager.BuffDefinitions.Remove(DefId);
			return;
		}

		Manager.StateSlotDefinitions.Remove(DefId);
	}

	/**
	 * 在临时 manager 上发起对应类型的单资产异步加载。
	 *
	 * @param Result 保存请求 Definition 类型、ID 与异步结果的测试状态。
	 * @param Callback 异步加载完成时执行的测试回调。
	 */
	void StartAsyncLoad(
		FTcsDefinitionAsyncLoadResult& Result,
		const FOnTcsDefinitionAsyncLoaded& Callback)
	{
		UTcsDefinitionManagerSubsystem& Manager = *Result.Manager;
		if (Result.AssetType == UTcsAttributeDefinition::PrimaryAssetType)
		{
			Manager.LoadAttributeDefinitionAsync(Result.DefId, Callback);
			return;
		}

		if (Result.AssetType == UTcsAttributeModifierDefinition::PrimaryAssetType)
		{
			Manager.LoadAttributeModifierDefinitionAsync(Result.DefId, Callback);
			return;
		}

		if (Result.AssetType == UTcsBuffDefinition::PrimaryAssetType)
		{
			Manager.LoadBuffDefinitionAsync(Result.DefId, Callback);
			return;
		}

		Manager.LoadStateSlotDefinitionAsync(Result.DefId, Callback);
	}
}



DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FWaitForTcsDefinitionAsyncLoads,
	TSharedRef<FTcsDefinitionAsyncLoadTestState>,
	TestState);

bool FWaitForTcsDefinitionAsyncLoads::Update()
{
	bool bAllCompleted = true;
	for (const TSharedRef<FTcsDefinitionAsyncLoadResult>& Result : TestState->Results)
	{
		bAllCompleted &= Result->bCallbackCompleted;
	}

	if (!bAllCompleted && FPlatformTime::Seconds() < TestState->TimeoutAtSeconds)
	{
		return false;
	}

	if (!bAllCompleted)
	{
		TestState->Test->AddError(TEXT("Timed out while waiting for registered DefinitionAsset async loads."));
		return true;
	}

	for (const TSharedRef<FTcsDefinitionAsyncLoadResult>& Result : TestState->Results)
	{
		if (!Result->Manager)
		{
			TestState->Test->AddError(TEXT("DefinitionManager was released before async load verification."));
			continue;
		}

		TestState->Test->TestTrue(
			FString::Printf(TEXT("Async load succeeds for %s"), *Result->AssetType.ToString()),
			Result->bSuccess);
		TestState->Test->TestNotNull(
			FString::Printf(TEXT("Async load returns an asset for %s"), *Result->AssetType.ToString()),
			Result->Definition.Get());
		TestState->Test->TestTrue(
			FString::Printf(TEXT("Async load writes %s to its typed cache"), *Result->AssetType.ToString()),
			IsWrittenToLoadedCache(*Result->Manager, Result->AssetType, Result->DefId));
	}

	return true;
}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTireflyCombatSystem_DefinitionLoading_ConfigurationSpec,
	"TireflyCombatSystem.DefinitionLoading.Configuration",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTireflyCombatSystem_DefinitionLoading_ConfigurationSpec::RunTest(const FString& Parameters)
{
	struct FTcsDefinitionTypeExpectation
	{
		FPrimaryAssetType AssetType;
		UClass* AssetClass;
		const TCHAR* Directory;
		bool bRequiresCommittedTestAsset;
	};

	const TArray<FTcsDefinitionTypeExpectation> Expectations = {
		{UTcsAttributeDefinition::PrimaryAssetType, UTcsAttributeDefinition::StaticClass(), TEXT("/Game/TCS_Test/Definition/Attribute"), true},
		{UTcsAttributeModifierDefinition::PrimaryAssetType, UTcsAttributeModifierDefinition::StaticClass(), TEXT("/Game/TCS_Test/Definition/AttributeModifier"), true},
		{UTcsBuffDefinition::PrimaryAssetType, UTcsBuffDefinition::StaticClass(), TEXT("/Game/TCS_Test/Definition/Buff"), true},
		{UTcsSkillDefinition::PrimaryAssetType, UTcsSkillDefinition::StaticClass(), TEXT("/Game/TCS_Test/Definition/Skill"), false},
		{UTcsSkillModifierDefinition::PrimaryAssetType, UTcsSkillModifierDefinition::StaticClass(), TEXT("/Game/TCS_Test/Definition/SkillModifier"), false},
		{UTcsStateSlotDefinition::PrimaryAssetType, UTcsStateSlotDefinition::StaticClass(), TEXT("/Game/TCS_Test/Definition/StateSlot"), true},
	};

	UAssetManager& AssetManager = UAssetManager::Get();
	for (const FTcsDefinitionTypeExpectation& Expectation : Expectations)
	{
		FPrimaryAssetTypeInfo TypeInfo;
		TestTrue(
			FString::Printf(TEXT("AssetManager registers %s"), *Expectation.AssetType.ToString()),
			AssetManager.GetPrimaryAssetTypeInfo(Expectation.AssetType, TypeInfo));
		TestEqual(
			FString::Printf(TEXT("%s has its concrete Definition class"), *Expectation.AssetType.ToString()),
			TypeInfo.AssetBaseClassLoaded.Get(),
			Expectation.AssetClass);
		TestFalse(
			FString::Printf(TEXT("%s does not scan Blueprint classes"), *Expectation.AssetType.ToString()),
			TypeInfo.bHasBlueprintClasses);
		TestFalse(
			FString::Printf(TEXT("%s is not editor-only"), *Expectation.AssetType.ToString()),
			TypeInfo.bIsEditorOnly);

		const bool bHasExpectedDirectory = TypeInfo.AssetScanPaths.ContainsByPredicate(
			[&Expectation](const FString& AssetScanPath)
			{
				return AssetScanPath == Expectation.Directory;
			});
		TestTrue(
			FString::Printf(TEXT("%s scans its dedicated concrete Definition directory"), *Expectation.AssetType.ToString()),
			bHasExpectedDirectory);

		TArray<FPrimaryAssetId> AssetIds;
		AssetManager.GetPrimaryAssetIdList(Expectation.AssetType, AssetIds);
		if (Expectation.bRequiresCommittedTestAsset)
		{
			TestTrue(
				FString::Printf(TEXT("%s discovers committed test assets"), *Expectation.AssetType.ToString()),
				!AssetIds.IsEmpty());
		}

		for (const FPrimaryAssetId& AssetId : AssetIds)
		{
			TestEqual(
				FString::Printf(TEXT("%s keeps the configured PrimaryAssetType"), *AssetId.ToString()),
				AssetId.PrimaryAssetType,
				Expectation.AssetType);
			TestFalse(
				FString::Printf(TEXT("%s exposes a non-empty DefId key"), *AssetId.ToString()),
				AssetId.PrimaryAssetName.IsNone());
			TestTrue(
				FString::Printf(TEXT("%s resolves to an AssetManager path"), *AssetId.ToString()),
				AssetManager.GetPrimaryAssetPath(AssetId).IsValid());
		}
	}

	const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
	TestNotNull(TEXT("Definition loading settings are available"), Settings);
	if (Settings)
	{
		const auto IsValidLoadingStrategy = [](const ETcsDefinitionLoadingStrategy Strategy)
		{
			return Strategy == ETcsDefinitionLoadingStrategy::PreloadAll ||
				Strategy == ETcsDefinitionLoadingStrategy::PreloadSelected ||
				Strategy == ETcsDefinitionLoadingStrategy::OnDemand;
		};

		TestTrue(TEXT("Attribute loading strategy is unified"), IsValidLoadingStrategy(Settings->AttributeDefinitionLoading.LoadingStrategy));
		TestTrue(TEXT("AttributeModifier loading strategy is unified"), IsValidLoadingStrategy(Settings->AttributeModifierDefinitionLoading.LoadingStrategy));
		TestTrue(TEXT("Buff loading strategy is unified"), IsValidLoadingStrategy(Settings->BuffDefinitionLoading.LoadingStrategy));
		TestTrue(TEXT("Skill loading strategy is unified"), IsValidLoadingStrategy(Settings->SkillDefinitionLoading.LoadingStrategy));
		TestTrue(TEXT("SkillModifier loading strategy is unified"), IsValidLoadingStrategy(Settings->SkillModifierDefinitionLoading.LoadingStrategy));
		TestTrue(TEXT("StateSlot loading strategy is unified"), IsValidLoadingStrategy(Settings->StateSlotDefinitionLoading.LoadingStrategy));
	}

	TestNull(
		TEXT("Abstract StateDefinition has no Blueprint query entry"),
		UTcsDefinitionManagerSubsystem::StaticClass()->FindFunctionByName(TEXT("GetStateDefinition")));
	TestNull(
		TEXT("State-like aggregate enumeration has no Blueprint query entry"),
		UTcsDefinitionManagerSubsystem::StaticClass()->FindFunctionByName(TEXT("GetAllStateLikeDefIds")));
	TestNull(
		TEXT("Runtime utility library has no weak StateDefinition enumeration entry"),
		UTcsGenericLibrary::StaticClass()->FindFunctionByName(TEXT("GetStateDefNames")));

	const FProperty* StateDefProperty = UTcsStateInstance::StaticClass()->FindPropertyByName(TEXT("StateDef"));
	TestNotNull(TEXT("StateInstance retains its internal StateDefinition GC reference"), StateDefProperty);
	if (StateDefProperty)
	{
		TestFalse(
			TEXT("StateInstance does not expose the abstract StateDefinition cache to Blueprint"),
			StateDefProperty->HasAnyPropertyFlags(CPF_BlueprintVisible));
	}

	return true;
}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTireflyCombatSystem_DefinitionLoading_RegisteredAssetAsyncSpec,
	"TireflyCombatSystem.DefinitionLoading.RegisteredAssetAsync",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTireflyCombatSystem_DefinitionLoading_RegisteredAssetAsyncSpec::RunTest(const FString& Parameters)
{
	UTcsDefinitionManagerSubsystem* Manager = GetDefinitionManagerForAutomationTest();
	if (!TestNotNull(TEXT("GameInstance DefinitionManager is available for async loading"), Manager))
	{
		return false;
	}

	const TArray<FPrimaryAssetType> AssetTypes = {
		UTcsAttributeDefinition::PrimaryAssetType,
		UTcsAttributeModifierDefinition::PrimaryAssetType,
		UTcsBuffDefinition::PrimaryAssetType,
		UTcsStateSlotDefinition::PrimaryAssetType,
	};

	TSharedRef<FTcsDefinitionAsyncLoadTestState> TestState = MakeShared<FTcsDefinitionAsyncLoadTestState>();
	TestState->Test = this;
	TestState->TimeoutAtSeconds = FPlatformTime::Seconds() + 30.0;

	UAssetManager& AssetManager = UAssetManager::Get();
	for (const FPrimaryAssetType& AssetType : AssetTypes)
	{
		TArray<FPrimaryAssetId> AssetIds;
		AssetManager.GetPrimaryAssetIdList(AssetType, AssetIds);
		if (!TestTrue(FString::Printf(TEXT("%s has an asset for async loading"), *AssetType.ToString()), !AssetIds.IsEmpty()))
		{
			continue;
		}

		const FPrimaryAssetId AssetId = AssetIds[0];
		const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(AssetId);
		if (!TestTrue(FString::Printf(TEXT("%s has a valid async load path"), *AssetId.ToString()), AssetPath.IsValid()))
		{
			continue;
		}

		TMap<FName, FTcsDefinitionSourceEntry>& SourceCache = GetSourceCache(*Manager, AssetType);
		if (!TestTrue(FString::Printf(TEXT("%s has a DefinitionManager source cache entry"), *AssetId.ToString()), SourceCache.Contains(AssetId.PrimaryAssetName)))
		{
			continue;
		}

		// 仅清除本次测试的 typed cache；source cache 保持由真实 DefMgr 初始化流程建立的状态。
		RemoveFromLoadedCache(*Manager, AssetType, AssetId.PrimaryAssetName);
		AssetManager.UnloadPrimaryAsset(AssetId);

		TSharedRef<FTcsDefinitionAsyncLoadResult> Result = MakeShared<FTcsDefinitionAsyncLoadResult>();
		Result->Manager = Manager;
		Result->DefId = AssetId.PrimaryAssetName;
		Result->AssetType = AssetType;

		StartAsyncLoad(*Result,
			FOnTcsDefinitionAsyncLoaded::CreateLambda(
				[Result](const FName CallbackDefId, const bool bSuccess, UPrimaryDataAsset* Definition)
				{
					Result->bCallbackCompleted = true;
					Result->bSuccess = bSuccess && CallbackDefId == Result->DefId;
					Result->Definition = Definition;
				}));
		TestState->Results.Add(Result);
	}

	if (TestState->Results.IsEmpty())
	{
		AddError(TEXT("No registered TCS DefinitionAsset was available for async loading validation."));
		return false;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForTcsDefinitionAsyncLoads(TestState));
	return true;
}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTireflyCombatSystem_DefinitionLoading_FailureContractSpec,
	"TireflyCombatSystem.DefinitionLoading.FailureContract",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FTireflyCombatSystem_DefinitionLoading_FailureContractSpec::RunTest(const FString& Parameters)
{
	UTcsDefinitionManagerSubsystem* Manager = GetDefinitionManagerForAutomationTest();
	if (!TestNotNull(TEXT("GameInstance DefinitionManager is available for failure contract"), Manager))
	{
		return false;
	}

	const FName MissingDefId(TEXT("Missing.Definition"));

	TestNull(TEXT("Missing BuffDefinition returns nullptr"), Manager->GetBuffDefinition(MissingDefId));
	TestNull(TEXT("Missing SkillDefinition returns nullptr"), Manager->GetSkillDefinition(MissingDefId));
	TestNull(TEXT("Missing StateSlotDefinition returns nullptr"), Manager->GetStateSlotDefinition(MissingDefId));
	TestNull(TEXT("Missing AttributeDefinition returns nullptr"), Manager->GetAttributeDefinition(MissingDefId));
	TestNull(TEXT("Missing AttributeModifierDefinition returns nullptr"), Manager->GetAttributeModifierDefinition(MissingDefId));
	TestNull(TEXT("Missing SkillModifierDefinition returns nullptr"), Manager->GetSkillModifierDefinition(MissingDefId));

	const auto VerifyMissingAsyncLoad = [this, Manager, MissingDefId](const TCHAR* EntryName, auto&& RequestAsyncLoad)
	{
		bool bCallbackCalled = false;
		bool bSuccess = true;
		FName CallbackDefId = NAME_None;
		UPrimaryDataAsset* Definition = nullptr;

		RequestAsyncLoad(FOnTcsDefinitionAsyncLoaded::CreateLambda(
			[&bCallbackCalled, &bSuccess, &CallbackDefId, &Definition](const FName InDefId, const bool bInSuccess, UPrimaryDataAsset* InDefinition)
			{
				bCallbackCalled = true;
				bSuccess = bInSuccess;
				CallbackDefId = InDefId;
				Definition = InDefinition;
			}));

		TestTrue(FString::Printf(TEXT("%s immediately reports an unregistered Definition"), EntryName), bCallbackCalled);
		TestFalse(FString::Printf(TEXT("%s does not report false success"), EntryName), bSuccess);
		TestEqual(FString::Printf(TEXT("%s preserves the requested DefId"), EntryName), CallbackDefId, MissingDefId);
		TestNull(FString::Printf(TEXT("%s returns no placeholder Definition"), EntryName), Definition);
	};

	VerifyMissingAsyncLoad(TEXT("LoadBuffDefinitionAsync"), [Manager, MissingDefId](const FOnTcsDefinitionAsyncLoaded& Callback) { Manager->LoadBuffDefinitionAsync(MissingDefId, Callback); });
	VerifyMissingAsyncLoad(TEXT("LoadSkillDefinitionAsync"), [Manager, MissingDefId](const FOnTcsDefinitionAsyncLoaded& Callback) { Manager->LoadSkillDefinitionAsync(MissingDefId, Callback); });
	VerifyMissingAsyncLoad(TEXT("LoadStateSlotDefinitionAsync"), [Manager, MissingDefId](const FOnTcsDefinitionAsyncLoaded& Callback) { Manager->LoadStateSlotDefinitionAsync(MissingDefId, Callback); });
	VerifyMissingAsyncLoad(TEXT("LoadAttributeDefinitionAsync"), [Manager, MissingDefId](const FOnTcsDefinitionAsyncLoaded& Callback) { Manager->LoadAttributeDefinitionAsync(MissingDefId, Callback); });
	VerifyMissingAsyncLoad(TEXT("LoadAttributeModifierDefinitionAsync"), [Manager, MissingDefId](const FOnTcsDefinitionAsyncLoaded& Callback) { Manager->LoadAttributeModifierDefinitionAsync(MissingDefId, Callback); });
	VerifyMissingAsyncLoad(TEXT("LoadSkillModifierDefinitionAsync"), [Manager, MissingDefId](const FOnTcsDefinitionAsyncLoaded& Callback) { Manager->LoadSkillModifierDefinitionAsync(MissingDefId, Callback); });

	return true;
}

#endif
