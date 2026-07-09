// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionRegistrySubsystem.h"

#include "TcsDeveloperSettings.h"
#include "TcsLogChannels.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Buff/TcsBuffDefinition.h"
#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillModifierDefinition.h"
#include "State/TcsStateSlotDefinition.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/Ticker.h"
#include "Engine/AssetManagerSettings.h"
#include "FileHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/UICommandList.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

struct FTcsSaveAllCommandHookState
{
	TSharedPtr<const FUICommandInfo> CommandInfo;
	FUIAction OriginalAction;
	bool bIsInstalled = false;
};

namespace TcsDefinitionRegistryPrivate
{
	constexpr TCHAR MainFrameCommandContextName[] = TEXT("MainFrame");
	constexpr TCHAR SaveAllCommandName[] = TEXT("SaveAll");
	constexpr TCHAR SaveAllCoverageLabel[] = TEXT("Save All");
	constexpr TCHAR ContentBrowserToolBarMenuName[] = TEXT("ContentBrowser.ToolBar");
	constexpr TCHAR ContentBrowserSaveSectionName[] = TEXT("Save");
	constexpr TCHAR ContentBrowserSaveButtonEntryName[] = TEXT("SaveButton");

	struct FTcsTrackedDefinitionType
	{
		UClass* AssetClass = nullptr;
		FName PrimaryAssetType;
		const TCHAR* DefinitionLabel = TEXT("Unknown");
	};

	UClass* ResolveConfiguredBaseClass(const FPrimaryAssetTypeInfo& TypeInfo);

	template <typename AssetType>
	void AddDefinition(
		TMap<FName, TSoftObjectPtr<AssetType>>& Cache,
		FName DefinitionId,
		const TSoftObjectPtr<AssetType>& AssetPtr,
		const TCHAR* DefinitionLabel)
	{
		if (DefinitionId.IsNone())
		{
			UE_LOG(LogTcs, Warning,
				TEXT("[UTcsDefinitionRegistrySubsystem] Skipping %s with empty DefId: %s"),
				DefinitionLabel,
				*AssetPtr.ToSoftObjectPath().ToString());
			return;
		}

		if (const TSoftObjectPtr<AssetType>* ExistingAsset = Cache.Find(DefinitionId))
		{
			if (ExistingAsset->ToSoftObjectPath() != AssetPtr.ToSoftObjectPath())
			{
				UE_LOG(LogTcs, Error,
					TEXT("[UTcsDefinitionRegistrySubsystem] Duplicate %s DefId '%s': '%s' conflicts with '%s'"),
					DefinitionLabel,
					*DefinitionId.ToString(),
					*ExistingAsset->ToSoftObjectPath().ToString(),
					*AssetPtr.ToSoftObjectPath().ToString());
			}
			return;
		}

		Cache.Add(DefinitionId, AssetPtr);
	}

	const TArray<FTcsTrackedDefinitionType>& GetTrackedDefinitionTypes()
	{
		static const TArray<FTcsTrackedDefinitionType> TrackedTypes = {
			{ UTcsAttributeDefinition::StaticClass(), UTcsAttributeDefinition::PrimaryAssetType, TEXT("UTcsAttributeDefinition") },
			{ UTcsAttributeModifierDefinition::StaticClass(), UTcsAttributeModifierDefinition::PrimaryAssetType, TEXT("UTcsAttributeModifierDefinition") },
			{ UTcsBuffDefinition::StaticClass(), UTcsBuffDefinition::PrimaryAssetType, TEXT("UTcsBuffDefinition") },
			{ UTcsSkillDefinition::StaticClass(), UTcsSkillDefinition::PrimaryAssetType, TEXT("UTcsSkillDefinition") },
			{ UTcsStateSlotDefinition::StaticClass(), UTcsStateSlotDefinition::PrimaryAssetType, TEXT("UTcsStateSlotDefinition") },
			{ UTcsSkillModifierDefinition::StaticClass(), UTcsSkillModifierDefinition::PrimaryAssetType, TEXT("UTcsSkillModifierDefinition") },
		};

		return TrackedTypes;
	}

	FString NormalizePackagePath(const FString& InPath)
	{
		FString Normalized = InPath;
		Normalized.TrimStartAndEndInline();
		while (Normalized.EndsWith(TEXT("/")))
		{
			Normalized.LeftChopInline(1, EAllowShrinking::No);
		}

		return Normalized;
	}

	bool IsPackagePathCoveredByDirectory(const FString& PackagePath, const FString& DirectoryPath)
	{
		if (DirectoryPath.IsEmpty())
		{
			return false;
		}

		if (PackagePath == DirectoryPath)
		{
			return true;
		}

		return PackagePath.StartsWith(DirectoryPath + TEXT("/"));
	}

	bool IsAssetCoveredByDirectories(const FAssetData& AssetData, const TArray<FDirectoryPath>& Directories)
	{
		const FString PackagePath = NormalizePackagePath(AssetData.PackagePath.ToString());
		for (const FDirectoryPath& Directory : Directories)
		{
			if (IsPackagePathCoveredByDirectory(PackagePath, NormalizePackagePath(Directory.Path)))
			{
				return true;
			}
		}

		return false;
	}

	bool IsAssetCoveredBySpecificAssets(const FAssetData& AssetData, const TArray<FSoftObjectPath>& SpecificAssets)
	{
		const FSoftObjectPath AssetPath = AssetData.ToSoftObjectPath();
		for (const FSoftObjectPath& SpecificAsset : SpecificAssets)
		{
			if (SpecificAsset == AssetPath)
			{
				return true;
			}
		}

		return false;
	}

	FString JoinSortedValues(const TSet<FString>& Values)
	{
		TArray<FString> SortedValues;
		SortedValues.Reserve(Values.Num());
		for (const FString& Value : Values)
		{
			SortedValues.Add(Value);
		}

		SortedValues.Sort();
		return SortedValues.Num() > 0 ? FString::Join(SortedValues, TEXT(", ")) : TEXT("<none>");
	}

