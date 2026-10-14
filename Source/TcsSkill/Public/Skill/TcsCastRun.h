// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Clock/TcsExpiryHeap.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsInstanceHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "Chain/TcsChainRun.h"
#include "Def/TcsSkillEnums.h"
#include "Skill/TcsCastRunHandle.h"
#include "Skill/TcsSkillEntryHandle.h"
#include "State/TcsStateSnapshot.h"



/**
 * 施法运行态（D5-2 / `SPEC-04-skill` §3.1 的 `FCastRun`）：池化实例——一次激活产生的全部运行数据。
 *
 * **非反射结构体**（无 `USTRUCT` 宏）：它是**池内池化纯数据**，本轮零过网与零脚本消费面
 * （同 `FTcsLearnedSkillEntry` / `FTcsChainRun` 的处置与理由）。字段里的句柄类型各自已是反射类型，
 * 但**本结构体自身不参与反射**——需要脚本面时按台账 `SCRIPT-10` 的触发条件另议。
 *
 * **持有纪律（引擎事实）**：池元素住 `TArray` 连续缓冲，**扩容即搬移** ⇒ 调用方 MUST NOT 跨
 * "可能触发池扩容的调用"缓存本结构指针（同 `FTcsChainRun` 的持有纪律）。
 *
 * **字段边界（MUST 守死）**：
 * - `Entity` **必须自持**（见其成员注释）——它不是冗余，缺它会让施法终结路径无法定位条目；
 * - `PhaseExpiryEntry` **自 Task 5 起有写入者**（时段推进落地）：它是**当前时段的取消锚点**，
 *   四路终结任一 MUST 经 `UTcsClockSubsystem::CancelExpiry` 取消它
 *   （`skill-cast-runtime` 能力的「施法运行态与实例池」需求明文）。
 *   **★ 本条取代了原文的一句（原文逐字保留，MUST NOT 读成被误删）**：
 *
 *   > - `PhaseExpiryEntry` 在 Task 3 **零写入者**（时段推进归 Task 5）——字段按设计承诺就位，
 *   >   **MUST NOT** 为它建机制（"零消费者不预建"管的是机制，不是设计明文承诺的字段位）；
 *
 *   该句的**时态**（"Task 3"）已随本轮落地而**自然到期**：Task 5 正是它所说的那个"归 Task 5"。
 *   ⇒ 被取代的是"**零写入者**"这一**当时事实**，**未被取代的是它的纪律**
 *   （"零消费者不预建"）：本轮为它建机制，是因为**消费者（时段推进与四路终结的取消锚点）
 *   在本轮真的出现了**；
 * - **MUST NOT** 声明属性捕获字段（`CapturedAttrs`）——该机制已于 2026-10-08 整体删除
 *   （判据见 `TcsSkillDefData.h` 的类注释；**MUST NOT 补回**）。
 */
struct FTcsCastRun
{
// 身份与归属
#pragma region Identity

public:
	// 本条 run 所属的账本条目（`Index` 只在实体桶内有意义 ⇒ 定位须配下面的 `Entity`）
	FTcsSkillEntryHandle EntryHandle;

	/**
	 * 条目所属实体（**必须自持，不是冗余**）。
	 *
	 * **为什么不能靠 `EntryHandle` 反查**：账本条目句柄的 `Index` **只在实体桶内定位**
	 * （`TcsSkillRegistry.h` 明文），句柄里**没有实体段** ⇒ 仅凭它无法跨实体找到条目。
	 * 而施法终结路径（打断 / 顶替）手上**只有运行句柄**（拿不到授予时的实体参数），
	 * 却必须做两件事：摘账本该条目的 `RunHandles`、按 `RunSource` 回收链挂条目。
	 * 缺本字段，这两件事要么做不到、要么退化成"遍历所有桶找这个句柄"（O(桶数) 的错误形态）。
	 */
	FTcsCombatEntityHandle Entity;

#pragma endregion


// 数值与进度
#pragma region Progress

public:
	// 激活时冻结的生效等级（`EffectiveLevel` 语义；运行中升级不追溯）
	int32 Level = 0;

