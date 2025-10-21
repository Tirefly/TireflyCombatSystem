// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StructUtils/InstancedStruct.h"
#include "TcsStateCondition.generated.h"



class UTcsStateInstance;



// 状态条件配置
USTRUCT(BlueprintType)
struct FTcsStateConditionConfig
{
	GENERATED_BODY()

public:
	// 状态条件类型
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UTcsStateCondition> ConditionClass;

	// 条件执行时的Payload数据
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FInstancedStruct Payload;

public:
	// 构造函数
	FTcsStateConditionConfig() = default;

	// 检查配置是否有效
	bool IsValid() const;
};



// 状态条件
UCLASS(BlueprintType, Blueprintable, Abstract, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsStateCondition : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 检查状态条件是否满足
	 * 
	 * @param StateInstance 状态实例，包含所有需要的信息
	 * @param Payload 条件执行时的Payload数据
	 * @param CurrentGameTime 当前游戏时间（可选，用于时间相关条件）
	 * @return 条件是否满足
	 */
	UFUNCTION(BlueprintNativeEvent, Category = TireflyCombatSystem)
	bool CheckCondition(
		UTcsStateInstance* StateInstance,
		const FInstancedStruct& Payload,
		float CurrentGameTime = 0.0f);
	virtual bool CheckCondition_Implementation(
		UTcsStateInstance* StateInstance,
		const FInstancedStruct& Payload,
		float CurrentGameTime = 0.0f) 
	{ 
		return false; 
	}
};
