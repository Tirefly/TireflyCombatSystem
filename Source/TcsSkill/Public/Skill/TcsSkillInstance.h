// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Clock/TcsExpiryHeap.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "Chain/TcsChainRun.h"
#include "Def/TcsSkillEnums.h"
#include "Skill/TcsSkillEntryHandle.h"
#include "Skill/TcsSkillInstanceHandle.h"
#include "State/TcsStateInstance.h"



/**
 * **技能运行态记录**（2026-10-14 重构，`PLN-REFACTOR-1` Task 3）：**基底 + 技能专属字段**。
 * 一次激活产生的全部运行数据，池化实例（设计语料：`D5-2` / `SPEC-04-skill` §3.1 的 `FCastRun`）。
 *
 * **★ 与旧类型的关系（MUST 读，防误读）**：本类型**取代**了 `FTcsCastRun`（**该名字已不存在于代码**）。
 * 重构前 `FTcsCastRun` 是一个装全部 11 字段的**独立结构**，与状态侧 `FTcsStateInstance` 各持一套
 * 语义相同的公共字段；现在它**继承基底**，公共字段上提、技能专属字段平铺在本类型里：
 *
 * ```
 * FTcsStateInstance（基底，两域公共）  DefTag / Entity / CascadeAnchor / ParamSnapshot / Level / ExpiryEntry
 *        ↑ 继承
 * FTcsSkillInstance（本类型）          + EntryHandle / PhaseIndex（技能域内字段）
 *                                     + ChainRunHandle / ActiveChainId / MainChainStart / MainChainStartPhaseTag
 * ```
 *
 * **★ 与原 11 字段的逐一归属（可复算）**：
 *
 * | 原 `FTcsCastRun` 字段 | 去向 | 判据 |
 * |---|---|---|
 * | `Entity` / `Level` / `ParamSnapshot` | **基底**（逐字同义） | 两域同角色 |
 * | `RunSource` | **基底 `CascadeAnchor`** | 它的三个消费点全是"按它一次摘净"（链挂的属性账本条目 / 本 run 物化的参数链条目 / 起链时传给链的来源锚点）⇒ 它是**级联撤销锚点**，**不是**"谁施加的" |
 * | `PhaseExpiryEntry` | **基底 `ExpiryEntry`** | 与 Buff 侧时值到期**同角色**：当前计时项的取消锚 |
 * | `EntryHandle` | 本类型 | 类型是 `FTcsSkillEntryHandle`——基底是**一个** struct、只能有一个 `Handle` 字段类型 |
 * | `PhaseIndex` | 本类型 | **与基底的 `ExpiryEntry` 同词根不同义**（一个是时段游标、一个是取消锚）⇒ MUST NOT 混为一谈 |
 * | `ChainRunHandle` / `ActiveChainId` | 本类型 | "起了哪条链"的句柄 与 "**实际**起的是哪条链"的身份（后者链运行态**反查不到**） |
 * | `MainChainStart` / `MainChainStartPhaseTag` | 本类型 | 激活时冻结的起链档位与时段标识 |
 *
 * **⇒ 11 = 5（上提基底）+ 6（留本类型）**，无字段丢失、无字段重复。
 *
 * **★ 为什么用继承、且 MUST NOT 再单开一个 payload 类型**（2026-10-14 用户裁定，与 Buff 侧同一条）：
 * 既然已选继承，把技能字段**再包一层**（`X.Payload.PhaseIndex`）只多一层间接、不增加任何隔离
 * ——共享机制本来就只 include 基底、**看不见**派生类型的字段。⇒ 字段**直接平铺**。**MUST NOT 事后补壳**。
 *
 * **★ 基底的 `CascadeAnchor` 在本域的语义 = 原 `RunSource`（台账 `CHAIN-7` 的闭合依据，MUST 读）**：
 * 技能运行态是链运行态的**第一个长生命周期持有者**——起链时 `Context.RunSource` 取本字段，
 * 链内 `ModifyAttribute` 挂的账本条目即以它为 `Source` ⇒ **终结时可按它一次摘净**（`RemoveBySource`）。
 * 本 run 自己物化的 `ParamChainRows` 条目同样以它为 `Source`（`MaterializeParamChainRows` 的形参）。
 * ⇒ 它在本域是纯粹的"**按它一次摘净**"，这正是它归基底 `CascadeAnchor` 而非"施加方来源"的判据。
 *
 * **两条历史沿革（原文逐字保留，MUST NOT 读成被误删）**：
 *
 * > - `PhaseExpiryEntry`（今基底 `ExpiryEntry`）在 Task 3 **零写入者**（时段推进归 Task 5）
 * >   ——字段按设计承诺就位，**MUST NOT** 为它建机制（"零消费者不预建"管的是机制，
 * >   不是设计明文承诺的字段位）；
 * > - `ChainRunHandle` 在 Task 3 **零读取者**（起链读取归 Task 5）。
 *
 * 两句的**时态均已到期**：Task 5 正是它们所说的那个"归 Task 5"，写入者 / 读取者后来真的出现了
 * ⇒ **被取代的是"零写入者 / 零读取者"这两个当时事实，未被取代的是它们的纪律**（"零消费者不预建"）。
 *
 * **一条真实的 ensure 陷阱（既有实测，现住基底，MUST 守）**：`CancelExpiry` 对**悬空**句柄是
 * `ensureMsgf`（留红字），而"到期回调正在跑"那一刻它自己的条目**仍然有效**（泵在回调返回之后才释放）
 * ⇒ **撤销 MUST 发生在回调入口复位该锚点之后**（本域 `FTcsCastOps::AdvancePhase` 第一条语句即是），
 * 使终结路径看到的永远是"活的或空的"、**永不会是"已死的"**。详见基底 `ExpiryEntry` 的成员注释。
 *
 * **一处 GC 地雷（现住基底的 `ParamSnapshot`，MUST 守）**：`FTcsParamSnapshotEntry::SourceRef` 是
 * `FInstancedStruct`，内层可放宿主自定义 struct 的 `UPROPERTY` 对象引用；而本结构体住池的 `TArray`
 * ——**不是 `UPROPERTY` 容器** ⇒ GC 的 `RefLink` 遍历走不到它 ⇒ 不补引用即**静默回收**
 * （表现为"数值来源变空"，不是崩溃）。⇒ 门面 `AddReferencedObjects` MUST 逐个在册实例走
 * `FInstancedStruct::AddStructReferencedObjects`（**MUST NOT** 用 `AddPropertyReferencesWithStructARO`
 * ——`FTcsParamSnapshotEntry` **不是反射结构体**，没有 `StaticStruct()` 可给）。
 *
 * **★ 两处与 Buff 侧刻意不同（不是漏了）**：
 * - **不持自己的句柄**：Buff 侧有 `Handle` 是因为它的 per-entity 桶要按句柄 `Find(Handle)` 定位；
 *   技能侧是**扁平池**（`SkillInstances` 一句柄类型一池，不由实体分桶），找在飞实例靠账本条目的
 *   `RunHandles` 遍历 ⇒ **没有"按自己句柄找自己"的需求**，加它即造死字段；
 * - **不持 `Phase`**：它守的是"移除广播窗口内重入移除同一句柄"（`STAT-5`），而技能侧同类问题
 *   **已由统一重入守卫解决**（`TTcsEntityBroadcastGuard`）⇒ 技能侧**零消费者**，按"零消费者不预建"不声明。
 *
 * **★ 基底的 `DefTag` 本域**填**（2026-10-14 用户裁定，MUST 读）**：重构前技能侧**不存**定义身份、
 * 每次去账本条目上取（`Entry.DefTag`）。继承之后该位置由基底提供 ⇒ **本域 MUST 填它、读取点改读自己**
 * （值等价、行为中性）。**但一处守卫 MUST 保留**：`FTcsCastFinishReason` 的终止事件载荷装配仍以
 * **账本条目是否存在**为条件——"条目已被撤销 ⇒ 不广播终止事件"是**有意的**（那条技能已经不在账本上），
 * **MUST NOT** 因为"现在自己持定义身份了"就顺手去掉该守卫（那会把不发事件变成发事件，属行为变更）。
 *
 * **一处真实的类注释订正（防后人误读）**：原文写"`PhaseExpiryEntry` 在 Task 3 **零写入者**（时段推进归 Task 5）"
 * ——该句的**时态已到期**（时段推进早已落地，写入者是 `FTcsCastOps::EnterPhase`）。**被取代的是"零写入者"
 * 这一当时事实，未被取代的是它的纪律**（"零消费者不预建"）：为它建机制，是因为**消费者真的出现了**
 * （时段推进与四路终结的取消锚点）。
 *
 * **MUST NOT** 声明属性捕获字段（`CapturedAttrs`）——该机制已于 2026-10-08 整体删除
 * （判据见 `TcsSkillDefData.h` 的类注释；**MUST NOT 补回**）。
 *
 * **非反射结构体**（无 `USTRUCT` 宏）：它是**池内池化纯数据**，本轮零过网与零脚本消费面
 * （同基底的处置与理由）。先例 = `FTcsLearnedSkillEntry` / `FTcsChainRun`。
 * 需要脚本面时按台账 `SCRIPT-10` 的触发条件另议。
 *
 * **持有纪律（引擎事实）**：池元素住 `TArray` 连续缓冲，**扩容即搬移** ⇒ 调用方 MUST NOT 跨
 * "可能触发池扩容的调用"缓存本结构指针（同 `FTcsChainRun` 的持有纪律）。
 */
