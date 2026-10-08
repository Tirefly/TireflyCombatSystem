// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Handle/TcsCombatEntityHandle.h"

#include "Def/TcsSkillActivateResult.h"
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
 * `TcsCastOps_Events.cpp`（事件广播）。
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
	 * @param Unit 施法单位。
	 * @param DefTag 技能定义身份。
	 * @param OutRun 输出运行态句柄（可选；失败时不写）。
	 * @return 返回激活结果（具名）。
	 */
	static ESkillActivateResult Activate(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit,
		FGameplayTag DefTag,
		FTcsCastRunHandle* OutRun);

#pragma endregion


// 运行态生命周期
#pragma region Lifetime

public:
	/**
	 * 终结一个运行态（**顶替路径**在 Task 3 是本函数唯一调用者；打断与自然完成归 Task 5）。
	 *
	 * **统一回收点**（台账 `CHAIN-7` 的落点纪律；Task 5 Step 3 把三条终结点全部汇到这里）：
	 * 三路终结 MUST 汇聚到同一个例程，**MUST NOT 各写一份清理**——三份清理必然漂移，
	 * 而漂移的后果是**静默泄漏**（条目留在账本里、属性不回退，且无人报错）。
	 *
	 * 本函数做四件事：① 发 `OnCastInterrupted`（**MUST NOT** 发 `OnCastCompleted`——顶替不是完成，
	 * 照抄 GAS 会让配 `OnCastCompleted` 起链的旧技能在被顶替瞬间真的打出主链）；
	 * ② 摘账本该条目的 `RunHandles` 中该句柄；③ 归还池槽位；④ 按 `RunSource` 回收链挂条目。
	 *
	 * @param Subsystem 技能门面。
	 * @param RunHandle 要终结的运行态句柄。
	 * @return 返回是否确实终结了一个在册运行态（悬空句柄 = 时序竞态，静默 false、不 ensure）。
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
	 * @param Unit 施法单位。
	 * @param Level 本次的生效等级。
	 */
	static void MakeContext(
		FTcsParamEvaluateContext& OutContext,
		FTcsCombatEntityHandle Unit,
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
	 * 按 `MainChainStart` 起主效果链（**本 Task 只实现 `MCS_OnCastStarted`**）。
	 *
	 * **未实现的档位 MUST 具名报出、MUST NOT 静默跳过**——那会让配了 `OnCastCompleted` 的技能
	 * "不报错也不生效"（静默错误）。
	 *
	 * **两个形参上的硬纪律（2026-10-08 静态自查补入，防一处真实悬空引用）**：
	 * - `Run` 是**只读引用**：`ExecuteChain` 会**同步执行**链步骤，而链步骤可**再起一次技能激活**
	 *   ⇒ 施法池 `TArray` 可能**扩容搬移** ⇒ 本函数**MUST NOT 在起链后经 `Run` 写任何字段**
	 *   （池元素"扩容即搬移"是本仓明文纪律）。结果经 `OutChainRun` 交回，**由调用方按句柄回写**。
	 * - `OutChainRun` 是唯一输出口（`ChainRun` 在"未实现的档位"与"链 id 无效"等早退路径上
	 *   保持默认无效值，调用方据此可判"没起链"）。
	 *
	 * @param Subsystem 技能门面。
	 * @param Run 运行态（**只读**——见上）。
	 * @param Def 技能定义内容。
	 * @param Entry 账本条目。
	 * @param OutChainRun 输出链运行态句柄（未起链时保持无效值）。
	 */
	static void StartMainChain(
		UTcsSkillSubsystem& Subsystem,
		const FTcsCastRun& Run,
		const FTcsSkillDefData& Def,
		const FTcsLearnedSkillEntry& Entry,
		FTcsChainRunHandle& OutChainRun);

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
