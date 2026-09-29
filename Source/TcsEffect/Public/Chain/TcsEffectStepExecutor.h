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
 * 动态登记项的**寿命信息**（2026-09-29，DEC-04 裁定 ⑤；修反射册 R-2 跨世界寿命缺陷）。
 *
 * 仅**动态**登记（宿主脚本插槽）需要：它把世界级 GC 对象塞进了**进程级**注册表，
 * 而注册表比世界活得久 ⇒ 必须能判"这条登记还属不属于当前世界"。
 * **静态自注册项不进本表**（纯函数无 UObject 寿命问题，永不过期）。
 *
 * 失效判据（任一成立即失效，见注册表 Find/Register）：
 * ① `Object` 弱引用为空（对象已被 GC）；② `World` 弱引用为空（世界已销毁）；③ `World` ≠ 查询方世界。
 */
struct FTcsEffectStepExecutorLifetime
{
	// 宿主执行器对象（弱引用——注册表 MUST NOT 强持有，强持有由门面的 UPROPERTY 负责）
	TWeakObjectPtr<UObject> Object;

	// 登记时所在的世界（`nullptr` = 无世界上下文，此时跳过世界校验）
	TWeakObjectPtr<const UWorld> World;
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
	 *
	 * **拒绝门 = "同世界活对象重复"**（2026-09-29，DEC-04 裁定 ⑤）：仅当既有登记**仍有效**
	 * （对象存活 && 世界一致 && 非静态自注册项）时才拒绝并保留首个；既有登记**已失效**时
	 * MUST 替换该条目——否则失效条目会毒化后续所有 PIE（旧行为：第二次 PIE 撞 Contains
	 * ⇒ 拒绝 + 保留首个 ⇒ 之后一直用旧世界的 stale 指针）。
	 *
	 * @param StepStruct 步骤 struct 反射类型。
	 * @param Executor 执行器（转发到宿主 UObject 的 `TFunction`）。
	 * @param LifetimeObject 宿主执行器对象（弱引用记录；空 = 不做对象寿命校验）。
	 * @param LifetimeWorld 登记时所在的世界（弱引用记录；空 = 不做世界校验——测试装置/纯 C++ 场景）。
	 */
	void Register(
		const UScriptStruct* StepStruct,
		FTcsStepExecute Executor,
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
	 * 按步骤反射类型查执行器（首次调用时解析全部待解析登记项）。
	 *
	 * **寿命校验**（2026-09-29）：动态登记项在对象已被 GC、世界已销毁、或与传入世界不一致时
	 * MUST 视为未命中——返回 nullptr、移除该条目、留 Warning 日志（MUST NOT 静默按"未登记"
	 * 处理：那会把"世界已更换"表现成"步骤类型写漏"，而后者走"未知步骤类型 → 断链"路径）。
	 *
	 * @param StepStruct 步骤 struct 反射类型。
	 * @param World 调用方所在的世界；**调用方持有世界时 MUST 传入**。空 = 跳过世界校验。
	 * @return 返回执行器指针；未登记或已失效返回 nullptr（**不 ensure**——"未知步骤类型"由解释器按断链处置）。
	 */
	const FTcsStepExecute* Find(const UScriptStruct* StepStruct, const UWorld* World = nullptr);

#pragma endregion


// 内核
#pragma region Core

private:
	// 解析待解析登记项（调用各 getter 取反射类型 → 建键；幂等，只跑一次）
	void ResolvePending();

	// 记录/覆盖一条动态登记的寿命信息（登记成功后调用）
	void RecordLifetime(const UScriptStruct* StepStruct, UObject* LifetimeObject, const UWorld* LifetimeWorld);

	// 既有动态登记是否失效（并移除之）；返回 true 表示"可被替换"
	bool DiscardIfStale(const UScriptStruct* StepStruct, const UWorld* World);

	// 待解析登记项（静态初始化期写入；首次查询时消费）
	TArray<FTcsEffectStepExecutorEntry> PendingEntries;

	// 已登记执行器（键 = 步骤 struct 反射类型）
	TMap<const UScriptStruct*, FTcsStepExecute> Executors;

	// 动态登记项的寿命信息（键同上；**只含动态项**——静态自注册项不在此表，故永不过期）
	TMap<const UScriptStruct*, FTcsEffectStepExecutorLifetime> Lifetimes;

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
