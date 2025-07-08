// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StructUtils/InstancedStruct.h"
#include "TireflyStateParameter.generated.h"



class UTireflyStateInstance;



// 状态参数值解析器
UCLASS(BlueprintType, Blueprintable, Abstract, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTireflyStateParamParser : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 解析状态参数值
	 * 
	 * @param Instigator 发起者
	 * @param Target 目标
	 * @param StateInstance 状态实例
	 * @param InstancedStruct 参数值结构体
	 * @param OutValue 解析的参数值结果
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "TireflyCombatSystem|State")
	void ParseStateParameter(
		AActor* Instigator,
		AActor* Target,
		UTireflyStateInstance* StateInstance,
		const FInstancedStruct& InstancedStruct,
		float& OutValue) const;
	virtual void ParseStateParameter_Implementation(
		AActor* Instigator,
		AActor* Target,
		UTireflyStateInstance* StateInstance,
		const FInstancedStruct& InstancedStruct,
		float& OutValue) const {}
};
