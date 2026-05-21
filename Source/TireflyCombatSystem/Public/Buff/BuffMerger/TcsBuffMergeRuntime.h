// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buff/TcsBuffInstance.h"
#include "TcsBuffMergeRuntime.generated.h"



/**
 * Buff merge 组的 dirty 原因。
 */
UENUM()
enum class ETcsBuffMergeDirtyReason : uint8
{
	None = 0,
	MembershipChanged = 1 << 0,
	RuntimeValueChanged = 1 << 1,
	ExecutionStageChanged = 1 << 2,
	SlotGateChanged = 1 << 3,
	ForceRebuild = 1 << 4,
};

ENUM_CLASS_FLAGS(ETcsBuffMergeDirtyReason)



/**
 * Buff merger 依赖的运行时输入标记。
 */
UENUM()
enum class ETcsBuffMergeDependencyFlags : uint8
{
	None = 0,
	MemberSet = 1 << 0,
	ApplyTimestamp = 1 << 1,
	Instigator = 1 << 2,
	RuntimeStack = 1 << 3,
	ExecutionStage = 1 << 4,
	SlotGateState = 1 << 5,
};

ENUM_CLASS_FLAGS(ETcsBuffMergeDependencyFlags)



/**
 * 单个 Buff merge group 的运行时缓存。
 */
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTcsBuffMergeGroupRuntime
{
	GENERATED_BODY()

public:
	/** 当前分组对应的 StateDefId。 */
	UPROPERTY()
	FName StateDefId = NAME_None;

	/** 当前分组中的 Buff 实例弱引用集合。 */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UTcsBuffInstance>> Members;

	/** 当前分组累计的 dirty 原因。 */
	UPROPERTY()
	ETcsBuffMergeDirtyReason DirtyReasons = ETcsBuffMergeDirtyReason::None;

	/** 最近一次在处理链中消费的 dirty 原因。 */
	UPROPERTY()
	ETcsBuffMergeDirtyReason LastProcessedDirtyReasons = ETcsBuffMergeDirtyReason::None;

	/** 最近一次解析到的 merger 依赖集合。 */
	UPROPERTY()
	ETcsBuffMergeDependencyFlags DependencyFlags = ETcsBuffMergeDependencyFlags::None;

public:
	/**
	 * 记录一个 Buff 成员。
	 *
	 * @param BuffInstance 需要加入当前分组的 Buff 实例
	 */
	void AddMember(UTcsBuffInstance* BuffInstance)
	{
		if (BuffInstance)
		{
			Members.AddUnique(BuffInstance);
		}
	}

	/**
	 * 用新的强引用列表覆盖当前成员缓存。
	 *
	 * @param InMembers 当前分组最终保留的成员集合
	 */
	void SetMembers(const TArray<UTcsBuffInstance*>& InMembers)
	{
		Members.Reset(InMembers.Num());
		for (UTcsBuffInstance* BuffInstance : InMembers)
		{
			AddMember(BuffInstance);
		}
	}

	/**
	 * 清理失效成员，并输出仍然有效的 Buff 实例。
	 *
	 * @param OutMembers 输出的有效成员集合
	 */
	void GatherValidMembers(TArray<UTcsBuffInstance*>& OutMembers)
	{
		OutMembers.Reset();
		Members.RemoveAll([&OutMembers](const TWeakObjectPtr<UTcsBuffInstance>& Entry)
		{
			if (UTcsBuffInstance* BuffInstance = Entry.Get())
			{
				OutMembers.Add(BuffInstance);
				return false;
			}

			return true;
		});
	}

	/**
	 * 追加 dirty reason。
	 *
	 * @param InDirtyReasons 需要并入的 dirty 原因
	 */
	void MarkDirty(ETcsBuffMergeDirtyReason InDirtyReasons)
	{
		DirtyReasons |= InDirtyReasons;
	}

	/**
	 * 清除指定 dirty reason。
	 *
	 * @param InDirtyReasons 需要清理的 dirty 原因
	 */
	void ClearDirty(ETcsBuffMergeDirtyReason InDirtyReasons)
	{
		DirtyReasons &= ~InDirtyReasons;
	}

	/**
	 * 判断当前分组是否仍有 dirty reason。
	 *
	 * @return 仍有 dirty 原因时返回 true
	 */
	bool HasDirty() const
	{
		return DirtyReasons != ETcsBuffMergeDirtyReason::None;
	}
};



/**
 * Buff merge runtime 相关的调试与格式化工具。
 */
namespace TcsBuffMergeRuntime
{
	/**
	 * 把 dirty reason flags 格式化为可读字符串。
	 *
	 * @param DirtyReasons 需要格式化的 dirty reason flags
	 * @return 调试输出使用的字符串
	 */
	TIREFLYCOMBATSYSTEM_API FString FormatDirtyReasons(ETcsBuffMergeDirtyReason DirtyReasons);

	/**
	 * 把 dependency flags 格式化为可读字符串。
	 *
	 * @param DependencyFlags 需要格式化的 dependency flags
	 * @return 调试输出使用的字符串
	 */
	TIREFLYCOMBATSYSTEM_API FString FormatDependencyFlags(ETcsBuffMergeDependencyFlags DependencyFlags);

	/**
	 * 统计当前 group 中仍然有效的成员数量。
	 *
	 * @param GroupRuntime 目标 Buff merge group runtime
	 * @return 当前有效成员数
	 */
	TIREFLYCOMBATSYSTEM_API int32 CountValidMembers(const FTcsBuffMergeGroupRuntime& GroupRuntime);
}