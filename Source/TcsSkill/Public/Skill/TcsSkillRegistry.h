// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "Skill/TcsCastRunHandle.h"
#include "Skill/TcsSkillEntryHandle.h"



/**
 * 已学技能条目（D5-13 / `SPEC-04-skill` §3.1 的 `FLearnedSkillEntry`）：**池化纯数据**——
 * 一个单位的一条"会这个技能"的全部持久状态。
 *
 * **非反射结构体**（无 `USTRUCT` 宏）：本记录只在 C++ 侧流转——本轮**零过网需求、零脚本消费面**。
 * 同款先例 = `FTcsStateInstance`（`TcsStateInstance.h:25`：非反射，反射化留给"实例要整体过网"的轮次）。
 * **脚本 / 蓝图侧的读写面另属台账 `SCRIPT-10`**（触发条件型：出现"脚本要读技能等级 / 学习来源 /
 * 在飞 run"的真实消费者时再定形态）——**MUST NOT** 以"将来可能要"为由现在就反射化
 * （那会得到"有类型、无门面可调"的空承诺，且违反"零消费者不预建"纪律）。
 *
 * **字段边界（MUST 守死）**：本结构体**只**持"持久等级 + 学习来源 + 在飞施法句柄"三样——
 * - **MUST NOT** 声明冷却轨道状态字段（归 `R6.5-a`：其消费者是冷却轨道状态）；
 * - **MUST NOT** 声明参数修正链字段（归参数链轮：`FTcsNumericParamModifier` 在那里才存在）。
 * 判据是**声明点早于类型存在点**：两者的元素类型此刻全仓不存在，而 `TArray<T>` 成员要求 `T` 至少可见
 * ⇒ 写进来就是**未声明标识符**（硬编译错误）。参数读取面（`GetNumericParam` / `IsSwitchSet` / `GetLevel`）
 * 因此取"**带 `Level` 键的修正器列表输入**"（形参传入），**MUST NOT** 依赖本结构体持该字段。
 */
struct FTcsLearnedSkillEntry
{
// 身份与归属
#pragma region Identity

public:
	/**
	 * 内容身份（**权威**）。
	 *
	 * `EffectiveDefId` 概念已于 D5-9 整体移除——"这个条目用哪份定义"的唯一答案是本字段；
	 * `GetDef()` 走登记表的**解析缓存**（const + 版本校验），热路径零回查定义库。
	 */
	FGameplayTag DefTag;

	// 桶内句柄（本条目的身份；`Index` / `Generation` 的语义由所属桶定义）
	FTcsSkillEntryHandle Handle;

	/**
	 * 宿主单位（学会该技能的单位；本条目所属的桶）。
	 *
	 * **为什么条目自持它**：门面按句柄取条目时句柄里没有单位段，若无此字段就只能"遍历所有桶找这个句柄"
	 * （O(桶数)）；自持后是 O(1) 定位。而且"这个技能在谁身上"本就是条目身份的一部分
	 * （同 `FTcsStateInstance::Unit` 的理由，同一份信息不是重复真相）。
	 */
	FTcsCombatEntityHandle Unit;

	/**
	 * 学习来源（"谁给的"——级联撤销锚点）。
	 *
	 * **只有一个来源句柄，与状态侧刻意不同**：状态实例持 `Source`（施加方来源）+ `CascadeAnchor`
	 * （级联撤销锚点）两个句柄（`TcsStateInstance.h:28-34`），因为它的修正器与内联触发行要按锚点
	 * 一次摘净；**账本条目不挂修正器、不挂触发行**（`ParamChain` 归参数链轮、行为归状态族）
	 * ⇒ 无独立级联锚点需求。**MUST NOT 照抄状态侧的两个句柄**——那会造出一个零消费者的字段。
	 */
	FTcsSourceHandle LearnSource;

#pragma endregion


// 级别
#pragma region Level

public:
	// 持久等级（**激活时快照进施法运行态**——"运行中升级不追溯"；等级来源归宿主业务侧）
	int32 Level = 0;

#pragma endregion


// 在飞施法
#pragma region CastRuns

public:
	/**
	 * 该条目当前在飞的施法运行句柄集。
	 *
	 * **本轮恒为空数组**（如实登记）：写入者是激活路径（建 run 后回填），归后续任务。
	 * 本字段此刻存在的理由是**数组元素类型必须完整**——它是 `FTcsCastRunHandle` 在本任务落地的唯一原因。
	 */
	TArray<FTcsCastRunHandle> RunHandles;

#pragma endregion
};



