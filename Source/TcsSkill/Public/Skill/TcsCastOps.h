// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Handle/TcsCombatEntityHandle.h"

#include "Def/TcsSkillActivateResult.h"
#include "Skill/TcsCastFinishReason.h"
#include "Skill/TcsCastRun.h"
#include "Skill/TcsCastRunHandle.h"
#include "Skill/TcsSkillEntryHandle.h"

class UTcsSkillSubsystem;
class UTcsEffectSubsystem;
struct FTcsSkillDefData;
struct FTcsLearnedSkillEntry;
struct FTcsEffectContext;

/**
 * 参数求值上下文（**只作 `const&` 形参 ⇒ 前向声明即可**，不必让本头依赖 `TcsParamValueSource.h`）。
 *
 * **为什么单列这条注**：本头原稿直接用该类型却未引入定义（`TcsStateSnapshot.h` 并**不**包含
 * `TcsParamValueSource.h`）⇒ 会是 `C2027: use of undefined type`。按本仓"头文件少暴露实现"
 * 的取向，**按引用使用的类型一律前向声明**；需要完整定义的 `.cpp` 各自显式 include。
 */
struct FTcsParamEvaluateContext;



/**
 * 施法运行态引擎函数（设计文档 §3.1 的既定分工）：**桶只存数据与索引，操作全在这里**。
 *
 * **为什么必须存在本类**（同 `FTcsSkillOps` / `FTcsStateOps` 的判据）：门面的职责是
 * **世界过滤 + 定义登记 + 注入面**；六道门禁的裁决流程、运行态的建/终结、快照构建、起链与广播
 * 按操作种类集中在本类，这样 `UTcsSkillSubsystem.cpp` 保持薄壳，而流程改动不与 UE 生命周期样板混在一起。
 *
 * **它不是"零消费者预建"**：本类的消费者**就是**门面的激活口（同族先例 = `FTcsStateOps` 的 `Apply`）。
 * **访问私有区是它的存在理由之一**——`Registry` / `CastRuns` / `RegisteredDefs` 是门面的私有成员，
 * 裁决逻辑落在门面类里会让"谁有权改运行态"与"谁是门面"混作一团
 * （状态侧同款：`FTcsStateOps` 是 `UTcsStateSubsystem` 的 friend）。
 *
 * **全部成员为静态函数**：本类**无实例**（不持状态）——状态一律经 `Subsystem` 参数传入。
 *
 * 文件划分：本头 + `TcsCastOps.cpp`（门禁裁决与运行态生命周期）/ `TcsCastOps_Snapshot.cpp`（快照构建）/
 * `TcsCastOps_Chain.cpp`（起链）/ `TcsCastOps_Phase.cpp`（时段推进与到期回调）/ `TcsCastOps_Query.cpp`
 * （三档查询契约与打断入口）。
 *
 * **导出宏是必需项**（判据 = "该符号是否被跨模块引用"）：宿主侧装置（`TcsDev`）若直接调本族静态成员，
 * 不带 `TCSSKILL_API` 必 `LNK2019`（状态侧同款实测）。
 */
