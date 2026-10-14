// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Pool/TcsInstancePool.h"

#include "Runtime/TcsEntityRuntimeGenerationIssuer.h"



/**
 * 从池句柄反推它的**幻影标签**（`TTcsInstanceHandle<TTag>` → `TTag`）。
 *
 * **为什么需要它**：共享桶必须能命名 `TTcsInstancePool<TPayload, TTag, Issuer>`，
 * 而 `TTag` 住各域的**句柄类型**里（`FTcsBuffInstanceHandle::GetInner()` 的返回类型）。
 * 共享层 MUST NOT include 三个域句柄头（那会把共享层绑到具体域，且三个头分属两模块）
 * ⇒ 由本 trait 从**调用方给的句柄类型**里把标签解出来。
 */
template <typename TInnerHandle>
struct TTcsEntityRuntimeHandleTag;

template <typename TTagType>
struct TTcsEntityRuntimeHandleTag<TTcsInstanceHandle<TTagType>>
{
	using Type = TTagType;
};



/**
 * **共享 per-entity 桶**（2026-10-14 立）：一个实体的全部运行态实例 + 槽位代际 + 空闲槽栈。
 *
 * **它是本次合并的核心产出**：状态侧手写桶（`FStateBucket`）与技能侧手写桶（`FSkillBucket`）
 * 曾各自实现一遍"槽位 + 代际 + 空闲栈 + 代际校验 + 稳定序遍历 + 重置"，实测**逐行同构**
 * ⇒ 现收敛为**一个模板的两个实例化**（`TPayload` = 状态实例 / 账本条目 / 施法运行态）。
 *
 * **载体 = `TTcsInstancePool`**（`TcsCore` 既有代际池，复用其分配 / 释放 / 解析 / 代际 /
 * 稳定序遍历 / GC 只读遍历 / 重置）。
 *
 * **★ 为什么必须注入进程级发号器（`FTcsStateGenerationIssuer`）——MUST NOT 改回池内自增**：
 * 桶是 **per-entity** 的，而句柄**不带实体段**；若各桶各从代际 1 起发，两个实体各自的首实例会得到
 * **相同的 `(Index, Generation)`** ⇒ "跨桶扫描式解析"（`FTcsBuffOps::Find` 即此类）**命中错实体**。
 * 这是本仓 **2026-10-05 实测过的缺陷**（`decisions-log.md:257` / `implementation-log.md:281`）。
 * 池的第三模板参正是为它开的口，且默认档保持池内自增 ⇒ 其余四个池使用者零行为差异。
 *
 * **槽位下标即句柄 `Index`**（桶内定位），代际则进程唯一 ⇒ 跨桶、跨世界身份都不撞。
 *
 * **遍历纪律（照既有两份手写桶的原文，MUST 保留）**：遍历期间 MUST NOT 增删实例
 * （增删会改动槽位与空闲栈）；需要增删时先自行收集句柄，遍历结束后再动（两段式）。
 * **就地改字段是允许的**——改字段不动槽位。
 *
 * @tparam TPayload 桶内元素类型（值语义纯数据；各域自己的运行态记录）。
 * @tparam THandleType 该域的**展平反射句柄类型**（提供 `GetInner()` / `SetInner()` / `Index`）。
 * @tparam TGenerationIssuer 代际发号策略。**默认 = 进程级**（`FTcsStateGenerationIssuer`），
 *         这是 per-entity 桶**必须**的档（见上条）。
 *         **唯一合法的非默认用法 = "不作为 per-entity 桶使用"**（如技能侧那张**扁平**的施法运行态池：
 *         它不由实体分桶、也不做跨桶扫描 ⇒ 无身份撞车风险）⇒ 该场景传 `FTcsPoolLocalGenerationIssuer`
 *         **保持池内自增**，从而**行为逐字不变**（代际数值不进任何判据，但"不变"是最省事的正确）。
 */
template <typename TPayload, typename THandleType, typename TGenerationIssuer = FTcsStateGenerationIssuer>
class TTcsEntityRuntimeBucket
{
// 类型
#pragma region Types

public:
	// 池句柄类型（由域句柄的 `GetInner()` 返回类型反推——共享层不认识具体标签）
	// **`DeclVal` 而非直接 `THandleType::GetInner()`**：后者是**非静态成员函数**，
	// `decltype` 对它求值会报 C2352（"调用非静态成员函数需要对象"）——`DeclVal` 只造类型不求值。
	using FInnerHandle = decltype(DeclVal<THandleType>().GetInner());

