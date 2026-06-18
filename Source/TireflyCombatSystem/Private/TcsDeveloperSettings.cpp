// Copyright Tirefly. All Rights Reserved.

#include "TcsDeveloperSettings.h"

#include "AssetRegistry/AssetData.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"

namespace
{
	/**
	 * 规范化内容目录路径，便于做唯一性比较。
	 *
	 * @param InPath 原始目录路径。
	 * @return 去除尾部斜杠后的规范化路径。
	 */
	FString NormalizeContentDirectoryPath(const FString& InPath)
	{
		FString Normalized = InPath;
		Normalized.TrimStartAndEndInline();

		while (Normalized.EndsWith(TEXT("/")))
		{
			Normalized.LeftChopInline(1, EAllowShrinking::No);
		}

		return Normalized;
	}
}



FTcsDataTableSyncConfigValidationResult UTcsDeveloperSettings::ValidateConfig(const FTcsDataTableSyncConfig& Config) const
{
	FTcsDataTableSyncConfigValidationResult Result;

	if (!Config.DefAssetClass)
	{
		Result.Errors.Add(FText::FromString(TEXT("DataTable Sync 配置缺少 DefAssetClass。")));
	}

	const FString ManagedDirectoryPath = NormalizeContentDirectoryPath(Config.ManagedDefAssetDirectory.Path);
	if (ManagedDirectoryPath.IsEmpty())
	{
		Result.Errors.Add(FText::FromString(TEXT("DataTable Sync 配置缺少 ManagedDefAssetDirectory。")));
	}
	else if (!FPackageName::IsValidLongPackageName(ManagedDirectoryPath, false))
	{
		Result.Errors.Add(FText::FromString(FString::Printf(
			TEXT("ManagedDefAssetDirectory 不是合法内容路径：%s"),
			*ManagedDirectoryPath)));
	}

	const FSoftObjectPath DataTablePath = Config.TargetDataTable.ToSoftObjectPath();
	if (DataTablePath.IsNull())
	{
		Result.Errors.Add(FText::FromString(TEXT("DataTable Sync 配置缺少 TargetDataTable。")));
	}
	else if (!DataTablePath.IsValid())
	{
		Result.Errors.Add(FText::FromString(FString::Printf(
			TEXT("TargetDataTable 软引用路径无效：%s"),
			*DataTablePath.ToString())));
	}

	Result.bValid = Result.Errors.IsEmpty();
	return Result;
}

bool UTcsDeveloperSettings::ValidateAllConfigs(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	OutErrors.Reset();
	OutWarnings.Reset();

	TSet<FString> ManagedDirectories;
	TSet<FString> TargetDataTables;

	for (const FTcsDataTableSyncConfig& Config : DataTableSyncConfigs)
	{
		const FTcsDataTableSyncConfigValidationResult ValidationResult = ValidateConfig(Config);
		OutErrors.Append(ValidationResult.Errors);
		OutWarnings.Append(ValidationResult.Warnings);

		const FString ManagedDirectoryPath = NormalizeContentDirectoryPath(Config.ManagedDefAssetDirectory.Path);
		if (!ManagedDirectoryPath.IsEmpty())
		{
			if (ManagedDirectories.Contains(ManagedDirectoryPath))
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("ManagedDefAssetDirectory 重复：%s"),
					*ManagedDirectoryPath)));
			}
			else
			{
				ManagedDirectories.Add(ManagedDirectoryPath);
			}
		}

		const FString TargetDataTablePath = Config.TargetDataTable.ToSoftObjectPath().ToString();
		if (!TargetDataTablePath.IsEmpty())
		{
			if (TargetDataTables.Contains(TargetDataTablePath))
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("TargetDataTable 重复：%s"),
					*TargetDataTablePath)));
			}
			else
			{
				TargetDataTables.Add(TargetDataTablePath);
			}
		}
	}

	return OutErrors.IsEmpty();
}

#if WITH_EDITOR
EDataValidationResult UTcsDeveloperSettings::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	EDataValidationResult Result = SuperResult;

	TArray<FText> Errors;
	TArray<FText> Warnings;
	if (!ValidateAllConfigs(Errors, Warnings))
	{
		for (const FText& Error : Errors)
		{
			Context.AddError(Error);
		}
		Result = EDataValidationResult::Invalid;
	}

	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
		if (Result == EDataValidationResult::Valid)
		{
			Result = EDataValidationResult::NotValidated;
		}
	}

	return Result;
}
#endif