struct FTcsSkillInstance : public FTcsStateInstance
{
// 技能域内字段
#pragma region DomainFields

public:
	/**
	 * 本条运行态所属的账本条目（`Index` 只在实体桶内有意义 ⇒ 定位须配基底的 `Entity`）。
	 *
	 * **为什么不能靠它反查实体**：账本条目句柄的 `Index` **只在实体桶内定位**
	 * （`TcsSkillRegistry.h` 明文），句柄里**没有实体段** ⇒ 仅凭它无法跨实体找到条目。
	 * 而施法终结路径（打断 / 顶替）手上**只有运行句柄**（拿不到授予时的实体参数），
	 * 却必须做两件事：摘账本该条目的 `RunHandles`、按基底 `CascadeAnchor` 回收链挂条目。
	 * 缺基底的 `Entity`，这两件事要么做不到、要么退化成"遍历所有桶找这个句柄"（O(桶数) 的错误形态）。
	 * **⇒ 该字段与基底的 `Entity` 是一对，MUST NOT 只留其一。**
	 */
	FTcsSkillEntryHandle EntryHandle;

	/**
	 * 当前时段下标（**自 Task 5 起真的会前进**）。
	 *
	 * **推进判据 MUST 取 `PhaseIndex + 1 < Phases.Num()`**，MUST NOT 取"有没有下一段"之外的东西
	 * （详见 `FTcsCastOps::AdvancePhase` 的顺序注）：最后一段到期**不**前进到 `Num`，而是直接走完成终结
	 * ⇒ 三段的技能 `PhaseIndex` 终值为 `2`、`PhaseChanged` 恰 2 次。
	 *
	 * **★ 与基底的 `ExpiryEntry` 同词根不同义**（MUST NOT 混为一谈）：本字段是**时段推进的下标**，
	 * 而 `ExpiryEntry` 是**计时项的取消锚**。
	 */
	int32 PhaseIndex = 0;

#pragma endregion


// 链与启动档
#pragma region Chain

public:
	/**
	 * 本条运行态发起的链运行态句柄。
	 *
	 * **自 Task 5 起读取者存在**：写入者是起链路径，读取者是终结路径与宿主装置。
	 * **MUST NOT** 因"没人读"就省略它——它是"这次施法起了哪条链"的唯一答案。
	 *
	 * **⚠ 但它答不了"起的是哪一条"（同批的 `ActiveChainId` 正是为此）**：链运行态
	 * **不持链身份**，`FindChain` 也只答"某条链是否登记" ⇒ 从本句柄**无法反查链身份**。
	 */
	FTcsChainRunHandle ChainRunHandle;

