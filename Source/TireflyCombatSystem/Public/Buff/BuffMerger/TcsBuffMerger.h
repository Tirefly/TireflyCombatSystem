// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Buff/BuffMerger/TcsBuffMergeRuntime.h"
#include "Buff/TcsBuffInstance.h"
#include "TcsBuffMerger.generated.h"



// Buff 合并器
UCLASS(Abstract, BlueprintType, Blueprintable, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsBuffMerger : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 返回当前 merger 依赖的运行时输入集合。
	 *
	 * 默认实现返回保守依赖集，优先保证正确性。
	 *
	 * @return 当前 merger 需要监听的依赖标记
	 */
	UFUNCTION(BlueprintNativeEvent, Category = TireflyCombatSystem)
	ETcsBuffMergeDependencyFlags GetDependencyFlags() const;
	virtual ETcsBuffMergeDependencyFlags GetDependencyFlags_Implementation() const;

	/**
	 * Buff 合并
	 *
	 * @param BuffsToMerge 要合并的 Buff 实例列表
	 * @param MergedBuffs 合并后的保留 Buff 实例列表
	 * @param MergedOutBuffs 合并过程中被淘汰的 Buff 实例列表
	 */
	UFUNCTION(BlueprintNativeEvent, Category = TireflyCombatSystem)
	void Merge(
		UPARAM(ref) TArray<UTcsBuffInstance*>& BuffsToMerge,
		TArray<UTcsBuffInstance*>& MergedBuffs,
		TArray<UTcsBuffInstance*>& MergedOutBuffs);
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<UTcsBuffInstance*>& BuffsToMerge,
		TArray<UTcsBuffInstance*>& MergedBuffs,
		TArray<UTcsBuffInstance*>& MergedOutBuffs) {}
};