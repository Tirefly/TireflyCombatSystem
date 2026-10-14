// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Handle/TcsCombatEntityHandle.h"

#include "Def/TcsCostTypes.h"
#include "Skill/TcsSkillEntryHandle.h"

class UTcsSkillSubsystem;
struct FTcsLearnedSkillEntry;
struct FTcsSkillDefData;

/**
 * 消耗引擎函数（设计文档 §3.1 的既定分工）：**桶只存数据与索引，操作全在这里**。
 *
 * **唯一实现**：门禁⑤（`TcsCastOps` 的激活序列）与**三个提交点**（施法开始 / 完成 / 手动）
 * **共用本族函数**，MUST NOT 各写一份判断——两处判断必然漂移，而漂移的后果正是
 * "门禁说付得起、提交点说付不起"这类自相矛盾。
 *
 * **★ 2026-10-12 用户改点 = 策略模式**：本族从"档位枚举 + 写死的属性扣减"改为
 * **"框架只做分派，扣减归策略"**。框架自带的属性资源策略
 * （`FTcsSkillCostPolicy_ResourceAttr`）**也走同一套**，不写死在代码里。
 * 判据（MUST 读，防后人改回枚举）：内建写死 + 自定义另开一条 = **两条会分叉的路**
 * ⇒ 门禁判的数与真扣的数会漂移，而那正是 TOCTOU 防护要防的形态。
 *
 * **全部成员为静态函数**：本类**无实例**（不持状态）——状态一律经 `Subsystem` 形参传入
 * （同 `FTcsSkillOps` / `FTcsCastOps` / `FTcsCooldownOps` 的形状）。
 *
 * **导出宏是必需项**（判据 = "该符号是否被跨模块引用"）：宿主侧装置（`TcsDev`）与终结路径都要调它，
 * 不带 `TCSSKILL_API` 必 `LNK2019`（同族实测）。
 */
class TCSSKILL_API FTcsCostOps
{
// 策略解析
#pragma region Resolve

public:
	/**
	 * 解析配置里的消耗策略实例（**唯一解析点**）。
	 *
	 * **取内层一律用单参数 `GetPtr<FTcsSkillCostPolicy>()`**（本仓 2026-09-30 源码核实）：
	 * 它在 `StructMemory == nullptr`（空载荷）**或**类型不符时**返回 `nullptr`**
	 * ⇒ 一个判据同时覆盖"载荷缺失"与"类型不符"。
	 * **★ MUST NOT 用双模板参数重载** `GetPtr<T, BaseT>()`——它在 `Shipping` 下**完全不校验**
	 * （`StructUtils.h` 源码自认 "this may crash in shipping!"）⇒ 同一份代码编辑器里安全、
	 * Shipping 里类型混淆，**编译查不出来**。
	 *
	 * @param Config 消耗配置。
	 * @return 返回策略指针；`CostMode` 为空或类型不符时返回 `nullptr`（**是否构成错误取决于
	 *         `Timing`**——`None` 档下空值是合法的，见 `FTcsSkillCostConfig` 的注释）。
	 */
	static const FTcsSkillCostPolicy* ResolvePolicy(const FTcsSkillCostConfig& Config);

