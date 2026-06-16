// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TcsSkillCooldownTracker.generated.h"



class UTcsSkillEntry;



/**
 * 技能冷却跟踪器：仅追踪处于冷却中的 SkillEntry。
 * 对标 FTcsBuffDurationTracker，在 TickComponent 中驱动时间递减，
 * 冷却结束时自动移出。
 */
USTRUCT()
struct FTcsSkillCooldownTracker
{
	GENERATED_BODY()

public:
	/** 将 Entry 加入冷却跟踪（由 StartCooldown 成功后调用）。 */
	void Add(UTcsSkillEntry* Entry);

	/** 将 Entry 移出冷却跟踪。 */
	void Remove(UTcsSkillEntry* Entry);

	/** 递减所有跟踪 Entry 的剩余冷却时间，并移除已到期条目。 */
	void Tick(float DeltaTime);

private:
	UPROPERTY()
	TMap<FName, TWeakObjectPtr<UTcsSkillEntry>> TrackedEntries;
};