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
 * **唯一实现**：门禁⑤（`TcsCastOps` 的激活序列）与三个提交点（到期 / 事件 / 手动）
 * **共用本族函数**，MUST NOT 各写一份判断——两处判断必然漂移，而漂移的后果正是
 * "门禁说付得起、提交点说付不起"这类自相矛盾。
 *
 * **全部成员为静态函数**：本类**无实例**（不持状态）——状态一律经 `Subsystem` 形参传入
 * （同 `FTcsSkillOps` / `FTcsCastOps` / `FTcsCooldownOps` 的形状）。
 *
 * **导出宏是必需项**（判据 = "该符号是否被跨模块引用"）：宿主侧装置（`TcsDev`）与终结路径都要调它，
 * 不带 `TCSSKILL_API` 必 `LNK2019`（同族实测）。
 */
class TCSSKILL_API FTcsCostOps
{
// 生效值
#pragma region Resolve

public:
	/**
	 * 求**本次生效的消耗量**（`Config.Amount` 的求值 ⊕ `CostAmount` 键上的减免折叠）。
	 *
	 * **★ 2026-10-12 用户裁定引入（B 形状）**：`Amount` 由静态 `double` 换成 `FTcsParamValue`
	 * 之后，"要付多少"不再是一个常数，而是**求值结果**；减免也不再需要另立系数通道，
	 * 而是**同一个键上的修正器**。两件事合成**一次求值**——这正是本函数存在的理由。
	 *
	 * **为什么是一个独立函数、而不是内联进 `CanAffordCost` / `CommitCost`**：
	 * 门禁⑤ 与**每个提交点**都要用同一个数（TOCTOU 防护的实质 = "门禁判的数与真扣的数一致"）
	 * ⇒ 求值 MUST 只有一份实现。**MUST NOT 求一次后跨"门禁 → 提交"复用**：
	 * 那会把 TOCTOU 窗口重新打开（门禁之后资源/修正器都可能变）。
	 *
	 * **两种减免形态**（`FoldTcsAttributeBands` 的既有算术，**零新机制**）：
	 * - **百分比** ⇒ `TAO_PercentAdd`（`基数 × (1 + ΣPercentAdd)`）；
	 * - **固定值** ⇒ `TAO_FlatAdd`（**全部缩放之后**的平坦加）。
	 * 作者写负数即减免（`PercentAdd = -0.3` / `FlatAdd = -10`）。
	 * **能力边界（如实）**：`FlatAdd` 排在 `PercentAdd` 之后 ⇒ "先打折再减固定值"可表达，
	 * 反之不可。这是与属性/伤害共用同一份算术的必然结果（`STAT-1` 明令禁止第二份折叠）。
	 *
	 * **值域 = `[0, +∞)`**：折叠后钳下界到 0——"减免过度 ⇒ 免费"是**合法配置结果**，
	 * 不是配置错误（配置错误的那一面判在 `CanAffordCost`：折叠**之前**的负基值仍拦）。
	 *
	 * **★ `OutBaseAmount` 为什么必须存在（装置拒绝面检查 `d` 实测抓到的设计缺陷）**：
	 * "钳 0"把**两件语义完全不同的事**压成了同一个值——
	 * ① **负基数**（作者把 `Amount` 填成 -50）= **配置错误**、"倒给资源"的漏洞形态 ⇒ MUST 拦；
	 * ② **折叠后转负**（基数 30、减免 -100）= **减免过度** = 合法的"免费"⇒ MUST 钳 0、不报错。
	 * 若只返回钳过的结果，`CanAffordCost` 就**永远看不到①**（它已被钳成 0）
	 * ⇒ 检查 `d` 当场从 `false` 翻成 `true`。⇒ 调用方 MUST 能拿到**未钳的基数**来判①。
	 *
	 * **`Entry` 可空**（`nullptr` ⇒ 无修正器可折叠 ⇒ 返回基数）：
	 * 装置与宿主会直接构造 `FTcsCostConfig` 验配置边界，那种场景没有账本条目，
	 * 而配置边界本就只依赖 `Config` 自身。**MUST NOT** 为此逼调用方造一个假条目。
	 *
	 * @param Subsystem 技能门面（形参保留以对齐同族签名；本函数不需要它）。
	 * @param Unit 单位实体句柄（求值上下文的 Subject / Instigator）。
	 * @param Level 本次生效等级（**等级表参数源就读它** ⇒ "按等级配消耗"的落点）。
	 * @param Config 消耗配置（基数来源）。
	 * @param Entry 账本条目（减免修正器来源；可为空 = 无减免）。
	 * @param OutBaseAmount 输出**未钳的基数**（可空）。**`CanAffordCost` MUST 传它**——
	 *        负基数的拦截依赖这个值（见上）。
	 * @return 返回本次生效的消耗量（已钳到 `[0, +∞)`）。
	 */
	static double ResolveEffectiveCost(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		int32 Level,
		const FTcsCostConfig& Config,
		const FTcsLearnedSkillEntry* Entry = nullptr,
		double* OutBaseAmount = nullptr);

#pragma endregion


// 门禁
#pragma region Afford

public:
	/**
	 * 该单位**现在**付得起吗（门禁⑤ 的判据；`D5-10` 的 Cost 策略分派）。
	 *
	 * **★ 五类分开处置（MUST NOT 二选一，判据见下）**：
	 *
	 * | `Policy` | 条件 | 返回 | 日志 |
	 * |---|---|---|---|
	 * | `ECP_None` | — | `true` | **无**（正确声明"不消耗"） |
	 * | `ECP_CustomFragment` | — | 按 `None` 处理 ⇒ `true` | **具名 `Warning`**（含档名） |
	 * | `ECP_ResourceAttr` | `ResourceAttr` 无效 | `false` | **`Warning`** |
	 * | `ECP_ResourceAttr` | `Amount < 0` | `false` | **`Warning`** |
	 * | `ECP_ResourceAttr` | `Amount == 0` | `true` | 无（"消耗 0 点"就是不消耗） |
	 * | `ECP_ResourceAttr` | 该单位**未持有**该属性 | `false` | **`Warning`**（与"不足"可区分） |
	 * | `ECP_ResourceAttr` | 正常 | `基值 >= Amount` | 不足时 `Log`（正常业务路径） |
	 *
	 * **★ 判据 = 本仓既有惯例，一律 fail-closed（MUST 读）**：本仓对"**配置写错**"的处置全部反着来，
	 * 措辞几乎逐字相同——`TcsFlowStepConditions.h:56` / `TcsStepModifyFlow.cpp:36` /
	 * `TcsEffectAttributeAccess.h:72`："**不静默通过**：静默会让'条件写错'表现成'步骤照跑'"。
	 * 取"恒可付"会**恰好制造本仓反复在拦的那类 bug**：技能明明配错，却照常放出来、**日志还清白**。
	 * ⇒ **区分点 = "正确声明不消耗"（`None` / `Amount == 0`，零日志）与"想配消耗但没配清"
	 * （无效 tag / 负值 / 没加属性，拦住 + `Warning`）是两件事**，混成一类就会把后者伪装成前者。
	 *
	 * **★ `Amount < 0` 为什么必须拦**：负 `Amount` 经 `SetBaseValue(Base - Amount)` 会让技能
	 * **倒给**资源（`Base - (-50)` = `Base + 50`）——这是**可被当作漏洞利用**的形态，
	 * 不是"配置不生效"。
	 *
	 * **★ 为什么 MUST 先确认"持有"**（本函数因此走 `TryGetBaseValue` 而不是"取值再比较"）：
	 * 底层的 `GetBaseValue` 对**未持有**返回 `0.0`，与"确实持有且基值为 0"**不可区分** ⇒
	 * 不先问持有，就会把"属性配错 / 压根没加"表现成"**付不起**"——而这两种红字的含义完全不同
	 * （`TcsEffectAttributeAccess.h:72` 是同一条判据的既有表述）。
	 *
	 * **拒绝面形态**：返回 `bool`，**MUST NOT ensure**——"付不起"是**正常业务路径**，不是契约违规
	 * （与 `AttributeCompare` 条件的处置同款："不通过 + 一条日志"）。
	 *
	 * **纯查询语义（MUST）**：MUST NOT 扣任何资源、MUST NOT 改账本、MUST NOT 发事件。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param Config 消耗配置。
	 * @param OutReason 输出"为什么付不起"的可读原因（可空）——宿主排查最需要的读数。
	 * @param Entry 账本条目（`CostAmount` 减免修正器来源；**可为空 = 无减免**）。
	 *        门禁⑤ 与 `PayCostForEntry` **MUST 传它**（否则减免不生效，且与提交点算出的数不一致）；
	 *        装置验配置边界时可传 `nullptr`（那种场景本就没有条目）。
	 * @return 返回是否付得起。
	 */
	static bool CanAffordCost(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		const FTcsCostConfig& Config,
		FString* OutReason = nullptr,
		const FTcsLearnedSkillEntry* Entry = nullptr);

#pragma endregion


// 提交
#pragma region Commit

public:
	/**
	 * 提交消耗（**唯一的扣减实现**——门禁⑤ 之后的提交点、`PayCost` 公开口、终结路径都调它）。
	 *
	 * **★ 扣减载体已裁定 = `SetBaseValue` 直写（读基值 → 减 → 写回）**，
	 * **MUST NOT 用 `ApplyModifier` 挂修正器**。判据三条，缺一都不成立：
	 *
	 * 1. **语义**：`SetBaseValue` 的注释原文即"**改写属性基础值（等级成长等宿主升级事务的落点）**"，
	 *    而"消耗资源"正是**永久改变基值**；挂修正器条目表达的是"**某个来源带来的增减**"
	 *    ——与"花掉了"不是一回事（"花掉 50 点蓝"**不该**能被一个来源摘除动作还回来）。
	 * 2. **来源锚点的矛盾**：`ApplyModifier` 路径**必须**给 `Source`（`FTcsSourceHandle`），
	 *    而资源扣减**不可**挂施法运行态来源（施法一终结 ⇒ 级联摘除 ⇒ **扣掉的血回来了**）；
	 *    挂"长寿命来源"则要**发明一个新发号器**，而该来源今天**零撤销消费者**
	 *    （消耗本就不可撤销）⇒ 正是"零消费者不预建"。
	 * 3. **`CHAIN-8` 已就同一问题给过判据**：修正器层**效力不等同**于直写基值——
	 *    ① 会被任一 `TAO_Override` **整体抹掉**；② 参与 `PercentAdd` / `Mul` **复利**；
	 *    ③ 可按来源摘除。**"花掉 50 点蓝"若被一个 Override 抹掉就是荒谬的** ⇒ 必须直写基值。
	 *
	 * **★ 一处 MUST NOT 的形状**：**MUST NOT 用属性侧的 `BeginBatch` / `Commit` 造一个"事务"包起来**。
	 * 判据：`D5-10` 的"激活事务"指的是**逻辑上的一次激活**，不是 M2 的属性事务批——包起来会让这次扣减
	 * 与其它属性改动**共享一个提交点**，而那个提交点今天**不存在**（`TryActivate` 无事务边界）。
	 * ⇒ 直调 `SetBaseValue`（其内部"未开批 = 隐式批，立即生效"）。
	 *
	 * **`ECP_None` ⇒ 零动作**；`ECP_CustomFragment` ⇒ 具名 `Warning` + **零提交**
	 * （MUST NOT 静默当作 `None` 扣了或没扣都说不清）。
	 *
	 * **提交失败 MUST 被报出**：`SetBaseValue` 自身对"单位未注册 / 属性名为空 / 未持有此属性"
	 * 返回 `false`（含 `ensureMsgf` 红字）⇒ 本函数 MUST 检查返回值并向上报失败，
	 * **MUST NOT 假设它一定成功**（否则 `PayCost` 会报成功而资源没扣）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param Config 消耗配置。
	 * @param OutApplied 输出实际扣减的数量（0 = 零动作；仅返回 true 时有意义）。
	 * @param Entry 账本条目（`CostAmount` 减免修正器来源；**可为空 = 无减免**，同 `CanAffordCost`）。
	 * @return 返回是否提交成功（`None` 档恒 true 且 `OutApplied = 0`）。
	 */
	static bool CommitCost(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		const FTcsCostConfig& Config,
		double* OutApplied = nullptr,
		const FTcsLearnedSkillEntry* Entry = nullptr);

