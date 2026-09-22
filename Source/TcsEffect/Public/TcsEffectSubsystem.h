// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ScriptInterface.h"

#include "Chain/TcsChainRun.h"
#include "Chain/TcsEffectChain.h"
#include "Chain/TcsEffectContext.h"
#include "Chain/TcsEffectStep.h"
#include "Host/TcsEntityQuery.h"
#include "Pool/TcsInstancePool.h"

#include "TcsEffectSubsystem.generated.h"



/**
 * 效果链门面与解释器（M4b 机制层）：世界级子系统，持**链定义登记表**与**链运行态池**。
 *
 * 分工（D4-14 注册制分派）：本模块只做"链 = 数据 + 解释器 + 执行器注册表"——步骤类型的执行器由
 * 领域模块经 `UE_DEFINE_EFFECT_STEP_EXECUTOR` 自登记（见 TcsEffectStepExecutor.h），本模块对步骤
 * 类型零硬编码 switch、对领域模块零编译依赖。
 *
 * 非 Tickable（**挂起零每帧成本**）：挂起中的链不接受任何轮询——唤醒一律由唤醒源驱动
 * （R3 落到期堆回调；事件/子链完成随各自轮次），故本门面不做帧推进。
 *
 * 文件落点：领域代码住 `Public/Chain/`、宿主契约住 `Public/Host/`；本头与日志通道住 `Public/` 根
 * （`cpp-module-structure` 规格：模块壳之外的对外头按领域子目录分层）。
 */
UCLASS()
class TCSEFFECT_API UTcsEffectSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

// 生命期
#pragma region Lifetime

public:
	// 世界类型过滤：仅游戏世界（Game/PIE/GamePreview）实例化——链解释器是运行时设施（对齐时钟/总线/属性门面）
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// 反初始化：确定性清空运行态池（占用统计回落、旧句柄凭代际失配失效）与链定义表；
	// 已在到期堆里的挂起条目到期时按代际校验静默丢弃（无泄漏——条目由堆在回调后回收）
	virtual void Deinitialize() override;

#pragma endregion


// 链定义登记表
#pragma region Chain

public:
	/**
	 * 登记链定义（键 = `Chain.ChainId`——调用方不另传 id，避免双真相）。
	 * 拒绝面（ensure 提示 + 返回 false）：`ChainId` 为空、同 id 重复登记
	 * （定义重名 = 加载期错误，**不得静默覆写**——与属性词表登记同款口径）。
	 *
	 * @param Chain 链定义（按值拷入登记表；TUniquePtr 持有使解释器持有的定义引用不随登记表增长而悬空）。
	 * @return 返回是否登记成功。
	 */
	bool RegisterChain(const FTcsEffectChain& Chain);

	/**
	 * 注销链定义。**有活动运行态引用该链时拒绝**（ensure 提示 + 返回 false）——
	 * 运行中的链不因定义被抽走而悬空（定义释放与运行态解耦）。
	 * 未登记的 id 属正常路径：Warning 日志 + 返回 false（不 ensure）。
	 *
	 * @param ChainId 链 id。
	 * @return 返回是否注销成功。
	 */
	bool UnregisterChain(FGameplayTag ChainId);

	/**
	 * 查询链定义（未登记返回 nullptr——正常查询路径，不 ensure）。
	 *
	 * @param ChainId 链 id。
	 * @return 返回链定义；未登记返回 nullptr。
	 */
	const FTcsEffectChain* FindChain(FGameplayTag ChainId) const;

#pragma endregion


// 执行
#pragma region Execution

public:
	/**
	 * 起链：分配运行态 → 立即执行至首个挂起点或走完（即时步骤同步执行）。
	 * 未登记的链 id 拒绝：Error 日志 + 无效句柄（执行期配置错误，不 ensure——避免内容问题演变成红字刷屏）。
	 *
	 * @param ChainId 链 id。
	 * @param Context 链黑板（按值接管——调用方 MoveTemp 移交可免拷贝）。
	 * @return 返回运行态句柄；**仅在链未走完时有效**（全即时链在返回前即释放运行态，句柄随之失效——
	 *         用 `IsRunActive` 判活性）。
	 */
	FTcsChainRunHandle ExecuteChain(FGameplayTag ChainId, FTcsEffectContext Context);

	/**
	 * 唤醒重入（唤醒源入口：到期堆回调 / 事件 / 子链完成）：从运行态 PC 续走，直至再次挂起或走完。
	 * 悬空、已释放、世界将拆的句柄**静默返回 false**——挂起条目与运行态的竞态是正常路径
	 * （代际校验拦截），不是契约违规。
	 *
	 * @param Handle 运行态句柄。
	 * @return 返回是否处理了唤醒（句柄有效）。
	 */
	bool ResumeRun(FTcsChainRunHandle Handle);

	/**
	 * 运行态是否仍活动（句柄经池代际校验；无效/已释放句柄返回 false）。
	 *
	 * @param Handle 运行态句柄。
	 * @return 返回是否活动。
	 */
	bool IsRunActive(FTcsChainRunHandle Handle) const;

#pragma endregion


// 宿主能力注入
#pragma region Injection

public:
	/**
	 * 注入实体查询实现（宿主/上层提供，D4-14；R3 无消费者——默认策略 Self/EventTarget 不需遍历）。
	 * 传默认构造的 `TScriptInterface` 即撤下注入；GC 安全（`UPROPERTY` 持有）。
	 *
	 * @param InEntityQuery 实体查询实现（通常为宿主组件或宿主侧 UObject）。
	 */
	void SetEntityQuery(const TScriptInterface<ITcsEntityQuery>& InEntityQuery);

	/**
	 * 取实体查询实现（未注入返回 nullptr——"能力尚未注入"是配置状态，不 ensure）。
	 *
	 * @return 返回实体查询实现；未注入返回 nullptr。
	 */
	ITcsEntityQuery* GetEntityQuery() const;

#pragma endregion


// 内核
#pragma region Core

private:
	// 解释器：从运行态 PC 推进至挂起/走完（逐步查执行器注册表分派；越界熔断与未知类型断链在此处置）
	void RunFrom(FTcsChainRunHandle Handle);

	// 释放运行态（清黑板与唤醒锚后归还池槽——槽位内容不跨生命周期残留）
	void ReleaseRun(FTcsChainRunHandle Handle);

	// 是否存在引用该链的活动运行态（注销前校验）
	bool HasActiveRunForChain(FGameplayTag ChainId);

	// 链定义表（键 = ChainId；TUniquePtr 持有——解释器在一次进入执行期间持定义引用，登记表增长不得使其悬空）
	TMap<FGameplayTag, TUniquePtr<FTcsEffectChain>> ChainDefs;

	// 链运行态池（D0-2 代际句柄防悬空；池元素地址不稳定——解释器每步入器前按句柄重解析，见 TcsChainRun.h）
	TTcsInstancePool<FTcsChainRun, FTcsChainRunTag> RunPool;

	// 实体查询实现（UPROPERTY：实现是 UObject，须经 GC 持有）
	UPROPERTY()
	TScriptInterface<ITcsEntityQuery> EntityQuery;

#pragma endregion
};
