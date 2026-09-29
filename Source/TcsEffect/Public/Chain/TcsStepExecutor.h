// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StructUtils/InstancedStruct.h"

#include "Chain/TcsChainRun.h"
#include "Chain/TcsEffectStep.h"

#include "TcsStepExecutor.generated.h"



/**
 * 步骤执行器插槽（2026-09-24，台账 SCRIPT-8）：**让宿主用任意 UE 脚本语言（C# / AS / Luau / TS / 蓝图）
 * 定义步骤行为，零 C++ 改动**——客制化、只服务宿主业务、不值得进插件的步骤语义的正解。
 *
 * **为什么需要它**：既有执行器签名 `FTcsStepExecute = TFunction<...>` 是**注册表的值**——
 * 键（`UScriptStruct*`）可反射而 `TFunction` 不可，故脚本层无法登记执行器（台账 SCRIPT-2 的"差一层签名"）。
 * 本基类**替代 `TFunction` 作注册值**：门面把本对象包成 `TFunction` 转发进既有注册表
 * （**键与查表逻辑零改动**、C++ 快路径原样保留），比"换 `TFunction` 签名"（SCRIPT-2）改动面小得多。
 *
 * **分发底座是 UE 自己的反射系统**（`UFunction::Invoke`），不是某个脚本语言专属 ⇒ **天然语言无关**。
 *
 * **双轨并存**：框架内置步骤仍走 `UE_DEFINE_EFFECT_STEP_EXECUTOR` 静态自注册宏（零反射开销）；
 * 本插槽只服务宿主扩展（多一次 `UFunction::Invoke` 是接受的取舍）。
 *
 * **形参 MUST 全反射**（`BlueprintNativeEvent` 触发 UHT 蓝图参数校验，`UhtFunction.cs:859`/`:1043-1053`）
 * ⇒ 传**运行态句柄**而非上下文（`FTcsEffectContext` 是非反射纯 C++ struct）；上下文经门面按句柄
 * 访问器读写（`GetRunTargets` / `SetRunVariable` / `GetRunCaster` 一组）。
 *
 * **GC 可见持有**：登记后门面以 `UPROPERTY` 数组持有本类实例——**裸 C++ 注册表持不住对象引用**
 * （不经 GC 的 `RefLink`），不持有则脚本执行器被静默回收，表现为"步骤不生效"而非崩溃
 * （同 `UTcsDamageSubsystem::AddReferencedObjects` 修复的 WAIT-8 缺陷形态）。
 */
UCLASS(Abstract, Blueprintable)
class TCSEFFECT_API UTcsStepExecutor : public UObject
{
	GENERATED_BODY()

// 执行入口
#pragma region Execute

public:
	/**
	 * 执行一步（语义与 `FTcsStepExecute` 完全一致）。
	 *
	 * **返回值表达步内挂起**：`TSR_Completed` = 本步完成、解释器 PC 前进；`TSR_Running` = 本步未完成、
	 * 解释器停驻原 PC 等唤醒源按运行态句柄重入（首入/被唤醒由本方法自行分辨——挂起锚记在运行态上，
	 * 经 `Run` 句柄与门面访问器读写）。
	 *
	 * **本方法 MUST NOT 在持有 `Run` 期间触发新的链执行**（池扩容会搬移运行态——`TcsChainRun.h` 的
	 * 持有纪律对脚本执行器同样适用）。
	 *
	 * @param ChainId 当前链 id（日志与自辨用）。
	 * @param Run 当前运行态句柄（上下文经门面按此句柄访问）。
	 * @param StepData 本步数据（`FInstancedStruct`——脚本层可定义纯数据 struct 并真进反射）。
	 * @return 返回本步是否完成。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Tcs|Effect|Chain")
	ETcsStepResult Execute(FGameplayTag ChainId, FTcsChainRunHandle Run, const FInstancedStruct& StepData);

	// 中性默认实现：按完成处理（抽象基类不可被实例化；派生类 MUST 覆写）
	virtual ETcsStepResult Execute_Implementation(FGameplayTag ChainId, FTcsChainRunHandle Run, const FInstancedStruct& StepData);

#pragma endregion
};