class TCSSKILL_API FTcsCastOps
{
// 激活
#pragma region Activate

public:
	/**
	 * 尝试激活一个已学技能——六道**具名**门禁的裁决与运行态的建立。
	 *
	 * **门禁序列（评估序）**：① 实体 Ready → ② 已学 → ③ 冷却（`R6.5` 占位，恒过）→
	 * ④ Instancing 四路分流 → ⑤ CanAfford（`R6.5` 占位，恒过）→ ⑥ 定义可解析。
	 *
	 * **评估可达性约束（MUST）**：第 ③④⑤ 道**均需读取定义内容**（冷却轨道 / 实例化与顶替位 /
	 * Cost 策略都是 Def 字段）⇒ 定义取不到时 MUST 立即返回 `SAR_DefInvalid` 并**不评估它们**；
	 * 否则它们会拿到"取不到定义"的空数据并返回**看似合理但错误**的枚举值（假读数）。
	 *
	 * **拒绝无副作用（MUST）**：任一道拒绝 ⇒ 不建 run、不改账本、不起链、不发事件。
	 *
	 * @param Subsystem 技能门面（提供账本、登记表、运行态池与注入面）。
	 * @param Entity 施法实体。
	 * @param DefTag 技能定义身份。
	 * @param OutRun 输出运行态句柄（可选；失败时不写）。
	 * @return 返回激活结果（具名）。
	 */
	static ESkillActivateResult Activate(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Entity,
		FGameplayTag DefTag,
		FTcsCastRunHandle* OutRun);

#pragma endregion


// 运行态生命周期
#pragma region Lifetime

public:
	/**
	 * **四路汇聚的唯一终结例程**（台账 `CHAIN-7` 的落地处）：
	 * ① 完成（全时段走完 / **瞬发当刻**）、② 打断（`CancelCast`）、③ 顶替、④ 世界拆解（池 `Reset`，
	 * **不逐槽走本函数、不动账本**——世界正在拆，账本随之消亡）。
	 *
	 * **MUST NOT 各写一份清理**：四路的回收动作**逐字相同**，只有"发哪一枚终止事件"分岔。
	 * 拆成多份必然漂移，而漂移的后果是**静默泄漏**（条目留在账本里、属性不回退，且无人报错）。
	 *
	 * **本函数按序做六件事（顺序有理由，逐条见 `.cpp`）**：
	 * ① 重入窗口守卫（在飞终结标记，命中即**返回 false 且静默**）；
	 * ② 取消当前时段的到期锚点（`PhaseExpiryEntry`）；
	 * ③ 按 `RunSource` 回收**链挂的 M2 属性账本条目**（`UTcsAttributeSubsystem::RemoveBySource`）
	 *    **与**本 run 物化的 **M5 参数链条目**（`FTcsParamChainOps::RemoveParamModifiersBySource`）
	 *    ——**两本账、同一个来源句柄，MUST NOT 只摘其一**；
	 * ④ 摘账本 `RunHandles` 中该句柄（与回填成对）；
	 * ⑤ 广播终止事件（**MUST 排在归还槽位之前**——载荷里的运行句柄在回调内仍须有效）；
	 * ⑥ 归还池槽位。
	 *
	 * **`Reason` 是唯一分岔输入**：`ECFR_Completed` → `OnCastCompleted`；`ECFR_Interrupted` /
	 * `ECFR_Replaced` → `OnCastInterrupted`。**MUST NOT 互相冒充**（照抄 GAS 的
	 * `EndAbility(bWasCancelled = false)` 会让配 `OnCastCompleted` 起链的技能在被打断时真的打出主链）。
	 *
	 * @param Subsystem 技能门面。
	 * @param RunHandle 要终结的运行态句柄。
	 * @param Reason 终结原因（决定发哪一枚事件）。
	 * @return 返回是否确实终结了一个在册运行态；**重入调用 / 悬空句柄返回 false 且静默**（不 ensure）。
	 */
	static bool FinishCastRun(
		UTcsSkillSubsystem& Subsystem,
		FTcsCastRunHandle RunHandle,
		ECastFinishReason Reason);

	/**
	 * 终结一个运行态（**顶替路径**的便捷入口）——等价于 `FinishCastRun(…, ECFR_Replaced)`。
	 *
	 * **为什么保留它而不是把顶替路径直接改成 `ECFR_Replaced`**：它是 Task 3 已交付的公开面
	 * （宿主装置 `TcsDevSkillCastProbe.cpp` 的夹具清理动作在调它）⇒ 改签名会波及既有装置；
	 * 且"顶替"这个语义在调用点比"终结"更可读。**MUST NOT** 在它里面再写一份清理
	 * （它只转发一条、零逻辑——这正是"四路只在一个地方分岔"的形式）。
	 *
	 * @param Subsystem 技能门面。
	 * @param RunHandle 要终结的运行态句柄。
	 * @return 返回是否确实终结了一个在册运行态。
	 */
	static bool TerminateRun(UTcsSkillSubsystem& Subsystem, FTcsCastRunHandle RunHandle);