	/**
	 * **本条运行态实际起来的链身份**（`OnPhaseEnter` / `OnCastCompleted` 两档起链时写入，
	 * 以及 `OnCastStarted` 档——后者由激活路径回写）。
	 *
	 * **为什么 MUST 记身份而不只记句柄**：重定向的验收判据是"**实际起来的链是哪一条**"，
	 * 而链运行态**不持链身份**、`FindChain` 也只答"某条链是否登记" ⇒ 从 `ChainRunHandle`
	 * **无法反查**这次起的是哪条链。缺本字段，重定向的读数就只能取"解析函数返回了什么"
	 * ——那是在测**解析**而不是测**起链**，一个"解析对了但起链仍用旧 id"的实现会全绿。
	 */
	FGameplayTag ActiveChainId;

	/**
	 * 本条运行态的**起链启动档位**（激活时从定义冻结）。
	 *
	 * **为什么冻结而不是每次回查定义**：`StartMainChain` 在"时段进入"与"完成终结"两处被调用，
	 * 而定义**可在这两处之间被注销**（`UnregisterSkillDef` 是合法操作，且顶替/完成的广播会让
	 * 订阅者有机会这么做）⇒ 回查会拿到 nullptr，表现为"该起链却没起、且无任何解释"。
	 * 冻结它之后，**这次施法按它自己开始时的档位走完**——这是"运行中改配置不追溯"的既有语义
	 * （同基底 `Level` 的冻结）。
	 */
	EMainChainStart MainChainStart = EMainChainStart::MCS_OnCastStarted;

	/**
	 * 该档位引用的时段标识（`MCS_OnPhaseEnter` 档用；冻结理由同上）。
	 */
	FGameplayTag MainChainStartPhaseTag;

#pragma endregion
};