	/**
	 * 组装策略求值上下文（`Subsystem` / `Entity` / `Entry` / `Level` 四件）。
	 *
	 * **等级从哪来**：`Entry` 存在时取**条目等级**（等级表参数源读的就是它）；
	 * 无条目（装置验配置边界）时取 0——那种场景的判据不依赖等级。
	 *
	 * @param Subsystem 技能门面。
	 * @param Entity 实体实体句柄。
	 * @param Entry 账本条目（可为空 = 无减免、无等级）。
	 * @return 返回装配好的上下文。
	 */
	static FTcsSkillCostContext MakeContext(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Entity,
		const FTcsLearnedSkillEntry* Entry);

#pragma endregion


// 门禁
#pragma region Afford

public:
	/**
	 * 该实体**现在**付得起吗（门禁⑤ 的判据）。
	 *
	 * **★ 三类分开处置（MUST NOT 二选一，判据见下）**：
	 *
	 * | `Timing` | `CostMode` | 返回 | 日志 |
	 * |---|---|---|---|
	 * | `ECT_None` | 任意（**一概不看**） | `true` | **无**（正确声明"不消耗"） |
	 * | 非 `None` | **空 / 类型不符** | `false` | **`Warning`**（配置错误，fail-closed） |
	 * | 非 `None` | 有效 | 由**策略**答 | 由策略决定 |
	 *
	 * **★ 判据 = `ECT_None` 是"不消耗"的唯一入口**（2026-10-12 用户裁定："只要 CostTiming 为 None，
	 * 那就不用管后面了，因为一定证明这个技能没有消耗"）⇒ `None` 档下**不看策略、零日志**。
	 *
	 * **★ 而 `Timing != None` + 空策略 MUST 拦住**（fail-closed，MUST NOT 静默按"不消耗"放过）：
	 * 判据 = 本仓既有惯例，措辞几乎逐字相同（`TcsFlowStepConditions.h:56` /
	 * `TcsStepModifyFlow.cpp:36` / `TcsEffectAttributeAccess.h:72`）："**不静默通过**：静默会让
	 * '条件写错'表现成'步骤照跑'"。取"恒可付"会**恰好制造本仓反复在拦的那类 bug**：
	 * 技能明明配错，却照常放出来、**日志还清白**。
	 * ⇒ **区分点 = "正确声明不消耗"（`Timing == None`，零日志）与"想配消耗但没配清"
	 * （`Timing != None` 而策略为空，拦住 + `Warning`）是两件事**，混成一类就会把后者伪装成前者。
	 *
	 * **★ 与换型前的行为差异（收紧，MUST 记录）**：换型前 `ECP_CustomFragment` 档是
	 * **免费放**（恒可付 + `Warning`、零扣减），而**同一个文件的下面**写着"资源都没法查就放技能，
	 * 等于免费放——那是'配置了消耗却静默不生效'的最坏形态"⇒ 同一个失败类别一处拦一处放。
	 * 现在真实现取代了降级，且空策略改为拦住 ⇒ **洞已堵上**。
	 * （**MUST NOT** 与 `CQM_Custom` 的降级简单类比：它回退到 `DefSwitches` = **一个有定义的行为**；
	 * 这里的空策略回退 = **免费**，**不是一个安全的默认**。）
	 *
	 * **纯查询语义（MUST）**：MUST NOT 扣任何资源、MUST NOT 改账本、MUST NOT 发事件。
	 *
	 * @param Subsystem 技能门面。
	 * @param Entity 实体实体句柄。
	 * @param Config 消耗配置。
	 * @param OutReason 输出"为什么付不起"的可读原因（可空）——宿主排查最需要的读数。
	 * @param Entry 账本条目（策略的减免修正器与等级来源；**可为空 = 无减免**）。
	 *        门禁⑤ 与 `PayCostForEntry` **MUST 传它**（否则减免不生效，且与提交点算出的数不一致）；
	 *        装置验配置边界时可传 `nullptr`（那种场景本就没有条目）。
	 * @return 返回是否付得起。
	 */
	static bool CanAffordCost(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Entity,
		const FTcsSkillCostConfig& Config,
		FString* OutReason = nullptr,
		const FTcsLearnedSkillEntry* Entry = nullptr);

#pragma endregion


// 提交
#pragma region Commit

public:
	/**
	 * 提交消耗（**唯一的提交实现**——门禁⑤ 之后的提交点、`PayCost` 公开口、终结路径都调它）。
	 *
	 * **★ 原子性（2026-10-12 用户裁定：框架把策略的提交包进属性批）**：
	 * 通过 `UTcsAttributeSubsystem::BeginBatch` / `Commit` 把**策略的整个提交**包起来。
	 *
	 * **批给什么、不给什么（MUST 读，防误判——源码核实 2026-10-12）**：
	 * - **给**：① **重算与广播推迟到最外层 `Commit`**（`BatchDepth` 嵌套计数，内层提交直接返回）；
	 *   ② **订阅者看不到中间态** ⇒ **关闭重入窗口**（"第一条扣减广播 → 订阅者花掉资源 →
	 *   第二条扣不动"这条真实窗口，单资源下不存在、多资源下存在）。
	 * - **不给**：**回滚**。`FTcsAttributePipeline::SetBaseValue` 的写入是**立即**的
	 *   （`Instance->BaseValue = NewBaseValue; Instance->bDirty = true;`），批只推迟 `Recalculate`。
	 * - ⇒ **"全有全无"的实际保障 = 先判后扣**（调用方 MUST 先过 `CanAffordCost`），**不是批本身**。
	 *   **MUST NOT** 把批当作事务/回滚来依赖。
	 *
	 * **★ 一处必须的守卫（本框架包批引入的新问题）**：`BeginBatch` / `Commit` 对
	 * **未注册实体**会触发 `ensureMsgf`（红字）。内建属性策略不可能走到那里（它先要
	 * `TryGetBaseValue` 成功 ⇒ 实体必然已注册），但**宿主自定义策略**可能"没注册实体也判可付"
	 * ⇒ 框架包批会**替宿主多报一条 ensure**。故包批前 MUST 用**不 ensure** 的
	 * `ResolveStore` 探测注册状态；未注册时**不包批**（策略照常提交，其自身失败面自行上报）。
	 *
	 * **★ 批的边界 = 紧邻本策略的提交**，MUST NOT 圈进施法激活的其它属性改动
	 * （如 `MaterializeParamChainRows`）——圈的边界就是"这一次消耗"。
	 *
	 * **★ 与旧禁令的关系（MUST 记录）**：换型前明文 **MUST NOT** 用属性侧 `BeginBatch` 包成本，
	 * 判据是"那个提交点今天**不存在**"（`TryActivate` 无事务边界）。本轮用户裁定**推翻该禁令**：
	 * 多资源下确实需要一个"本组扣减的提交点"（防重入）。旧禁令的**前提已不成立**，
	 * 但它防的那件事（与无关属性改动共享提交点）**依然要防** ⇒ 由"批边界 = 紧邻本策略"承担。
	 *
	 * **`ECT_None` ⇒ 零动作**（不看策略、零日志）。
	 * **`Timing != None` 而策略为空 ⇒ 零提交 + `Warning`**（与 `CanAffordCost` 同判据）。
	 *
	 * **提交失败 MUST 被报出**：策略内部的写口（如 `SetBaseValue`）自身对"实体未注册 / 属性名为空 /
	 * 未持有此属性"返回 `false` ⇒ 本函数 MUST 把该失败**向上报**，
	 * **MUST NOT 假设它一定成功**（否则 `PayCost` 会报成功而资源没扣）。
	 *
	 * **★ 内建策略的扣减载体 = `SetBaseValue` 直写（读基值 → 减 → 写回）**，
	 * **MUST NOT 用 `ApplyModifier` 挂修正器**。判据三条（与换型前同，MUST 保留）：
	 * 1. **语义**：`SetBaseValue` 的注释原文即"改写属性基础值（等级成长等宿主升级事务的落点）"，
	 *    而"消耗资源"正是**永久改变基值**；挂修正器表达的是"某个来源带来的增减"——与"花掉了"不是一回事；
	 * 2. **来源锚点的矛盾**：`ApplyModifier` **必须**给 `Source`，而资源扣减**不可**挂施法运行态来源
	 *    （施法一终结 ⇒ 级联摘除 ⇒ **扣掉的血回来了**）；挂长寿命来源则要发明新发号器，
	 *    而该来源今天**零撤销消费者**（消耗本就不可撤销）；
	 * 3. **`CHAIN-8`**：修正器层**效力不等同**于直写基值——会被任一 `TAO_Override` 整体抹掉、
	 *    参与复利、可按来源摘除。**"花掉 50 点蓝"若被一个 Override 抹掉就是荒谬的**。
	 * **读基值（而不是 `EvaluateCurrent`）**：从**当前值**减会在有修正器时**多减**。
	 *
	 * @param Subsystem 技能门面。
	 * @param Entity 实体实体句柄。
	 * @param Config 消耗配置。
	 * @param OutApplied 输出实际扣减的数量（0 = 零动作；仅返回 true 时有意义）。
	 * @param Entry 账本条目（策略的减免修正器与等级来源；**可为空 = 无减免**，同 `CanAffordCost`）。
	 * @return 返回是否提交成功（`None` 档恒 true 且 `OutApplied = 0`）。
	 */
	static bool CommitCost(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Entity,
		const FTcsSkillCostConfig& Config,
		double* OutApplied = nullptr,
		const FTcsLearnedSkillEntry* Entry = nullptr);

