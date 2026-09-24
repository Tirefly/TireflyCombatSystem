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
#include "Trigger/TcsEffectTriggerInstance.h"
#include "Trigger/TcsTriggerRegistry.h"

#include "TcsEffectSubsystem.generated.h"

class UTcsTriggerEvaluator;



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

	/**
	 * GC 引用收集（**链定义登记表的 GC 可见持有**，2026-09-23 与 TcsDamage 同批修复 T-8）。
	 *
	 * 为什么必须自己实现：`ChainDefs` 是 `TMap<FGameplayTag, TUniquePtr<FTcsEffectChain>>`——
	 * **裸 C++ 容器不经 GC 的 `RefLink`**，`TUniquePtr` 更不是 GC 可见持有。链步骤里可以放
	 * **任意宿主自定义 struct**（D4-16 无公共基类、picker 不设限），只要那个 struct 带
	 * `UPROPERTY` 对象引用（如 `TScriptInterface` 委托 / `TObjectPtr` 资产引用），GC 就看不见它，
	 * 表现为**静默回收**——步骤跑到那里取到空引用，而非崩溃。
	 *
	 * 手法 = 逐链调 `FReferenceCollector::AddPropertyReferencesWithStructARO`
	 * （引擎 `UDataTable::AddReferencedObjects` 对 `RowMap` 用的同一招，`DataTable.cpp:300`）——
	 * 递归走步骤 struct 的反射属性，对带 `WithAddStructReferencedObjects` 的 `FInstancedStruct`
	 * 会继续进内层实例内存（`InstancedStruct.cpp:506`）。
	 *
	 * @param InThis 本对象（引擎静态 ARO 签名约定，须自行 Cast）。
	 * @param Collector GC 引用收集器。
	 */
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

#pragma endregion


// 链定义登记表
#pragma region Chain

public:
	/**
	 * 登记链定义（键 = `Chain.ChainId`——调用方不另传 id，避免双真相）。
	 * 拒绝面（ensure 提示 + 返回 false）：`ChainId` 为空、同 id 重复登记
	 * （定义重名 = 加载期错误，**不得静默覆写**——与属性词表登记同款口径）。
	 *
	 * **反射面（2026-09-24）**：`UFUNCTION()` **无 specifier** 是有意的——形参 `FTcsEffectChain`
	 * 是 `USTRUCT()` 非 `BlueprintType`，加 `BlueprintCallable` 会被 UHT 拒（蓝图参数校验）；
	 * 无 specifier 时 UHT 不校验参数，而宿主脚本层（UnrealSharp）照常可达。蓝图侧不可见属接受项
	 * （R0 §9"蓝图不承诺"）。
	 *
	 * @param Chain 链定义（按值拷入登记表；TUniquePtr 持有使解释器持有的定义引用不随登记表增长而悬空）。
	 * @return 返回是否登记成功。
	 */
	UFUNCTION()
	bool RegisterChain(const FTcsEffectChain& Chain);

	/**
	 * 注销链定义。**有活动运行态引用该链时拒绝**（ensure 提示 + 返回 false）——
	 * 运行中的链不因定义被抽走而悬空（定义释放与运行态解耦）。
	 * 未登记的 id 属正常路径：Warning 日志 + 返回 false（不 ensure）。
	 *
	 * **反射面（2026-09-24）**：`UFUNCTION()` 无 specifier（理由同 `RegisterChain`）。
	 *
	 * @param ChainId 链 id。
	 * @return 返回是否注销成功。
	 */
	UFUNCTION()
	bool UnregisterChain(FGameplayTag ChainId);

	/**
	 * 查询链定义（未登记返回 nullptr——正常查询路径，不 ensure）。
	 *
	 * **无反射面（2026-09-24）**：返回**裸 struct 指针** `const FTcsEffectChain*`——UHT 不支持
	 * struct 指针作反射返回（引擎全仓零先例），故本方法**仅 C++ 可用**。脚本层查"链是否已登记"
	 * 走 `IsChainRegistered`（下）；脚本层**不需要**读链定义内容（链是它自己登记的）。
	 *
	 * @param ChainId 链 id。
	 * @return 返回链定义；未登记返回 nullptr。
	 */
	const FTcsEffectChain* FindChain(FGameplayTag ChainId) const;

	/**
	 * 链定义是否已登记（**脚本层可达的查询口**——`FindChain` 返回裸指针，反射层表达不了）。
	 *
	 * **反射面（2026-09-24）**：`UFUNCTION()` 无 specifier；形参/返回全反射，无类型约束问题。
	 *
	 * @param ChainId 链 id。
	 * @return 返回是否已登记。
	 */
	UFUNCTION()
	bool IsChainRegistered(FGameplayTag ChainId) const;

