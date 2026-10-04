// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "Parameter/TcsParamValueSource.h"

#include "TcsEntityLevelProvider.generated.h"



// 实体等级读口（D3-11 修订）：宿主实现"这个实体几级"，等级类参数源经它把句柄解析成等级
// （UINTERFACE + UFUNCTION(BlueprintNativeEvent) = UE 原生反射分发，脚本/蓝图/任意脚本语言均可实现）
UINTERFACE(MinimalAPI, Blueprintable)
class UTcsEntityLevelProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * 实体等级读口（宿主契约）：等级类参数源的**唯一**等级来源。
 *
 * **为什么住 TcsState 而不是 TcsCore**：等级是**状态层**的词汇（四个等级源全在本模块），
 * 而 `FTcsParamEvaluateContext` 住 TcsCore——把域读口塞进 Core 会让 Core 持有一个只有上层
 * 模块使用的接口类型（且日后每个域都会想往那里塞自己的读口）。**先例 = 同族的
 * `ITcsAttributeProvider` 住 `TcsAttribute`**（读口随消费者所属的领域层）。
 *
 * **未注入 = `nullptr` 是配置状态不是错误**：等级类源据此落兜底路径，不留红字、不 ensure
 * （同 `ITcsParamTableReader` 可空的口径）。
 */
class ITcsEntityLevelProvider
{
	GENERATED_BODY()

// 等级读取契约
#pragma region Query

public:
	/**
	 * 取实体等级。
	 *
	 * **不带 `const`**：`BlueprintNativeEvent` 上的 `const` 会让 UHT 生成错误的 thunk 签名，
	 * 是硬规则（先例 `ITcsParamTableReader::TryGetNumericParam` / `ITcsAttributeProvider`）。
	 *
	 * @param Entity 目标实体句柄（无效时由实现方决定落什么值——本接口不规定）。
	 * @return 返回该实体的等级；0 = 无等级语义（等级类源据此落兜底）。
	 */
	UFUNCTION(BlueprintNativeEvent)
	int32 GetEntityLevel(FTcsCombatEntityHandle Entity);

#pragma endregion
};



/**
 * 状态域求值上下文（PV-1 扩展机制第二步的实例：结构体继承 + 源内 checked cast）。
 *
 * **为什么等级读口住派生上下文而不进 Core 上下文**：`LevelProvider` 的唯一消费者是
 * 本模块的四个等级源；把它加成 `FTcsParamEvaluateContext` 的字段会让 Core 持有只有上层
 * 使用的类型。派生上下文是**已立规的扩展位**（先例 = 同族 `FTcsAttributeEvaluateContext`
 * 持 `ITcsAttributeProvider`，PV-1 明文支持多层派生）。
 *
 * **判定纪律**：域侧源取上下文后 MUST 以
 * `Context.GetScriptStruct()->IsChildOf(FTcsStateEvaluateContext::StaticStruct())` 判定，
 * 失败落兜底（不崩溃、不 ensure）；判定通过后 `static_cast` 取字段。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsStateEvaluateContext : public FTcsParamEvaluateContext
{
	GENERATED_BODY()

// 类型标识
#pragma region Type

public:
	// 覆写类型标识（checked cast 锚点——派生上下文自报具体类型）
	virtual const UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}

#pragma endregion


// 等级读口
#pragma region Provider

public:
	// 宿主实体等级读口（空 = 本上下文不提供等级读取，等级类源落兜底）
	UPROPERTY(BlueprintReadWrite, Category = "Param Evaluate")
	TScriptInterface<ITcsEntityLevelProvider> LevelProvider;

#pragma endregion
};
