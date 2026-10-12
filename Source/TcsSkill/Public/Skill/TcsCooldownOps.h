// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Clock/TcsExpiryHeap.h"
#include "Handle/TcsCombatEntityHandle.h"

#include "Skill/TcsCooldownState.h"
#include "Skill/TcsSkillEntryHandle.h"

class UTcsSkillSubsystem;
struct FTcsLearnedSkillEntry;
struct FTcsSkillDefData;

/**
 * 冷却引擎函数（设计文档 §3.1 的既定分工）：**桶只存数据与索引，操作全在这里**。
 *
 * **单一真值入口**：起轨 / 判可放 / 改轨 / 清轨 / 到期回收**全部**住本类——
 * 门禁③（`TcsCastOps` 的激活序列）与三个提交点（到期 / 事件 / 手动）**共用同一份实现**，
 * MUST NOT 各写一份判断（同一件事两个入口必然漂移，而漂移的后果是"门禁说能放、提交点说付不起"
 * 这类自相矛盾）。
 *
 * **全部成员为静态函数**：本类**无实例**（不持状态）——状态一律经 `Subsystem` 形参传入
 * （同 `FTcsSkillOps` / `FTcsCastOps` / `FTcsParamChainOps` 的形状）。
 *
 * **冷却的真值在哪（MUST 分清，这是本类最容易读错的一处）**：
 * - **自身轨**（`GroupTag` 无效）⇒ 真值住**账本条目**的 `FTcsLearnedSkillEntry::CooldownTracks[i]`；
 * - **组轨**（`GroupTag` 有效）⇒ 真值住**单位级共享槽**（`UTcsSkillSubsystem` 的
 *   `CooldownGroups`，键 = `(单位, 组标签)`），**MUST NOT** 再往条目里写一份
 *   （公共冷却的作用域是**单位**，"同组所有技能共用一条线"就是它的定义——
 *   写两份会让同组两个条目各起各的线，那就**不是 GCD 了**）。
 *
 * **导出宏是必需项**（判据 = "该符号是否被跨模块引用"）：宿主侧装置（`TcsDev`）要直接调本族静态成员
 * 或经门面转调，不带 `TCSSKILL_API` 必 `LNK2019`（同族实测）。
 */
class TCSSKILL_API FTcsCooldownOps
{
// 起轨
#pragma region Start

public:
	/**
	 * 起该条目的**全部自身轨 + 所属组轨**（自动档与 `ECT_Manual` 档**走完全同一条路径**）。
	 *
	 * **`MUST NOT` 为手动档另写一份**：手动档的定义是"框架**不挑时机**"，不是"框架**不干活**"
	 * ——宿主调 `StartCooldown` 之后要做的事，与自动档在终结/激活点要做的事**逐字相同**。
	 *
	 * **CDR 快照于此刻**（`D5-15` 明文"快照于冷却触发时刻，中途变化不追溯"）：本函数开头取一次
	 * `CooldownPct` 并作用于**全部**轨道的时长——取多次会让同一组轨道拿到不同系数。
	 *
	 * **重复起冷 = 重启**（自身轨）；**组轨相反：同组已有活槽时复用该槽、不重起不叠加**
	 * （语义依据 = "公共冷却 = 共享组轨道"：重起会让"放完 A 再放 B"把 GCD 刷成满的，
	 * 而 GCD 的作用恰恰是**不许**这样做）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param EntryHandle 账本条目句柄。
	 * @return 返回实际起的轨道条数（0 = 无轨道 / 条目悬空 / 定义取不到 / 时钟不可得）。
	 */
	static int32 StartCooldownOnEntry(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle EntryHandle);

#pragma endregion


// 判可放
#pragma region Query

public:
	/**
	 * 该条目**现在能不能放**（门禁③ 的判据；`D5-15` 原文："**可施放 = 全部自身轨道清空 +
	 * 所属组槽清空**"）。
	 *
	 * **写成一条短路遍历**：任一自身轨未清 **或** 任一所属组槽未清 ⇒ `true`。
	 *
	 * **边界（MUST 处理，不得崩）**：
	 * - `Tracks` 为空 ⇒ **恒可放**（`D5-15`："默认无冷却（空 Tracks）"）；
	 * - `GroupTag` 无效 ⇒ 该轨只在自己条目上算；
	 * - **条目状态数组比定义短**（定义被热改过）⇒ 缺的那些轨道按"未在跑"算，**不越界读**。
	 *
	 * **为什么判据是"`EndTimeSeconds > Now`"而不是"`ExpiryEntry` 有效"**：到期时刻是**时轴上的事实**，
	 * 而句柄只是"堆里有没有这条"的簿记。只看句柄的话，一旦某条到期条目被别的路径撤掉而状态没清
	 * （异常路径），该轨会**永久堵死**——症状是"这个技能再也放不出来了，且没有任何日志"。
	 * ⇒ 时间判据是**权威**，句柄只用于撤销。
	 *
	 * @param Subsystem 技能门面。
	 * @param Entry 账本条目（判据主体）。
	 * @param Def 该条目的定义内容（轨道配置来源）。
	 * @param OutHeldBy 输出"滞留在哪一轨 / 哪个组"的可读描述（可空）——
	 *        宿主最需要的读数就是它（只说"在冷却"没法排查）。
	 * @return 返回是否处于冷却中。
	 */
	static bool IsEntryOnCooldown(
		UTcsSkillSubsystem& Subsystem,
		const FTcsLearnedSkillEntry& Entry,
		const FTcsSkillDefData& Def,
		FString* OutHeldBy = nullptr);