	/**
	 * **按条目提交消耗**（`ECT_Manual` 档的专用入口 + 宿主主动重试口）：
	 * 解析条目 → 取其 Def 的 `Cost` → **重查一次 `CanAffordCost`**（TOCTOU 防护）→
	 * 通过则 `CommitCost`。
	 *
	 * **★ `PayCost` 的定位 MUST 写清**：它是 **`ECT_Manual` 档的专用提交入口**。
	 * **自动档（`OnCastStarted` / `OnCastCompleted`）MUST NOT 走它**——
	 * 自动档直调 `CommitCost`，否则会多一层"按 Timing 判断要不要提交"的分支，
	 * 而提交点应当**已经知道**自己该不该提交（那正是"单一挂点按 `Reason` 分派"的形式）。
	 *
	 * **重查是 TOCTOU 防护的落点**（`PLN-R6.5-1` §0.2 裁决 ④）：门禁⑤ 通过到本提交点之间，
	 * 资源可能已被花掉 ⇒ 必须**再判一次**。判据与门禁**同一函数**（MUST NOT 各写一份）。
	 *
	 * **为什么手动档 MUST 不自动执行任何东西**：`ECT_Manual` 的定义就是"框架不挑时机"
	 * （宿主在自己的确定逻辑处调本口）——框架替它挑时机就违背了该档的存在理由。
	 *
	 * @param Subsystem 技能门面。
	 * @param Entity 实体实体句柄。
	 * @param EntryHandle 账本条目句柄。
	 * @return 返回是否提交成功（`None` 档恒 true；付不起 / 悬空 / 定义取不到 ⇒ false）。
	 */
	static bool PayCostForEntry(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Entity,
		FTcsSkillEntryHandle EntryHandle);

