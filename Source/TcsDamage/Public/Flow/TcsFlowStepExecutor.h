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
 * 动态登记项的**寿命信息**（2026-09-29，DEC-04 裁定 ⑤；修反射册 R-2 跨世界寿命缺陷）。
 *
 * 与 Effect 侧同款：仅**动态**登记（宿主脚本插槽）需要——它把世界级 GC 对象塞进了**进程级**
 * 注册表，而注册表比世界活得久 ⇒ 必须能判"这条登记还属不属于当前世界"。
 * **静态自注册项不进本表**（纯函数无 UObject 寿命问题，永不过期）。
 *
 * 失效判据（任一成立即失效）：① `Object` 弱引用为空（对象已被 GC）；② `World` 弱引用为空（世界已销毁）；③ `World` ≠ 查询方世界。
 */
struct FTcsFlowStepExecutorLifetime
{
	// 宿主执行器对象（弱引用——注册表 MUST NOT 强持有，强持有由门面的 UPROPERTY 负责）
	TWeakObjectPtr<UObject> Object;

	// 登记时所在的世界（`nullptr` = 无世界上下文，此时跳过世界校验）
	TWeakObjectPtr<const UWorld> World;
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

	/**
	 * 动态注册入口（D7-5 双入口之反射面：脚本层 / 测试装置 / 运行期补登）。
	 *
	 * **拒绝门 = "同世界活对象重复"**（2026-09-29，DEC-04 裁定 ⑤）：仅当既有登记**仍有效**
	 * （对象存活 && 世界一致 && 非静态自注册项）时才拒绝并保留首个；既有登记**已失效**时
	 * MUST 替换该条目——否则失效条目会毒化后续所有 PIE。
	 *
	 * @param StepStruct 步骤 struct 反射类型。
	 * @param Executor 执行器（转发到宿主 UObject 的 `TFunction`）。
	 * @param LifetimeObject 宿主执行器对象（弱引用记录；空 = 不做对象寿命校验）。
	 * @param LifetimeWorld 登记时所在的世界（弱引用记录；空 = 不做世界校验——测试装置/纯 C++ 场景）。
	 */
	void Register(
		const UScriptStruct* StepStruct,
		FTcsFlowStepExecute Executor,
		UObject* LifetimeObject = nullptr,
		const UWorld* LifetimeWorld = nullptr);

	/**
	 * 移除一条**动态**登记（2026-09-29，DEC-04 裁定 ⑤）。静态自注册项 MUST NOT 被移除
	 * ——它们是代码而非登记，本入口对它们返回 false。
	 *
	 * 注意：本入口是**整理手段，不是正确性前提**——`Find` 的失效判据自足，
	 * 即使从不调用本入口也不会解引用已失效对象。
	 *
	 * @return 是否真的移除了动态条目。
	 */
	bool Unregister(const UScriptStruct* StepStruct);

	/**
	 * 取全部**动态**登记项的键（静态自注册项不在其列）。
	 *
	 * 用途：门面 `Deinitialize` 按世界收口时需要键清单——键（步骤 struct 类型）与值（宿主执行器对象）
	 * 不同型，无法从对象反查键，故由注册表提供本清单供逐个 `Unregister`。
	 *
	 * @return 动态登记项的键数组（顺序不保证）。
	 */
	TArray<const UScriptStruct*> GetDynamicKeys() const;

#pragma endregion


// 查询
#pragma region Query

public:
	/**
	 * 按步骤反射类型查执行器（首次调用解析待解析表）。
	 *
	 * **寿命校验**（2026-09-29）：动态登记项在对象已被 GC、世界已销毁、或与传入世界不一致时
	 * MUST 视为未命中——返回 nullptr、移除该条目、留 Warning 日志（MUST NOT 静默按"未登记"
	 * 处理：那会把"世界已更换"表现成"步骤类型写漏"）。
	 *
	 * @param StepStruct 步骤 struct 反射类型。
	 * @param World 调用方所在的世界；**调用方持有世界时 MUST 传入**。空 = 跳过世界校验。
	 * @return 返回执行器指针；未登记或已失效返回 nullptr（不 ensure——"未知步骤类型"由解释器按中止处置）。
	 */
	const FTcsFlowStepExecute* Find(const UScriptStruct* StepStruct, const UWorld* World = nullptr);

#pragma endregion


// 内核
#pragma region Core

private:
	// 解析待解析登记项（幂等；只跑一次）
	void ResolvePending();

	// 记录/覆盖一条动态登记的寿命信息（登记成功后调用）
	void RecordLifetime(const UScriptStruct* StepStruct, UObject* LifetimeObject, const UWorld* LifetimeWorld);

	// 既有动态登记是否失效（并移除之）；返回 true 表示"可被替换"
	bool DiscardIfStale(const UScriptStruct* StepStruct, const UWorld* World);

	TArray<FTcsFlowStepExecutorEntry> PendingEntries;
	TMap<const UScriptStruct*, FTcsFlowStepExecute> Executors;

	// 动态登记项的寿命信息（键同上；**只含动态项**——静态自注册项不在此表，故永不过期）
	TMap<const UScriptStruct*, FTcsFlowStepExecutorLifetime> Lifetimes;

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