	// 按句柄判可放（门面公开口用；条目/定义解析失败 ⇒ false = 不阻拦，交给上游具名拒绝）
	static bool IsEntryOnCooldownByHandle(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle EntryHandle,
		FString* OutHeldBy = nullptr);

	/**
	 * 取某轨的剩余秒数（UI 轮询面；`D5-15`："真值在 Entry，UI 轮询"）。
	 *
	 * **组轨的读数取自共享槽**（那才是真正在拦它的东西）——按条目里那份空状态读会恒得 0，
	 * 表现为"UI 显示没冷却、但技能放不出来"。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param EntryHandle 账本条目句柄。
	 * @param TrackIndex 轨道下标（对应 `FTcsCooldownPolicy::Tracks` 的序）。
	 * @return 返回剩余秒数（未在冷却 / 越界 / 悬空 ⇒ 0）。
	 */
	static double GetTrackRemaining(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle EntryHandle,
		int32 TrackIndex);

	/**
	 * 取某轨本次冷却的**展示总时长**（径向进度的分母）。
	 *
	 * **不变量 `Remaining <= Total` MUST 恒成立**：否则 UI 径向会画出超过 100% 的弧。
	 * `AdjustTrack` 因此只可能让 `Total` **变大**（见该函数的注释）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param EntryHandle 账本条目句柄。
	 * @param TrackIndex 轨道下标。
	 * @return 返回总时长（未在冷却 / 越界 / 悬空 ⇒ 0）。
	 */
	static double GetTrackTotal(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle EntryHandle,
		int32 TrackIndex);

