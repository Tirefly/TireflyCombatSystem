// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EventBus/TcsEventBus.h"
#include "GameplayTagContainer.h"

#include "Chain/TcsChainRun.h"

class UTcsChainEventWaitHandler;
class UTcsEventBusSubsystem;



/**
 * 事件等待表（唤醒源②"事件匹配"的登记面；**纯逻辑类**，非 UObject——与 `FTcsTriggerRegistry`
 * 同款分工：内核住纯 C++ 类，UObject 门面只做反射面与生命周期）。
 *
 * **职责**：按 tag 登记"哪些运行态在等这个事件" + **订阅计数配对**（该 tag 首个等待者订阅一次、
 * 最后一个等待者离开时退订）。**MUST NOT** 每个等待者各订一次，两条硬依据：
 * - 总线 `Subscribe` 只收 `UTcsEventHandler*`（传不了 lambda），派发只传 `(EventTag, Payload)`、
 *   **不传订阅句柄** ⇒ 每条等待各订一次并不能让回调自辨身份（收益为零、成本为正）；
 * - 既有明文纪律（`FTcsTriggerRegistry` 类注释）：那会让订阅表随数量膨胀且退订易漏。
 *
 * **本表零 GC 引用**（句柄与 tag 都是值类型；事件载荷在派发当场写进运行态黑板、不留存）⇒
 * **不需要** `AddReferencedObjects`——与触发行登记表（持 `FInstancedStruct`）形态不同，别照抄那一半。
 *
 * **"一条订阅"的口径**：本表内**每个 tag 一条**。同一 tag 若同时被触发行与链等待使用，总线上会有
 * **两条**订阅——两条各属一个 Handler，是必需而非泄漏（一条订阅只对一个 Handler 派发）。
 * 人工检查"订阅数不随等待者数量增长"时按本口径读。
 *
 * **摘除的三条路径**（与规格的"退订三路径"一一对应）：①事件命中——**门面在 `ResumeRun` 之前**摘
 * （不依赖步骤行为）；②超时——门面未参与该路，由步骤唤醒后自行摘；③运行态释放——`ReleaseRun` 摘。
 * 三条都收敛到 `RemoveWaiter`。
 *
 * 单游戏线程访问（D0-4）。
 */
class TCSEFFECT_API FTcsChainEventWaitRegistry
{
// 装配
#pragma region Setup

public:
	/**
	 * 装配共享 Handler（订阅的承接者）。由门面在创建后调用一次——
	 * **本表只持弱引用**（与总线订阅表同款），Handler 的强引用归门面 `UPROPERTY`。
	 *
	 * @param InHandler 共享 Handler（门面持有其生命周期）。
	 */
	void SetHandler(UTcsChainEventWaitHandler* InHandler);

	/**
	 * 全清（世界反初始化/确定性清理）：**全量退订** + 清空两张表——不留跨世界残留订阅。
	 *
	 * @param Bus 总线门面（可为空 = 世界已拆、总线不可得，则只清表）。
	 */
	void Reset(UTcsEventBusSubsystem* Bus);

#pragma endregion


// 登记
#pragma region Registration

public:
	/**
	 * 登记等待者（装"等事件"锚）：该 tag 首个等待者时**订阅一次**。
	 *
	 * 拒绝面（Warning + 返回 false，**不 ensure**——都是时序或配置问题而非契约违规）：
	 * `Bus` 为空（世界拆解期）、`EventTag` 无效、订阅被总线拒绝。
	 *
	 * @param Handle 等待的运行态句柄。
	 * @param EventTag 等的事件 Tag。
	 * @param Bus 总线门面（订阅装配用；空 = 拒绝登记）。
	 * @return 返回是否登记成功。
	 */
	bool AddWaiter(FTcsChainRunHandle Handle, FGameplayTag EventTag, UTcsEventBusSubsystem* Bus);

	/**
	 * 摘等待者（**三条路径共用**：命中 / 超时 / 运行态释放）：计数归零即退订。
	 *
	 * @param Handle 等待的运行态句柄。
	 * @param EventTag 等的事件 Tag。
	 * @param Bus 总线门面（计数归零时退订；可为空 = 只摘登记不退订）。
	 * @return 返回是否确实摘掉了一条登记（重复摘除/未登记返回 false——正常竞态，不 ensure）。
	 */
	bool RemoveWaiter(FTcsChainRunHandle Handle, FGameplayTag EventTag, UTcsEventBusSubsystem* Bus);

#pragma endregion


// 查询
#pragma region Query

public:
	/**
	 * 取某 tag 的等待者**快照**（唤醒路由用——**MUST 用副本**：遍历中会因 `ResumeRun` 执行步骤
	 * 而新增/摘除等待者，直接遍历表本体等于边遍历边改容器）。
	 *
	 * @param EventTag 事件 Tag。
	 * @param OutHandles 输出等待者句柄（登记序；本方法不保证调用方传入数组已清空——由调用方负责）。
	 */
	void CollectWaiters(FGameplayTag EventTag, TArray<FTcsChainRunHandle>& OutHandles) const;

	// 订阅表条数（观测/人工检查用：应等于"被等待的 tag 数"，与等待者数量无关）
	int32 GetSubscriptionCount() const
	{
		return TagSubscriptions.Num();
	}

	// 等待者总数（观测/人工检查用）
	int32 GetWaiterCount() const;

#pragma endregion


// 内核
#pragma region Core

private:
	// 摘除后的收尾：该 tag 计数归零则退订 + 移除订阅表项
	void DropSubscriptionIfUnused(FGameplayTag EventTag, UTcsEventBusSubsystem* Bus);

	// Tag → 订阅句柄（**同一 tag 共用一条**；计数归零时移除）
	TMap<FGameplayTag, FTcsEventSubscriptionHandle> TagSubscriptions;

	// Tag → 等待者集合（**值语义**、登记序；不持运行态指针——池扩容即搬移）
	TMap<FGameplayTag, TArray<FTcsChainRunHandle>> TagWaiters;

	// 共享 Handler（弱引用——与总线订阅表同款；强引用归门面 UPROPERTY，不 root 会被 GC 掉）
	TWeakObjectPtr<UTcsChainEventWaitHandler> Handler;

#pragma endregion
};