	/**
	 * 清空全部运行态（门面 `Deinitialize` 的确定性清空口）。
	 *
	 * **不逐槽清理实例内容**：池的零策略纪律同款——`Reset` 使全部旧句柄代际失配。
	 *
	 * @param Subsystem 技能门面。
	 * @return 返回清空前的在册运行态数（观测用）。
	 */
	static int32 ResetAll(UTcsSkillSubsystem& Subsystem);

	/**
	 * 取某运行态（悬空句柄返回 nullptr——**正常查询路径，不 ensure**）。
	 *
	 * @param Subsystem 技能门面。
	 * @param RunHandle 运行态句柄。
	 * @return 返回运行态；悬空句柄返回 nullptr。
	 */
	static FTcsCastRun* Find(UTcsSkillSubsystem& Subsystem, FTcsCastRunHandle RunHandle);

	// 在册运行态数（观测与装置断言用；**验收读数取它**，MUST NOT 取"Activate 被调用几次"）
	static int32 CountRuns(UTcsSkillSubsystem& Subsystem);

#pragma endregion


// Timing 触发点（冷却 / Cost 的终结侧分派）
#pragma region TimingDispatch

public:
	/**
	 * **冷却的终结侧触发点**（唯一挂点——`FinishCastRun` 内按 `Reason` 分派）。
	 *
	 * **四路终结在此分派，MUST NOT 去四个调用点各挂一份**：末段走完 / 瞬发当刻 / 打断 / 顶替
	 * 全部汇聚到 `FinishCastRun`，而它的签名自带 `ECastFinishReason Reason`。
	 *
	 * **分派表**：`ECFR_Interrupted` / `ECFR_Replaced` ⇒ 算"被打断"（起 `ECT_OnCastInterrupted`
	 * 档——顶替是打断的一种终止形态，`SPEC-04-skill` §11.5 裁决 1）；`ECFR_Completed`
	 * ⇒ `ECT_OnCastCompleted` 档（**含瞬发**：瞬发以 `ECFR_Completed` 终结，故天然被覆盖，
	 * **不需要额外挂点**）。`ECT_Manual` / `ECT_OnCastStarted` 两档在此**零动作**
	 * （前者"框架不挑时机"、后者已在激活路径起过）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Entry 账本条目（配置来源与起冷主体）。
	 * @param Def 该条目的定义内容（冷却策略来源；调用方 MUST 用**回收之后重新解析**的那一份）。
	 * @param Reason 终结原因（分派输入）。
	 */
	static void DispatchCooldownTiming(
		UTcsSkillSubsystem& Subsystem,
		const FTcsLearnedSkillEntry& Entry,
		const FTcsSkillDefData& Def,
		ECastFinishReason Reason);