/**
 * 单个单位的技能桶（`FSkillBucket`）：一个单位的全部在册条目 + 槽位代际 + 空闲槽栈。
 *
 * **为什么自建槽位而不用 TcsCore 的 `TTcsInstancePool`**（与 `FStateBucket` 同款理由）：
 * 那个池是"一个池 = 一个句柄空间"，而账本条目按单位持有槽位，句柄 `Index` 在单位内定位。
 * `Generation` 则由本模块的出线发号器统一分配不复用的奇数号，使同一 `(Index, Generation)`
 * 在跨桶和跨世界时唯一。释放 +1 置为偶数；旧句柄不能匹配其它桶。
 *
 * **刻意不带导出宏**（2026-10-05 实测同款）：本类型持 `TUniquePtr` 元素 ⇒ 不可复制；
 * 加 `TCSSKILL_API` 会让 MSVC 强制实例化 `TMap` 的复制路径并报 `C2280`。跨模块可达性由**门面**承担——
 * `UTcsSkillSubsystem` 带 `TCSSKILL_API`，它作为成员持有本类型，消费方经门面访问即可。
 */
struct FSkillBucket
{
// 槽位
#pragma region Slots

public:
	/**
	 * 分配槽位（优先复用空闲槽；每次取进程内不复用的正奇数代际）。
	 *
	 * @return 返回新槽的句柄（`Index` 为桶内下标，`Generation` 为当前代际）。
	 */
	FTcsSkillEntryHandle AllocateSlot();

	/**
	 * 释放槽位（代际 +1 使旧句柄悬空；槽位入空闲栈）。
	 *
	 * @param SlotIndex 桶内槽位下标。
	 */
	void ReleaseSlot(uint32 SlotIndex);

#pragma endregion


// 访问
#pragma region Access

public:
	/**
	 * 按句柄解析条目（**代际校验**：失配 / 越界返回 nullptr）。
	 *
	 * @param Handle 账本条目句柄。
	 * @return 返回条目指针；脏句柄返回 nullptr。
	 */
	FTcsLearnedSkillEntry* Find(const FTcsSkillEntryHandle& Handle);

	// 按句柄解析条目（只读重载；语义同上）
	const FTcsLearnedSkillEntry* Find(const FTcsSkillEntryHandle& Handle) const;

	/**
	 * 句柄是否指向在册条目（代际校验，返回 false 而不 ensure）。
	 *
	 * @param Handle 账本条目句柄。
	 * @return 返回是否在册。
	 */
	bool IsValidHandle(const FTcsSkillEntryHandle& Handle) const;

	/**
	 * 按内容身份找条目（**同一单位"会同一个技能"至多一条** ⇒ 命中即唯一，先命中先返回）。
	 *
	 * **为什么给这个口**：激活路径拿到的输入是 `(单位, DefTag)` 而不是句柄，而逐个 `ForEach`
	 * 自己找会把"遍历期间不得增删"的纪律散到每个调用点。本函数只做查找、不改桶。
	 *
	 * **返回值是裸指针**：桶内元素地址在**本函数返回后**仍稳定（不涉及增删），但调用方
	 * MUST NOT 跨"可能增删桶"的调用（如撤销、或广播后重入）缓存它——那类路径下 MUST 重新查找
	 * （激活路径的顶替分支即如此处理）。
	 *
	 * @param DefTag 技能定义身份。
	 * @return 返回条目指针；无该技能返回 nullptr。
	 */
	FTcsLearnedSkillEntry* FindByDefTag(FGameplayTag DefTag);

	// 按内容身份找条目（只读重载；语义同上）
	const FTcsLearnedSkillEntry* FindByDefTag(FGameplayTag DefTag) const;

	/**
	 * 遍历在册条目（**按槽位下标升序**——稳定序，同输入同输出是可复现验收的前提）。
	 * 访问者返回 false 即提前终止。
	 *
	 * **遍历期间 MUST NOT 增删条目**（增删会改动槽位/空闲栈）：需要增删时先自行收集句柄，
	 * 遍历结束后再动（两段式，同 `FTcsTriggerRegistry::UnregisterRowsBySource` 的手法）。
	 * **就地改字段是允许的**——改字段不动槽位。
	 *
	 * @param Visitor 访问者（返回是否继续）。
	 */
	void ForEach(TFunctionRef<bool(const FTcsLearnedSkillEntry&)> Visitor) const;

	// 遍历在册条目（可写重载——仅供"就地改字段"的场景，见只读重载的纪律说明）
	void ForEach(TFunctionRef<bool(FTcsLearnedSkillEntry&)> Visitor);

	/**
	 * 在册条目数（观测与统计用）。
	 *
	 * @return 返回在册条目数。
	 */
	int32 Num() const;

#pragma endregion


// 生命周期
#pragma region Lifetime

public:
	// 清空桶（逐槽归还：代际 +1 置偶数使全部旧句柄失效；进程发号器不复位）
	void Reset();

#pragma endregion


// 内核
#pragma region Core

private:
	/**
	 * 由槽位下标 + 当前代际拼句柄（**唯一拼装点**——避免各处自行拼装出语义不一致的句柄）。
	 *
	 * @param SlotIndex 桶内槽位下标。
	 * @return 返回该槽当前代际的句柄。
	 */
	FTcsSkillEntryHandle MakeHandle(uint32 SlotIndex) const;

