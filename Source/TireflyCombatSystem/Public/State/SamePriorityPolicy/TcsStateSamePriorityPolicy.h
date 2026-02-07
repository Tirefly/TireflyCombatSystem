// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TcsStateSamePriorityPolicy.generated.h"



class UTcsStateInstance;



/**
 * 同优先级状态排序策略基类
 *
 * 用于在 PriorityOnly 槽位中，当多个状态具有相同优先级时，决定哪个状态应该被激活。
 *
 * 使用场景：
 * - Buff 槽位：通常使用 UseNewest 策略，新的 Buff 覆盖旧的
 * - 技能队列槽位：通常使用 UseOldest 策略，先释放的技能先执行
 */
UCLASS(BlueprintType, Blueprintable, Abstract, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsStateSamePriorityPolicy : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 获取状态的排序键
	 *
	 * @param State 状态实例
	 * @return 排序键（越大越靠前，即优先级越高）
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "TireflyCombatSystem|State")
	int64 GetOrderKey(const UTcsStateInstance* State) const;
	virtual int64 GetOrderKey_Implementation(const UTcsStateInstance* State) const { return 0; }
};
