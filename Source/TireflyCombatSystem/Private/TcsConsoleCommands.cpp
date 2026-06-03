// Copyright Tirefly. All Rights Reserved.

#include "TcsConsoleCommands.h"

#include "TcsLogChannels.h"

#include "HAL/IConsoleManager.h"
#include "Misc/DefaultValueHelper.h"



namespace
{
	// 当前模块注册过的控制台对象句柄；模块关闭时统一注销。
	TArray<IConsoleObject*> RegisteredConsoleObjects;

	// 控制 `FTcsSTEvaluator_SlotDebug` 是否允许构造周期性槽位快照。
	bool bStateDebugEvaluatorSnapshotsEnabled = false;

	// 解析后的命令参数映射表。
	using FConsoleArgumentMap = TMap<FString, FString>;

	/**
	 * 解析单个 `key=value` 参数。
	 *
	 * @param Argument 原始参数文本
	 * @param OutKey 输出参数键
	 * @param OutValue 输出参数值
	 * @return 解析成功时返回 true，否则返回 false
	 */
	bool TryParseKeyValueArgument(const FString& Argument, FString& OutKey, FString& OutValue)
	{
		if (!Argument.Split(TEXT("="), &OutKey, &OutValue))
		{
			return false;
		}

		OutKey.TrimStartAndEndInline();
		OutValue.TrimStartAndEndInline();
		return !OutKey.IsEmpty() && !OutValue.IsEmpty();
	}

	/**
	 * 把原始控制台参数数组归一化成不区分大小写的 `key=value` 字典。
	 *
	 * @param Args 原始参数数组
	 * @return 归一化后的参数映射表
	 */
	FConsoleArgumentMap BuildArgumentMap(const TArray<FString>& Args)
	{
		FConsoleArgumentMap ParsedArguments;
		for (const FString& Arg : Args)
		{
			FString Key;
			FString Value;
			if (!TryParseKeyValueArgument(Arg, Key, Value))
			{
				continue;
			}

			Key.ToLowerInline();
			ParsedArguments.Add(MoveTemp(Key), MoveTemp(Value));
		}

		return ParsedArguments;
	}

	/**
	 * 从解析后的参数字典中查找指定键的值。
	 *
	 * @param Arguments 参数映射表
	 * @param Key 目标键名
	 * @return 若存在则返回对应值指针，否则返回 nullptr
	 */
	const FString* FindArgumentValue(const FConsoleArgumentMap& Arguments, const TCHAR* Key)
	{
		FString NormalizedKey(Key);
		NormalizedKey.ToLowerInline();
		return Arguments.Find(NormalizedKey);
	}

	/**
	 * 切换常驻状态调试求值器的快照生成开关。
	 *
	 * @param Args 控制台原始参数数组
	 */
	void HandleSetStateDebugEvaluatorEnabledCommand(const TArray<FString>& Args)
	{
		const FConsoleArgumentMap Arguments = BuildArgumentMap(Args);
		const FString* EnableValue = FindArgumentValue(Arguments, TcsConsoleCommands::Arguments::Enable);
		if (!EnableValue)
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] Missing required argument: %s"),
				TcsConsoleCommands::State::DebugEvaluatorEnable,
				TcsConsoleCommands::Arguments::Enable);
			return;
		}

		int32 ParsedEnableValue = 0;
		if (!FDefaultValueHelper::ParseInt(*EnableValue, ParsedEnableValue) || (ParsedEnableValue != 0 && ParsedEnableValue != 1))
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid enable value '%s'. Expected %s."),
				TcsConsoleCommands::State::DebugEvaluatorEnable,
				**EnableValue,
				TcsConsoleCommands::Arguments::EnableValueDescription);
			return;
		}

		bStateDebugEvaluatorSnapshotsEnabled = (ParsedEnableValue != 0);
		UE_LOG(LogTcsState, Log, TEXT("[%s] Recurring state slot snapshots are now %s."),
			TcsConsoleCommands::State::DebugEvaluatorEnable,
			bStateDebugEvaluatorSnapshotsEnabled ? TEXT("enabled") : TEXT("disabled"));
	}

	/**
	 * 注册单个控制台对象并保存其句柄，便于模块关闭时统一注销。
	 *
	 * @param ConsoleObject 注册结果对象
	 */
	void RegisterConsoleObject(IConsoleObject* ConsoleObject)
	{
		if (ConsoleObject)
		{
			RegisteredConsoleObjects.Add(ConsoleObject);
		}
	}
}



namespace TcsConsoleCommandRuntime
{
	void RegisterConsoleCommands()
	{
		if (!RegisteredConsoleObjects.IsEmpty())
		{
			return;
		}

		IConsoleManager& ConsoleManager = IConsoleManager::Get();

		RegisterConsoleObject(ConsoleManager.RegisterConsoleCommand(
			TcsConsoleCommands::State::DebugEvaluatorEnable,
			TcsConsoleCommands::State::DebugEvaluatorEnableHelp,
			FConsoleCommandWithArgsDelegate::CreateStatic(&HandleSetStateDebugEvaluatorEnabledCommand),
			ECVF_Default));
	}

	void UnregisterConsoleCommands()
	{
		IConsoleManager& ConsoleManager = IConsoleManager::Get();
		for (int32 Index = RegisteredConsoleObjects.Num() - 1; Index >= 0; --Index)
		{
			if (IConsoleObject* ConsoleObject = RegisteredConsoleObjects[Index])
			{
				ConsoleManager.UnregisterConsoleObject(ConsoleObject, false);
			}
		}

		RegisteredConsoleObjects.Reset();
		bStateDebugEvaluatorSnapshotsEnabled = false;
	}

	bool IsStateDebugEvaluatorSnapshotsEnabled()
	{
		return bStateDebugEvaluatorSnapshotsEnabled;
	}
}