	/**
	 * 当前时段下标（**自 Task 5 起真的会前进**）。
	 *
	 * **推进判据 MUST 取 `PhaseIndex + 1 < Phases.Num()`**，MUST NOT 取"有没有下一段"之外的东西
	 * （详见 `FTcsCastOps::AdvancePhase` 的顺序注）：最后一段到期**不**前进到 `Num`，而是直接走完成终结
	 * ⇒ 三段的技能 `PhaseIndex` 终值为 `2`、`PhaseChanged` 恰 2 次。
	 */
	int32 PhaseIndex = 0;

#pragma endregion


// 快照
#pragma region Snapshot

public:
	/**
	 * 激活瞬间冻结的参数快照（D5-12）。
	 *
	 * **本字段带来一处 GC 地雷**：`FTcsParamSnapshotEntry::SourceRef` 是 `FInstancedStruct`，
	 * 其内层可放**宿主自定义 struct 的 `UPROPERTY` 对象引用**；而本结构体住池的 `TArray`
	 * ——**不是 `UPROPERTY` 容器** ⇒ GC 的 `RefLink` 遍历走不到它 ⇒ 不补引用即**静默回收**
	 * （表现为"数值来源变空"，不是崩溃）。⇒ 门面 `AddReferencedObjects` MUST 逐个在册 run
	 * 走 `FInstancedStruct::AddStructReferencedObjects`（**MUST NOT** 用
	 * `AddPropertyReferencesWithStructARO`——`FTcsParamSnapshotEntry` **不是反射结构体**，
	 * 没有 `StaticStruct()` 可给）。
	 */
	FTcsParamSnapshot ParamSnapshot;

#pragma endregion


// 链与来源
#pragma region Chain

public:
	/**
	 * 本 run 发起的链运行态句柄。
	 *
	 * **自 Task 5 起读取者存在**（原文写"Task 3 零读取者"，该时态已到期）：写入者是起链路径，
	 * 读取者是终结路径与宿主装置。
	 * **MUST NOT** 因"没人读"就省略它——它是"这次施法起了哪条链"的唯一答案。
	 *
	 * **⚠ 但它答不了"起的是哪一条"（同批新增的 `ActiveChainId` 正是为此）**：链运行态
	 * **不持链身份**，`FindChain` 也只答"某条链是否登记" ⇒ 从本句柄**无法反查链身份**。
	 */
	FTcsChainRunHandle ChainRunHandle;

	/**
	 * **本 run 实际起来的链身份**（`OnPhaseEnter` / `OnCastCompleted` 两档起链时写入，
	 * 以及 `OnCastStarted` 档——后者由激活路径回写）。
	 *
	 * **为什么 MUST 记身份而不只记句柄**：重定向的验收判据是"**实际起来的链是哪一条**"，
	 * 而链运行态**不持链身份**、`FindChain` 也只答"某条链是否登记" ⇒ 从 `ChainRunHandle`
	 * **无法反查**这次起的是哪条链。缺本字段，重定向的读数就只能取"解析函数返回了什么"
	 * ——那是在测**解析**而不是测**起链**，一个"解析对了但起链仍用旧 id"的实现会全绿。
	 */
	FGameplayTag ActiveChainId;

	/**
	 * 本 run 的**起链启动档位**（激活时从定义冻结）。
	 *
	 * **为什么冻结而不是每次回查定义**：`StartMainChain` 在"时段进入"与"完成终结"两处被调用，
	 * 而定义**可在这两处之间被注销**（`UnregisterSkillDef` 是合法操作，且顶替/完成的广播会让
	 * 订阅者有机会这么做）⇒ 回查会拿到 nullptr，表现为"该起链却没起、且无任何解释"。
	 * 冻结它之后，**这次施法按它自己开始时的档位走完**——这是"运行中改配置不追溯"的既有语义
	 * （同 `Level` 的冻结）。
	 */
	EMainChainStart MainChainStart = EMainChainStart::MCS_OnCastStarted;

	/**
	 * 该档位引用的时段标识（`MCS_OnPhaseEnter` 档用；冻结理由同上）。
	 */
	FGameplayTag MainChainStartPhaseTag;

	/**
	 * 本 run 的来源锚点（**台账 `CHAIN-7` 的闭合依据**）。
	 *
	 * 施法运行态是链运行态的**第一个长生命周期持有者**：起链时 `Context.RunSource` 取本字段，
	 * 链内 `ModifyAttribute` 挂的账本条目即以它为 `Source` ⇒ **终结时可按它一次摘净**
	 * （`RemoveBySource`）。**回收例程本身归 Task 5 Step 3**（施法终结三路统一回收点），
	 * 本 Task 只落锚点与传递。
	 */
	FTcsSourceHandle RunSource;

#pragma endregion


// 时间锚
#pragma region TimeAnchor

public:
	/**
	 * 当前时段的到期条目（时段推进用）。
	 *
	 * **自 Task 5 起它是"当前时段的取消锚点"**（写入者是 `FTcsCastOps::EnterPhase`，
	 * 读取者/撤销者是四路终结的 `FinishCastRun`）——原文写"零写入者"，该时态已到期，
	 * 取代说明见本结构体的类注释（**原文逐字保留**）。
	 *
	 * **仍然成立的纪律**：消费者一律经 `UTcsClockSubsystem::PushExpiry` / `CancelExpiry`
	 * （`FTcsExpiryHeap` 是泵私有，**MUST NOT** 直接触达 `AdvanceTo`）。
	 *
	 * **⚠ 一条真实的 ensure 陷阱（本轮实测，MUST 守）**：`CancelExpiry` 对**悬空**句柄是
	 * `ensureMsgf`（留红字）。而"到期回调正在跑"那一刻它自己的条目**仍然有效**
	 * （`FireDueItem` 在回调**返回之后**才释放）⇒ 撤销必须发生在**回调入口复位之后**
	 * （`AdvancePhase` 第一条语句即是），使终结例程看到的永远是"活的或空的"、**永不会是"已死的"**。
	 */
	FTcsTimeEntryHandle PhaseExpiryEntry;

#pragma endregion
};