	/**
	 * 取该条目的轨道数（**读定义**，不是读运行态）。
	 *
	 * **为什么不数"正在跑的轨道"**：该读数的用途是"给 UI 画几根冷却条"，而"这个技能有几条轨道"
	 * 是**定义**的属性（空 `Tracks` = 这个技能永远不冷却）；数运行态会让"正好全部冷却都跑完了"
	 * 与"这个技能根本没有冷却"给出同一个 0。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param EntryHandle 账本条目句柄。
	 * @return 返回轨道数（悬空 / 定义取不到 ⇒ 0）。
	 */
	static int32 GetTrackCount(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle EntryHandle);

#pragma endregion


// 改轨与清轨
#pragma region Mutate

public:
	/**
	 * 调整某轨的**剩余**时长（`D5-16` 承诺的 `AdjustCooldown`）。
	 *
	 * **`Delta` 作用于剩余、不作用于总时长**：正确声明应当是"还要等多久变多少"。
	 * `Delta > 0` ⇒ 延长；`Delta < 0` ⇒ 缩短；缩短到 `<= 0` ⇒ **等于清轨**（提前结束）。
	 *
	 * **★ 改完 MUST 重挂到期堆**（`CancelExpiry` 旧的 + `PushExpiry` 新的）：
	 * **MUST NOT** 只改 `EndTimeSeconds` 而不重挂——堆里的旧到期时刻会继续生效，
	 * 症状是"改了却不生效"或"提前到期"（两个方向都可能出现，取决于调整的方向）。
	 *
	 * **`Total` 的处置**：只增不减（维持 `Remaining <= Total` 的不变量）；
	 * 缩短不会让分母变小，因为"这条冷却是多久"是**已经发生过的事实**。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param EntryHandle 账本条目句柄。
	 * @param TrackIndex 轨道下标。
	 * @param Delta 剩余时长的增量（秒；正 = 延长、负 = 缩短）。
	 * @return 返回是否确实改动了某条在跑的轨道（否则 false，不 ensure）。
	 */
	static bool AdjustTrack(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle EntryHandle,
		int32 TrackIndex,
		double Delta);

	/**
	 * 清某轨（`D5-16` 承诺的 `ResetCooldown`）——撤到期条目 + 状态整体复位。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param EntryHandle 账本条目句柄。
	 * @param TrackIndex 轨道下标。
	 * @return 返回是否确实清掉了一条在跑的轨道（否则 false，不 ensure）。
	 */
	static bool ClearTrack(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle EntryHandle,
		int32 TrackIndex);

#pragma endregion


// 生命周期配对
#pragma region Lifetime

public:
	/**
	 * 摘掉某条目的全部冷却（**撤销技能 / 注销单位前的配对清理**）。
	 *
	 * **为什么必须有它（本仓"成对性"纪律的落点）**：冷却持有**外部资源**——到期堆条目与
	 * 单位级组槽。撤销条目时若不清：
	 * ① 自身轨的到期条目**失去唯一的取消锚点**（条目槽被 `ReleaseSlot` 整体重置 ⇒ 句柄没了），
	 *    只能等它自己到期再空跑一次；
	 * ② **组槽会留下**，而组槽是**单位级**的 ⇒ 它继续拦着同组的**其它**技能，
	 *    表现为"我把 A 撤了，B 却还是放不出来"——**远离根因的一类**。
	 *
	 * **调用点 = 撤销路径**（`FTcsSkillOps::Revoke` / `RevokeBySource` / `UnregisterUnit`），
	 * **MUST 排在释放槽位之前**（槽位一放，条目内容就没了，锚点也就找不回来了）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @param Entry 目标条目（就地复位其轨道状态）。
	 */
	static void ReleaseEntryCooldowns(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsLearnedSkillEntry& Entry);

	/**
	 * 摘掉某单位的**全部**冷却（含**全部组槽**）——`UnregisterUnit` 与门面反初始化用。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 */
	static void ReleaseUnitCooldowns(UTcsSkillSubsystem& Subsystem, FTcsCombatEntityHandle Unit);

