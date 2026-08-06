// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/TcsAttributeEvaluationSnapshot.h"
#include "TcsSourceHandle.h"



class UTcsAttributeComponent;
class UTcsStateInstance;
class UTcsSkillEntry;



/** AttributeModifier OperandEvaluator 单轮求值使用的只读运行时上下文。 */
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeOperandEvaluatorContext
{
	// 当前 Application 的目标 AttributeComponent。
	const UTcsAttributeComponent* TargetAttributeComponent = nullptr;

	// 当前 Application 的目标 Actor。
	AActor* Target = nullptr;

	// 从 SourceHandle 派生的发起者 Actor；允许为空。
	AActor* Instigator = nullptr;

	// 当前 Application 的有效来源句柄。
	const FTcsSourceHandle* SourceHandle = nullptr;

	// 可选来源 StateInstance；不得通过 SourceHandle 反查获得。
	UTcsStateInstance* SourceStateInstance = nullptr;

	// 可选来源 SkillEntry；不得通过 SourceHandle 反查获得。
	UTcsSkillEntry* SourceSkillEntry = nullptr;

	// 当前 Application 共享的只读 Attribute 数值 Snapshot。
	const FTcsAttributeEvaluationSnapshot* AttributeSnapshot = nullptr;
};