	/**
	 * **Cost 的终结侧触发点**——只处理 `ECT_OnCastCompleted` 一档。
	 *
	 * **其余两档各有归属，MUST NOT 在此代劳**：`OnCastStarted` 在激活路径（门禁后、建 run 前）；
	 * `Manual` 由宿主自己调 `PayCost`（框架零动作——"某个特定事件触发消耗"这类项目玩法归这一档，
	 * 2026-10-12 用户裁定删除 `ECT_OnEvent`，判据见 `TcsCostTypes.h`）。
	 *
	 * **被打断 / 被顶替时 MUST NOT 提交**：`ETcsSkillCostTiming` 刻意没有 `OnCastInterrupted` 档
	 * （`D5-4` 已砍返还 ⇒ 该档两种读法一为重复一为荒谬）。"施法开始时扣"这一档已经覆盖了
	 * "被打断也扣了"这个语义。
	 *
	 * **提交走 `PayCostForEntry`**（而不是直调 `CommitCost`）：前者**自带一次 `CanAffordCost` 重查**
	 * ＝TOCTOU 防护（门禁⑤ 通过到此刻之间资源可能已被花掉）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Entry 账本条目。
	 * @param Def 该条目的定义内容（Cost 配置来源；调用方 MUST 用**回收之后重新解析**的那一份）。
	 * @param Reason 终结原因。
	 */
	static void DispatchCostTiming(
		UTcsSkillSubsystem& Subsystem,
		const FTcsLearnedSkillEntry& Entry,
		const FTcsSkillDefData& Def,
		ECastFinishReason Reason);

#pragma endregion


// 快照
#pragma region Snapshot

public:
	/**
	 * 构建技能参数快照——**写入点**，`ValueConvention` 的规范转换在此发生一次。
	 *
	 * **为什么技能侧要自己一份**（不是"建第二套快照"）：状态侧的 `FTcsStateOps::BuildSnapshot`
	 * 形参**硬编码 `const FTcsBuffDef&`** ⇒ 技能侧**物理上无法复用**。**复用面 = 快照类型
	 * `FTcsParamSnapshot` + 读取壳 `UTcsStateParamTableReader` + 绑定作用域
	 * `FTcsStateSnapshotScope`**（MUST NOT 建第二套快照类型、MUST NOT 出现第二处键查找语义）；
	 * **构建函数按 Def 类型各一份**，两者 MUST 保持**同一套写入点规则**。
	 *
	 * 逐行规则（与状态侧逐字同款）：`Overrides` 命中该行 `Key` ⇒ 取覆盖值（**不再过一次值约定**
	 * ——覆盖值是调用方给出的规范值）；否则 `Row.Base.Evaluate(Ctx)` 后按该行 `ValueConvention`
	 * 经 `FTcsValueConvention::ConvertToCanonical` 转规范值（**能力位为假的源不转**——判据由源自身
	 * 声明，MUST NOT 建"源类型 × 可配约定"的中心名单）。
	 *
	 * @param OutSnapshot 输出快照（函数内先 `Reset` 再填——重建语义）。
	 * @param Def 技能定义内容。
	 * @param Ctx 求值上下文（由调用方装配）。
	 * @param Overrides 覆盖值表（可空）。
	 */
	static void BuildSkillSnapshot(
		FTcsParamSnapshot& OutSnapshot,
		const FTcsSkillDefData& Def,
		const FTcsParamEvaluateContext& Ctx,
		const TMap<FGameplayTag, double>& Overrides);

#pragma endregion


// 时段推进
#pragma region Phase

public:
	/**
	 * 进入（或推进到）某时段——**登记该段的到期条目**。
	 *
	 * **时长求值 MUST 取本次激活冻结的快照**（`FTcsCastRun.ParamSnapshot`，经
	 * `UTcsStateParamTableReader` + `FTcsStateSnapshotScope`，与 `ParamChainRows` 物化**同一套绑定手法**）
	 * ⇒ 引用类时长（`Param()`）读到的是**激活时刻**的值 ⇒ "被修正器改变时段时长 ⇒ 下一次激活生效"
	 * 的语义**天然成立**（不需要任何"重算"代码）。
	 *
	 * **到期时刻 = `Clock->GetClock().Elapsed + Duration`**（`Elapsed` 时轴上的**绝对**时刻），
	 * 入堆经 `UTcsClockSubsystem::PushExpiry`——**`FTcsExpiryHeap` 是泵私有，MUST NOT 直接触达**
	 * （`AdvanceTo` 保持泵私有，时间推进的唯一驱动是帧首泵点）。
	 *
	 * **时钟不可得（世界拆解 / 单测无时钟）⇒ 静默降级**：`Log` 级、**零红字**、
	 * **不留待推进时段**（退化为瞬发语义）、**MUST NOT ensure**。理由：那是时序而非配置错误。
	 *
	 * @param Subsystem 技能门面。
	 * @param RunHandle 运行态句柄。
	 * @return 返回是否真的排定了一个待推进时段（false = 无待推进时段或时钟不可得 ⇒ 调用方应走完成终结）。
	 */
	static bool EnterPhase(UTcsSkillSubsystem& Subsystem, FTcsCastRunHandle RunHandle);

