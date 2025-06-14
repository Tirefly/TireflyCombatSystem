// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TireflyState.h"
#include "TireflyStateMerger.generated.h"

// 状态合并器
UCLASS(BlueprintType, Blueprintable, Abstract, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTireflyStateMerger : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 状态合并
	 * 
	 * @param StatesToMerge 要合并的状态列表
	 * @param MergedStates 合并后的状态列表
	 * @param bSameInstigator 是否来自同一个发起者
	 */
	UFUNCTION(BlueprintNativeEvent, Category = TireflyCombatSystem)
	void Merge(
		UPARAM(ref) TArray<FTireflyStateInstance>& StatesToMerge,
		TArray<FTireflyStateInstance>& MergedStates,
		bool bSameInstigator);
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<FTireflyStateInstance>& StatesToMerge,
		TArray<FTireflyStateInstance>& MergedStates,
		bool bSameInstigator) {}
}; 