	FString DescribeConfiguredDirectories(const TArray<const FPrimaryAssetTypeInfo*>& MatchingTypeInfos)
	{
		TSet<FString> Paths;
		for (const FPrimaryAssetTypeInfo* TypeInfo : MatchingTypeInfos)
		{
			if (!TypeInfo)
			{
				continue;
			}

			for (const FDirectoryPath& Directory : TypeInfo->GetDirectories())
			{
				Paths.Add(NormalizePackagePath(Directory.Path));
			}
		}

		return JoinSortedValues(Paths);
	}

	FString DescribeConfiguredBaseClasses(const TArray<const FPrimaryAssetTypeInfo*>& MatchingTypeInfos)
	{
		TSet<FString> BaseClasses;
		for (const FPrimaryAssetTypeInfo* TypeInfo : MatchingTypeInfos)
		{
			if (!TypeInfo)
			{
				continue;
			}

			if (UClass* ResolvedBaseClass = ResolveConfiguredBaseClass(*TypeInfo))
			{
				BaseClasses.Add(ResolvedBaseClass->GetPathName());
			}
			else
			{
				BaseClasses.Add(TypeInfo->GetAssetBaseClass().ToString());
			}
		}

		return JoinSortedValues(BaseClasses);
	}

	bool HasAnyScanLocation(const TArray<const FPrimaryAssetTypeInfo*>& MatchingTypeInfos)
	{
		for (const FPrimaryAssetTypeInfo* TypeInfo : MatchingTypeInfos)
		{
			if (!TypeInfo)
			{
				continue;
			}

			if (!TypeInfo->GetDirectories().IsEmpty() || !TypeInfo->GetSpecificAssets().IsEmpty())
			{
				return true;
			}
		}

		return false;
	}

	UClass* ResolveConfiguredBaseClass(const FPrimaryAssetTypeInfo& TypeInfo)
	{
		const TSoftClassPtr<UObject>& AssetBaseClass = TypeInfo.GetAssetBaseClass();
		if (!AssetBaseClass.IsNull())
		{
			if (UClass* LoadedClass = AssetBaseClass.Get())
			{
				return LoadedClass;
			}

			if (UClass* SynchronousClass = AssetBaseClass.LoadSynchronous())
			{
				return SynchronousClass;
			}
		}

		return TypeInfo.AssetBaseClassLoaded;
	}

	FText BuildCoverageIssueNotificationText(const bool bTriggeredBySave, const FString& PackageFileName, const TArray<FString>& Issues)
	{
		check(!Issues.IsEmpty());

		FString Summary;
		if (bTriggeredBySave)
		{
			const FString SavedLabel = PackageFileName.IsEmpty()
				? TEXT("本次保存")
				: FString::Printf(TEXT("保存 %s"), *FPaths::GetCleanFilename(PackageFileName));
			Summary = FString::Printf(
				TEXT("%s 后仍检测到 %d 项 TCS AssetManager 覆盖勘误。"),
				*SavedLabel,
				Issues.Num());
		}
		else
		{
			Summary = FString::Printf(
				TEXT("检测到 %d 项 TCS AssetManager 覆盖勘误。"),
				Issues.Num());
		}

		Summary += FString::Printf(TEXT("\n%s"), *Issues[0]);
		if (Issues.Num() > 1)
		{
			Summary += FString::Printf(TEXT("\n另有 %d 项，详见 Output Log。"), Issues.Num() - 1);
		}
		else
		{
			Summary += TEXT("\n详见 Output Log。");
		}

		return FText::FromString(Summary);
	}

	void ShowCoverageIssueNotification(const bool bTriggeredBySave, const FString& PackageFileName, const TArray<FString>& Issues)
	{
		static TWeakPtr<SNotificationItem> ActiveCoverageNotification;

		if (const TSharedPtr<SNotificationItem> ExistingNotification = ActiveCoverageNotification.Pin())
		{
			ExistingNotification->ExpireAndFadeout();
		}

		FNotificationInfo NotificationInfo(BuildCoverageIssueNotificationText(bTriggeredBySave, PackageFileName, Issues));
		NotificationInfo.bFireAndForget = true;
		NotificationInfo.bUseLargeFont = false;
		NotificationInfo.bUseSuccessFailIcons = true;
		NotificationInfo.FadeOutDuration = 0.2f;
		NotificationInfo.ExpireDuration = bTriggeredBySave ? 6.0f : 5.0f;
		NotificationInfo.WidthOverride = FOptionalSize(720.0f);

		const TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(NotificationInfo);
		if (NotificationItem.IsValid())
		{
			NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
			ActiveCoverageNotification = NotificationItem;
		}
	}

	FString BuildCoverageIssueSaveLabel(const int32 SaveCount, const FString& FirstPackageFileName, const bool bTriggeredBySaveAll)
	{
		if (bTriggeredBySaveAll)
		{
			return SaveAllCoverageLabel;
		}

		if (SaveCount > 1)
		{
			return FString::Printf(TEXT("Save All（共 %d 个包）"), SaveCount);
		}

		if (!FirstPackageFileName.IsEmpty())
		{
			return FirstPackageFileName;
		}

		return FString();
	}
}

void UTcsDefinitionRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if WITH_EDITOR
	if (GIsEditor)
	{
		RegisterEditorCallbacks();
		RegisterContentBrowserSaveButtonHook();
		RegisterSaveAllCommandHook();
		RefreshDefinitionsNow();
		ReportAssetManagerCoverageIssues(false);
	}
#endif
}

void UTcsDefinitionRegistrySubsystem::Deinitialize()
{
#if WITH_EDITOR
	UnregisterContentBrowserSaveButtonHook();
	UnregisterSaveAllCommandHook();
	UnregisterEditorCallbacks();
	ClearQueuedRefresh();
#endif

	DefinitionsRefreshed.Clear();
	Super::Deinitialize();
}

