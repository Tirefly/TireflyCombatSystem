// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "TcsFlowTemplate.generated.h"



/**
 * 瞬时流程模板（D7-5 流程管线宿主化）：**流程 = 数据**——阶段构成本身是项目知识，
 * 插件不预设（"不同项目有不同项目的说法"，用户原话）；标准十阶段只是插件自带的标准步骤库
 * 与官方默认模板（可整表替换，Task 4）。
 *
 * 步骤元素与效果链同形（`FInstancedStruct`，步骤无公共基类）：类型合法性由**流程步骤注册表**
 * 在执行期判定（未注册 → 中止 + Error，见 `TcsFlowStepExecutor.h`）。
 */
USTRUCT()
struct TCSDAMAGE_API FTcsFlowTemplate
{
	GENERATED_BODY()

// 身份与步骤
#pragma region Definition

public:
	// 模板唯一标识（= 登记表键）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	FGameplayTag TemplateId;

	// 有序步骤数组（模板只读：执行过程 MUST NOT 改写）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TArray<FInstancedStruct> Steps;

#pragma endregion
};
