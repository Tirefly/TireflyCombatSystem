// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Def/TcsBuffDef.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"
#include "Parameter/TcsParamTableReader.h"

#include "Host/TcsEntityLevelProvider.h"

#include "State/TcsStateEnums.h"
#include "State/TcsStateHandle.h"
#include "State/TcsStateInstance.h"
#include "State/TcsStateParamTableReader.h"
#include "State/TcsStateRegistry.h"

#include "TcsStateSubsystem.generated.h"

class UTcsEventBusSubsystem;
class UTcsClockSubsystem;



/**
 * 状态门面（D3-1 "中央注册表"的落地形态，2026-10-04 R5 收窄轮定名）：世界级子系统，
 * 持**per-unit 状态桶**与**本世界的状态定义登记表**，是状态实例的唯一入口。
 *
 * 分工：本类 = 门面（世界过滤 / 定义登记 / 句柄与阶段纪律）；
 * `FTcsStateRegistry` = 登记表（per-unit 桶 + 槽位代际）；`FTcsStateOps` = 引擎函数（施加 / 移除 / 查询流程）。
 * 桶只存数据与索引，操作全在引擎函数——**这是设计文档 §3.1 的既定分工**，不是可选项。
 *
 * 非 Tickable：到期推进归 M0 时钟泵（`UTcsClockSubsystem` 的到期堆），本门面零每帧成本。
 *
 * **定义怎么进来（单向）**：定义库 `UTcsDefinitionSubsystem` 住 `TcsIntegration`，而
 * `TcsIntegration` **依赖** `TcsState`——本类 MUST NOT 反向 include 它（成环）。故定义经
 * **登记口**（`RegisterStateDef`）写入；依赖方向单向：定义库写进门，门不反查定义库
 * （同款先例 = 定义库把触发行写进 `UTcsEffectSubsystem::RegisterTriggerRow`）。
 *
 * **文件落点**：门面头与日志通道住 `Public/` 根（`cpp-module-structure` 规格），
 * 领域代码住 `Public/State/` 与 `Private/State/`。
 */
UCLASS()
class TCSSTATE_API UTcsStateSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

// 生命期
#pragma region Lifetime

public:
	/**
	 * 初始化：建**快照参数表反射壳**（`ParamTableReader`）。
	 *
	 * **为什么在初始化期就建**（而不是第一次物化时懒建）：壳是门面持有的**单例**，
	 * 建成之后物化路径**零分配**；而物化发生在施加流程里（可能正持着实例指针），
	 * 那个位置不该出现"可能触发 GC 的分配"这一类副作用。
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 世界类型过滤：仅游戏世界（Game/PIE/GamePreview）实例化——状态注册表是运行时设施（对齐时钟/总线/属性/效果链门面）
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/**
	 * 反初始化：**先逐条撤销时间条目**（时值 + 周期）→ 登记表复位 → **全部桶清空**
	 * （跨世界确定性清理——旧句柄一律凭代际失配失效）。
	 *
	 * **撤销条目的时机**：必须有（R5 Task 3 接线）——条目持句柄，桶清空后回调仍会到达，
	 * 靠代际校验兜底是"能成立"，但每次世界切换都会往堆里留一批无谓条目。
	 */
	virtual void Deinitialize() override;

	/**
	 * GC 引用收集（**状态定义登记表的 GC 可见持有**）。
	 *
	 * 为什么必须自己实现：`RegisteredDefs` 是 `TMap<FGameplayTag, TUniquePtr<FTcsBuffDef>>`——
	 * 本类的**非 `UPROPERTY` 成员**，GC 的 `RefLink` 遍历**走不到它**；而定义内容里有
	 * `FInstancedStruct`（参数行的数值来源 / 描述的视图载荷），其**内层内存可以放宿主自定义 struct 的
	 * `UPROPERTY` 对象引用**（D4-16 类型不设限）⇒ 不补引用即**静默回收**
	 * （表现为"参数行/视图里那个对象变成空引用"，不是崩溃）。
	 *
	 * **判据是"容器是否 GC 可见"，与"值语义还是指针语义"无关**——与 `ChainDefs` / `TriggerDefs`
	 * 三处同款（WAIT-8 那一类缺口）。
	 *
	 * **本轮的一处诚实说明**：`FTcsBuffDef` 当前字段里只有 `TSoftObjectPtr`（资产模板引用），
	 * 而 `FInstancedStruct` 今天在状态定义里**通常是空的**——故本段今天大概率是空跑。
	 * 仍然补上：登记表是"将来会装东西"的容器，缺一段 ARO 是**静默失败**类缺陷（不是崩溃），
	 * 而修复成本是零（GC 期一次遍历），不值得赌"以后没人往里放对象引用"。
	 *
	 * **实例记录（`FTcsStateInstance`）不在本函数范围内**：它按纪律 MUST NOT 持任何
	 * `UObject` 引用（见 `TcsStateInstance.h` 的说明），故无需补引用——这正是那条纪律的收益之一。
	 *
	 * @param InThis 本对象（引擎静态 ARO 签名约定，须自行 Cast）。
	 * @param Collector GC 引用收集器。
	 */
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