#if WITH_EDITOR
void UTcsDefinitionRegistrySubsystem::RequestRefresh()
{
	if (!GIsEditor)
	{
		return;
	}

	if (bIsRefreshing)
	{
		bRefreshRequestedWhileRefreshing = true;
		return;
	}

	if (bIsRefreshQueued)
	{
		return;
	}

	bIsRefreshQueued = true;
	DeferredRefreshHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UTcsDefinitionRegistrySubsystem::HandleDeferredRefresh),
		0.0f);
}

void UTcsDefinitionRegistrySubsystem::RefreshDefinitionsNow()
{
	if (!GIsEditor)
	{
		return;
	}

	if (bIsRefreshing)
	{
		bRefreshRequestedWhileRefreshing = true;
		return;
	}

	ClearQueuedRefresh();

	TGuardValue<bool> RefreshGuard(bIsRefreshing, true);
	RebuildSnapshot();

	const bool bShouldReportCoverageIssues = bShouldReportCoverageIssuesAfterRefresh;
	bShouldReportCoverageIssuesAfterRefresh = false;
	if (bShouldReportCoverageIssues)
	{
		ReportAssetManagerCoverageIssues(false);
	}

	bHasCompletedInitialRefresh = true;
	++RefreshRevision;

	DefinitionsRefreshed.Broadcast(this);

	if (bRefreshRequestedWhileRefreshing)
	{
		bRefreshRequestedWhileRefreshing = false;
		RequestRefresh();
	}

	UE_LOG(LogTcs, Verbose,
		TEXT("[UTcsDefinitionRegistrySubsystem] Published refresh revision %d"),
		RefreshRevision);
}

void UTcsDefinitionRegistrySubsystem::RegisterEditorCallbacks()
{
	if (bHasRegisteredEditorCallbacks)
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	AssetAddedHandle = AssetRegistry.OnAssetAdded().AddUObject(this, &UTcsDefinitionRegistrySubsystem::OnAssetAdded);
	AssetUpdatedHandle = AssetRegistry.OnAssetUpdated().AddUObject(this, &UTcsDefinitionRegistrySubsystem::OnAssetUpdated);
	AssetRemovedHandle = AssetRegistry.OnAssetRemoved().AddUObject(this, &UTcsDefinitionRegistrySubsystem::OnAssetRemoved);
	AssetRenamedHandle = AssetRegistry.OnAssetRenamed().AddUObject(this, &UTcsDefinitionRegistrySubsystem::OnAssetRenamed);
	InMemoryAssetCreatedHandle = AssetRegistry.OnInMemoryAssetCreated().AddUObject(this, &UTcsDefinitionRegistrySubsystem::OnInMemoryAssetCreated);
	InMemoryAssetDeletedHandle = AssetRegistry.OnInMemoryAssetDeleted().AddUObject(this, &UTcsDefinitionRegistrySubsystem::OnInMemoryAssetDeleted);

	if (UAssetManagerSettings* AssetManagerSettings = GetMutableDefault<UAssetManagerSettings>())
	{
		AssetManagerSettingsChangedHandle = AssetManagerSettings->OnSettingChanged().AddUObject(
			this,
			&UTcsDefinitionRegistrySubsystem::OnAssetManagerSettingsChanged);
	}

	PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddUObject(
		this,
		&UTcsDefinitionRegistrySubsystem::OnPackageSaved);

	bHasRegisteredEditorCallbacks = true;
}

void UTcsDefinitionRegistrySubsystem::UnregisterEditorCallbacks()
{
	if (!bHasRegisteredEditorCallbacks)
	{
		return;
	}

	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		if (AssetAddedHandle.IsValid())
		{
			AssetRegistry.OnAssetAdded().Remove(AssetAddedHandle);
		}

		if (AssetUpdatedHandle.IsValid())
		{
			AssetRegistry.OnAssetUpdated().Remove(AssetUpdatedHandle);
		}

		if (AssetRemovedHandle.IsValid())
		{
			AssetRegistry.OnAssetRemoved().Remove(AssetRemovedHandle);
		}

		if (AssetRenamedHandle.IsValid())
		{
			AssetRegistry.OnAssetRenamed().Remove(AssetRenamedHandle);
		}

		if (InMemoryAssetCreatedHandle.IsValid())
		{
			AssetRegistry.OnInMemoryAssetCreated().Remove(InMemoryAssetCreatedHandle);
		}

		if (InMemoryAssetDeletedHandle.IsValid())
		{
			AssetRegistry.OnInMemoryAssetDeleted().Remove(InMemoryAssetDeletedHandle);
		}
	}

	if (UAssetManagerSettings* AssetManagerSettings = GetMutableDefault<UAssetManagerSettings>())
	{
		if (AssetManagerSettingsChangedHandle.IsValid())
		{
			AssetManagerSettings->OnSettingChanged().Remove(AssetManagerSettingsChangedHandle);
		}
	}

	if (PackageSavedHandle.IsValid())
	{
		UPackage::PackageSavedWithContextEvent.Remove(PackageSavedHandle);
	}

	AssetAddedHandle.Reset();
	AssetUpdatedHandle.Reset();
	AssetRemovedHandle.Reset();
	AssetRenamedHandle.Reset();
	InMemoryAssetCreatedHandle.Reset();
	InMemoryAssetDeletedHandle.Reset();
	AssetManagerSettingsChangedHandle.Reset();
	PackageSavedHandle.Reset();
	bHasRegisteredEditorCallbacks = false;
}

