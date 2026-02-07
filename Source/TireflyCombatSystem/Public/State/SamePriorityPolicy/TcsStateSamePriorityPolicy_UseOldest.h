// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/SamePriorityPolicy/TcsStateSamePriorityPolicy.h"
#include "TcsStateSamePriorityPolicy_UseOldest.generated.h"



/**
 * UseOldest 策略：使用最旧的状态
 *
 * 排序键 = -ApplyTimestamp（越旧越大）
 *
 * 适用场景：
 * - 技能队列槽位：先释放的技能先执行（FIFO 队列）
 * - 任务队列槽位：先接受的任务先完成
 */
UCLASS(BlueprintType, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsStateSamePriorityPolicy_UseOldest : public UTcsStateSamePriorityPolicy
{
	GENERATED_BODY()

public:
	virtual int64 GetOrderKey_Implementation(const UTcsStateInstance* State) const override;
};