	/**
	 * 时段到期推进（到期堆回调的落点）：`++PhaseIndex` → 发 `PhaseChanged` → 还有待推进时段则注册下一段，
	 * 否则走**完成终结**。
	 *
	 * **`PhaseChanged` 的语义 = "变化"，不是"进入"**：进入第 1 段**不计**（激活时 `PhaseIndex` 已是 0，
	 * 它是起点不是变化）⇒ 三段技能恰发 **2** 次。判据读的是"发生过一次前进"，不是"到达过某个下标"。
	 *
	 * **最后一段到期 MUST NOT 先把 `PhaseIndex` 前进到 `Num`**：那会让它落在"已越界"的状态上，
	 * 而本函数的越界分支（`Log` + 完成终结）就成了**正常路径**——"正常路径报日志"会让日志失去信号意义，
	 * 也让 `PhaseTable` 档的"无当前时段"降级在本该有时段时被误触发。⇒ **最后一段直接完成**，
	 * `PhaseIndex` 停在最后一个有效下标。
	 *
	 * @param Subsystem 技能门面。
	 * @param RunHandle 运行态句柄。
	 */
	static void AdvancePhase(UTcsSkillSubsystem& Subsystem, FTcsCastRunHandle RunHandle);

#pragma endregion


// 查询契约
#pragma region Query

public:
	/**
	 * **当前这次施法能不能被打断**（D5-1 查询契约）——打断结算与宿主移动系统询问的**唯一合法入口**。
	 *
	 * **宿主 MUST NOT 自行读 `FTcsSkillDefData` 的开关字段代判**（那会在三档之间制造**第二真相**）。
	 *
	 * 三档按 `ECastQueryMode` 分派：
	 * - `CQM_DefSwitches`（默认档）⇒ 读 `bInterruptibleDefault` / `bCanMoveDefault`；
	 * - `CQM_PhaseTable` ⇒ 读**当前时段**（`Phases[PhaseIndex]`）的字段；
	 * - `CQM_Custom` ⇒ **本轮未实现**：具名 `Warning` + 回退默认档。
	 *
	 * **两处具名降级（MUST，不是可选项）**：
	 * - `PhaseTable` 档**无当前时段**（`Phases` 空 或 `PhaseIndex` 越界）⇒ 具名 `Log` + **回退 `DefSwitches` 档**。
	 *   **MUST NOT** 越界读；**MUST NOT** 返回无条件"永不可打断"——后者是一处**无声的行为变更**
	 *   （它会让瞬发技能变得不可打断，而瞬发是本能力明文的主路径）。
	 * - `Custom` 档 ⇒ 具名 `Warning`（含档名）+ 回退 `DefSwitches` 档。**回退是本轮明确选定的行为，不是漏做**。
	 *
	 * **纯查询语义（MUST）**：MUST NOT 改运行态、MUST NOT 发事件、MUST NOT 动账本。
	 *
	 * @param Subsystem 技能门面。
	 * @param RunHandle 运行态句柄。
	 * @return 返回当前是否可打断；**悬空句柄 ⇒ 返回 false**（不可打断是"不动作"的保守档——
	 *         终结一个已不存在的 run 没有意义，且此处 MUST NOT ensure：那是时序竞态）。
	 */
	static bool IsInterruptibleNow(UTcsSkillSubsystem& Subsystem, FTcsCastRunHandle RunHandle);

	// 当前是否允许移动（分派与降级规则同 `IsInterruptibleNow`；只是读另一列字段）
	static bool CanMoveNow(UTcsSkillSubsystem& Subsystem, FTcsCastRunHandle RunHandle);