bool UTcsDefinitionRegistrySubsystem::HandleDeferredCoverageIssueNotification(float DeltaTime)
{
	DeferredCoverageIssueNotificationHandle.Reset();

	if (!bHasPendingCoverageIssueNotification)
	{
		return false;
	}

	bHasPendingCoverageIssueNotification = false;
	const FString SaveLabel = TcsDefinitionRegistryPrivate::BuildCoverageIssueSaveLabel(
		PendingCoverageIssueSaveCount,
		PendingCoverageIssueFirstPackageFileName,
		bPendingCoverageIssueTriggeredBySaveAll);
	PendingCoverageIssueSaveCount = 0;
	PendingCoverageIssueFirstPackageFileName.Reset();
	bPendingCoverageIssueTriggeredBySaveAll = false;

	ReportAssetManagerCoverageIssues(true, SaveLabel);
	return false;
}

void UTcsDefinitionRegistrySubsystem::RegisterContentBrowserSaveButtonHook()
{
	auto RegisterHook = [this]()
	{
		if (!UToolMenus::IsToolMenuUIEnabled())
		{
			return;
		}

		FToolMenuOwnerScoped OwnerScoped(this);
		UToolMenu* const ToolBarMenu = UToolMenus::Get()->ExtendMenu(TcsDefinitionRegistryPrivate::ContentBrowserToolBarMenuName);
		if (!ToolBarMenu)
		{
			return;
		}

		UToolMenus::Get()->RemoveEntry(
			TcsDefinitionRegistryPrivate::ContentBrowserToolBarMenuName,
			TcsDefinitionRegistryPrivate::ContentBrowserSaveSectionName,
			TcsDefinitionRegistryPrivate::ContentBrowserSaveButtonEntryName);

		FToolMenuSection& SaveSection = ToolBarMenu->FindOrAddSection(TcsDefinitionRegistryPrivate::ContentBrowserSaveSectionName);
		SaveSection.AddDynamicEntry(
			TcsDefinitionRegistryPrivate::ContentBrowserSaveButtonEntryName,
			FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				if (!GEngine)
				{
					return;
				}

				const TSharedRef<SWidget> SaveButton =
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(FText::FromString(TEXT("Save all modified assets.")))
					.ContentPadding(2.f)
					.OnClicked_Lambda([]()
					{
						if (UTcsDefinitionRegistrySubsystem* const Registry = GEngine ? GEngine->GetEngineSubsystem<UTcsDefinitionRegistrySubsystem>() : nullptr)
						{
							Registry->HandleContentBrowserSaveButton();
						}
						return FReply::Handled();
					})
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("MainFrame.SaveAll"))
							.ColorAndOpacity(FSlateColor::UseForeground())
						]
						+ SHorizontalBox::Slot()
						.Padding(FMargin(3, 0, 0, 0))
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(STextBlock)
							.TextStyle(FAppStyle::Get(), "NormalText")
							.Text(FText::FromString(TEXT("Save All")))
						]
					];

				InSection.AddEntry(
					FToolMenuEntry::InitWidget(
						TcsDefinitionRegistryPrivate::ContentBrowserSaveButtonEntryName,
						SaveButton,
						FText::GetEmpty(),
						true,
						false));
			}));

		UToolMenus::Get()->RefreshMenuWidget(TcsDefinitionRegistryPrivate::ContentBrowserToolBarMenuName);
	};

	if (UToolMenus::TryGet() && UToolMenus::IsToolMenuUIEnabled())
	{
		RegisterHook();
	}
	else if (!ContentBrowserSaveButtonStartupCallbackHandle.IsValid())
	{
		ContentBrowserSaveButtonStartupCallbackHandle = UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateLambda(RegisterHook));
	}
}

void UTcsDefinitionRegistrySubsystem::UnregisterContentBrowserSaveButtonHook()
{
	if (ContentBrowserSaveButtonStartupCallbackHandle.IsValid())
	{
		UToolMenus::UnRegisterStartupCallback(ContentBrowserSaveButtonStartupCallbackHandle);
		ContentBrowserSaveButtonStartupCallbackHandle.Reset();
	}

	UToolMenus::UnregisterOwner(this);
	if (UToolMenus* const ToolMenus = UToolMenus::TryGet())
	{
		ToolMenus->RefreshMenuWidget(TcsDefinitionRegistryPrivate::ContentBrowserToolBarMenuName);
	}
}

void UTcsDefinitionRegistrySubsystem::RegisterSaveAllCommandHook()
{
	if (SaveAllCommandHookState.IsValid() && SaveAllCommandHookState->bIsInstalled)
	{
		return;
	}

	if (!FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	}

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	TSharedRef<FUICommandList>& MainFrameCommandBindings = MainFrameModule.GetMainFrameCommandBindings();
	const TSharedPtr<const FUICommandInfo> SaveAllCommandInfo = FInputBindingManager::Get().FindCommandInContext(
		TcsDefinitionRegistryPrivate::MainFrameCommandContextName,
		TcsDefinitionRegistryPrivate::SaveAllCommandName);
	if (!SaveAllCommandInfo.IsValid())
	{
		UE_LOG(LogTcs, Warning,
			TEXT("[UTcsDefinitionRegistrySubsystem] Failed to locate MainFrame.SaveAll command; Save All coverage hook is disabled."));
		return;
	}

	const FUIAction* ExistingAction = MainFrameCommandBindings->GetActionForCommand(SaveAllCommandInfo);
	if (!ExistingAction || !ExistingAction->IsBound())
	{
		UE_LOG(LogTcs, Warning,
			TEXT("[UTcsDefinitionRegistrySubsystem] Failed to capture existing Save All action; Save All coverage hook is disabled."));
		return;
	}

	if (!SaveAllCommandHookState.IsValid())
	{
		SaveAllCommandHookState = MakeShared<FTcsSaveAllCommandHookState>();
	}

	SaveAllCommandHookState->CommandInfo = SaveAllCommandInfo;
	SaveAllCommandHookState->OriginalAction = *ExistingAction;
	MainFrameCommandBindings->MapAction(
		SaveAllCommandInfo,
		FUIAction(
			FExecuteAction::CreateUObject(this, &UTcsDefinitionRegistrySubsystem::HandleSaveAllCommand),
			SaveAllCommandHookState->OriginalAction.CanExecuteAction,
			SaveAllCommandHookState->OriginalAction.GetActionCheckState,
			SaveAllCommandHookState->OriginalAction.IsActionVisibleDelegate,
			SaveAllCommandHookState->OriginalAction.RepeatMode));
	SaveAllCommandHookState->bIsInstalled = true;
}

