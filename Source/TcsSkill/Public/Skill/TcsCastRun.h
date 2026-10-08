// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Clock/TcsExpiryHeap.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsInstanceHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "Chain/TcsChainRun.h"
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
 * - `Unit` **必须自持**（见其成员注释）——它不是冗余，缺它会让施法终结路径无法定位条目；
 * - `PhaseExpiryEntry` 在 Task 3 **零写入者**（时段推进归 Task 5）——字段按设计承诺就位，
 *   **MUST NOT** 为它建机制（"零消费者不预建"管的是机制，不是设计明文承诺的字段位）；
 * - **MUST NOT** 声明属性捕获字段（`CapturedAttrs`）——该机制已于 2026-10-08 整体删除
 *   （判据见 `TcsSkillDefData.h` 的类注释；**MUST NOT 补回**）。
 */
struct FTcsCastRun
{
// 身份与归属
#pragma region Identity

public:
	// 本条 run 所属的账本条目（`Index` 只在单位桶内有意义 ⇒ 定位须配下面的 `Unit`）
	FTcsSkillEntryHandle EntryHandle;

	/**
	 * 条目所属单位（**必须自持，不是冗余**）。
	 *
	 * **为什么不能靠 `EntryHandle` 反查**：账本条目句柄的 `Index` **只在单位桶内定位**
	 * （`TcsSkillRegistry.h` 明文），句柄里**没有单位段** ⇒ 仅凭它无法跨单位找到条目。
	 * 而施法终结路径（打断 / 顶替）手上**只有运行句柄**（拿不到授予时的单位参数），
	 * 却必须做两件事：摘账本该条目的 `RunHandles`、按 `RunSource` 回收链挂条目。
	 * 缺本字段，这两件事要么做不到、要么退化成"遍历所有桶找这个句柄"（O(桶数) 的错误形态）。
	 */
	FTcsCombatEntityHandle Unit;

#pragma endregion


// 数值与进度
#pragma region Progress

public:
	// 激活时冻结的生效等级（`EffectiveLevel` 语义；运行中升级不追溯）
	int32 Level = 0;

	// 当前时段下标（Task 3 恒为 0——时段推进归 Task 5）
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
	 * **Task 3 零读取者**（如实登记）：写入者是起链路径，读取者归 Task 5 的终结路径
	 * （按它摘链锚）。**MUST NOT** 因"没人读"就省略它——它是"这次施法起了哪条链"的唯一答案。
	 */
	FTcsChainRunHandle ChainRunHandle;

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
	 * **Task 3 零写入者**（时段推进归 Task 5）：字段按设计承诺就位，**MUST NOT** 为它建机制
	 * （`FTcsExpiryHeap` 是泵私有，消费者一律经 `UTcsClockSubsystem::PushExpiry`）。
	 */
	FTcsTimeEntryHandle PhaseExpiryEntry;

#pragma endregion
};
