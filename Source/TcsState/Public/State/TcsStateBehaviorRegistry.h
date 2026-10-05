// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EventBus/TcsEventBus.h"
#include "GameplayTagContainer.h"

#include "State/TcsStateHandle.h"

class UTcsEventBusSubsystem;
class UTcsStateBehaviorHandler;
struct FTcsBuffDef;



/**
 * 行为兴趣登记表：每个精确 Tag 共用一条立即订阅，末个实例移除时配对退订。
 *
 * 本表只持句柄、Tag 与 Handler 弱引用，零 GC 所有权，不需要 AddReferencedObjects。
 * Fragment 留在 Def，Handler 的 UPROPERTY 强引用留在门面，实例不持策略或订阅句柄。
 * 派发者先收集实例句柄快照，再逐回调校验登记与实例活性；回调中移除不会使遍历悬空。
 */
class TCSSTATE_API FTcsStateBehaviorRegistry
{
// 装配
#pragma region Setup

public:
	// 设置共享 Handler 的弱引用（强持有归门面）
	void SetHandler(UTcsStateBehaviorHandler* InHandler);

	// 全量退订并清表；Bus 为空表示世界已拆，此时只清本地登记
	void Reset(UTcsEventBusSubsystem* Bus);

#pragma endregion


// 登记与撤销
#pragma region Registration

public:
	/**
	 * 解析定义的行为兴趣并登记实例。无兴趣是正常路径，不留下空实例登记。
	 * 无效片段在本次登记时各留一次 Warning，派发时不重复报错。
	 *
	 * @param Handle 已在册实例句柄（跨单位唯一代际由状态槽位分配器保证）。
	 * @param Def 定义内容（只读片段与兴趣，不持有其引用）。
	 * @param Bus 总线门面（有兴趣时须非空）。
	 * @return 返回登记是否成功；重复登记幂等，解析失败片段按无行为处理。
	 */
	bool AddInstance(
		FTcsStateHandle Handle,
		const FTcsBuffDef& Def,
		UTcsEventBusSubsystem* Bus);

	/**
	 * 摘除该实例的全部兴趣，某 Tag 末个实例离开时退订。
	 *
	 * @param Handle 待摘实例句柄。
	 * @param Bus 总线门面（可为空，此时只清本地登记）。
	 * @return 返回是否确实摘掉登记；重复摘除返回 false，不 ensure。
	 */
	bool RemoveInstance(
		FTcsStateHandle Handle,
		UTcsEventBusSubsystem* Bus);

#pragma endregion


// 查询
#pragma region Query

public:
	/**
	 * 收集某事件 Tag 的实例句柄快照，保持登记顺序。
	 *
	 * @param EventTag 精确兴趣 Tag。
	 * @param OutHandles 输出数组（追加，不清空调用方内容）。
	 */
	void CollectInterests(
		FGameplayTag EventTag,
		TArray<FTcsStateHandle>& OutHandles) const;

	/**
	 * 本实例是否仍登记该兴趣（回调前复核用，防止快照内已移除者收到后续回调）。
	 *
	 * @param Handle 实例句柄。
	 * @param EventTag 本次事件 Tag。
	 * @return 返回登记是否仍存在。
	 */
	bool IsInterested(
		FTcsStateHandle Handle,
		FGameplayTag EventTag) const;

	// 实际总线订阅数（按 Tag 计数，不随实例数膨胀）
	int32 GetSubscriptionCount() const;

	// 至少登记一个有效兴趣的实例数
	int32 GetInstanceCount() const;

#pragma endregion


// 内核
#pragma region Core

private:
	// Tag 到总线订阅句柄（每 Tag 一条）
	TMap<FGameplayTag, FTcsEventSubscriptionHandle> TagSubscriptions;

	// Tag 到兴趣该 Tag 的实例（登记顺序）
	TMap<FGameplayTag, TArray<FTcsStateHandle>> TagInstances;

	// 实例到去重后的兴趣列表（一次移除摘净全部兴趣）
	TMap<FTcsStateHandle, TArray<FGameplayTag>> InstanceInterests;

	// Handler 弱引用（本表不延长对象生命周期）
	TWeakObjectPtr<UTcsStateBehaviorHandler> Handler;

#pragma endregion
};
