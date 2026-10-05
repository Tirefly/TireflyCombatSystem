// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "State/TcsStateEnums.h"
#include "State/TcsStateHandle.h"
#include "State/TcsStateInstance.h"
#include "State/TcsStateSnapshot.h"

class UTcsStateSubsystem;
class UTcsClockSubsystem;
class ITcsEntityLevelProvider;
class ITcsParamTableReader;
struct FStateBucket;
struct FStateStackPolicy;
struct FTcsBuffDef;
struct FTcsStateEvaluateContext;
struct FTcsStateStackDecision;
struct FTcsStateStackRequest;
struct FTcsTimeEntryHandle;



/**
 * 状态引擎函数（设计文档 §3.1 的既定分工）：**桶只存数据与索引，操作全在这里**。
 *
 * 为什么是"引擎函数"而不是把逻辑写进门面：门面的职责是**世界过滤 + 定义登记 + 门面签名**；
 * 施加 / 移除 / 查询的流程逻辑按操作种类集中在本类，这样 `UTcsStateSubsystem.cpp` 保持薄壳，
 * 而流程改动不会与 UE 生命周期样板混在一起（同款先例 = `TcsDamage` 的流程步骤库拆分）。
 *
 * **全部成员为静态函数**：本类**无实例**（不持状态）——状态一律经 `Subsystem` 参数传入。
 * 用 `class` 而非 namespace 是为了对齐设计文档的 `FStateOps` 命名，并让"这些函数是一族"在
 * 调用点（`FTcsStateOps::Apply(...)`）一眼可辨。
 *
 * 文件划分：本头 + `TcsStateOps.cpp`（施加与移除）/ `TcsStateOps_Query.cpp`（查询）/
 * `TcsStateOps_Events.cpp`（Tag 定义与广播）/ `TcsStateOps_Snapshot.cpp`（快照构建与时长求值）/
 * `TcsStateOps_Lifetime.cpp`（时长 / 周期条目入堆与撤堆、时长操作）/
 * `TcsStateOps_Modifier.cpp`（属性访问解析点 + 修正器挂载与摘除）/
 * `TcsStateOps_Stack.cpp`（五轴共存决策 + 刷新与叠层的更新流水）。
 *
 * **导出宏是必需项**（`WAIT-9` 判据："该符号是否被跨模块引用"）：宿主侧装置（`TcsDev`）直接调
 * `MakeContext` 等静态成员 ⇒ 不带 `TCSSTATE_API` 必 `LNK2019`（2026-10-05 实测）。
 */