	/**
	 * **打断入口**（`CancelCast`）：先过查询契约，**MUST NOT 无条件打断**。
	 *
	 * 结算序：① `IsInterruptibleNow()` 判可打断性 → ② **不可打断 ⇒ 拒绝**（返回 false + 具名 `Log`；
	 * 运行态**原样继续**、零事件、零账本改动）→ ③ 可打断 ⇒ 走**四路共用的终结例程**并记原因为"打断"。
	 *
	 * **止于未来不追溯**（M4 约定）：MUST NOT 回退**已发生**的副作用——按来源回收账本条目是
	 * **生命周期终结**（"这件事到此为止"），**不是"回滚历史"**（"把那一下抹掉"）。
	 *
	 * **"来源优先级"仲裁如实登记为未落面**：设计原词是"来源优先级 vs `IsInterruptibleNow()`"，
	 * 而**全仓无任何类型声明"打断来源的优先级"**（技能关系字段 `Priority` 的语义属 `R5.5-e`
	 * 关系字段检查器，尚未定稿）⇒ 本轮**只落可打断性裁决**，**MUST NOT** 为它发明一个零消费者的
	 * 优先级形参（"零消费者不预建"）。
	 *
	 * @param Subsystem 技能门面。
	 * @param RunHandle 运行态句柄。
	 * @return 返回是否真的打断了（已终结）；不可打断 / 悬空句柄 ⇒ false。
	 */
	static bool CancelCast(UTcsSkillSubsystem& Subsystem, FTcsCastRunHandle RunHandle);

#pragma endregion


// 内核
#pragma region Core

private:
	/**
	 * 装配本次激活的参数求值上下文。
	 *
	 * **为什么单独抽出来**：快照构建与时段时长求值（Task 5）要用**同一份**装配，
	 * 两处各配一次就会出现"时值与参数行不同档"的双口径（同状态侧 `MakeContext` 的理由）。
	 *
	 * **本轮不含子系统形参**（如实登记）：技能侧尚无注入的等级读口
	 * （`ITcsEntityLevelProvider` 住 `TcsState`，技能侧参数源本轮不消费它）⇒ 装配不需要门面。
	 * Task 5 若要接等级源，在此加形参即可（调用点只有一处）。
	 *
	 * @param OutContext 输出上下文。
	 * @param Entity 施法实体。
	 * @param Level 本次的生效等级。
	 */
	static void MakeContext(
		FTcsParamEvaluateContext& OutContext,
		FTcsCombatEntityHandle Entity,
		int32 Level);

	/**
	 * 解析运行态（**先判有效性再取指针**）。
	 *
	 * **为什么不能直接 `CastRuns.Resolve`**：池的 `Resolve` 对悬空句柄会 **ensure**
	 * （它是"调用方给错句柄"的契约违规口径），而本族的查询面 MUST 走
	 * "**时序竞态静默返回 nullptr**"的语义 ⇒ 必须用非确保的 `IsValid` 先挡一道。
	 *
	 * @param Subsystem 技能门面。
	 * @param RunHandle 运行态句柄。
	 * @return 返回运行态；悬空句柄返回 nullptr（不 ensure）。
	 */
	static FTcsCastRun* ResolveRun(UTcsSkillSubsystem& Subsystem, FTcsCastRunHandle RunHandle);

	/**
	 * 在给定条目上找**经池复核有效**的在飞运行态。
	 *
	 * **为什么要复核**：账本的 `RunHandles` 可能残留已归还池槽位的陈旧句柄（例如终结路径
	 * 与撤销路径的时序交错）⇒ 直接取首个元素会把"已结束"误判成"在飞行"。
	 *
	 * @param Subsystem 技能门面。
	 * @param Entry 账本条目。
	 * @return 返回在飞句柄；无在飞返回无效句柄。
	 */
	static FTcsCastRunHandle FindActiveRun(
		UTcsSkillSubsystem& Subsystem,
		const FTcsLearnedSkillEntry& Entry);

	/**
	 * **起链触发点**——"现在是谁在问该不该起链"。
	 *
	 * **为什么用触发点而不是让三个调用点各自判档位**：档位判定的规则有四条
	 * （三档已落地 + `Custom` 具名未实现），散在三个调用点上就是**四份判据**——
	 * 而它们必然漂移（例如给 `OnPhaseEnter` 补一条"打平取后挂者"式的细节时，
	 * 只有一处被改到）。⇒ 档位判定**单点**在本枚举 + `StartMainChain` 里，
	 * 三个调用点只报"我是谁"，不判"我该不该起"。
	 */
	enum class ECastChainTrigger : uint8
	{
		// 激活流程末尾（`MCS_OnCastStarted` 档的触发点）
		ECT_OnCastStarted,