	/**
	 * 取该条目的**冷却缩减（CDR）系数**——`CooldownPct` 契约键上的折叠结果。
	 *
	 * **精确语义与钳制上下界（MUST 写死，不许"合理处理"）**：
	 * - 该键**未命中**（参数表里没有、账本里也没有）⇒ `CDR = 0`（**无缩减**），零日志；
	 * - 命中后**钳制到 `[0, 1]`**：负值 ⇒ `0`（**负的 CDR 不得变成冷却延长**——那是"配了个负减免
	 *   反而被罚"的静默怪相）；`> 1` ⇒ `1`（**不得产生负时长**）；
	 * - 非有限值（NaN / Inf）⇒ 记 `Warning` 并按 `0` 处理（把"配错"报出来，MUST NOT 静默）；
	 * - **生效式**：`EffectiveDuration = Duration × (1 - CDR)` ⇒ `CooldownPct = 0.3` 表示
	 *   "时长打七折"，`= 1` 表示"冷却为 0"。
	 *
	 * **快照语义归调用方**：本函数只负责"此刻的值是多少"；"只在起冷时取一次"由
	 * `StartCooldownOnEntry` 的开头保证（`D5-15`："中途变化不追溯"）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Entry 账本条目（参数账本来源）。
	 * @param Def 该条目的定义内容（参数行来源；可空）。
	 * @return 返回钳制后的 CDR（`[0, 1]`）。
	 */
	static double EvaluateCooldownPct(
		UTcsSkillSubsystem& Subsystem,
		const FTcsLearnedSkillEntry& Entry,
		const FTcsSkillDefData* Def);

#pragma endregion


// 到期回调
#pragma region Expiry

public:
	/**
	 * 自身轨到期（到期堆回调的落点）。
	 *
	 * **★ 第一条语句即把该轨状态整体复位**（`State = FTcsCooldownTrackState();`）——
	 * 这一步同时兑现两条纪律，MUST NOT 拆开或挪到后面：
	 * ① **防 `ensure` 红字**：`CancelExpiry` 对**悬空**句柄是 `ensureMsgf`，而"到期回调正在跑"
	 *    那一刻它自己的条目**仍然有效**（`FireDueItem` 在回调**返回之后**才释放）
	 *    ⇒ 回调内若再撤本句柄就是"取消一个正在跑的条目"（留红字）。复位后 `IsValid()` 为假，
	 *    任何后续清理路径天然跳过。**同款先例 = `FTcsCastRun::PhaseExpiryEntry` 的既有注释、
	 *    `FTcsCastOps::AdvancePhase` 的第一条语句。**
	 * ② **池槽复用纪律**：本状态虽住 `TArray` 而非池，但"整体重置而非逐字段赋值"是同一条纪律
	 *    （将来给该结构加字段时自动是干净的）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄（**回调捕获**，不是从 `OwnerId` 解出来的）。
	 * @param EntryHandle 账本条目句柄（同上）。
	 * @param TrackIndex 轨道下标（同上）。
	 */
	static void ExpireTrack(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FTcsSkillEntryHandle EntryHandle,
		int32 TrackIndex);

	/**
	 * 组轨到期（到期堆回调的落点）——**整槽移除**。
	 *
	 * 与自身轨的差别：组槽是"一个组一条线"，到期即**该组解除**（不需要逐条目遍历去清）——
	 * 这正是"组槽住单位、条目只引用它"的收益：解除是 O(1) 的。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄（回调捕获）。
	 * @param GroupTag 组身份（回调捕获）。
	 */
	static void ExpireGroup(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FGameplayTag GroupTag);

#pragma endregion


// 内核
#pragma region Core

public:
	/**
	 * 取该单位**当前时刻**的组槽表（未注册返回 nullptr——正常查询路径，不 ensure）。
	 *
	 * **为什么公开这个口**：到期回调与组槽写入发生在引擎函数里，而 `CooldownGroups` 是门面私有区；
	 * 藏起来只会让下一处再往 friend 名单加一个名字（同 `GetClockSubsystem` 的理由）。
	 * **返回指针的稳定性**：外层表存值类型 `TMap`，`TMap` 元素扩容即搬移 ⇒ 调用方
	 * **MUST NOT 跨"可能新增其它单位"的调用缓存它**（同本仓 `TMap`/`TSet` 的既有纪律）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Unit 单位实体句柄。
	 * @return 返回该单位的组槽表；该单位没有活组槽时返回 nullptr。
	 */
	static TMap<FGameplayTag, FTcsCooldownGroupState>* FindGroupSlots(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit);

#pragma endregion
};
