// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Skill/TcsSkillModifierInstance.h"
#include "State/TcsStateParamInstance.h"
#include "StructUtils/InstancedStruct.h"
#include "TcsSourceHandle.h"
#include "TcsSkillModifierRuntime.generated.h"



class UTcsSkillEntry;
class UTcsSkillModifierDefinition;


/**
 * SkillModifier 冲突组键。
 *
 * 用于把同一目标参数上的同组 Modifier 归并到同一个互斥恢复集合中。
 */
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTcsSkillModifierConflictKey
{
	GENERATED_BODY()

public:
	/** SkillModifier 定义标识符。 */
	UPROPERTY()
	FName SkillModifierDefId;

	/** 目标 SkillEntry。使用弱引用，避免阻止 SkillEntry 正常回收。 */
	UPROPERTY()
	TWeakObjectPtr<UTcsSkillEntry> TargetSkillEntry;

public:
	/**
	 * 判断两个冲突组键是否指向同一组互斥集合。
	 *
	 * @return 当 SkillModifierDefId 和 TargetSkillEntry 全部一致时返回 true。
	 */
	bool operator==(const FTcsSkillModifierConflictKey& Other) const;
};


/**
 * 计算 SkillModifier 冲突组键的哈希值。
 *
 * @return 可用于 `TMap` / `TSet` 的哈希值。
 */
TIREFLYCOMBATSYSTEM_API uint32 GetTypeHash(const FTcsSkillModifierConflictKey& Key);


/**
 * SkillModifier 账本层运行时记录。
 *
 * 该结构表示 selector 展开后的单条落地记录，而不是一次 apply 请求或 batch 容器。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSkillModifierRuntimeEntry
{
	GENERATED_BODY()

public:
	/** 账本层唯一运行时 ID。 */
	UPROPERTY()
	int32 RuntimeModifierId = INDEX_NONE;

	/** SkillModifier 定义标识符。 */
	UPROPERTY()
	FName SkillModifierDefId;

	/** 来源定义资产。使用弱引用，避免账本反向延长 Def 生命周期。 */
	UPROPERTY()
	TWeakObjectPtr<UTcsSkillModifierDefinition> Definition;

	/** 当前记录命中的目标 SkillEntry。 */
	UPROPERTY()
	TWeakObjectPtr<UTcsSkillEntry> TargetSkillEntry;

	/** 当前记录命中的目标参数标签。 */
	UPROPERTY()
	FGameplayTag TargetParamTag;

	/** 当前记录命中的目标参数类型。 */
	UPROPERTY()
	ETcsStateParameterType TargetParamType = ETcsStateParameterType::SPT_Numeric;

	/** 当前记录的来源句柄。 */
	UPROPERTY()
	FTcsSourceHandle SourceHandle;

	/** 当前记录的优先级。 */
	UPROPERTY()
	int32 Priority = 0;

	/** 当前记录的合并策略。 */
	UPROPERTY()
	ETcsSkillModifierMergePolicy MergePolicy = ETcsSkillModifierMergePolicy::Stack;

	/** 当前记录是否处于激活状态。 */
	UPROPERTY()
	bool bActive = true;

	/** 已解析完成的求值器对象。 */
	UPROPERTY()
	TObjectPtr<UObject> ResolvedEvaluator = nullptr;

	/** 已解析完成的求值器配置。 */
	UPROPERTY()
	FInstancedStruct ResolvedConfig;

public:
	/**
	 * 构造当前记录所属的冲突组键。
	 *
	 * @return 由 SkillModifierDefId 和 TargetSkillEntry 组成的冲突组键。
	 */
	FTcsSkillModifierConflictKey MakeConflictKey() const;
};


