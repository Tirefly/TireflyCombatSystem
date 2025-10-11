// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TcsStateMerger.generated.h"



class UTcsStateInstance;



// 状态合并器
UCLASS(BlueprintType, Blueprintable, Abstract, ClassGroup = (TcsCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsStateMerger : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 状态合并
	 * 
	 * @param StatesToMerge 要合并的状态列表
	 * @param MergedStates 合并后的状态列表
	 */
	UFUNCTION(BlueprintNativeEvent, Category = TcsCombatSystem)
	void Merge(
		UPARAM(ref) TArray<UTcsStateInstance*>& StatesToMerge,
		TArray<UTcsStateInstance*>& MergedStates);
	virtual void Merge_Implementation(
		UPARAM(ref) TArray<UTcsStateInstance*>& StatesToMerge,
		TArray<UTcsStateInstance*>& MergedStates) {}
}; 