#pragma endregion


// 定义登记
#pragma region Definition

public:
	/**
	 * 登记状态定义（**身份由调用方显式传入**——键 = `DefTag`）。
	 *
	 * **为什么身份是形参而不是从 `Def` 里读**：身份归**资产**声明（`UTcsStateDef::DefTag`——它是
	 * 资产身份、`GetPrimaryAssetId()` 的取值来源、作者期的校验对象），而 `FTcsBuffDef` 是**定义内容**
	 * （运行期副本）。若为了"从内容里读身份"再往 `FTcsStateDefBase` 里塞一份 `DefTag`，策划就得在
	 * 同一个资产里把同一个 tag 手填两遍——那是**双真相**（正是本仓反复点名的缺陷形态），不是便利。
	 * 故：**身份住在资产、内容住在数据 struct、运行期副本由登记口把两者的身份一次对齐**
	 * （调用方 = 定义库，其缓存键本就是 `DefTag`，零额外信息）。
	 *
	 * **登记表自持副本**（`TUniquePtr` 持有使 `GetRegisteredStateDef` 返回的指针地址稳定）：
	 * 定义内容里含 `FInstancedStruct`（参数行的数值来源 / 描述的视图载荷），其内层可放宿主自定义
	 * struct 的 `UPROPERTY` 对象引用 ⇒ 只持指针不持副本会让内容失主。
	 *
	 * 拒绝面（**Error 日志** + 返回 false，不 ensure——调用方是定义库，这里的失败在上游已被拦）：
	 * `DefTag` 无效、同 id 重复登记（**不静默覆写**——口径同链/触发定义登记）。
	 *
	 * @param DefTag 定义身份（资产侧 `UTcsStateDef::DefTag`）。
	 * @param Def 定义内容（按值拷入登记表）。
	 * @return 返回是否登记成功。
	 */
	bool RegisterStateDef(FGameplayTag DefTag, const FTcsBuffDef& Def);

	/**
	 * 注销状态定义（未登记 = Warning + false，不 ensure——正常清理路径）。
	 *
	 * **有该定义的存活实例时不代为撤销**：已建实例持有定义副本的身份（`DefTag`）而非指针，
	 * 注销只影响"新施加能否解析到定义"（同 `TTcsInstancePool` 的零策略纪律）。
	 *
	 * @param DefTag 定义身份。
	 * @return 返回是否注销成功。
	 */
	bool UnregisterStateDef(FGameplayTag DefTag);

	/**
	 * 按身份解析本世界已登记的状态定义（未登记返回 nullptr——正常查询路径，不 ensure）。
	 *
	 * **无反射面**：返回**裸 struct 指针** `const FTcsBuffDef*`——UHT 不支持 struct 指针作反射返回
	 * （同 `UTcsDefinitionSubsystem::ResolveStateDef` / `UTcsEffectSubsystem::FindChain`）。
	 *
	 * @param DefTag 定义身份。
	 * @return 返回定义；未登记返回 nullptr。
	 */
	const FTcsBuffDef* GetRegisteredStateDef(FGameplayTag DefTag) const;

	// 本世界已登记的定义数（观测与装置断言用）
	int32 GetRegisteredStateDefCount() const;

#pragma endregion


// 施加与查询
#pragma region Apply

