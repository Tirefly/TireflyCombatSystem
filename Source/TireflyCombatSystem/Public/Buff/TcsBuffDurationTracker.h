// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TcsBuffDurationTracker.generated.h"



class UTcsBuffInstance;



/**
 * Buff 持续时间跟踪器：仅追踪有有限时长的 Buff 实例，
 * 对标 TickComponent 的频率执行时间递减。
 */
USTRUCT()
struct FTcsBuffDurationTracker
{
	GENERATED_BODY()

public:
	/** 把一个 Buff 实例加入持续时间跟踪集合。 */
	void Add(UTcsBuffInstance* BuffInstance);

	/** 把一个 Buff 实例移出持续时间跟踪集合。 */
	void Remove(UTcsBuffInstance* BuffInstance);

	/** 清理当前跟踪集合中的失效或已过期 Buff 实例。 */
	void RefreshInstances();

	/** @return 只读的跟踪集合（供 BuffComponent 的 Tick 迭代使用）。 */
	const TSet<TObjectPtr<UTcsBuffInstance>>& GetTrackedInstances() const { return TrackedInstances; }

private:
	/** 当前需要参与有限时长 Tick 的 Buff 实例集合。 */
	UPROPERTY()
	TSet<TObjectPtr<UTcsBuffInstance>> TrackedInstances;
};