class TCSSTATE_API FTcsStateOps
{
// 施加
#pragma region Apply

public:
	/**
	 * 施加（或按共存策略刷新 / 叠层 / 替换）一个状态——`UTcsStateSubsystem::ApplyState` 的实现体。
	 *
	 * **共存判据归 `Def.StackPolicy` 五轴**（R5 Task 5 落地，原"同单位 + 同 `DefTag` 即刷新"的暂用判据已替换）：
	 * 组键 = `(单位, DefTag)` + 轴附加项，命中后按容量与溢出政策决定刷新 / 叠层 / 替换 / 拒绝。
	 * 决策与更新流水住 `TcsStateOps_Stack.cpp`，本函数只做校验、决策分派与"新建"一支。
	 *
	 * **`Source` 是"续杯 / 叠层"的判据**（施加方身份）：生命周期长于一次施加的调用方 MUST 传稳定的
	 * 来源句柄；传无效值时门面会发新号，共存决策按"未声明来源"处理（视为同来源 ⇒ 续杯）。
	 * **撤销锚点不是它**：级联撤销走实例的 `CascadeAnchor`（门面为每条实例恒发新号，调用方无从指定）。
	 *
	 * @param Subsystem 状态门面（提供登记表、桶、发号器与广播面）。
	 * @param Target 被施加方实体。
	 * @param DefTag 状态定义身份（须已登记）。
	 * @param Source 施加方来源句柄（无效则由门面发号）。
	 * @param Instigator 发起者实体（无效则取 `Target`——"谁施加的"缺省即被施加方）。
	 * @param ParamTable 施加方参数表（空 = 引用类参数源落兜底）。
	 * @param Overrides 施加方参数覆盖（参与快照构建）。
	 * @return 返回施加结果（`Applied` / `Refreshed` / `Stacked` / `Rejected` 四档均可达）。
	 */
	static EApplyResult Apply(
		UTcsStateSubsystem& Subsystem,
		FTcsCombatEntityHandle Target,
		FGameplayTag DefTag,
		FTcsSourceHandle Source,
		FTcsCombatEntityHandle Instigator,
		const TScriptInterface<ITcsParamTableReader>& ParamTable,
		const TMap<FGameplayTag, double>& Overrides);

#pragma endregion


// 堆叠（共存决策与刷新）
#pragma region Stacking

public:
	/**
	 * 共存决策（**纯函数**：只算不写；实例字段由 `RefreshStacked` 写入）。
	 *
	 * 单次入口：**在桶内找同组成员 + 出决策一次做完**——决策载荷只解析一次（解析失败会留 `Warning`，
	 * 分成两次调用会让一次施加刷出 2 条重复 Warning）。组键判定与决策树见 `state-stacking-policies` 能力。
	 *
	 * 决策树：无成员 ⇒ 新建；Custom 不接受 ⇒ 拒绝；满仓 ⇒ 按溢出政策拒绝或替换；
	 * 未满仓 ⇒ 同来源续杯（层数不变）/ 异来源叠一层（层数由 Custom 或内置规则给出）。
	 *
	 * @param Request 本次施加的决策输入。
	 * @param Bucket 被施加方单位的桶。
	 * @param Policy 该定义的堆叠策略。
	 * @return 返回决策结果（`ExistingHandle` = 命中的同组成员；无成员时无效）。
	 */
	static FTcsStateStackDecision ResolveStackDecision(
		const FTcsStateStackRequest& Request,
		const FStateBucket& Bucket,
		const FStateStackPolicy& Policy);

	/**
	 * 刷新 / 叠层的**全部更新流水**（层数 → 快照重建 → 修正器重挂 → 时长重挂 → 广播）。
	 *
	 * 顺序有两条硬约束：① **层数先落**（物化器按 `ValueStack` 读它算数值，层数后落会让本次
	 * 叠层的数值停留在旧层数）；② **广播最后**（`StackChanged` 先于 `Refreshed`，两笔载荷均为新层数）。
	 *
	 * 挂载提交会广播（属性变更事件）⇒ 订阅者可重入状态操作 ⇒ 本函数在提交后 MUST 重新定位实例与桶；
	 * 实例已被重入移除时按"已完成但已无意义"处理：留 `Warning`、不补播事件、回执仍按决策档给。
	 *
	 * @param Subsystem 状态门面。
	 * @param Def 状态定义内容（取策略、等级基准与修正器模板）。
	 * @param Instance 命中的既有实例（**须已在册**；层数会被就地更新）。
	 * @param Request 本次施加的决策输入（提供发起者 / 单位 / 定义身份与参数表位）。
	 * @param Decision 共存决策结果（提供目标层数与决策种类）。
	 * @param ParamTable 施加方参数表（快照重建与时值求值用）。
	 * @param Overrides 施加方参数覆盖（快照重建用）。
	 * @return 返回施加结果（`Stacked` = 层数变化；`Refreshed` = 层数不变）。
	 */
	static EApplyResult RefreshStacked(
		UTcsStateSubsystem& Subsystem,
		const FTcsBuffDef& Def,
		FTcsStateInstance& Instance,
		const FTcsStateStackRequest& Request,
		const FTcsStateStackDecision& Decision,
		const TScriptInterface<ITcsParamTableReader>& ParamTable,
		const TMap<FGameplayTag, double>& Overrides);

#pragma endregion


// 移除
#pragma region Removal

public:
	/**
	 * 按句柄移除一个实例（**先撤时间条目 → 先广播后释放槽位**——见方法内注释的硬约束）。
	 *
	 * @param Subsystem 状态门面。
	 * @param Handle 实例句柄。
	 * @param Cause 移除原因。
	 * @return 返回是否移除成功（脏句柄返回 false + Warning，不 ensure）。
	 */
	static bool Remove(UTcsStateSubsystem& Subsystem, FTcsStateHandle Handle, EStateRemoveCause Cause);