#pragma endregion


// 触发行登记
#pragma region Trigger

public:
	/**
	 * 登记触发行（M4a 订阅侧；宿主/上层登记，门面持有并按 `Def.EventTag` 装配总线订阅）。
	 *
	 * **订阅计数配对**：同一 `EventTag` 的多行**共用一条订阅**——首次出现该 Tag 时订阅一次，
	 * 该 Tag 的行数归零时才退订（`MUST NOT` 每行各订一次）。
	 * **通道 = 立即**：收集协议要求"修正提交落在事件发布返回之前"。
	 *
	 * 拒绝面（ensure 提示 + 返回无效句柄）：`Def.EventTag` 或 `Def.EffectChainId` 无效；
	 * 总线不可得（世界拆解期）→ Warning + 无效句柄（时序而非配置错误）。
	 *
	 * @param Instance 触发实例（填 `Def` 与 `Source`；`Self` 由登记表覆写）。
	 * @return 返回行句柄；拒绝时返回无效句柄。
	 */
	UFUNCTION()
	FTcsEffectTriggerHandle RegisterTriggerRow(const FTcsEffectTriggerInstance& Instance);

	/**
	 * 摘除单行（**先校验代际**——陈旧句柄被拒且不误伤复用该槽位的新行；失配不 ensure）。
	 *
	 * @param Handle 行句柄。
	 * @return 返回是否摘除成功。
	 */
	UFUNCTION()
	bool UnregisterTriggerRow(FTcsEffectTriggerHandle Handle);

	/**
	 * 按来源全量摘除（级联退订锚点——与 M2 `RemoveBySource` 同款语义）。
	 *
	 * **无反射面（2026-09-24）**：形参 `FTcsSourceHandle` 是非反射纯 C++ struct（无 `USTRUCT` 宏）
	 * ⇒ 反射层表达不了。脚本层若要按来源级联摘除，需先反射化该 struct（属台账 S-1 的 B 类未覆盖项）。
	 *
	 * @param Source 来源句柄。
	 * @return 返回摘除的行数。
	 */
	int32 UnregisterTriggerRowsBySource(const FTcsSourceHandle& Source);

	/**
	 * 点灯/灭灯（行级开关；`GateTags` **全部**点亮才通过该道门）。
	 *
	 * **反射面（2026-09-24）**：`UFUNCTION()` 无 specifier；形参全反射（tag + bool）。
	 *
	 * @param GateTag 开关 Tag。
	 * @param bLit 是否点亮。
	 */
	UFUNCTION()
	void SetTriggerGateTag(FGameplayTag GateTag, bool bLit);

	/**
	 * 开关是否点亮。
	 *
	 * **反射面（2026-09-24）**：`UFUNCTION()` 无 specifier。
	 */
	UFUNCTION()
	bool IsTriggerGateTagLit(FGameplayTag GateTag) const;

	/**
	 * 登记行数（观测/装置断言用）。
	 *
	 * **反射面（2026-09-24）**：`UFUNCTION()` 无 specifier。
	 */
	UFUNCTION()
	int32 GetTriggerRowCount() const;

	/**
	 * 设置求值随机流种子（D0-1 确定性纪律：概率条件的随机值由门面供给——
	 * 条件求值器内部 MUST NOT 取随机数）。默认 0 = 确定的固定序列（可复现）。
	 *
	 * **反射面（2026-09-24）**：`UFUNCTION()` 无 specifier。
	 *
	 * @param Seed 随机流种子。
	 */
	UFUNCTION()
	void SetTriggerRandomSeed(int32 Seed);

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
	 * 脚本层起链入口（2026-09-24，提案 `add-scripting-reflection-surface`）：形参**全反射**——
	 * 上下文由门面按 `Caster` 装配默认值（`Caster` = `Instigator` = 传入句柄，`Targets` = 仅该句柄）。
	 *
	 * **为什么需要它**：`ExecuteChain` 的形参 `FTcsEffectContext` 是**非反射纯 C++ struct**
	 * （`Chain/TcsEffectContext.h:24`，无 `USTRUCT` 宏）⇒ 反射层表达不了、该方法的 `UFUNCTION`
	 * 标记会让 UHT 报 `Unable to find 'struct' with name 'FTcsEffectContext'`（同款先例 =
	 * `FTcsEffectTriggerInstance.Source` 与 `FTcsAttrModInstance.Source` 的既有编译实证）。
	 * 故脚本层起链走本入口；**完整黑板**（自定义 `EventPayload` / `Variables` / 多目标）待上下文
	 * 反射化落地后开放（台账 S-3）。
	 *
	 * 与 `UTcsCombatEntityComponent::ExecuteChainById` 的分工：那个从组件自持的实体句柄取 `Caster`
	 * （有组件时更顺手）；本入口用于**没有组件、只持实体句柄**的纯逻辑脚本（如 Buff 系统）。
	 *
	 * **反射面**：`UFUNCTION()` 无 specifier（理由同 `RegisterChain`——`FTcsCombatEntityHandle`
	 * 虽是 `BlueprintType`，但 `FTcsChainRunHandle` 不是，蓝图参数校验会拦）。
	 *
	 * @param ChainId 链 id。
	 * @param Caster 施法者实体句柄（同时作为 `Instigator` 与默认目标）。
	 * @return 返回运行态句柄；仅在链未走完时有效。
	 */
	UFUNCTION()
	FTcsChainRunHandle ExecuteChainForCaster(FGameplayTag ChainId, FTcsCombatEntityHandle Caster);

	/**
	 * 唤醒重入（唤醒源入口：到期堆回调 / 事件 / 子链完成）：从运行态 PC 续走，直至再次挂起或走完。
	 * 悬空、已释放、世界将拆的句柄**静默返回 false**——挂起条目与运行态的竞态是正常路径
	 * （代际校验拦截），不是契约违规。
	 *
	 * **反射面（2026-09-24）**：`UFUNCTION()` 无 specifier；形参 `FTcsChainRunHandle` 已升格
	 * `USTRUCT()`（见 `TcsChainRun.h`）。
	 *
	 * @param Handle 运行态句柄。
	 * @return 返回是否处理了唤醒（句柄有效）。
	 */
	UFUNCTION()
	bool ResumeRun(FTcsChainRunHandle Handle);

	/**
	 * 运行态是否仍活动（句柄经池代际校验；无效/已释放句柄返回 false）。
	 *
	 * **反射面（2026-09-24）**：`UFUNCTION()` 无 specifier；形参 `FTcsChainRunHandle` 已升格
	 * `USTRUCT()`——这是脚本层"接住句柄 → 传回查询"闭环的另一半。
	 *
	 * @param Handle 运行态句柄。
	 * @return 返回是否活动。
	 */
	UFUNCTION()
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
	 * **无反射面（2026-09-24）**：本对（`SetEntityQuery`/`GetEntityQuery`）与实体查询契约
	 * `ITcsEntityQuery` 一起归台账 **S-5**——该接口三方法零 `UFUNCTION` 且形参含 `TFunctionRef`
	 * （头文件自注"C++ 专用面：蓝图不可表达"），须先换签名。在 `ITcsEntityQuery` 反射化之前，
	 * 脚本层无法实现实体查询、故也无需经反射注入它。**本对方法保持纯 C++**。
	 *
	 * @return 返回实体查询实现；未注入返回 nullptr。
	 */
	ITcsEntityQuery* GetEntityQuery() const;

