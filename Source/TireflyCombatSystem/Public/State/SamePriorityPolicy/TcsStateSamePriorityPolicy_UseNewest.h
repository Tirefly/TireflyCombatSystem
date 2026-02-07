// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/SamePriorityPolicy/TcsStateSamePriorityPolicy.h"
#include "TcsStateSamePriorityPolicy_UseNewest.generated.h"



/**
 * UseNewest 策略：使用最新的状态
 *
 * 排序键 = ApplyTimestamp（越新越大）
 *
 * 适用场景：
 * - Buff 槽位：新的 Buff 覆盖旧的同优先级 Buff
 * - 状态槽位：新的状态替换旧的同优先级状态
 */
UCLASS(BlueprintType, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsStateSamePriorityPolicy_UseNewest : public UTcsStateSamePriorityPolicy
{
	GENERATED_BODY()

public:
	virtual int64 GetOrderKey_Implementation(const UTcsStateInstance* State) const override;
};