	/**
	 * 注销单位：逐条移除该单位全部实例后删桶。
	 *
	 * @param Subsystem 状态门面。
	 * @param Unit 单位实体句柄。
	 * @return 返回移除的实例数。
	 */
	static int32 UnregisterUnit(UTcsStateSubsystem& Subsystem, FTcsCombatEntityHandle Unit);

#pragma endregion


// 时间条目（到期 / 周期）
#pragma region Time

public:
	/**
	 * 装配状态域求值上下文（快照构建与时值求值的**唯一装配点**）。
	 *
	 * 为什么集中一处：上下文有五个字段（参数表 / 主体 / 发起者 / 等级 / 等级读口），
	 * 分散装配迟早出现"某一处少填一个字段"的静默降级（源会落兜底而不是报错）。
	 *
	 * @param OutContext 输出上下文。
	 * @param Subsystem 状态门面（提供宿主等级读口）。
	 * @param Target 被施加方实体。
	 * @param Instigator 发起者实体（无效则取 `Target`）。
	 * @param EffectiveLevel 生效等级（本轮 = `Def.LevelBase`）。
	 * @param ParamTable 施加方参数表（可空）。
	 */
	static void MakeContext(
		FTcsStateEvaluateContext& OutContext,
		UTcsStateSubsystem& Subsystem,
		FTcsCombatEntityHandle Target,
		FTcsCombatEntityHandle Instigator,
		int32 EffectiveLevel,
		const TScriptInterface<ITcsParamTableReader>& ParamTable);

	/**
	 * 挂时值到期条目（`Finite` 路径）。
	 *
	 * 回调捕获**世界弱引用 + 句柄**：世界已拆解或句柄悬空（代际失配）时静默丢弃
	 * （时序竞态语义——不留红字、不 ensure）。
	 *
	 * @param Subsystem 状态门面。
	 * @param Instance 目标实例（须已在册）。
	 */
	static void PushExpiry(UTcsStateSubsystem& Subsystem, FTcsStateInstance& Instance);

	/**
	 * 挂周期条目（`Period > 0` 路径）：每次回调广播 `TcsEvent.State.Periodic` 后**重新入堆**。
	 *
	 * @param Subsystem 状态门面。
	 * @param Instance 目标实例（须已在册）。
	 */
	static void PushPeriod(UTcsStateSubsystem& Subsystem, FTcsStateInstance& Instance);

	/**
	 * 撤销某实例的全部时间条目（时值 + 周期）。
	 *
	 * **撤销顺序的前置一步**：移除 / 到期 / 单位注销 MUST 先调本函数再走广播链——
	 * 条目持句柄，槽位复用后回调仍会到达；先撤条目才不产生无谓回调。
	 *
	 * @param Subsystem 状态门面。
	 * @param Instance 目标实例（字段被就地清空，可重复调用）。
	 */
	static void CancelTimeEntries(UTcsStateSubsystem& Subsystem, FTcsStateInstance& Instance);

#pragma endregion


// 修正器（属性账本）
#pragma region Modifier

public:
	/**
	 * 挂载修正器（施加与刷新共用）：物化 `Def.ModifierRows` 并按**一个属性批**写入属性账本。
	 *
	 * **批是硬要求**（两条理由，缺一不可）：① 多个修正器只该触发**一次**重算与一次广播；
	 * ② 摘旧与挂新必须在同一批内完成——否则订阅者会看到"摘了一半"的中间态，
	 * 且逐条挂载会多次广播、把重入窗口放大成 N 个。
	 *
	 * **提交点之后调用方 MUST 重新定位实例与桶**：提交会重算 + 广播（属性变更事件），
	 * 订阅者可在其中重入状态操作（施加/移除/注销单位）——那会让本函数调用方手里的
	 * 实例指针与桶引用**双双失效**（槽位释放后可能被复用）。
	 *
	 * 正常分支（零模板 + 非刷新 / 单位无属性账本）MUST 静默返回：状态与属性是两套登记，
	 * "这个单位没有属性可改"不是错误（不 ensure、不留 Warning）。
	 *
	 * @param Subsystem 状态门面（提供登记表与快照读取壳）。
	 * @param Def 状态定义内容（取 `ModifierRows`）。
	 * @param Instance 目标实例（物化输入；本函数只读它）。
	 * @param bStripFirst 是否先按来源摘除旧条目（**刷新路径传 true**：账本恒与最近一次快照一致、条数不累加；
	 *        施加路径传 false——来源句柄是施加时才发放的，账本上不可能有同来源条目）。
	 */
	static void MountModifiers(
		UTcsStateSubsystem& Subsystem,
		const FTcsBuffDef& Def,
		const FTcsStateInstance& Instance,
		bool bStripFirst);