public:
	/**
	 * 施加状态（**本轮的共存决策 = 同单位 + 同 `DefTag`**，即 `GroupBy = None` 的语义）。
	 *
	 * 三条出口（返回值与广播一一对应）：
	 * - 无同组成活实例 ⇒ 分配槽位、建实例、广播 `TcsEvent.State.Applied`，返回 `EAR_Applied`；
	 * - 命中同组成活实例 ⇒ **刷新**原实例（层数/时值按本轮口径更新）并广播 `TcsEvent.State.Refreshed`，
	 *   返回 `EAR_Refreshed`；
	 * - 定义未登记 / 目标无效 ⇒ 广播零笔、返回 `EAR_Rejected` + Warning（**不 ensure**——内容缺口
	 *   不是契约违规，且发现期已把"能否按 tag 寻址"拦过一次）。
	 *
	 * **`: 本步的替换点`**：上面的"同组"判据是 **R5 Task 5 五轴共存决策的暂用位**——
	 * 快照构建（本轮落地）与时间条目重挂都挂在它下面，Task 5 只需替换共存判据本身，
	 * **事件面与返回值语义不变**（`Stacked` 档随 Task 5 变为可达）。
	 *
	 * **两个可选形参的缺省语义**（都不构成错误面）：
	 * - `Instigator` 无效 ⇒ 取 `Target`（"谁施加的"缺省即被施加方）；
	 * - `ParamTable` 空 ⇒ 引用类参数源落兜底（`ParamRef` 的 Fallback）。
	 *
	 * **无反射面**：形参含 `TMap` 与未反射化句柄（本轮不加 `UFUNCTION`——脚本面整体留 `SCRIPT` 系列）。
	 *
	 * @param Target 被施加方实体（无效即拒绝）。
	 * @param DefTag 状态定义身份（须已在本世界登记）。
	 * @param Source 施加方来源句柄（**无效则由本门面发号**——同一次施加内触发行与修正器共用它）。
	 * @param Instigator 发起者实体（可选；无效则取 `Target`）。
	 * @param ParamTable 施加方参数表（可选；空则引用类源落兜底）。
	 * @param Overrides 施加方参数覆盖（参与快照构建：命中即取覆盖值，否则取定义默认）。
	 * @return 返回施加结果。
	 */
	EApplyResult ApplyState(
		FTcsCombatEntityHandle Target,
		FGameplayTag DefTag,
		FTcsSourceHandle Source,
		FTcsCombatEntityHandle Instigator,
		const TScriptInterface<ITcsParamTableReader>& ParamTable,
		const TMap<FGameplayTag, double>& Overrides);

	/**
	 * 按句柄取实例（脏句柄返回 nullptr 且不 ensure——时序竞态是正常路径）。
	 *
	 * **无反射面**：返回裸 struct 指针（UHT 表达不了）。
	 *
	 * @param Handle 状态实例句柄。
	 * @return 返回实例；脏句柄返回 nullptr。
	 */
	const FTcsStateInstance* GetState(FTcsStateHandle Handle) const;

	/**
	 * 句柄是否指向在册实例（代际校验；脏句柄返回 false，不 ensure）。
	 *
	 * @param Handle 状态实例句柄。
	 * @return 返回是否在册。
	 */
	bool IsStateActive(FTcsStateHandle Handle) const;

	/**
	 * 遍历某单位的在册实例（**按桶内槽位下标升序**——稳定序，同输入同输出）。
	 * 访问者返回 false 即提前终止。
	 *
	 * **遍历期间 MUST NOT 增删实例**（会改动槽位/空闲栈）——需要增删时先收集句柄、
	 * 遍历结束后再动（两段式，同 `FTcsTriggerRegistry::UnregisterRowsBySource`）。
	 *
	 * **无反射面**：形参含 `TFunctionRef`（C++ 专用面，同 `ITcsEntityQuery`）。
	 *
	 * @param Unit 单位实体句柄。
	 * @param Visitor 访问者（返回是否继续）。
	 */
	void ForEachState(FTcsCombatEntityHandle Unit, TFunctionRef<bool(const FTcsStateInstance&)> Visitor) const;

	// 某单位在册实例数（观测与装置断言用）
	int32 GetStateCount(FTcsCombatEntityHandle Unit) const;

	// 全部门面在册实例数（跨单位合计；观测用；`TcsState.PoolSlots` 一类的统计口留待需要时）
	int32 GetTotalStateCount() const;

