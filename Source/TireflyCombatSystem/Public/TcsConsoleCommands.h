// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"



/**
 * TCS 控制台命令共享命名面。
 *
 * 该头文件只承载命令字符串、参数约定、帮助文本，以及少量字符串拼装辅助函数；
 * 不负责命令注册、运行时状态保存或具体执行逻辑。
 */
namespace TcsConsoleCommands
{

#pragma region Generic

	// TCS 模块控制台命令统一前缀。
	inline constexpr TCHAR RootPrefix[] = TEXT("tcs");

#pragma endregion


#pragma region Arguments

	namespace Arguments
	{
		// 开关控制参数键。
		inline constexpr TCHAR Enable[] = TEXT("enable");

		// 开关参数值说明。
		inline constexpr TCHAR EnableValueDescription[] = TEXT("0|1");
	}

#pragma endregion


#pragma region State

	namespace State
	{
		// 控制常驻状态调试求值器是否允许构造快照字符串的控制台命令。
		inline constexpr TCHAR DebugEvaluatorEnable[] = TEXT("tcs.state.debug_evaluator.enable");

		// 状态调试求值器开关命令帮助文本。
		inline constexpr TCHAR DebugEvaluatorEnableHelp[] =
			TEXT("Enable or disable recurring StateTree slot snapshot generation. Args: enable=<0|1>");
	}

#pragma endregion

}



/**
 * TCS 控制台命令运行时入口。
 *
 * 该命名空间只暴露给 TCS 模块内部调用，用于模块启动/关闭以及调试求值器查询；
 * 具体实现仍然留在 `TcsConsoleCommands.cpp` 中。
 */
namespace TcsConsoleCommandRuntime
{
	/** 注册 TCS 第一阶段控制台命令。 */
	TIREFLYCOMBATSYSTEM_API void RegisterConsoleCommands();

	/** 注销 TCS 第一阶段控制台命令。 */
	TIREFLYCOMBATSYSTEM_API void UnregisterConsoleCommands();

	/**
	 * 查询常驻状态调试求值器是否允许构造周期性槽位快照。
	 *
	 * @return 若显式调试开关已开启则返回 true，否则返回 false
	 */
	TIREFLYCOMBATSYSTEM_API bool IsStateDebugEvaluatorSnapshotsEnabled();
}