	// 幻影标签（池的类型区分参数）
	using TTagType = typename TTcsEntityRuntimeHandleTag<FInnerHandle>::Type;

private:
	// 桶的实际载体：发号策略由模板参给定（默认进程级——见类注释）
	using FPool = TTcsInstancePool<TPayload, TTagType, TGenerationIssuer>;

#pragma endregion


// 槽位
#pragma region Slots

public:
	/**
	 * 分配槽位（优先复用空闲槽；每次取**进程内不复用的正奇数**代际）。
	 *
	 * @return 返回新槽的句柄（`Index` 为桶内下标，`Generation` 为当前代际）。
	 */
	THandleType AllocateSlot()
	{
		THandleType Handle;
		Handle.SetInner(Pool.Allocate());
		return Handle;
	}

	/**
	 * 释放槽位（代际 +1 使旧句柄悬空；槽位入空闲栈；**槽位内容重置**不跨生命周期残留）。
	 *
	 * **对已空闲 / 越界下标静默容忍**（返回 false 而不 ensure）——重复释放属正常竞态收尾，
	 * 与池 `Free`（收句柄、对契约违规 `ensure`）口径刻意分开。
	 *
	 * @param SlotIndex 桶内槽位下标。
	 * @return 返回是否确实释放了一个在册槽。
	 */
	bool ReleaseSlot(uint32 SlotIndex)
	{
		if (!Pool.IsSlotAllocated(SlotIndex))
		{
			return false;
		}

		// 先清内容再置空闲（顺序不影响结果，但清完再归还更贴近"槽位内容不跨生命周期残留"的读法）
		Pool.ResetSlotContent(SlotIndex);
		return Pool.ReleaseSlotByIndex(SlotIndex);
	}

#pragma endregion


// 访问
#pragma region Access

public:
	/**
	 * 按句柄解析实例（**代际校验**：失配 / 越界 / 空闲槽返回 nullptr，**不 ensure**）。
	 *
	 * **不 ensure 是刻意的**：脏句柄是**时序竞态**语义（调用方手上的句柄过期了），
	 * 不是配置错误——与"非法配置"的 `ensure` 口径分开。拒绝 MUST NOT 影响同桶其它实例。
	 */
	TPayload* Find(const THandleType& Handle)
	{
		return const_cast<TPayload*>(static_cast<const TTcsEntityRuntimeBucket*>(this)->Find(Handle));
	}

	// 按句柄解析实例（只读重载；语义同上）
	const TPayload* Find(const THandleType& Handle) const
	{
		return Pool.Resolve(Handle.GetInner());
	}

	// 句柄是否指向在册实例（代际校验，返回 false 而不 ensure）
	bool IsValidHandle(const THandleType& Handle) const
	{
		return Find(Handle) != nullptr;
	}

	/**
	 * 按**谓词**找实例（**按槽位下标升序**，先命中先返回）。
	 *
	 * **为什么给这个通用口而不是让共享层认识业务键**：域侧要找的键各不相同
	 * （技能按 `DefTag` 找"同一实体会同一个技能"至多一条），而共享层 MUST NOT 认识域词汇。
	 * 谓词版既保住稳定序，又不引入第二个遍历实现。只做查找、不改桶。
	 *
	 * @param Predicate 谓词（返回是否命中）。
	 * @return 返回首个命中的实例指针；无命中返回 nullptr。
	 */
	TPayload* FindByPredicate(TFunctionRef<bool(const TPayload&)> Predicate)
	{
		return const_cast<TPayload*>(static_cast<const TTcsEntityRuntimeBucket*>(this)->FindByPredicate(Predicate));
	}

	// 按谓词找实例（只读重载；语义同上）
	const TPayload* FindByPredicate(TFunctionRef<bool(const TPayload&)> Predicate) const
	{
		const TPayload* Found = nullptr;
		ForEach([&Predicate, &Found](const TPayload& Item)
		{
			if (Predicate(Item))
			{
				Found = &Item;
				return false;
			}

			return true;
		});

		return Found;
	}

	/**
	 * 遍历在册实例（**按槽位下标升序**——稳定序，同输入同输出是可复现验收的前提）。
	 * 访问者返回 false 即提前终止。遍历期间 MUST NOT 增删实例（见类注释）。
	 */
	void ForEach(TFunctionRef<bool(TPayload&)> Visitor)
	{
		Pool.ForEach([&Visitor](TPayload& Item)
		{
			Visitor(Item);
		});
	}