#pragma endregion


// 移除与生命周期操作
#pragma region Removal

public:
	/**
	 * 显式移除（原因由调用方给：`Removed` / `Cancelled`）。
	 *
	 * **撤销顺序是硬约束**：`Phase → Expiring` → **广播** → 归还槽位（`Phase → Inactive`）。
	 * 即"先广播后释放"——订阅者在回调里仍能用载荷句柄 `GetState` 读到实例
	 * （否则会出现"收到移除事件却查不到实例"）。**本条 MUST NOT 为了省事改成先释放**。
	 *
	 * 脏句柄 ⇒ 返回 false + Warning（不 ensure——竞态语义）。
	 *
	 * @param Handle 状态实例句柄。
	 * @param Cause 移除原因（仅 `Removed` / `Cancelled` 语义成立；传 `Expired` 请改用 `ExpireState`）。
	 * @return 返回是否移除成功。
	 */
	bool RemoveState(FTcsStateHandle Handle, EStateRemoveCause Cause);

	/**
	 * 到期移除（`Cause` 固定为 `Expired`）——到期回调的收口入口（R5 Task 3 由到期堆回调驱动）。
	 *
	 * @param Handle 状态实例句柄。
	 * @return 返回是否移除成功。
	 */
	bool ExpireState(FTcsStateHandle Handle);

	/**
	 * 注销单位：对该单位**每条在册实例逐条广播** `Removed` 后删桶。
	 *
	 * 顺序 = 逐个 `RemoveState(handle, ESRC_Removed)`（复用同一条撤销链，不另写一套）→ 删桶。
	 * MUST NOT 先删桶再广播：订阅者那时已经查不到实例。
	 *
	 * @param Unit 单位实体句柄。
	 * @return 返回移除的实例数（未注册返回 0——正常路径，不 ensure）。
	 */
	int32 UnregisterUnit(FTcsCombatEntityHandle Unit);

	/**
	 * 延长剩余时长（`Delta` 秒，可负）。
	 *
	 * **到期堆同步**：撤销旧条目 + 按新余量重入堆（句柄配对清理）——R5 Task 3 接线完成，
	 * 不再是"只改字段"。
	 *
	 * **不钳到总时长**：延长可以超过 `DurationTime`（"灼烧延长"的 D3-15 原义未定上限；
	 * 若要改成"钳到总时长"那是另一条决策，届时改这一处即可）。
	 *
	 * **`EDP_Infinite` 上调它 = Warning + 无操作**：无限时值没有到期条目可调，
	 * 这是**配置状态**不是错误（同"未注入 provider"的口径），故不 ensure、不改字段。
	 * 注意：定义未登记时无法判定时值策略 ⇒ 同样 Warning + 无操作（不猜）。
	 *
	 * @param Handle 状态实例句柄。
	 * @param Delta 增量秒数（正 = 延长）。
	 * @return 返回是否操作成功。
	 */
	bool ExtendDuration(FTcsStateHandle Handle, double Delta);

	/**
	 * 设置剩余时长为绝对值（秒）。
	 *
	 * 拒绝面与"到期堆同步"的口径同 `ExtendDuration`（含 `Infinite` 与定义缺失两条 Warning 路径）。
	 *
	 * @param Handle 状态实例句柄。
	 * @param Seconds 新的剩余秒数（负值按 0 处理；置 0 使其在下一个泵点到期）。
	 * @return 返回是否操作成功。
	 */
	bool SetRemaining(FTcsStateHandle Handle, double Seconds);

#pragma endregion


// 宿主等级读口
#pragma region LevelProvider