void UTcsDefinitionRegistrySubsystem::UnregisterSaveAllCommandHook()
{
	if (!SaveAllCommandHookState.IsValid() || !SaveAllCommandHookState->bIsInstalled || !SaveAllCommandHookState->CommandInfo.IsValid())
	{
		return;
	}

	if (!FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		SaveAllCommandHookState.Reset();
		return;
	}

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	TSharedRef<FUICommandList>& MainFrameCommandBindings = MainFrameModule.GetMainFrameCommandBindings();
	MainFrameCommandBindings->MapAction(SaveAllCommandHookState->CommandInfo, SaveAllCommandHookState->OriginalAction);
	SaveAllCommandHookState.Reset();
}

void UTcsDefinitionRegistrySubsystem::HandleSaveAllCommand()
{
	if (!SaveAllCommandHookState.IsValid())
	{
		return;
	}

	PendingCoverageIssueSaveCount = 0;
	PendingCoverageIssueFirstPackageFileName.Reset();
	bPendingCoverageIssueTriggeredBySaveAll = false;

	TGuardValue<bool> SaveAllGuard(bIsExecutingSaveAllCommand, true);
	SaveAllCommandHookState->OriginalAction.Execute();

	if (bHasPendingCoverageIssueNotification)
	{
		return;
	}

	if (!FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	RefreshAssetManagerCoverageIssues(AssetRegistry);
	if (!AssetManagerCoverageIssues.IsEmpty())
	{
		ReportAssetManagerCoverageIssues(true, TcsDefinitionRegistryPrivate::SaveAllCoverageLabel);
	}
}

void UTcsDefinitionRegistrySubsystem::HandleContentBrowserSaveButton()
{
	PendingCoverageIssueSaveCount = 0;
	PendingCoverageIssueFirstPackageFileName.Reset();
	bPendingCoverageIssueTriggeredBySaveAll = false;

	TGuardValue<bool> SaveAllGuard(bIsExecutingSaveAllCommand, true);
	UEditorLoadingAndSavingUtils::SaveDirtyPackages(false, true);

	if (bHasPendingCoverageIssueNotification)
	{
		return;
	}

	if (!FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	RefreshAssetManagerCoverageIssues(AssetRegistry);
	if (!AssetManagerCoverageIssues.IsEmpty())
	{
		ReportAssetManagerCoverageIssues(true, TcsDefinitionRegistryPrivate::SaveAllCoverageLabel);
	}
}

bool UTcsDefinitionRegistrySubsystem::HandleDeferredRefresh(float DeltaTime)
{
	ClearQueuedRefresh();
	RefreshDefinitionsNow();
	return false;
}

void UTcsDefinitionRegistrySubsystem::RebuildSnapshot()
{
	AttributeDefinitions.Empty();
	AttributeModifierDefinitions.Empty();
	SkillModifierDefinitions.Empty();
	BuffDefinitions.Empty();
	SkillDefinitions.Empty();
	StateSlotDefinitions.Empty();

	const UAssetManagerSettings* AssetManagerSettings = GetDefault<UAssetManagerSettings>();
	if (!AssetManagerSettings)
	{
		UE_LOG(LogTcs, Error,
			TEXT("[UTcsDefinitionRegistrySubsystem] Failed to get AssetManagerSettings while rebuilding snapshot"));
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	for (const FPrimaryAssetTypeInfo& TypeInfo : AssetManagerSettings->PrimaryAssetTypesToScan)
	{
		ScanPrimaryAssetType(TypeInfo, AssetRegistry);
	}

	RefreshAssetManagerCoverageIssues(AssetRegistry);

	UE_LOG(LogTcs, Log,
		TEXT("[UTcsDefinitionRegistrySubsystem] Rebuilt snapshot: %d Attributes, %d AttributeModifiers, %d SkillModifiers, %d Buffs, %d Skills, %d StateSlots"),
		AttributeDefinitions.Num(),
		AttributeModifierDefinitions.Num(),
		SkillModifierDefinitions.Num(),
		BuffDefinitions.Num(),
		SkillDefinitions.Num(),
		StateSlotDefinitions.Num());
}

void UTcsDefinitionRegistrySubsystem::RefreshAssetManagerCoverageIssues(IAssetRegistry& AssetRegistry)
{
	AssetManagerCoverageIssues.Reset();

	const UAssetManagerSettings* AssetManagerSettings = GetDefault<UAssetManagerSettings>();
	if (!AssetManagerSettings)
	{
		AssetManagerCoverageIssues.Add(TEXT("无法读取 AssetManagerSettings，TCS DefinitionAsset 覆盖检查未执行。"));
		return;
	}

	for (const TcsDefinitionRegistryPrivate::FTcsTrackedDefinitionType& TrackedType : TcsDefinitionRegistryPrivate::GetTrackedDefinitionTypes())
	{
		if (!TrackedType.AssetClass || IsDefinitionAssetTypeIgnored(TrackedType.AssetClass))
		{
			continue;
		}

		TArray<const FPrimaryAssetTypeInfo*> MatchingTypeInfos;
		for (const FPrimaryAssetTypeInfo& TypeInfo : AssetManagerSettings->PrimaryAssetTypesToScan)
		{
			if (TypeInfo.PrimaryAssetType == TrackedType.PrimaryAssetType)
			{
				MatchingTypeInfos.Add(&TypeInfo);
			}
		}

		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssetsByClass(TrackedType.AssetClass->GetClassPathName(), AssetDataList, true);

		TSet<FString> ObservedPackagePaths;
		for (const FAssetData& AssetData : AssetDataList)
		{
			ObservedPackagePaths.Add(TcsDefinitionRegistryPrivate::NormalizePackagePath(AssetData.PackagePath.ToString()));
		}

		if (MatchingTypeInfos.IsEmpty())
		{
			AssetManagerCoverageIssues.Add(FString::Printf(
				TEXT("缺少 PrimaryAssetType '%s' 对 %s 的配置。当前发现的资产目录：%s。请在 Project Settings -> Asset Manager 中新增该类型并补齐扫描目录。"),
				*TrackedType.PrimaryAssetType.ToString(),
				TrackedType.DefinitionLabel,
				*TcsDefinitionRegistryPrivate::JoinSortedValues(ObservedPackagePaths)));
			continue;
		}

		const bool bHasExpectedBaseClass = MatchingTypeInfos.ContainsByPredicate(
			[&TrackedType](const FPrimaryAssetTypeInfo* TypeInfo)
			{
				const UClass* ConfiguredBaseClass = TypeInfo ? TcsDefinitionRegistryPrivate::ResolveConfiguredBaseClass(*TypeInfo) : nullptr;
				return ConfiguredBaseClass && ConfiguredBaseClass->IsChildOf(TrackedType.AssetClass);
			});

		if (!bHasExpectedBaseClass)
		{
			AssetManagerCoverageIssues.Add(FString::Printf(
				TEXT("PrimaryAssetType '%s' 对 %s 的规则失配：当前基类为 %s，期望基类为 %s。请修正 AssetBaseClass，避免定义资产被错误归类或漏扫。"),
				*TrackedType.PrimaryAssetType.ToString(),
				TrackedType.DefinitionLabel,
				*TcsDefinitionRegistryPrivate::DescribeConfiguredBaseClasses(MatchingTypeInfos),
				*TrackedType.AssetClass->GetPathName()));
		}

		if (AssetDataList.IsEmpty())
		{
			if (!TcsDefinitionRegistryPrivate::HasAnyScanLocation(MatchingTypeInfos))
			{
				AssetManagerCoverageIssues.Add(FString::Printf(
					TEXT("PrimaryAssetType '%s' 已存在，但 %s 没有配置任何扫描目录或 SpecificAssets。请至少补齐一个可用扫描路径。"),
					*TrackedType.PrimaryAssetType.ToString(),
					TrackedType.DefinitionLabel));
			}

			continue;
		}

		TSet<FString> MissingPackagePaths;
		for (const FAssetData& AssetData : AssetDataList)
		{
			bool bIsCovered = false;
			for (const FPrimaryAssetTypeInfo* TypeInfo : MatchingTypeInfos)
			{
				if (!TypeInfo)
				{
					continue;
				}

				if (TcsDefinitionRegistryPrivate::IsAssetCoveredByDirectories(AssetData, TypeInfo->GetDirectories()) ||
					TcsDefinitionRegistryPrivate::IsAssetCoveredBySpecificAssets(AssetData, TypeInfo->GetSpecificAssets()))
				{
					bIsCovered = true;
					break;
				}
			}

			if (!bIsCovered)
			{
				MissingPackagePaths.Add(TcsDefinitionRegistryPrivate::NormalizePackagePath(AssetData.PackagePath.ToString()));
			}
		}

		if (!MissingPackagePaths.IsEmpty())
		{
			AssetManagerCoverageIssues.Add(FString::Printf(
				TEXT("PrimaryAssetType '%s' 对 %s 的扫描路径漏配。未覆盖目录：%s。当前已配置目录：%s。请把缺失目录加入扫描路径，或显式加入 SpecificAssets。"),
				*TrackedType.PrimaryAssetType.ToString(),
				TrackedType.DefinitionLabel,
				*TcsDefinitionRegistryPrivate::JoinSortedValues(MissingPackagePaths),
				*TcsDefinitionRegistryPrivate::DescribeConfiguredDirectories(MatchingTypeInfos)));
		}
	}
}

void UTcsDefinitionRegistrySubsystem::ReportAssetManagerCoverageIssues(bool bTriggeredBySave, const FString& PackageFileName) const
{
	if (AssetManagerCoverageIssues.IsEmpty())
	{
		return;
	}

	TcsDefinitionRegistryPrivate::ShowCoverageIssueNotification(bTriggeredBySave, PackageFileName, AssetManagerCoverageIssues);

	if (bTriggeredBySave)
	{
		UE_LOG(LogTcs, Error,
			TEXT("[UTcsDefinitionRegistrySubsystem] TCS DefinitionAsset 的 AssetManagerSettings 勘误仍未修复；本次 Save 后再次提示。Package: %s"),
			PackageFileName.IsEmpty() ? TEXT("<unknown>") : *PackageFileName);
	}
	else
	{
		UE_LOG(LogTcs, Error,
			TEXT("[UTcsDefinitionRegistrySubsystem] 检测到 TCS DefinitionAsset 的 AssetManagerSettings 勘误，请按下列提示修正。"));
	}

	for (const FString& Issue : AssetManagerCoverageIssues)
	{
		UE_LOG(LogTcs, Error, TEXT("  - %s"), *Issue);
	}
}

bool UTcsDefinitionRegistrySubsystem::IsDefinitionAssetTypeIgnored(const UClass* DefinitionClass) const
{
	const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
	if (!Settings || !DefinitionClass)
	{
		return false;
	}

	for (const TSubclassOf<UPrimaryDataAsset>& IgnoredType : Settings->IgnoredDefinitionAssetTypes)
	{
		const UClass* IgnoredClass = IgnoredType.Get();
		if (IgnoredClass && (IgnoredClass == DefinitionClass || IgnoredClass->IsChildOf(DefinitionClass)))
		{
			return true;
		}
	}

	return false;
}

void UTcsDefinitionRegistrySubsystem::ScanPrimaryAssetType(const FPrimaryAssetTypeInfo& TypeInfo, IAssetRegistry& AssetRegistry)
{
	if (TypeInfo.PrimaryAssetType != UTcsAttributeDefinition::PrimaryAssetType &&
		TypeInfo.PrimaryAssetType != UTcsAttributeModifierDefinition::PrimaryAssetType &&
		TypeInfo.PrimaryAssetType != UTcsBuffDefinition::PrimaryAssetType &&
		TypeInfo.PrimaryAssetType != UTcsSkillDefinition::PrimaryAssetType &&
		TypeInfo.PrimaryAssetType != UTcsStateSlotDefinition::PrimaryAssetType &&
		TypeInfo.PrimaryAssetType != UTcsSkillModifierDefinition::PrimaryAssetType)
	{
		return;
	}

	const TArray<FDirectoryPath> Directories = TypeInfo.GetDirectories();
	for (const FDirectoryPath& Directory : Directories)
	{
		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssetsByPath(FName(*Directory.Path), AssetDataList, true);

		if (TypeInfo.PrimaryAssetType == UTcsAttributeDefinition::PrimaryAssetType)
		{
			ScanAttributeDefinitions(AssetDataList);
		}
		else if (TypeInfo.PrimaryAssetType == UTcsAttributeModifierDefinition::PrimaryAssetType)
		{
			ScanAttributeModifierDefinitions(AssetDataList);
		}
		else if (TypeInfo.PrimaryAssetType == UTcsBuffDefinition::PrimaryAssetType)
		{
			ScanBuffDefinitions(AssetDataList);
		}
		else if (TypeInfo.PrimaryAssetType == UTcsSkillDefinition::PrimaryAssetType)
		{
			ScanSkillDefinitions(AssetDataList);
		}
		else if (TypeInfo.PrimaryAssetType == UTcsStateSlotDefinition::PrimaryAssetType)
		{
			ScanStateSlotDefinitions(AssetDataList);
		}
		else if (TypeInfo.PrimaryAssetType == UTcsSkillModifierDefinition::PrimaryAssetType)
		{
			ScanSkillModifierDefinitions(AssetDataList);
		}
	}

	for (const FSoftObjectPath& SpecificAsset : TypeInfo.GetSpecificAssets())
	{
		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssetsByPackageName(SpecificAsset.GetLongPackageFName(), AssetDataList);

		if (TypeInfo.PrimaryAssetType == UTcsAttributeDefinition::PrimaryAssetType)
		{
			ScanAttributeDefinitions(AssetDataList);
		}
		else if (TypeInfo.PrimaryAssetType == UTcsAttributeModifierDefinition::PrimaryAssetType)
		{
			ScanAttributeModifierDefinitions(AssetDataList);
		}
		else if (TypeInfo.PrimaryAssetType == UTcsBuffDefinition::PrimaryAssetType)
		{
			ScanBuffDefinitions(AssetDataList);
		}
		else if (TypeInfo.PrimaryAssetType == UTcsSkillDefinition::PrimaryAssetType)
		{
			ScanSkillDefinitions(AssetDataList);
		}
		else if (TypeInfo.PrimaryAssetType == UTcsStateSlotDefinition::PrimaryAssetType)
		{
			ScanStateSlotDefinitions(AssetDataList);
		}
		else if (TypeInfo.PrimaryAssetType == UTcsSkillModifierDefinition::PrimaryAssetType)
		{
			ScanSkillModifierDefinitions(AssetDataList);
		}
	}
}

void UTcsDefinitionRegistrySubsystem::ScanAttributeDefinitions(const TArray<FAssetData>& AssetDataList)
{
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSoftObjectPtr<UTcsAttributeDefinition> AssetPtr(AssetData.ToSoftObjectPath());
		const UTcsAttributeDefinition* Asset = AssetPtr.LoadSynchronous();
		if (!Asset)
		{
			continue;
		}

		TcsDefinitionRegistryPrivate::AddDefinition(
			AttributeDefinitions,
			Asset->AttributeDefId,
			AssetPtr,
			TEXT("Attribute"));
	}
}

void UTcsDefinitionRegistrySubsystem::ScanAttributeModifierDefinitions(const TArray<FAssetData>& AssetDataList)
{
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSoftObjectPtr<UTcsAttributeModifierDefinition> AssetPtr(AssetData.ToSoftObjectPath());
		const UTcsAttributeModifierDefinition* Asset = AssetPtr.LoadSynchronous();
		if (!Asset)
		{
			continue;
		}

		TcsDefinitionRegistryPrivate::AddDefinition(
			AttributeModifierDefinitions,
			Asset->AttributeModifierDefId,
			AssetPtr,
			TEXT("AttributeModifier"));
	}
	}

void UTcsDefinitionRegistrySubsystem::ScanSkillModifierDefinitions(const TArray<FAssetData>& AssetDataList)
{
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSoftObjectPtr<UTcsSkillModifierDefinition> AssetPtr(AssetData.ToSoftObjectPath());
		const UTcsSkillModifierDefinition* Asset = AssetPtr.LoadSynchronous();
		if (!Asset)
		{
			continue;
		}

		TcsDefinitionRegistryPrivate::AddDefinition(
			SkillModifierDefinitions,
			Asset->ModifierId,
			AssetPtr,
			TEXT("SkillModifier"));
	}
}