	/**
	 * 按**级联锚点**级联摘除该实例的修正器（批内提交）——移除 / 到期路径的撤销步骤之一。
	 *
	 * **为什么会开一个批**：`RemoveBySource` 在批外会立即 flush + 广播；批内则只标脏，
	 * 由本函数紧随其后的 `Commit` 一次性收口（一个锚点的 N 条修正器 = 一次重算 + 一次广播）。
	 *
	 * **调用位置有硬约束**：它可能广播（`Commit`），故调用方 MUST 在**取实例指针之前**调用它，
	 * 之后重新定位桶与实例（`FTcsStateOps::Remove` 即按此顺序书写）。
	 *
	 * @param Subsystem 状态门面。
	 * @param Unit 单位实体句柄。
	 * @param Source 级联锚点（= `FTcsStateInstance::CascadeAnchor`；**不是**"施加方来源句柄"）。
	 */
	static void StripModifiers(
		UTcsStateSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSourceHandle Source);

#pragma endregion


// 内联触发行（TRIG-4 的行为半）
#pragma region TriggerRows

public:
	/**
	 * 登记该实例的内联触发行（`Def.Triggers` 逐条）——`Source` = 实例的**级联锚点**。
	 *
	 * **调用时机有硬约束**：MUST 排在 `Applied` 广播**之前**——新登记的行要能看见自己的 `Applied`
	 * （"状态在，行为就在"的起点）；排在其后则"施加瞬间的行为"永远不触发。
	 *
	 * **撤销口径 = 按锚点级联**（`UnregisterTriggerRowsBySource`）：实例 MUST NOT 存行句柄
	 * （D3-7 v2 纪律：订阅句柄归注册表），锚点唯一 ⇒ 一次摘净且不误伤别条实例。
	 *
	 * 门面不可得（世界拆解期）⇒ `Warning` + 跳过：状态本身照常生效（状态与触发行是两套登记）。
	 *
	 * @param Subsystem 状态门面（取世界用）。
	 * @param Instance 目标实例（只读它的 `Unit` / `CascadeAnchor`）。
	 * @param Def 状态定义内容（取 `Triggers`）。
	 */
	static void WireTriggerRows(
		UTcsStateSubsystem& Subsystem,
		const FTcsStateInstance& Instance,
		const FTcsBuffDef& Def);

	/**
	 * 退订该实例的内联触发行（按级联锚点一次摘净）。
	 *
	 * **调用时机有硬约束**：MUST 排在 `Removed` / `Expired` 广播**之前**——该实例自己的行
	 * MUST NOT 因自己的死亡事件起链（"状态走了，行为先走"）。
	 *
	 * @param Subsystem 状态门面（取世界用）。
	 * @param Instance 目标实例（只读它的 `CascadeAnchor`）。
	 */
	static void UnwireTriggerRows(
		UTcsStateSubsystem& Subsystem,
		const FTcsStateInstance& Instance);

#pragma endregion


// 行为片段（Def 配置与订阅旁表）
#pragma region Behavior

public:
	/**
	 * 接线行为兴趣，必须排在 Applied 广播之前；零片段不创建 Handler、不占订阅。
	 *
	 * @param Subsystem 状态门面（拥有注册表与 Handler 强引用）。
	 * @param Instance 已在册实例（只读身份，不存实例指针）。
	 * @param Def 定义内容（片段配置只由 Def 持有）。
	 */
	static void WireBehaviors(
		UTcsStateSubsystem& Subsystem,
		const FTcsStateInstance& Instance,
		const FTcsBuffDef& Def);