public:
	/**
	 * 注入实体等级读口（宿主实现"这个实体几级"；等级类参数源经它把句柄解析成等级）。
	 *
	 * **未注入 = `nullptr` 是配置状态不是错误**（同 `ITcsParamTableReader` 可空的口径）——
	 * 等级类源据此落兜底路径，不留红字、不 ensure。
	 *
	 * **为什么住门面而不是全局单例**：等级是**世界内的实体属性**（同一份数据在 PIE 与 Game
	 * 世界各自独立），注入物按世界持有；跨世界不共享是特性不是缺陷。
	 *
	 * 注入物以 `UPROPERTY` 持有（`TScriptInterface` 的 GC 纪律：非 `UPROPERTY` 的接口指针
	 * 会被 GC 静默回收，表现为"读口突然变空"）。
	 *
	 * @param InProvider 宿主读口（可传空以撤销注入）。
	 */
	void SetEntityLevelProvider(TScriptInterface<ITcsEntityLevelProvider> InProvider);

	// 取当前注入的等级读口（未注入返回空接口）
	TScriptInterface<ITcsEntityLevelProvider> GetEntityLevelProvider() const
	{
		return EntityLevelProvider;
	}

#pragma endregion


// 内核
#pragma region Core

public:
	/**
	 * 取事件总线门面 / 时钟泵门面（**世界拆解期都可能为 nullptr——调用方须容忍**）。
	 *
	 * **为什么是公开口而不是把消费者塞进 friend 名单**：条目的入堆/撤堆发生在引擎函数里
	 * （`FTcsStateOps` / `FTcsStateDuration`），它们**本来就**够得着门面私有区；但"取另一个
	 * 子系统的指针"是任何调用方（含将来的行为 Fragment 派发）都要做的事，藏起来只会让下一处
	 * 再往 friend 名单里加一个名字。返回裸指针即"可能为空"的显式声明。
	 */
	UTcsEventBusSubsystem* GetEventBus() const;

	UTcsClockSubsystem* GetClockSubsystem() const;

	/**
	 * 取快照参数表反射壳（修正器物化把"该实例的快照"作为**本次求值的参数表**装进求值上下文的载体）。
	 *
	 * **绑定纪律不在壳里、在作用域里**：绑定/恢复由 `FTcsStateSnapshotScope`（RAII，栈式）负责，
	 * 见 `State/TcsStateParamTableReader.h`——调用方 MUST NOT 手工 `Bind` 跨过可能广播的调用
	 * （那会让"当前绑定"变成一处隐式的跨帧状态）。
	 *
	 * @return 返回壳（初始化后非空；未初始化返回 `nullptr`，物化侧按"无参数表"降级）。
	 */
	UTcsStateParamTableReader* GetParamTableReader();

private:
	// 广播一次状态事件（载荷由实例快照 + 原因拼装；总线不可得时 Warning 并丢弃——不 ensure）
	void BroadcastStateEvent(FGameplayTag EventTag, const FTcsStateInstance& Instance, EStateRemoveCause Cause);

	// 阶段迁移（非法迁移 = ensure——**配置错误**语义，与脏句柄的竞态口径分开）
	void TransitionPhase(FTcsStateInstance* Instance, EStatePhase NewPhase);

	/**
	 * 施加 / 移除 / 查询流程（`FTcsStateOps`）需要读写下面四个成员——
	 * 故开放**最小集**（friend 类不给访问域继承：引擎函数访问这些成员靠的是 friend 声明，
	 * 而非把它们放到 public）。门面只做世界过滤与登记口，流程逻辑全在引擎函数里（设计文档 §3.1 的分工）。
	 */
	friend class FTcsStateOps;

	// 状态定义登记表（键 = DefTag；TUniquePtr 持有使解析返回的指针地址稳定）
	TMap<FGameplayTag, TUniquePtr<FTcsBuffDef>> RegisteredDefs;

	// per-unit 状态桶登记表
	FTcsStateRegistry Registry;

	// 来源发号器（施加时发放实例的来源句柄；Task 0 的统一发号器 = 进程内唯一、永不复用）
	FTcsSourceHandleRegistry SourceRegistry;

	// 宿主等级读口（未注入 = 空接口；`UPROPERTY` 持有以满足 TScriptInterface 的 GC 纪律）
	UPROPERTY()
	TScriptInterface<ITcsEntityLevelProvider> EntityLevelProvider;

	// 快照参数表反射壳（`Initialize` 建的单例；绑定由 `FTcsStateSnapshotScope` 作用域负责）
	UPROPERTY()
	TObjectPtr<UTcsStateParamTableReader> ParamTableReader;

#pragma endregion
};