	// 遍历在册实例（只读重载；可见集与可写重载逐字相同）
	void ForEach(TFunctionRef<bool(const TPayload&)> Visitor) const
	{
		Pool.ForEach([&Visitor](const TPayload& Item)
		{
			Visitor(Item);
		});
	}

	/**
	 * **GC 引用收集专用**的只读遍历（**唯一合法调用者是门面的 `AddReferencedObjects`**）。
	 *
	 * **为什么不能借用上面的只读 `ForEach`**：GC 的引用收集**不在游戏线程上**，而池的常规
	 * 进入点都 `ensure(IsInGameThread())` ⇒ 在 ARO 里调它会**每次 GC 留一条红字**，
	 * 且 `ensure` 每站点每进程只报一次 ⇒ 症状是"偶尔一条红字"、极易被忽略。
	 * **MUST NOT 拿它做业务遍历**——那等于静默放弃 D0-4 的单线程纪律。
	 */
	void ForEachForGC(TFunctionRef<void(const TPayload&)> Visitor) const
	{
		Pool.ForEachForGC(Visitor);
	}

	// 在册实例数（观测与统计用）
	int32 Num() const
	{
		return Pool.NumAllocated();
	}

#pragma endregion


// 生命周期
#pragma region Lifetime

public:
	/**
	 * 清空桶（逐槽归还：代际推进使全部旧句柄失效；**进程发号器不复位**）。
	 *
	 * **不逐槽清理实例内容**——与池 `Reset` 同款零策略：持有外部资源的实例由调用方在
	 * 本调用**之前**自行清理（门面 `Deinitialize` 正是先撤时间条目再清桶）。
	 */
	void Reset()
	{
		Pool.Reset();
	}

#pragma endregion


// 内核
#pragma region Core

private:
	FPool Pool;

#pragma endregion
};



/**
 * **共享 per-entity 注册表**（2026-10-14 立）：`实体 → 桶` 的登记表，持全部在册实例。
 *
 * **为什么用 `TUniquePtr` 间接层（MUST 保留）**：`TMap` 扩容会**搬移元素**；若桶按值存放，
 * 遍历期间为另一个实体建桶就可能让手上的桶指针悬空。间接层使**桶地址稳定**（只有指针搬家），
 * 遍历 / 嵌套修改因此安全。
 *
 * **为什么给"遍历全部桶"的口**：句柄**不带实体段** ⇒ 状态侧 `GetState(Handle)` 只能靠
 * "扫全部桶做代际校验式定位"。把它做成公开遍历而不是 friend 掉整个 `Buckets`：前者是
 * **一个受控的读口**，后者会把"桶怎么存"变成外部可依赖的事实。
 *
 * @tparam TPayload 桶内元素类型。
 * @tparam THandleType 该域的展平反射句柄类型。
 */
template <typename TPayload, typename THandleType>
class FTcsEntityRuntimeRegistry
{
// 类型
#pragma region Types

public:
	using FBucket = TTcsEntityRuntimeBucket<TPayload, THandleType>;

#pragma endregion


// 分配
#pragma region Allocation

public:
	/**
	 * 取或建实体桶（不存在则新建）。
	 *
	 * **返回引用是安全的**：桶经 `TUniquePtr` 持有，本次调用之后的 `TMap` 扩容只搬指针、不搬桶。
	 */
	FBucket& FindOrAddBucket(FTcsCombatEntityHandle Entity)
	{
		if (const TUniquePtr<FBucket>* Existing = Buckets.Find(Entity))
		{
			return **Existing;
		}

		return *Buckets.Add(Entity, MakeUnique<FBucket>());
	}

#pragma endregion


// 访问
#pragma region Access

public:
	/**
	 * 按句柄解析实例（先定位实体桶，再代际校验；任一步失败返回 nullptr）。
	 */
	TPayload* Find(FTcsCombatEntityHandle Entity, const THandleType& Handle)
	{
		FBucket* Bucket = FindBucket(Entity);
		return Bucket ? Bucket->Find(Handle) : nullptr;
	}

	// 按句柄解析实例（只读重载；语义同上）
	const TPayload* Find(FTcsCombatEntityHandle Entity, const THandleType& Handle) const
	{
		const FBucket* Bucket = FindBucket(Entity);
		return Bucket ? Bucket->Find(Handle) : nullptr;
	}

