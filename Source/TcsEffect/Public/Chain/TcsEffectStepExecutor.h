// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Chain/TcsChainRun.h"
#include "Chain/TcsEffectContext.h"
#include "Chain/TcsEffectStep.h"



/**
 * 步骤执行器签名（D4-17）：步骤数据**只读**，上下文与运行态可写。
 * 返回值表达步内挂起（`TSR_Running` = 本步未完成、解释器停驻原 PC 等唤醒重入）；
 * 挂起锚记在运行态上（`FTcsChainRun::PendingExpiry`）——步骤据此自辨"首入 / 被唤醒"。
 */
using FTcsStepExecute = TFunction<ETcsStepResult(const FInstancedStruct& StepData, FTcsEffectContext& Context, FTcsChainRun& Run)>;



/**
 * 待解析登记项：静态初始化期只把"步骤类型 getter + 执行器"挂进表里，**不调用 getter**
 * （静态初始化期触 UObject 是雷区——引擎 `FNativeGameplayTag` 用 `GetIfAllocated()` 规避同款问题）。
 */
struct FTcsEffectStepExecutorEntry
{
	// 步骤类型 getter（通常即 `&FTcsXxxStep::StaticStruct`；首次查询时才调用）
	UScriptStruct* (*GetStepStruct)() = nullptr;

	// 步骤执行器
	FTcsStepExecute Executor;
};



/**
 * 静态自注册器（D4-14；宏展开的载体）：模块静态初始化期构造 → 把本项挂入待解析表。
 * **零 UObject 触达**——反射类型延迟到注册表首次查询时才解析。
 */
struct TCSEFFECT_API FTcsEffectStepExecutorRegistrar
{
	FTcsEffectStepExecutorRegistrar(UScriptStruct* (*InStepStructGetter)(), FTcsStepExecute InExecutor);
};



/**
 * 步骤执行器注册表（D4-14 注册制分派）：机制层对步骤类型**零硬编码 switch**——
 * 领域模块（TcsDamage/TcsTargeting/…）在自己的实现文件里自登记，解释器按步骤 struct 的
 * **反射类型**查表分派。
 *
 * 键 = `const UScriptStruct*`（执行期唯一可得的类型身份是 `FInstancedStruct::GetScriptStruct()`；
 * 指针身份免去名字往返，且绕开 UHT 对 USTRUCT 反射名去 `F` 前缀的差异）。
 *
 * 进程级单例（步骤是代码而非世界状态），函数局部静态——单游戏线程访问（D0-4）。
 */
class TCSEFFECT_API FTcsEffectStepExecutorRegistry
{
// 注册
#pragma region Registration

public:
	// 进程级单例（函数局部静态——首次任一路径触达时建立）
	static FTcsEffectStepExecutorRegistry& Get();

	// 静态自注册入口（注册器构造调用——静态初始化期安全：只写待解析表）
	void AddPending(FTcsEffectStepExecutorEntry Entry);

	/**
	 * 动态注册入口（D4-17 双入口之反射面：脚本层 / 测试装置 / 运行期补登）。
	 * 同类型重复登记拒绝（ensure 提示 + 保留首个登记，不静默覆写）。
	 *
	 * @param StepStruct 步骤 struct 反射类型。
	 * @param Executor 执行器。
	 */
	void Register(const UScriptStruct* StepStruct, FTcsStepExecute Executor);

#pragma endregion


// 查询
#pragma region Query

public:
	/**
	 * 按步骤反射类型查执行器（首次调用时解析全部待解析登记项）。
	 *
	 * @param StepStruct 步骤 struct 反射类型。
	 * @return 返回执行器指针；未登记返回 nullptr（**不 ensure**——"未知步骤类型"由解释器按断链处置）。
	 */
	const FTcsStepExecute* Find(const UScriptStruct* StepStruct);

#pragma endregion


// 内核
#pragma region Core

private:
	// 解析待解析登记项（调用各 getter 取反射类型 → 建键；幂等，只跑一次）
	void ResolvePending();

	// 待解析登记项（静态初始化期写入；首次查询时消费）
	TArray<FTcsEffectStepExecutorEntry> PendingEntries;

	// 已登记执行器（键 = 步骤 struct 反射类型）
	TMap<const UScriptStruct*, FTcsStepExecute> Executors;

	// 待解析项是否已消费
	bool bPendingResolved = false;

#pragma endregion
};



/**
 * 声明一个步骤执行器的自注册器（供跨 TU 引用；与 UE_DEFINE_EFFECT_STEP_EXECUTOR 配对）。
 */
#define UE_DECLARE_EFFECT_STEP_EXECUTOR(ExecutorFn) \
	extern FTcsEffectStepExecutorRegistrar ExecutorFn##StepExecutorRegistrar;

/**
 * 登记一个步骤执行器（D4-14；仿 GAMEPLAY_TAG 模式——模块静态初始化期自登记，零 StartupModule 样板）。
 * 用法（步骤实现 .cpp）：`UE_DEFINE_EFFECT_STEP_EXECUTOR(FTcsStepWaitDelay, ExecuteStepWaitDelay)`。
 *
 * @param StepType 步骤数据 struct 类型（取 `StepType::StaticStruct` 作反射类型 getter）
 * @param ExecutorFn 执行器函数（签名同 FTcsStepExecute 的形参表）
 */
#define UE_DEFINE_EFFECT_STEP_EXECUTOR(StepType, ExecutorFn) \
	FTcsEffectStepExecutorRegistrar ExecutorFn##StepExecutorRegistrar(&StepType::StaticStruct, &ExecutorFn);
