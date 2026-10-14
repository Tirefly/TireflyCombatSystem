// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "Clock/TcsExpiryHeap.h"



/**
 * 单条冷却轨道的**运行态**（非反射——沿 `FTcsSkillInstance` 的处置）。
 *
 * **为什么不反射**：同 `FTcsSkillInstance` / `FTcsStateInstance`——它只在 C++ 侧流转，
 * 本轮**零过网需求、零脚本消费面**；反射化留给"运行态要整体过网"的轮次（`SPEC-04-skill` §6）。
 *
 * **为什么与 `FTcsCooldownTrack` 分开**：Def 侧那条是**作者配的时长**（可编辑、被序列化），
 * 本结构是**这次冷却实际按什么跑**（起冷瞬间冻结下来的绝对时刻与句柄）——两者是"意图"与
 * "事实"的关系。**回落判据**：`Duration` 会被 CDR 打折、`EndTimeSeconds` 依时钟；
 * 把它压回 Def 侧就等于把运行期状态写进资产。
 */
struct FTcsCooldownTrackState
{
	/**
	 * 起冷瞬间冻结的时长（**CDR 已作用**）。
	 *
	 * **`D5-15` 明文"快照于冷却触发时刻，中途变化不追溯"** ⇒ 它是**值**不是引用：
	 * 起冷之后再改 `CooldownPct` 不影响本轨。查询口径 `Remaining = EndTimeSeconds - Now`，
	 * 而 `Total` 正是"这条冷却是多久"的答案——UI 画径向进度要用它。
	 */
	double Total = 0.0;

	// 到期绝对时刻（`FTcsClock::Elapsed` 时轴；与本仓全部到期锚点同一根时轴）
	double EndTimeSeconds = 0.0;

	/**
	 * 到期堆条目句柄（**取消锚点**）。
	 *
	 * **★ 复位纪律（MUST 守死，判据见 `PLN-R6.5-1` Task 1 Step 6）**：到期回调的**第一条语句**
	 * 即把本字段置空。判据：`CancelExpiry` 对**悬空**句柄会 `ensureMsgf` 留红字
	 * （`TcsExpiryHeap.h:92`），而"到期回调正在跑"那一刻它自己的条目**仍然有效**
	 * （`FireDueItem` 在回调**返回之后**才释放，`:223-246`）⇒ 回调内若再 `CancelExpiry` 本句柄，
	 * 就是"取消一个正在跑的条目"。复位后 `IsValid()` 为假，后续清理路径天然跳过。
	 * **同款先例 = `FTcsSkillInstance::PhaseExpiryEntry` 的既有注释。**
	 */
	FTcsTimeEntryHandle ExpiryEntry;
};



/**
 * **实体级**共享组冷却槽（住 `UTcsSkillSubsystem`）：同组的所有条目**共用一条线**。
 *
 * **为什么住实体而不住条目**：公共冷却（GCD）的语义就是"同一个实体不能连续放任意技能"——
 * 它的作用域是**实体**，与"是哪个技能"无关。若按条目各存一份，同组两个条目会各起各的线，
 * 那就**不是 GCD 了**（症状：放完 A 之后 B 立刻能放）。
 *
 * **`GroupTag` 为什么也存一份**：槽是按 `TMap<FGameplayTag, ...>` 存放的，键本身就是 tag
 * ——本字段是"槽自述它是哪一组"，供**过期清理与事件载荷**直接读取（不必把 map 的键再传一遍）。
 */
struct FTcsCooldownGroupState
{
	// 起冷瞬间冻结的时长（**CDR 已作用**；语义同 FTcsCooldownTrackState::Total）
	double Total = 0.0;

	// 到期绝对时刻（`FTcsClock::Elapsed` 时轴）
	double EndTimeSeconds = 0.0;

	// 到期堆条目句柄（取消锚点；复位纪律同 FTcsCooldownTrackState::ExpiryEntry）
	FTcsTimeEntryHandle ExpiryEntry;

	// 本槽所属的组身份（与 TMap 的键一致；供过期清理与事件载荷直读）
	FGameplayTag GroupTag;
};