	/**
	 * 广播 `TcsEvent.Cast.CostApplied`（**统一出口**——载荷装配只有一处，全部提交点共用）。
	 *
	 * **为什么由调用方发而不是 `CommitCost` 内部发**：`CommitCost` 是**纯提交**——
	 * 它连"技能"都不需要知道（激活路径的 `OnCastStarted` 档就是直调它）。而事件载荷需要
	 * **条目身份**（`EntryHandle` / `DefTag`）⇒ 那只有拿到条目的调用方才有
	 * ⇒ 职责切分：**`CommitCost` 提交，本函数发事件**。
	 *
	 * **只有真的扣了才发**（扣减量 `<= 0` ⇒ 零动作、零事件）：
	 * "消耗 0 点"与"不消耗"都不是"扣了一笔"，发它们会让订阅者收到无意义的通知。
	 *
	 * **判据取 `AppliedAmount` 而不是配置基数**（参数化换型连带）：
	 * 配置里的 `Amount` 是**参数源载体**、不是数字；而"有没有真扣"只有**实际扣减量**知道
	 * ——"减免到 0"那一刻恰好就是"没扣"。
	 *
	 * **★ 载荷的 `ResourceAttr` 是内建策略专属字段（如实登记）**：只有
	 * `FTcsSkillCostPolicy_ResourceAttr` 才有一个明确的"被扣属性"；**宿主自定义策略**扣什么
	 * 框架不认识 ⇒ 该字段留无效。MUST NOT 为此试图去猜（那等于让框架认识宿主的资源语义）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Entry 账本条目（载荷身份来源）。
	 * @param Config 消耗配置（载荷的资源键来源——仅内建策略可填）。
	 * @param AppliedAmount **实际扣掉的量**（由 `CommitCost` 的 `OutApplied` 给出）。
	 *        载荷的 `Amount` MUST 取它，**MUST NOT 取配置基数**——
	 *        后者在换型后已不是数字，且"发出扣了 30 而实际扣了 21"会直接误导订阅者。
	 */
	static void BroadcastCostApplied(
		UTcsSkillSubsystem& Subsystem,
		const FTcsLearnedSkillEntry& Entry,
		const FTcsSkillCostConfig& Config,
		double AppliedAmount);

#pragma endregion
};