void UTcsDefinitionRegistrySubsystem::ScanBuffDefinitions(const TArray<FAssetData>& AssetDataList)
{
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSoftObjectPtr<UTcsBuffDefinition> AssetPtr(AssetData.ToSoftObjectPath());
		const UTcsBuffDefinition* Asset = AssetPtr.LoadSynchronous();
		if (!Asset)
		{
			continue;
		}

		TcsDefinitionRegistryPrivate::AddDefinition(
			BuffDefinitions,
			Asset->StateDefId,
			AssetPtr,
			TEXT("Buff"));
	}
}

void UTcsDefinitionRegistrySubsystem::ScanSkillDefinitions(const TArray<FAssetData>& AssetDataList)
{
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSoftObjectPtr<UTcsSkillDefinition> AssetPtr(AssetData.ToSoftObjectPath());
		const UTcsSkillDefinition* Asset = AssetPtr.LoadSynchronous();
		if (!Asset)
		{
			continue;
		}

		TcsDefinitionRegistryPrivate::AddDefinition(
			SkillDefinitions,
			Asset->StateDefId,
			AssetPtr,
			TEXT("Skill"));
	}
}

void UTcsDefinitionRegistrySubsystem::ScanStateSlotDefinitions(const TArray<FAssetData>& AssetDataList)
{
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSoftObjectPtr<UTcsStateSlotDefinition> AssetPtr(AssetData.ToSoftObjectPath());
		const UTcsStateSlotDefinition* Asset = AssetPtr.LoadSynchronous();
		if (!Asset)
		{
			continue;
		}

		TcsDefinitionRegistryPrivate::AddDefinition(
			StateSlotDefinitions,
			Asset->StateSlotDefId,
			AssetPtr,
			TEXT("StateSlot"));
	}
}

