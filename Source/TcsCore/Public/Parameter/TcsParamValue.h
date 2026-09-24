// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Parameter/TcsParamValueSource.h"
#include "Parameter/TcsParamSource_Literal.h"
#include "TcsParamValue.generated.h"



/**
 * 全插件统一"数值配置"载体（PV-1，取代 D2-12 FTcsParamScalar）：
 * 以 **`FInstancedStruct`** 持有参数值来源策略实例，默认 Literal 字面量源。
 * 消费面：Def 参数行/时值字段/模板 Operand 等全量切换（PV-6）；
 * 运行侧账本仍恒为已解析规范值（D2-13 不变式不动）。
 *
 * **载体形态 = 裸 `FInstancedStruct`（2026-09-24 换型，提案 `switch-strategy-carrier-to-plain-instanced-struct`）**：
 * 原为 `TInstancedStruct<FTcsParamValueSource>`，换型的唯一理由是**宿主脚本层可配性**——
 * `TInstancedStruct<T>` 字段在 UnrealSharp 侧**导出为空壳**（无字段读写代码），脚本层配不了任何数值；
 * 裸形态是引擎官方文档注释给出的写法（`InstancedStruct.h:20-27`），且 **StateTree 用的正是裸形态**
 * （D3-7 v3 宣称的"StateTree 同构"以裸形态才真正成立）。
 *
 * **类型限定由 `meta = (BaseStruct = ...)` 承担**（编辑器 picker 只列源族）；
 * **代价（明示接受）**：丢 `TInstancedStruct` 的编译期 `enable_if` 限定 ⇒ 异族赋值不再编译报错，
 * 改由**运行期**校验兜底——`GetPtr<T>()` 做 `IsChildOf` 检查、不符**返回 nullptr**（非野调用），
 * 调用点 MUST 判空（本文件 `Evaluate` 即按此书写）。
 *
 * **兼容性**：换型不破坏已存资产——引擎保证两者同尺寸且"反射层视为同一物"
 * （`InstancedStruct.h:538`），UHT 产出的属性结构逐字段相同，资产序列化的是**内层类型身份**
 * （`Ar << TObjectPtr<UScriptStruct>`）而非容器类型。
 */
USTRUCT(BlueprintType)
struct TCSCORE_API FTcsParamValue
{
	GENERATED_BODY()

// 值来源策略
#pragma region Source

public:
	// 默认构造：Source 初始化为 Literal 字面量源（默认值 0）
	FTcsParamValue()
	{
		Source.InitializeAs<FTcsParamSource_Literal>();
	}

public:
	// 参数值来源策略（默认 Literal；picker 经 BaseStruct metadata 限定到源族）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value",
		meta = (BaseStruct = "/Script/TcsCore.TcsParamValueSource"))
	FInstancedStruct Source;

#pragma endregion

// 求值便利转发
#pragma region Evaluate

public:
	// 求值便利转发（PV-1 增补：消解调用点每次 .Source.Get() 的噪音；空载体兜 0——防 Source 失效后误用）
	// 换型连带（2026-09-24）：裸载体无编译期类型限定 ⇒ 用 GetPtr 做运行期 IsChildOf 校验 + 判空
	double Evaluate(const FTcsParamEvaluateContext& Ctx) const
	{
		const FTcsParamValueSource* SourcePtr = Source.GetPtr<FTcsParamValueSource>();
		return SourcePtr ? SourcePtr->Evaluate(Ctx) : 0.0;
	}

#pragma endregion
};
