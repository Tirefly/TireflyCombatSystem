// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "TcsChainRedirect.generated.h"



/**
 * 链重定向行（**定义侧，纯反射数据**）——"单位 U 上的链 A 改走链 B"。
 *
 * **它在做什么（D5-7 的链级让渡点）**：技能自己声明的 `CastChainId` 是**全局定义**（默认档）；
 * 而"某个装备 / 某个天赋在的时候，这个技能该走另一条链"这件事由本结构表达。
 * 典型用法 = 升级形态："学了 `slash_combo_5` 之后，`slash_combo` 改走 `slash_combo_5`"。
 *
 * **为什么 MUST 反射**（需求明文）：D5-7 的原词是「**数据重定向行，策划零代码**」——若做成
 * "只能由 C++ 注册"的形状，策划每加一条让渡都要程序员改代码。**MUST NOT** 那样做。
 *
 * **★ 本结构体 MUST NOT 携带 `Source`（2026-10-09 裁定，与 `FTcsNumericParamModifier` 同一条判据）**：
 * 来源**因实例而异**——同一个"链 A → 链 B"的行，甲玩家靠装备给、乙玩家靠天赋给 ⇒ 它属于
 * **这一次施加**，不属于这条声明。而且 `FTcsSourceHandle` 是**非反射裸 struct**
 * （`TcsCore/Handle/TcsSourceHandle.h:20` 是裸 `struct`，无 `USTRUCT`）⇒ **不能作 `UPROPERTY`**
 * ⇒ 放进本结构体只会得到一个"配不了也存不下"的**假字段**（既不进细节面板、也不进资产）。
 * ⇒ **`Source` 住挂载侧**：`FTcsChainRedirectRegistry::Mount(Entry, Source)` 的**形参**，
 * 并随栈记录一起存放（见该类的 `FRecord`）。先例 = M2 `FTcsAttrModDefTableRow` 无 `Source`，
 * 它是 `MakeFromDef(Row, Operand, InSource)` 的形参。
 *
 * **边界（如实登记，MUST NOT 被读成"策划今天就配得了"）**：本行是**数据形状**，
 * 而承载它的**资产类**本轮不存在（无 `UTcsChainRedirectDef`，且技能侧本轮零资产载体）
 * ⇒ 今天的挂载入口是 C++ 形参。资产载体归 `R6.5-g` 同批——本轮落"反射形状"的意义是
 * **将来加资产时不发生数据迁移**（形状已定），而不是假装载体已在。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsChainRedirectEntry
{
	GENERATED_BODY()

// 命中键
#pragma region Match

public:
	// 归属单位（解析命中的第一段键；"在谁身上让渡"）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Redirect")
	FTcsCombatEntityHandle Unit;

	// 被替换的链身份（技能自己声明的 `CastChainId`，或上一级让渡的结果）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Redirect")
	FGameplayTag TargetChainId;

#pragma endregion


// 替换与排座次
#pragma region Replace

public:
	// 替换成的链身份（命中后实际起来的链）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Redirect")
	FGameplayTag ReplacementChainId;

	/**
	 * 优先级：同 `(单位, 目标链)` 上多条命中时**取本值大者**，打平取**后挂者**（设计原词"后挂/高优先"）。
	 *
	 * **默认 `0` 是判据不是偏好**：与 M2/M5 的 `OverridePriority` 同款——默认档取"不改变相对次序"的
	 * 中性值，于是"只挂一条"与"挂多条但都不填优先级"两种情形都退化为**后挂者胜**这一条可预测规则。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Redirect")
	int32 Priority = 0;

#pragma endregion
};



/**
 * Custom 契约（**注入点形状预留**）——宿主/上层回答"这个链 id 还要不要再让渡一次"。
 *
 * **它站在解析顺序的最后一环**：`CastChainId` 解析 = 全局定义 → 链重定向栈 → **本契约**。
 *
 * **⚠ 本轮（R6 Task 5）本接口"可声明、不可求值"（如实登记）**：其求值归 `R6.5-g`。
 * 若宿主**已注入**实现，解析路径 MUST **具名报出"未实现"**（`Warning`）——MUST NOT 静默忽略
 * （那会让宿主以为它生效了，正是本仓反复拦的静默错误）；**未注入**时不报（没有东西被拒绝求值，
 * 报"未实现"反而会污染"零红字"这条验收信号）。
 *
 * **形态取 C++ 纯虚而非 `BlueprintNativeEvent`**（与 `ITcsEntityQuery` 同款）：本接口与它是同一类东西
 * ——`D4-14` 反向依赖击穿的**宿主注入契约**，调用方是框架自己的解析路径（不是脚本流程里的回调），
 * 故取 C++ 面即可，不必承担 `BlueprintNativeEvent` 那套 `_Implementation` 默认体的负担。
 */
UINTERFACE(MinimalAPI)
class UTcsChainRedirectResolver : public UInterface
{
	GENERATED_BODY()
};

/**
 * 链重定向 Custom 契约（宿主实现）。
 *
 * **为什么单独一层而不是直接读 `FTcsChainRedirectEntry`**：数据行表达的是"确定的映射"，
 * 而本契约表达的是"**算出来的**映射"（按等级、按形态、按剧情状态决定改走哪条链）——
 * 前者策划零代码，后者必须宿主给逻辑。两者不是同一种东西，MUST NOT 合并。
 */
