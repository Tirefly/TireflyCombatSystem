// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/ScriptInterface.h"

#include "Attribute/TcsAttrModInstance.h"
#include "Flow/TcsDamageFlowDelegate.h"
#include "Flow/TcsFlowAttributes.h"
#include "Parameter/TcsParamValue.h"

#include "TcsFlowDataSteps.generated.h"



/**
 * 通用数据步骤（D7-5"编辑器拼流程零 C++"）：**数据化黑板写入**与**数据化委托调用**。
 * 两个 struct 与十步共享同一 Conditions 契约（同名字段 + 执行器首行调 `ShouldRunFlowStep`）。
 *
 * 说明：计划 sketch 写两个独立 `.h`，此为落地收拢（同族两个小 struct 同住一头——见 plan2 Task 4 注记）。
 */

// 数据化黑板写入（"破甲阶段" = 一个本步骤）
USTRUCT()
struct TCSDAMAGE_API FTcsFlowModify
{
	GENERATED_BODY()

	// 目标黑板键
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	FGameplayTag TargetKey;

	// 运算带（带序唯一真相；SortKey 不参与求值顺序）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	ETcsAttributeOp Op = ETcsAttributeOp::TAO_Add;

	// 操作数（PV 载体；Literal 直配，ParamRef / 黑板键引用随其来源策略轮）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	FTcsParamValue Operand;

	// 注：**数据步骤无法携带消耗策略**（`FTcsConsumePolicy` 含 `TFunction OnConsumed` 回调，
	// 纯 C++ struct 不可反射、不可作 UPROPERTY——UHT 实证）→ 消耗型提交只能来自 C++ 步骤或事件响应。
	// 这与"过网结构 MUST 纯反射数据"纪律同源（回调永不过网）。

	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TArray<FInstancedStruct> Conditions;
};

// 数据化委托调用（轻量公式挂法：把宿主 delegate 的结果写进黑板键）
USTRUCT()
struct TCSDAMAGE_API FTcsFlowDelegate
{
	GENERATED_BODY()

	// 目标黑板键
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	FGameplayTag TargetKey;

	// 委托（宿主实现；与 `FTcsStepDamage` 同款降级位）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TScriptInterface<ITcsDamageFlowDelegate> Delegate;

	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TArray<FInstancedStruct> Conditions;
};