	/**
	 * 摘除实例的全部行为兴趣，必须排在 Removed / Expired 广播之前。
	 *
	 * @param Subsystem 状态门面。
	 * @param Instance 待移除实例（只读其身份）。
	 */
	static void UnwireBehaviors(
		UTcsStateSubsystem& Subsystem,
		const FTcsStateInstance& Instance);

#pragma endregion


// 查询
#pragma region Query

public:
	/**
	 * 按句柄查实例。
	 *
	 * **为何不需要"单位"形参**：本函数先在全部桶里做一次**代际校验式定位**（脏句柄上
	 * `Instance.Unit` 是陈旧值、定位必然失败 ⇒ 返回 nullptr，正是我们要的），再进该桶解析。
	 * 代价 = 一次 O(桶数) 遍历（桶数量级 = 在场单位数，且查询不是热路径）。
	 * 之所以不为"句柄 → 单位"另建旁路登记表：那会引入**第二处身份真相**（表与桶可能不同步），
	 * 收益只是把一次遍历换成一次哈希——不值。
	 *
	 * @param Subsystem 状态门面。
	 * @param Handle 实例句柄。
	 * @return 返回实例指针；脏句柄返回 nullptr。
	 */
	static const FTcsStateInstance* Find(UTcsStateSubsystem& Subsystem, FTcsStateHandle Handle);

	/**
	 * 取本世界已登记的状态定义（转发 `UTcsStateSubsystem::GetRegisteredStateDef`——
	 * 时长操作要读 `DurationPolicy`，而登记表是门面私有成员）。
	 *
	 * @param Subsystem 状态门面。
	 * @param DefTag 定义身份。
	 * @return 返回定义；未登记返回 nullptr。
	 */
	static const FTcsBuffDef* GetDef(UTcsStateSubsystem& Subsystem, FGameplayTag DefTag);

	/**
	 * 广播一次状态事件（转发门面的私有广播实现——引擎函数与门面共用同一处载荷拼装）。
	 *
	 * @param Subsystem 状态门面。
	 * @param EventTag 事件 Tag（六枚之一）。
	 * @param Instance 实例（被广播时的快照）。
	 * @param Cause 移除原因（仅 Expired / Removed 有意义）。
	 */
	static void Broadcast(
		UTcsStateSubsystem& Subsystem,
		FGameplayTag EventTag,
		const FTcsStateInstance& Instance,
		EStateRemoveCause Cause);

	/**
	 * 阶段迁移（转发门面的私有实现——非法迁移 = `ensure`，见门面注释）。
	 *
	 * @param Subsystem 状态门面。
	 * @param Instance 目标实例；nullptr = 无操作（调用方已确认存在，此处只做防御）。
	 * @param NewPhase 目标阶段。
	 */
	static void Transit(UTcsStateSubsystem& Subsystem, FTcsStateInstance* Instance, EStatePhase NewPhase);

	/**
	 * 取某单位桶内在册实例数。
	 *
	 * @param Subsystem 状态门面。
	 * @param Unit 单位实体句柄。
	 * @return 返回到在册实例数。
	 */
	static int32 CountStates(UTcsStateSubsystem& Subsystem, FTcsCombatEntityHandle Unit);

#pragma endregion


// 快照
#pragma region Snapshot

public:
	/**
	 * 构建（或重建）参数快照——写入点，`ValueConvention` 的规范转换在此发生一次。
	 *
	 * 逐行规则：`Overrides` 命中该行 `Key` ⇒ 取覆盖值（**不再过一次值约定**——覆盖值是调用方
	 * 给出的规范值）；否则 `Def.Base.Evaluate(Ctx)` 后按该行 `ValueConvention` 转规范值。
	 *
	 * @param OutSnapshot 输出快照（函数内先 `Reset` 再填——重建语义）。
	 * @param Def 定义内容。
	 * @param Ctx 求值上下文（由调用方装配）。
	 * @param Overrides 施加方参数覆盖。
	 */
	static void BuildSnapshot(
		FTcsParamSnapshot& OutSnapshot,
		const FTcsBuffDef& Def,
		const FTcsParamEvaluateContext& Ctx,
		const TMap<FGameplayTag, double>& Overrides);