class ITcsChainRedirectResolver
{
	GENERATED_BODY()

// 解析契约
#pragma region Resolve

public:
	/**
	 * 求"这条链是否还要再让渡一次"。
	 *
	 * @param ChainId 当前解析结果（全局定义 → 数据栈之后的链身份）。
	 * @param OutReplacementChainId 输出替换后的链身份（返回 false 时内容未定义，调用方不得使用）。
	 * @return 返回是否发生让渡。
	 */
	virtual bool ResolveChainRedirect(FGameplayTag ChainId, FGameplayTag& OutReplacementChainId) = 0;

#pragma endregion
};



/**
 * 链重定向栈（**账本侧**）：挂载记录集 + 按 `(单位, 目标链)` 的解析。
 *
 * **为什么存"记录"而不是直接用 `FTcsChainRedirectEntry`**：定义侧行**没有** `Source`
 * （见该结构体的裁定），而摘除 MUST 按来源成对（"不存在摘不掉的条目"）；后挂裁决还需要一个
 * **挂载序**。两样都属**这一次施加**，故落在本记录里。
 *
 * **顺序 = 数组序**（不是哈希序）：`Resolve` 的"打平取后挂者"要求**稳定的挂载序**，
 * 而 `TMap` / `TSet` 迭代序不承诺稳定 ⇒ 用 `TArray` 线性扫描。
 * 条目数是**每单位每链**量级（内容侧几条到几十条），不是热路径规模。
 *
 * **无导出宏（判据同 `FSkillBucket`）**：本类由门面按值持有、消费者不直接触达它
 * ⇒ 跨模块可达性由门面（`UTcsSkillSubsystem`，带 `TCSSKILL_API`）承担。
 *
 * **零 GC 负担**：记录只含 `FGameplayTag` / 句柄 / `int32` / `uint64`，**无任何 `UObject` 引用**
 * ⇒ 门面 `AddReferencedObjects` MUST NOT 为它新增遍历（同账本条目"不持对象引用"的纪律）。
 */
class FTcsChainRedirectRegistry
{
// 挂载与摘除
#pragma region Mount

public:
	/**
	 * 挂载一条重定向记录。
	 *
	 * **来源无效 ⇒ 拒绝**（返回 false）：无锚点的条目摘不掉 ⇒ 违反成对性判据
	 * （同 `FTcsParamChainOps::ApplyParamModifiers` 的无效来源守卫）。
	 * **目标链或替换链身份无效 ⇒ 拒绝**：无效 tag 的条目永远不可能命中，挂着只是占位。
	 *
	 * @param Entry 定义侧行（`Source` 不在这里——见该结构体的裁定）。
	 * @param Source 本次挂载的来源锚点（级联摘除用）。
	 * @return 返回是否挂载成功。
	 */
	bool Mount(const FTcsChainRedirectEntry& Entry, FTcsSourceHandle Source);

	/**
	 * 按来源级联摘除（摘掉该来源挂上的全部记录），返回摘除条数。
	 *
	 * **`0` 条命中 = 正常路径，MUST NOT ensure**（同 M2 `RemoveBySource` 的口径）。
	 *
	 * @param Source 归属来源句柄（无效 = 0 命中，返回 0）。
	 * @return 返回摘除的记录条数。
	 */
	int32 UnmountBySource(FTcsSourceHandle Source);

#pragma endregion


// 解析
#pragma region Resolve

public:
	/**
	 * 解析某单位上某条链的最终身份。
	 *
	 * **未命中任何记录 ⇒ 原样返回 `GlobalChainId`**（**正常路径**：既不是错误，也 MUST NOT 报"未实现"。
	 * 测试判据即"未命中时解析结果 = 技能自己声明的链"）。
	 *
	 * **多条命中 ⇒ `Priority` 大者胜、打平取后挂者**（设计原词"后挂/高优先"）。
	 *
	 * @param Unit 归属单位。
	 * @param GlobalChainId 全局定义给出的链身份（默认档）。
	 * @return 返回解析后的链身份。
	 */
	FGameplayTag Resolve(FTcsCombatEntityHandle Unit, FGameplayTag GlobalChainId) const;

#pragma endregion


// 观测与生命周期
#pragma region Observe

public:
	// 在册记录数（观测与装置断言用）
	int32 Num() const;

	// 全量清空（门面 `Deinitialize` 的确定性清空口——世界拆解时账本随之消亡）
	void Reset();

#pragma endregion


// 内核
#pragma region Core

private:
	// 一条挂载记录 = 定义侧行 + 这一次施加的来源与挂载序（两者都属实例，故不进定义侧行）
	struct FRecord
	{
		// 定义侧行（命中键、替换身份与优先级）
		FTcsChainRedirectEntry Entry;

		// 本次挂载的来源锚点（级联摘除用；非反射，故住记录而不住行）
		FTcsSourceHandle Source;

		// 挂载序（"打平取后挂者"的裁决键；只增不减，不复用）
		uint64 Sequence = 0;
	};

	// 挂载记录集（数组序 = 挂载序；MUST NOT 换成无序容器——见类注释）
	TArray<FRecord> Records;

	// 挂载序发号器（本栈内唯一即可：裁决只在栈内比较，"后挂者"是同栈内的相对关系）
	uint64 NextSequence = 0;

#pragma endregion
};