	// 槽位数组（值语义；下标即句柄 `Index`）
	TArray<FTcsLearnedSkillEntry> Entries;

	// 每槽代际（0 = 从未分配；分配取进程唯一奇数，释放 +1 为偶数）
	TArray<uint32> SlotGenerations;

	// 空闲槽位栈（释放后入栈，分配时复用）
	TArray<uint32> FreeSlots;

#pragma endregion
};



/**
 * per-unit 技能账本注册表：`单位 → 桶` 的登记表，持全部在册条目。
 *
 * **为什么用 `TUniquePtr` 间接层**：`TMap` 扩容会**搬移元素**，若桶按值存放，
 * 遍历期间为另一个单位建桶就可能让手上的桶指针悬空。间接层使**桶地址稳定**。
 *
 * **脏句柄一律拒绝 + `Warning`（不 `ensure`）**：代际失配 / 下标越界 / 单位未注册都是
 * **时序竞态**语义（调用方手上的句柄过期了），不是配置错误——与"非法配置"的 `ensure` 口径刻意分开。
 * 拒绝 MUST NOT 影响同桶其它条目。
 *
 * **导出宏理由同 `FSkillBucket`**（二者是同一处组合的两个类型）。
 */
class FTcsSkillRegistry
{
// 分配
#pragma region Allocation

public:
	/**
	 * 取或建单位桶（不存在则新建）。
	 *
	 * **返回引用是安全的**：桶经 `TUniquePtr` 持有，本次调用之后的 `TMap` 扩容只搬指针、不搬桶。
	 *
	 * @param Unit 单位实体句柄。
	 * @return 返回该单位的桶。
	 */
	FSkillBucket& FindOrAddBucket(FTcsCombatEntityHandle Unit);

#pragma endregion


// 访问
#pragma region Access

public:
	/**
	 * 按句柄解析条目（先定位单位桶，再代际校验；任一步失败返回 nullptr）。
	 *
	 * @param Unit 单位实体句柄。
	 * @param Handle 账本条目句柄。
	 * @return 返回条目指针；脏句柄 / 单位未注册返回 nullptr。
	 */
	FTcsLearnedSkillEntry* Find(FTcsCombatEntityHandle Unit, const FTcsSkillEntryHandle& Handle);

	// 按句柄解析条目（只读重载；语义同上）
	const FTcsLearnedSkillEntry* Find(FTcsCombatEntityHandle Unit, const FTcsSkillEntryHandle& Handle) const;

	// 取单位桶（未注册返回 nullptr——正常查询路径，不 ensure）
	FSkillBucket* FindBucket(FTcsCombatEntityHandle Unit);

	// 取单位桶（只读重载）
	const FSkillBucket* FindBucket(FTcsCombatEntityHandle Unit) const;

	// 是否已为该单位建桶
	bool ContainsUnit(FTcsCombatEntityHandle Unit) const;

	// 已建桶数（观测用）
	int32 NumBuckets() const;

	// 全部在册条目数（跨单位合计；观测与统计用）
	int32 NumEntries() const;

	/**
	 * 遍历全部桶（**按单位句柄的 `TMap` 序**——不定序，MUST NOT 依赖它做可复现断言）。
	 * 访问者返回 false 即提前终止。
	 *
	 * **为什么给这个口**：按来源级联撤销要扫"该单位"的桶，而门面按单位取桶即可；
	 * 全桶遍历供宿主清理与统计使用（受控读口，不暴露"桶怎么存"）。
	 *
	 * @param Visitor 访问者（单位 + 该单位桶；返回是否继续）。
	 */
	void ForEachBucket(TFunctionRef<bool(FTcsCombatEntityHandle, const FSkillBucket&)> Visitor) const;

	// 遍历全部桶（可写重载——仅供"就地改条目字段"的场景）
	void ForEachBucket(TFunctionRef<bool(FTcsCombatEntityHandle, FSkillBucket&)> Visitor);

#pragma endregion


// 生命周期
#pragma region Lifetime

public:
	/**
	 * 注销单位：清空并删除该单位的桶（该单位全部旧句柄随之失效）。
	 *
	 * **调用方 MUST 先完成该单位每条条目的撤销流程**——本函数只管登记表，
	 * 不代为清理外部资源（同 `TTcsInstancePool::Reset` 的零策略纪律）。
	 *
	 * @param Unit 单位实体句柄。
	 * @return 返回是否确实删掉了一个桶（未注册返回 false——正常路径，不 ensure）。
	 */
	bool RemoveBucket(FTcsCombatEntityHandle Unit);

	// 全量清空（门面 `Deinitialize` 的确定性清空口——全部旧句柄失效）
	void Reset();

#pragma endregion


// 内核
#pragma region Core

private:
	// 单位 → 桶（TUniquePtr 间接层使桶地址不随登记表扩容而失效）
	TMap<FTcsCombatEntityHandle, TUniquePtr<FSkillBucket>> Buckets;

#pragma endregion
};
