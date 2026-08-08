// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/TcsAttributeEvaluationSnapshot.h"
#include "Attribute/TcsAttributeModifierDependency.h"
#include "TcsSourceHandle.h"



class UTcsAttributeComponent;
class UTcsStateInstance;
class UTcsSkillEntry;



/** AttributeModifier OperandEvaluator 单轮求值使用的只读运行时上下文。 */
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeOperandEvaluatorContext
{
public:
	/** 读取目标组件 Attribute 的 CurrentValue Snapshot，并登记自动依赖。 */
	bool ReadTargetAttributeCurrentValue(FName AttributeId, float& OutValue) const;

	/** 读取目标组件 Attribute 的 BaseValue Snapshot；BaseValue 不属于自动观察范围。 */
	bool ReadTargetAttributeBaseValue(FName AttributeId, float& OutValue) const;

	/** 读取来源 StateInstance 的 Numeric StateParam effective 值。 */
	bool ReadSourceStateNumericParamEffectiveValue(FGameplayTag ParamTag, float& OutValue) const;

	/** 读取来源 SkillEntry 的 Numeric StateParam effective 值；该来源不自动观察。 */
	bool ReadSourceSkillEntryNumericParamEffectiveValue(FGameplayTag ParamTag, float& OutValue) const;

	/** @return 当前 Application 的目标 Actor。 */
	AActor* GetTarget() const { return Target; }

	/** @return 从 SourceHandle 派生的发起者 Actor；允许为空。 */
	AActor* GetInstigator() const { return Instigator; }

	/** @return 当前 Application 的有效来源句柄。 */
	const FTcsSourceHandle* GetSourceHandle() const { return SourceHandle; }

private:
	friend class UTcsAttributeComponent;

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

	// 当前父实例求值期间收集的自动依赖键。
	TArray<FTcsAttributeModifierDependencyKey>* DependencyCollector = nullptr;
};
