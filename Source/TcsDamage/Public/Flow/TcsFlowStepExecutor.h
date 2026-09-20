// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Flow/TcsDamageFlowContext.h"



/**
 * 流程步骤执行器签名（D7-5）：**返回 false = 中止流程剩余步骤**（流程失败/打断——已产生的
 * 副作用不回滚，止于未来）。流程**无挂起语义**（单帧同步完成，区别于效果链的 `ETcsStepResult`），
 * 故用布尔中止通道而非挂起枚举。
 */
using FTcsFlowStepExecute = TFunction<bool(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)>;



/**
 * 待解析登记项（同 Effect 侧形状）：静态初始化期只把"步骤类型 getter + 执行器"挂进表，
 * **不调用 getter**——静态初始化期触 UObject 是雷区（引擎 `FNativeGameplayTag` 用 `GetIfAllocated()` 规避同款问题）。
 */
struct FTcsFlowStepExecutorEntry
{
	// 步骤类型 getter（通常即 `&FTcsXxxStep::StaticStruct`）
	UScriptStruct* (*GetStepStruct)() = nullptr;

	// 步骤执行器
	FTcsFlowStepExecute Executor;
};



/**
 * 静态自注册器（宏展开的载体）：模块静态初始化期构造 → 挂入待解析表（零 UObject 触达）。
 */
struct TCSDAMAGE_API FTcsFlowStepExecutorRegistrar
{
	FTcsFlowStepExecutorRegistrar(UScriptStruct* (*InStepStructGetter)(), FTcsFlowStepExecute InExecutor);
};



/**
 * 流程步骤注册表（与 TcsEffect 的执行器注册表**同构、独立实例**——两套流程各自演进，
 * 不共享表：效果链步骤与流程步骤是两类词汇）。
 *
 * 键 = 步骤 struct 的反射类型；双入口（静态自注册宏 + 动态 `Register`）；单游戏线程访问（D0-4）。
 */
class TCSDAMAGE_API FTcsFlowStepExecutorRegistry
{
// 注册
#pragma region Registration

public:
	// 进程级单例（函数局部静态）
	static FTcsFlowStepExecutorRegistry& Get();

	// 静态自注册入口（注册器构造调用——静态初始化期安全）
	void AddPending(FTcsFlowStepExecutorEntry Entry);

	// 动态注册入口（同类型重复登记拒绝：ensure + 保留首个）
	void Register(const UScriptStruct* StepStruct, FTcsFlowStepExecute Executor);

#pragma endregion


// 查询
#pragma region Query

public:
	/**
	 * 按步骤反射类型查执行器（首次调用解析待解析表）。
	 *
	 * @param StepStruct 步骤 struct 反射类型。
	 * @return 返回执行器指针；未登记返回 nullptr（不 ensure——"未知步骤类型"由解释器按中止处置）。
	 */
	const FTcsFlowStepExecute* Find(const UScriptStruct* StepStruct);

#pragma endregion


// 内核
#pragma region Core

private:
	// 解析待解析登记项（幂等；只跑一次）
	void ResolvePending();

	TArray<FTcsFlowStepExecutorEntry> PendingEntries;
	TMap<const UScriptStruct*, FTcsFlowStepExecute> Executors;
	bool bPendingResolved = false;

#pragma endregion
};



/**
 * 声明一个流程步骤执行器的自注册器（与 UE_DEFINE_FLOW_STEP_EXECUTOR 配对）。
 */
#define UE_DECLARE_FLOW_STEP_EXECUTOR(ExecutorFn) \
	extern FTcsFlowStepExecutorRegistrar ExecutorFn##StepExecutorRegistrar;

/**
 * 登记一个流程步骤执行器（D7-5；仿 GAMEPLAY_TAG 模式——模块静态初始化期自登记）。
 * 用法（步骤实现 .cpp）：`UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowStepCollectStart, ExecuteFlowStepCollectStart)`。
 *
 * @param StepType 步骤数据 struct 类型（取 `StepType::StaticStruct` 作反射类型 getter）
 * @param ExecutorFn 执行器函数（签名同 FTcsFlowStepExecute 的形参表）
 */
#define UE_DEFINE_FLOW_STEP_EXECUTOR(StepType, ExecutorFn) \
	FTcsFlowStepExecutorRegistrar ExecutorFn##StepExecutorRegistrar(&StepType::StaticStruct, &ExecutorFn);