		// 时段推进进入新段（`MCS_OnPhaseEnter` 档的触发点）
		ECT_OnPhaseEnter,

		// 完成终结（`MCS_OnCastCompleted` 档的触发点）
		ECT_OnCastCompleted,
	};

	/**
	 * 按 `MainChainStart` 与**触发点**决定是否起主效果链，并逐档处理。
	 *
	 * **四档逐档处理，MUST NOT 留白**：`MCS_OnCastStarted` / `MCS_OnPhaseEnter`（按
	 * `MainChainStartPhaseTag` 匹配**当前**时段——对"进入"触发点而言当前段就是刚进入的那段）/
	 * `MCS_OnCastCompleted` 三档已落地；`MCS_Custom` **归宿主编排 ⇒ 保持具名未实现**（`Warning`）。
	 * **未实现的档位 MUST 具名报出、MUST NOT 静默跳过**——那会让配了该档的技能"不报错也不生效"
	 * （静默错误，本仓反复拦的形态）。
	 *
	 * **档位与触发点不匹配 ⇒ 静默返回**（那是**正常路径**：配 `OnCastCompleted` 的技能在激活时
	 * 本来就不该起链）。**匹配检查在起链之前、事件与回收之前**——顺序见各调用点。
	 *
	 * **链身份的解析在本函数内完成**（三级顺序）：全局定义（`Def.CastChainId`）→ **链重定向栈**
	 * → Custom 契约（**本轮具名未实现**）。**MUST NOT 修改共享 Def**——重定向是**解析期让渡**
	 * （决定"这次起哪条链"），**不是数据改写**（把 Def 上的字段改掉）。
	 *
	 * **两个形参上的硬纪律（2026-10-08 静态自查补入，防一处真实悬空引用）**：
	 * - `Run` 是**只读引用**：`ExecuteChain` 会**同步执行**链步骤，而链步骤可**再起一次技能激活**
	 *   ⇒ 施法池 `TArray` 可能**扩容搬移** ⇒ 本函数**MUST NOT 在起链后经 `Run` 写任何字段**
	 *   （池元素"扩容即搬移"是本仓明文纪律）。结果经 `OutChainRun` 交回，**由调用方按句柄回写**。
	 * - `OutChainRun` 是唯一输出口（`ChainRun` 在"档位不匹配"与"链 id 无效"等早退路径上
	 *   保持默认无效值，调用方据此可判"没起链"）。
	 *
	 * @param Subsystem 技能门面。
	 * @param RunHandle 运行态句柄（**回写与重新解析都靠它**；`Run` 是不带自身句柄的结构体，
	 *        故句柄 MUST 显式传入——**MUST NOT** 从账本条目的 `RunHandles` 反查，
	 *        那在 `CI_InstancePerExecution` 下会取错那一发）。
	 * @param Run 运行态（**只读**——见上）。
	 * @param Def 技能定义内容。
	 * @param Entry 账本条目。
	 * @param Trigger 当前触发点（档位判定的输入之一）。
	 * @param OutChainRun 输出链运行态句柄（未起链时保持无效值）。
	 */
	static void StartMainChain(
		UTcsSkillSubsystem& Subsystem,
		FTcsCastRunHandle RunHandle,
		const FTcsCastRun& Run,
		const FTcsSkillDefData& Def,
		const FTcsLearnedSkillEntry& Entry,
		ECastChainTrigger Trigger,
		FTcsChainRunHandle& OutChainRun);