#pragma endregion


// 内核
#pragma region Core

private:
	// 求值器内部面（求值器不住本类，但需读登记表/点灯/随机流——故经 friend 开放最小集）
	friend class UTcsTriggerEvaluator;

	// 收集某事件 Tag 的全部行句柄（快照；求值器用）
	void CollectTriggerRowsForTag(FGameplayTag EventTag, TArray<FTcsEffectTriggerHandle>& OutHandles) const;

	// 行句柄按 Priority 降序 + 登记序稳定排序（求值器用）
	void SortTriggerRowsByPriority(TArray<FTcsEffectTriggerHandle>& Handles) const;

	// 按句柄解析行（代际校验；悬空返回 nullptr——求值器用）
	const FTcsEffectTriggerInstance* FindTriggerRow(FTcsEffectTriggerHandle Handle) const;

	// 取下一个 [0,1) 随机值（仅概率类条件用；每行各取一次——求值器用）
	double NextTriggerRandomValue();

	// 取事件总线门面（登记/退订装配用；世界拆解期可能为 nullptr）
	class UTcsEventBusSubsystem* GetEventBus() const;

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

	// 触发求值器（**UPROPERTY 持有是必需的**：总线订阅表持弱引用——不 root 会被 GC 掉、
	// 订阅静默失效。宿主 `UTcsDevScreenObserver` 的既有注释即该现象的先例）
	UPROPERTY()
	TObjectPtr<UTcsTriggerEvaluator> TriggerEvaluator;

	// 触发行登记表（纯逻辑类；值语义持有行 + 订阅计数配对 + 点灯集 + 随机流）
	FTcsTriggerRegistry TriggerRegistry;

#pragma endregion
};