	/**
	 * 求值一个状态定义的总时长（秒）——`DurationTime` 经快照口径求值。
	 *
	 * **为什么经快照读而不是直接求值**：`DurationTime` 是参数行的一员（可配等级表），
	 * 施加时它已按同一上下文求值并冻结；两处各求一次就会出现"时值与参数行不同档"的双口径。
	 * 故先在快照里按键找，找不到才回落实时求值（定义没把它写进 `Params` 的形态）。
	 *
	 * @param Subsystem 状态门面（提供时钟/等级读口以装配上下文）。
	 * @param Def 定义内容。
	 * @param Snapshot 已构建的快照（可为空表）。
	 * @param Target 被施加方实体（装配上下文用）。
	 * @param Instigator 发起者实体。
	 * @param ParamTable 施加方参数表。
	 * @return 返回总时长（秒）；`EDP_Infinite` 或求值失败返回 0。
	 */
	static double EvaluateTotalDuration(
		UTcsStateSubsystem& Subsystem,
		const FTcsBuffDef& Def,
		const FTcsParamSnapshot& Snapshot,
		FTcsCombatEntityHandle Target,
		FTcsCombatEntityHandle Instigator,
		const TScriptInterface<ITcsParamTableReader>& ParamTable);

	/**
	 * 按定义的时值/周期语义重挂时间条目（施加与刷新共用一处）。
	 *
	 * 规则：`Finite` ⇒ 撤销旧时值条目、按 `DurationTime` 求值后重入堆；`Infinite` ⇒ 撤旧条目不重挂。
	 * `Period > 0` ⇒ 按 `PeriodRefresh` 处理（`Keep` 保留剩余、`Reset` 满额重建、`Immediate`
	 * 先按周期语义走一次再重建）；`Period <= 0` ⇒ 撤旧周期条目不重挂。
	 *
	 * @param Subsystem 状态门面。
	 * @param Instance 目标实例。
	 * @param Def 定义内容。
	 * @param Snapshot 已构建的快照。
	 * @param ParamTable 施加方参数表（时值求值用）。
	 * @param bRefresh 是否刷新路径（true 时 `PeriodRefresh` 才起作用）。
	 */
	static void ScheduleTime(
		UTcsStateSubsystem& Subsystem,
		FTcsStateInstance& Instance,
		const FTcsBuffDef& Def,
		const FTcsParamSnapshot& Snapshot,
		const TScriptInterface<ITcsParamTableReader>& ParamTable,
		bool bRefresh);

#pragma endregion


// 生命周期操作
#pragma region LifetimeOps

public:
	/**
	 * 时长操作的共用前置（解析实例 + 读定义 + 判定时值策略）。
	 *
	 * @param Subsystem 状态门面。
	 * @param Handle 实例句柄。
	 * @param OutInstance 输出解析到的实例（失败时不动）。
	 * @return 返回是否允许继续（`false` = 已留 Warning，调用方直接返回 false）。
	 */
	static bool PrepareDurationOp(
		UTcsStateSubsystem& Subsystem,
		FTcsStateHandle Handle,
		FTcsStateInstance*& OutInstance);

	/**
	 * 时长操作的后置（撤销旧条目 + 按新余量重入堆）——`ExtendDuration` / `SetRemaining` 共用。
	 *
	 * @param Subsystem 状态门面。
	 * @param Instance 目标实例（其 `DurationRemaining` 已被调用方改成新值）。
	 */
	static void RescheduleExpiry(UTcsStateSubsystem& Subsystem, FTcsStateInstance& Instance);

#pragma endregion
};