bool UTcsDefinitionRegistrySubsystem::IsTrackedDefinitionClass(const FAssetData& AssetData) const
{
	if (!AssetData.IsValid())
	{
		return false;
	}

	UClass* AssetClass = AssetData.GetClass(EResolveClass::Yes);
	if (!AssetClass)
	{
		return false;
	}

	return AssetClass->IsChildOf(UTcsAttributeDefinition::StaticClass()) ||
		AssetClass->IsChildOf(UTcsAttributeModifierDefinition::StaticClass()) ||
		AssetClass->IsChildOf(UTcsBuffDefinition::StaticClass()) ||
		AssetClass->IsChildOf(UTcsSkillDefinition::StaticClass()) ||
		AssetClass->IsChildOf(UTcsStateSlotDefinition::StaticClass()) ||
		AssetClass->IsChildOf(UTcsSkillModifierDefinition::StaticClass());
}

bool UTcsDefinitionRegistrySubsystem::IsTrackedDefinitionObject(const UObject* AssetObject) const
{
	return AssetObject && (
		AssetObject->IsA(UTcsAttributeDefinition::StaticClass()) ||
		AssetObject->IsA(UTcsAttributeModifierDefinition::StaticClass()) ||
		AssetObject->IsA(UTcsBuffDefinition::StaticClass()) ||
		AssetObject->IsA(UTcsSkillDefinition::StaticClass()) ||
		AssetObject->IsA(UTcsStateSlotDefinition::StaticClass()) ||
		AssetObject->IsA(UTcsSkillModifierDefinition::StaticClass()));
}