	/**
	 * 起某条链并把**实际起的链身份**写回运行态。
	 *
	 * **为什么需要"实际身份"字段**：重定向的验收判据是"**实际起来的链是哪一条**"，
	 * 而它**无法从 `ChainRunHandle` 反查**（链运行态本身不持链身份；`FindChain` 只答"某条链
	 * 是否登记"，答不了"这次起的是哪条"）。若靠"重定向表里有几条"或"解析函数返回了什么"来断言，
	 * 那是在测**解析**而不是测**起链**——一个"解析对了但起链仍用旧 id"的实现会全绿。
	 * ⇒ 把身份记在运行态上（`FTcsCastRun::ActiveChainId`），读数取它。
	 *
	 * **重入安全（MUST）**：`ExecuteChain` 可能经链步骤再起一次激活 ⇒ 调用方传进来的 `Run`
	 * 指针/引用**可能已悬空** ⇒ 本函数**按句柄重新解析**后才回写，MUST NOT 经形参引用回写。
	 *
	 * @param Subsystem 技能门面。
	 * @param RunHandle 运行态句柄（**回写靠它，不靠引用**）。
	 * @param ChainId 已经解析完的、要起的链身份。
	 * @return 返回是否确实起了链（链 id 无效 / 门面不可得 ⇒ false）。
	 */
	static bool ExecuteMainChain(
		UTcsSkillSubsystem& Subsystem,
		FTcsCastRunHandle RunHandle,
		FGameplayTag ChainId);

	/**
	 * 解析链身份（三级顺序）：全局定义 → 链重定向栈 → Custom 契约。
	 *
	 * **未命中任何重定向行 ⇒ 原样返回 `GlobalChainId`**——那是**正常路径**，
	 * **MUST NOT** 报"未实现"（报它会让"零意外红字"这条验收信号被自己的正常路径淹没）。
	 *
	 * **Custom 契约（`ITcsChainRedirectResolver`）的求值归 `R6.5-g`**：本轮若宿主**已注入**实现，
	 * 具名 `Warning` 报"未实现"并跳过；**未注入**时不报（没有东西被拒绝求值）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Entity 归属实体（数据栈按 `(实体, 目标链)` 命中）。
	 * @param GlobalChainId 全局定义给出的链身份（默认档）。
	 * @return 返回解析后的链身份（未命中任何让渡 ⇒ 与入参相同）。
	 */
	static FGameplayTag ResolveCastChainId(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Entity,
		FGameplayTag GlobalChainId);

	/**
	 * 按触发点尝试起主链并把结果按句柄回写（**激活末尾 / 时段进入两处共用**）。
	 *
	 * **为什么是成员而不是文件内自由函数**：它要用 `StartMainChain` 与 `ECastChainTrigger`，
	 * 两者都在本类**私有区**（外部只该走三个公开触发点）⇒ 自由函数够不着。
	 * 收成私有静态成员后，"档位判定 + 重新解析 + 按句柄回写"这一串只有一份实现。
	 *
	 * **它做三件事**：① 逐步重新解析（运行态 / 条目 / 定义——任何广播之后指针都可能失效）；
	 * ② 调 `StartMainChain`（档位判定单点在那里）；③ 起了链则**按句柄重新解析后回写** `ChainRunHandle`
	 * （链身份由 `ExecuteMainChain` 写，此处不过手——保持单一写入点）。
	 *
	 * @param Subsystem 技能门面。
	 * @param RunHandle 运行态句柄（**回写靠它**）。
	 * @param Entity 实体（条目定位用）。
	 * @param EntryHandle 条目句柄（条目定位用）。
	 * @param Trigger 当前触发点。
	 */
	static void TryStartMainChainAt(
		UTcsSkillSubsystem& Subsystem,
		FTcsCastRunHandle RunHandle,
		FTcsCombatEntityHandle Entity,
		FTcsSkillEntryHandle EntryHandle,
		ECastChainTrigger Trigger);

	/**
	 * 广播一枚施法事件（**统一出口**——载荷装配只有一处，全部广播点共用）。
	 *
	 * @param Subsystem 技能门面。
	 * @param EventTag 事件 Tag（`TcsEvent.Cast.*` 之一）。
	 * @param RunHandle 运行态句柄（载荷身份）。
	 * @param Run 运行态（载荷其余字段来源）。
	 * @param Entry 该运行态对应的账本条目（载荷定义身份来源）。
	 */
	static void Broadcast(
		UTcsSkillSubsystem& Subsystem,
		FGameplayTag EventTag,
		FTcsCastRunHandle RunHandle,
		const FTcsCastRun& Run,
		const FTcsLearnedSkillEntry& Entry);

#pragma endregion
};
