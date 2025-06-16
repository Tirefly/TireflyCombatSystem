// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TireflyStateMerger.generated.h"



class UTireflyStateInstance;



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
	 */
	UFUNCTION(BlueprintNativeEvent, Category = TireflyCombatSystem)
	void Merge(
		UPARAM(ref) TArray<UTireflyStateInstance*>& StatesToMerge,
		TArray<UTireflyStateInstance*>& MergedStates);
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<UTireflyStateInstance*>& StatesToMerge,
		TArray<UTireflyStateInstance*>& MergedStates) {}
}; 