void UTcsDefinitionRegistrySubsystem::OnAssetAdded(const FAssetData& AssetData)
{
	if (IsTrackedDefinitionClass(AssetData))
	{
		RequestRefresh();
	}
}

void UTcsDefinitionRegistrySubsystem::OnAssetUpdated(const FAssetData& AssetData)
{
	if (IsTrackedDefinitionClass(AssetData))
	{
		RequestRefresh();
	}
}

void UTcsDefinitionRegistrySubsystem::OnAssetRemoved(const FAssetData& AssetData)
{
	RequestRefresh();
}

void UTcsDefinitionRegistrySubsystem::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	RequestRefresh();
}

void UTcsDefinitionRegistrySubsystem::OnInMemoryAssetCreated(UObject* AssetObject)
{
	if (IsTrackedDefinitionObject(AssetObject))
	{
		RequestRefresh();
	}
}

void UTcsDefinitionRegistrySubsystem::OnInMemoryAssetDeleted(UObject* AssetObject)
{
	if (IsTrackedDefinitionObject(AssetObject))
	{
		RequestRefresh();
	}
}

void UTcsDefinitionRegistrySubsystem::OnAssetManagerSettingsChanged(UObject* SettingsObject, FPropertyChangedEvent& PropertyChangedEvent)
{
	bShouldReportCoverageIssuesAfterRefresh = true;
	RequestRefresh();
}

void UTcsDefinitionRegistrySubsystem::OnPackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext ObjectSaveContext)
{
	if (!GIsEditor || !FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	RefreshAssetManagerCoverageIssues(AssetRegistry);
	QueueCoverageIssueReportAfterSave(PackageFileName);
}

void UTcsDefinitionRegistrySubsystem::QueueCoverageIssueReportAfterSave(const FString& PackageFileName)
{
	if (DeferredCoverageIssueNotificationHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DeferredCoverageIssueNotificationHandle);
		DeferredCoverageIssueNotificationHandle.Reset();
	}

	if (AssetManagerCoverageIssues.IsEmpty())
	{
		bHasPendingCoverageIssueNotification = false;
		PendingCoverageIssueSaveCount = 0;
		PendingCoverageIssueFirstPackageFileName.Reset();
		return;
	}

	bHasPendingCoverageIssueNotification = true;
	++PendingCoverageIssueSaveCount;
	bPendingCoverageIssueTriggeredBySaveAll = bPendingCoverageIssueTriggeredBySaveAll || bIsExecutingSaveAllCommand;
	if (PendingCoverageIssueFirstPackageFileName.IsEmpty())
	{
		PendingCoverageIssueFirstPackageFileName = PackageFileName;
	}

	DeferredCoverageIssueNotificationHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UTcsDefinitionRegistrySubsystem::HandleDeferredCoverageIssueNotification),
		0.25f);
}

void UTcsDefinitionRegistrySubsystem::ClearQueuedRefresh()
{
	bIsRefreshQueued = false;
	if (DeferredRefreshHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DeferredRefreshHandle);
		DeferredRefreshHandle.Reset();
	}

	if (DeferredCoverageIssueNotificationHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DeferredCoverageIssueNotificationHandle);
		DeferredCoverageIssueNotificationHandle.Reset();
	}
}
#endif