	/**
	 * **按条目提交消耗**（`ECT_Manual` 档的专用入口 + 宿主主动重试口）：
	 * 解析条目 → 取其 Def 的 `Cost` → **重查一次 `CanAffordCost`**（TOCTOU 防护）→
	 * 通过则 `CommitCost`。
	 *
	 * **★ `PayCost` 的定位 MUST 写清**：它是 **`ECT_Manual` 档的专用提交入口**。
	 * **自动档（`OnCastStarted` / `OnCastCompleted` / `OnEvent`）MUST NOT 走它**——
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
	 * @param Unit 单位实体句柄。
	 * @param EntryHandle 账本条目句柄。
	 * @return 返回是否提交成功（`None` 档恒 true；付不起 / 悬空 / 定义取不到 ⇒ false）。
	 */
	static bool PayCostForEntry(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle EntryHandle);

	/**
	 * 广播 `TcsEvent.Cast.CostApplied`（**统一出口**——载荷装配只有一处，全部提交点共用）。
	 *
	 * **为什么由调用方发而不是 `CommitCost` 内部发**：`CommitCost` 是**纯扣减**——
	 * 它连"技能"都不需要知道（激活路径的 `OnCastStarted` 档就是直调它）。而事件载荷需要
	 * **条目身份**（`EntryHandle` / `DefTag`）⇒ 那只有拿到条目的调用方才有
	 * ⇒ 职责切分：**`CommitCost` 扣减，本函数发事件**。
	 *
	 * **只有真的扣了才发**（扣减量 `<= 0` / `Policy != ECP_ResourceAttr` ⇒ 零动作、零事件）：
	 * "消耗 0 点"与"不消耗"都不是"扣了一笔"，发它们会让订阅者收到无意义的通知。
	 *
	 * **判据取 `AppliedAmount` 而不是 `Config.Amount`**（2026-10-12 B 形状换型连带）：
	 * 换型后 `Config.Amount` 是**参数源载体**、不是数字；而"有没有真扣"只有**实际扣减量**知道
	 * ——"减免到 0"那一刻恰好就是"没扣"。
	 *
	 * @param Subsystem 技能门面。
	 * @param Entry 账本条目（载荷身份来源）。
	 * @param Config 消耗配置（载荷的资源键来源）。
	 * @param AppliedAmount **实际扣掉的量**（由 `CommitCost` 的 `OutApplied` 给出）。
	 *        载荷的 `Amount` MUST 取它，**MUST NOT 取 `Config.Amount` 的旧字段语义**——
	 *        后者在换型后已不是数字，且"发出扣了 30 而实际扣了 21"会直接误导订阅者。
	 */
	static void BroadcastCostApplied(
		UTcsSkillSubsystem& Subsystem,
		const FTcsLearnedSkillEntry& Entry,
		const FTcsCostConfig& Config,
		double AppliedAmount);

#pragma endregion
};