/**
 * SkillModifier 运行时索引聚合结构。
 *
 * 负责统一维护账本主表与多组辅助索引，但不直接操作 SkillEntry 的参数实例链。
 */
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTcsSkillModifierRuntimeIndex
{
	GENERATED_BODY()

public:
	/** 运行时账本主表：RuntimeModifierId -> RuntimeEntry。 */
	UPROPERTY()
	TMap<int32, FTcsSkillModifierRuntimeEntry> RuntimeEntriesById;

	/** 来源索引：SourceHandle.Id -> RuntimeModifierId[]。 */
	TMap<int32, TArray<int32>> RuntimeIdsBySourceHandleId;

	/** 目标 SkillEntry 索引：TargetSkillEntry -> RuntimeModifierId[]。 */
	TMap<TWeakObjectPtr<UTcsSkillEntry>, TArray<int32>> RuntimeIdsByTargetEntry;

	/** 冲突组索引：(SkillModifierDefId + TargetSkillEntry) -> RuntimeModifierId[]。 */
	TMap<FTcsSkillModifierConflictKey, TArray<int32>> RuntimeIdsByConflictKey;

public:
	/**
	 * 向账本与全部辅助索引中注册一条运行时记录。
	 *
	 * @return 当记录有效且成功写入主表与辅助索引时返回 true。
	 */
	bool AddRuntimeEntry(const FTcsSkillModifierRuntimeEntry& Entry);

	/**
	 * 从账本与全部辅助索引中移除一条运行时记录。
	 *
	 * @param RuntimeModifierId 要移除的运行时记录 ID。
	 * @param OutRemovedEntry 可选输出，被移除的完整运行时记录。
	 * @return 当记录存在且已成功移除时返回 true。
	 */
	bool RemoveRuntimeEntry(int32 RuntimeModifierId, FTcsSkillModifierRuntimeEntry* OutRemovedEntry = nullptr);

	/**
	 * 按 runtime id 查询单条运行时记录的只读视图。
	 *
	 * @param RuntimeModifierId 要查询的运行时记录 ID。
	 * @return 命中时返回记录指针，否则返回 nullptr。
	 */
	const FTcsSkillModifierRuntimeEntry* FindRuntimeEntry(int32 RuntimeModifierId) const;

	/**
	 * 按 runtime id 查询单条运行时记录的可写视图。
	 *
	 * @param RuntimeModifierId 要查询的运行时记录 ID。
	 * @return 命中时返回记录指针，否则返回 nullptr。
	 */
	FTcsSkillModifierRuntimeEntry* FindRuntimeEntryMutable(int32 RuntimeModifierId);

	/**
	 * 按来源句柄查找所有运行时记录。
	 *
	 * @param SourceHandle 要查询的来源句柄。
	 * @param OutEntries 输出的运行时记录视图集合。
	 * @return 当找到了至少一条匹配记录时返回 true。
	 */
	bool FindBySourceHandle(const FTcsSourceHandle& SourceHandle, TArray<const FTcsSkillModifierRuntimeEntry*>& OutEntries) const;

	/**
	 * 按目标 SkillEntry 查找所有运行时记录。
	 *
	 * @param SkillEntry 要查询的目标 SkillEntry。
	 * @param OutEntries 输出的运行时记录视图集合。
	 * @return 当找到了至少一条匹配记录时返回 true。
	 */
	bool FindBySkillEntry(UTcsSkillEntry* SkillEntry, TArray<const FTcsSkillModifierRuntimeEntry*>& OutEntries) const;

	/**
	 * 按冲突组键查找所有运行时记录。
	 *
	 * @param Key 要查询的冲突组键。
	 * @param OutEntries 输出的运行时记录视图集合。
	 * @return 当找到了至少一条匹配记录时返回 true。
	 */
	bool FindConflictSet(const FTcsSkillModifierConflictKey& Key, TArray<const FTcsSkillModifierRuntimeEntry*>& OutEntries) const;

	/**
	 * 按目标 SkillEntry 批量移除全部运行时记录。
	 *
	 * @param SkillEntry 要清理的目标 SkillEntry。
	 * @param OutRemovedEntries 输出的被移除记录集合。
	 * @return 当至少移除了 1 条记录时返回 true。
	 */
	bool RemoveAllForSkillEntry(UTcsSkillEntry* SkillEntry, TArray<FTcsSkillModifierRuntimeEntry>& OutRemovedEntries);

	/**
	 * 清空账本主表与全部辅助索引。
	 *
	 * @return 无返回值。
	 */
	void Reset();
};
