// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buff/BuffMerger/TcsBuffMergeRuntime.h"
#include "GameplayTagContainer.h"
#include "TcsStateSlot.generated.h"



class UTcsStateInstance;
class UTcsStateSlotDefinition;



// 状态槽激活模式枚举
UENUM(BlueprintType)
enum class ETcsStateSlotActivationMode : uint8
{
	SSAM_PriorityOnly    UMETA(DisplayName = "Priority Only",
		ToolTip = "优先级激活模式：只有最高优先级状态激活，适用于互斥行为如Action"),
	SSAM_AllActive       UMETA(DisplayName = "All Active",
		ToolTip = "全部激活模式：所有状态都可同时激活，适用于可共存效果如Buff/Debuff")
};



// Gate关闭行为
UENUM(BlueprintType)
enum class ETcsStateSlotGateClosePolicy : uint8
{
	SSGCP_HangUp	UMETA(DisplayName = "Hang Up States",
		ToolTip = "Gate关闭时挂起槽位中的状态：\n- 持续时间：继续计时\n- 叠层合并：正常参与（保持叠层数准确）\n- 逻辑执行：暂停StateTree执行"),
	SSGCP_Pause		UMETA(DisplayName = "Pause States",
		ToolTip = "Gate关闭时暂停槽位中的状态：\n- 持续时间：完全冻结\n- 叠层合并：正常参与（保持叠层数准确）\n- 逻辑执行：暂停StateTree执行"),
	SSGCP_Cancel	UMETA(DisplayName = "Cancel States",
		ToolTip = "Gate关闭时直接取消槽位中的状态")
};



// 优先级抢占策略
UENUM(BlueprintType)
enum class ETcsStatePreemptionPolicy : uint8
{
	SPP_HangUpLowerPriority		UMETA(DisplayName = "Hang Up Lower Priority",
		ToolTip = "高优先级状态抢占时，低优先级状态进入挂起：\n- 持续时间：继续计时\n- 叠层合并：正常参与（保持叠层数准确）\n- 逻辑执行：暂停StateTree执行"),
	SPP_PauseLowerPriority		UMETA(DisplayName = "Pause Lower Priority",
		ToolTip = "高优先级状态抢占时，低优先级状态进入暂停：\n- 持续时间：完全冻结\n- 叠层合并：正常参与（保持叠层数准确）\n- 逻辑执行：暂停StateTree执行"),
	SPP_CancelLowerPriority		UMETA(DisplayName = "Cancel Lower Priority",
		ToolTip = "高优先级状态抢占时，低优先级状态被取消")
};






// 状态槽数据容器
USTRUCT()
struct FTcsStateSlot
{
	GENERATED_BODY()

public:
	// 槽位定义资产缓存（运行时构建时写入，用于避免反复回表查询）
	UPROPERTY(Transient)
	TObjectPtr<UTcsStateSlotDefinition> StateSlotDef = nullptr;

	// 槽位中的状态实例数组
	UPROPERTY()
	TArray<UTcsStateInstance*> States;

	// Gate状态 (用于StateTree联动,控制槽位是否允许激活状态)
	UPROPERTY()
	bool bIsGateOpen = true;

	// 当前槽位维护的 Buff merge group 运行时缓存。
	UPROPERTY(Transient)
	TMap<FName, FTcsBuffMergeGroupRuntime> BuffMergeGroups;

	// 当前槽位待处理的 Buff merge dirty group 集合。
	UPROPERTY(Transient)
	TSet<FName> DirtyBuffMergeStateDefIds;

	// 当前槽位是否需要先从 States 全量重建 Buff merge group runtime。
	UPROPERTY(Transient)
	bool bBuffMergeRequiresFullRebuild = true;

	// 缓存对应的槽位定义资产
	void CacheStateSlotDef(const UTcsStateSlotDefinition* InStateSlotDef)
	{
		StateSlotDef = const_cast<UTcsStateSlotDefinition*>(InStateSlotDef);
	}

	// 获取缓存的槽位定义资产
	const UTcsStateSlotDefinition* GetStateSlotDef() const
	{
		return StateSlotDef.Get();
	}

	// 标记指定 StateDefId 对应的 Buff merge group 需要重新处理。
	void MarkBuffMergeGroupDirty(FName StateDefId, ETcsBuffMergeDirtyReason DirtyReason)
	{
		if (StateDefId.IsNone())
		{
			return;
		}

		FTcsBuffMergeGroupRuntime& GroupRuntime = BuffMergeGroups.FindOrAdd(StateDefId);
		GroupRuntime.StateDefId = StateDefId;
		GroupRuntime.MarkDirty(DirtyReason);
		DirtyBuffMergeStateDefIds.Add(StateDefId);
	}

	// 把当前槽位中的所有已知 Buff merge group 标记为脏。
	void MarkAllBuffMergeGroupsDirty(ETcsBuffMergeDirtyReason DirtyReason)
	{
		for (TPair<FName, FTcsBuffMergeGroupRuntime>& Pair : BuffMergeGroups)
		{
			Pair.Value.MarkDirty(DirtyReason);
			DirtyBuffMergeStateDefIds.Add(Pair.Key);
		}
	}

	// 标记当前槽位的 Buff merge runtime 需要执行一次全量重建。
	void MarkBuffMergeRequiresFullRebuild()
	{
		bBuffMergeRequiresFullRebuild = true;
	}

	FTcsStateSlot()
		: StateSlotDef(nullptr)
		, bIsGateOpen(true)
		, bBuffMergeRequiresFullRebuild(true)
	{}
};