	// 取实体桶（未注册返回 nullptr——正常查询路径，不 ensure）
	FBucket* FindBucket(FTcsCombatEntityHandle Entity)
	{
		const TUniquePtr<FBucket>* Found = Buckets.Find(Entity);
		return Found ? Found->Get() : nullptr;
	}

	// 取实体桶（只读重载）
	const FBucket* FindBucket(FTcsCombatEntityHandle Entity) const
	{
		const TUniquePtr<FBucket>* Found = Buckets.Find(Entity);
		return Found ? Found->Get() : nullptr;
	}

	// 是否已为该实体建桶
	bool ContainsEntity(FTcsCombatEntityHandle Entity) const
	{
		return Buckets.Contains(Entity);
	}

	// 已建桶数（观测用）
	int32 NumBuckets() const
	{
		return Buckets.Num();
	}

	// 全部在册实例数（跨实体合计；观测与统计用）
	int32 NumItems() const
	{
		int32 Count = 0;
		for (const TPair<FTcsCombatEntityHandle, TUniquePtr<FBucket>>& Pair : Buckets)
		{
			if (Pair.Value.IsValid())
			{
				Count += Pair.Value->Num();
			}
		}

		return Count;
	}

	/**
	 * 遍历全部桶（**按实体句柄的 `TMap` 序**——不定序，MUST NOT 依赖它做可复现断言）。
	 * 访问者返回 false 即提前终止。
	 */
	void ForEachBucket(TFunctionRef<bool(FTcsCombatEntityHandle, const FBucket&)> Visitor) const
	{
		for (const TPair<FTcsCombatEntityHandle, TUniquePtr<FBucket>>& Pair : Buckets)
		{
			if (!Pair.Value.IsValid())
			{
				continue;
			}

			if (!Visitor(Pair.Key, *Pair.Value))
			{
				return;
			}
		}
	}

	// 遍历全部桶（可写重载——仅供"就地改实例字段"的场景）
	void ForEachBucket(TFunctionRef<bool(FTcsCombatEntityHandle, FBucket&)> Visitor)
	{
		for (TPair<FTcsCombatEntityHandle, TUniquePtr<FBucket>>& Pair : Buckets)
		{
			if (!Pair.Value.IsValid())
			{
				continue;
			}

			if (!Visitor(Pair.Key, *Pair.Value))
			{
				return;
			}
		}
	}

#pragma endregion


// 生命周期
#pragma region Lifetime

public:
	/**
	 * 注销实体：清空并删除该实体的桶（该实体全部旧句柄随之失效）。
	 *
	 * **调用方 MUST 先完成该实体每条实例的撤销流程**（广播 + 外部资源摘除）——本函数只管登记表，
	 * 不代为广播、不代为撤销外部资源（同池的零策略纪律）。
	 *
	 * **`Reset()` 兼作"回收该实体全部槽位"**：池 `Reset` 按当前在册槽位数一次性回落占用统计
	 * ⇒ 不留统计残留（这正是它优于"逐个 ReleaseSlot"的地方）。
	 *
	 * @return 返回是否确实删掉了一个桶（未注册返回 false——正常路径，不 ensure）。
	 */
	bool RemoveBucket(FTcsCombatEntityHandle Entity)
	{
		// 桶随 TUniquePtr 析构而消失——先 Reset 使代际推进（旧句柄凭代际失配判悬空，
		// 比"查不到桶"更精确：将来若桶按实体句柄复用，失配判据依然成立）
		if (TUniquePtr<FBucket> Removed; Buckets.RemoveAndCopyValue(Entity, Removed))
		{
			if (Removed.IsValid())
			{
				Removed->Reset();
			}

			return true;
		}

		return false;
	}

	// 全量清空（门面 `Deinitialize` 的确定性清空口——全部旧句柄失效）
	void Reset()
	{
		for (TPair<FTcsCombatEntityHandle, TUniquePtr<FBucket>>& Pair : Buckets)
		{
			if (Pair.Value.IsValid())
			{
				Pair.Value->Reset();
			}
		}

		Buckets.Reset();
	}

#pragma endregion


// 内核
#pragma region Core

private:
	// 实体 → 桶（TUniquePtr 间接层使桶地址不随登记表扩容而失效）
	TMap<FTcsCombatEntityHandle, TUniquePtr<FBucket>> Buckets;

#pragma endregion
};
