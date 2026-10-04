// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "Parameter/TcsParamTableReader.h"
#include "State/TcsStateSnapshot.h"

#include "TcsStateParamTableReader.generated.h"



/**
 * 参数快照的读取适配器（**本体，纯 C++ 类**）：把 `FTcsParamSnapshot` 当参数表暴露出来。
 *
 * **它是"从快照取值"的唯一入口**——修正器物化（R5 Task 4）与引用类参数源都经它读快照，
 * 消费方 MUST NOT 自行遍历 `Entries` 去拼一套平行的键查找（那会制造第二处查找语义）。
 *
 * **为什么本体保持纯 C++ 类**（2026-10-04 裁定，2026-10-05 复核仍成立）：它**不持任何 UObject**、
 * 不参与寿命管理——构造方保证快照在读取期间存活；做成 `UObject` 会把快照指针的寿命绑到 GC 上，
 * 而本体自己并不需要反射（反射面由下面的薄壳承担，两者分工而非重复）。
 *
 * **寿命契约**：本类**不持有**快照——构造方 MUST 保证快照在读取期间存活（实例里的快照在实例
 * 在册期间稳定；栈上构造的快照按作用域）。
 */
class FTcsStateParamTableReader
{
// 构造
#pragma region Lifetime

public:
	// 绑定一份快照（不接管所有权）
	explicit FTcsStateParamTableReader(const FTcsParamSnapshot* InSnapshot)
		: Snapshot(InSnapshot)
	{
	}

#pragma endregion


// 读取
#pragma region Query

public:
	/**
	 * 按键读取快照内的规范值。
	 *
	 * @param Key 参数键。
	 * @param OutValue 输出读到的值（miss 时内容未定义，调用方不得使用）。
	 * @return 返回是否命中；false = miss（未绑定快照也算 miss）。
	 */
	bool TryGetNumericParam(FGameplayTag Key, double& OutValue) const
	{
		return Snapshot ? Snapshot->TryGetNumericParam(Key, OutValue) : false;
	}

#pragma endregion


// 内核
#pragma region Core

private:
	// 被读取的快照（裸指针——本类不参与寿命管理，见类型注释）
	const FTcsParamSnapshot* Snapshot = nullptr;

#pragma endregion
};



/**
 * 参数快照读取适配器的**反射壳**（2026-10-04，随修正器物化落地）：把快照作为"本次求值的参数表"
 * 装进反射上下文 `FTcsParamEvaluateContext::ParamTable` 的载体。
 *
 * **为什么现在才建壳**：`state-param-snapshot` 能力原先把"实现 `ITcsParamTableReader` 的时机"
 * 留白为"出现第一个要把快照当参数表挂进上下文的消费方"——物化求值（修正器模板里的引用类操作数
 * 要从**该状态自己的快照**取值）正是那个消费方。
 *
 * **为什么不是给本体加反射、而是一层薄壳**：本体保持零 UObject（它可以在任意 C++ 路径上按值传递），
 * 壳只承担"过反射边界"这一件事——它是 `UObject` 且持一份**借用**来的快照指针。
 *
 * **它只做委托**：键查询原样转给本体，查找语义仍只有一份实现（壳内 MUST NOT 出现第二套遍历）。
 *
 * **寿命与绑定纪律**：壳持的是**裸指针**，只在绑定作用域（`FTcsStateSnapshotScope`）内有效——
 * 作用域外 MUST NOT 留存该指针，也 MUST NOT 把壳的绑定当作跨帧状态。
 */
UCLASS()
class TCSSTATE_API UTcsStateParamTableReader : public UObject, public ITcsParamTableReader
{
	GENERATED_BODY()

// 绑定
#pragma region Binding

public:
	/**
	 * 绑定（或解绑）一份快照（**借用**——本类不接管所有权、不做寿命管理）。
	 *
	 * @param InSnapshot 快照指针；传 `nullptr` 即解绑（此后一切查询落 miss）。
	 */
	void Bind(const FTcsParamSnapshot* InSnapshot)
	{
		BoundSnapshot = InSnapshot;
	}

	// 取当前绑定的快照（绑定作用域保存/恢复用；未绑定返回 nullptr）
	const FTcsParamSnapshot* GetBoundSnapshot() const
	{
		return BoundSnapshot;
	}

#pragma endregion


// 读取契约（委托本体）
#pragma region Query

public:
	/**
	 * 读取数值参数（`ITcsParamTableReader` 的 C++ 实现体：**纯转发**给本体）。
	 *
	 * @param Key 参数键。
	 * @param OutValue 输出读到的值（miss 时内容未定义）。
	 * @return 返回是否命中。
	 */
	virtual bool TryGetNumericParam_Implementation(FGameplayTag Key, double& OutValue) override
	{
		// 就地构造本体（本体是零状态的值类型，构造成本 = 存一个指针）——避免壳内再存一份
		// "与绑定快照同步的副本"（那会制造第二处绑定真相）
		return FTcsStateParamTableReader(BoundSnapshot).TryGetNumericParam(Key, OutValue);
	}

#pragma endregion


// 内核
#pragma region Core

private:
	// 当前借用的快照（未绑定 = nullptr；生命周期由绑定作用域保证）
	const FTcsParamSnapshot* BoundSnapshot = nullptr;

#pragma endregion
};



/**
 * 快照绑定作用域（RAII）：作用域内把壳绑到某份快照，退出时**恢复上一份绑定**。
 *
 * **为什么是栈式恢复而不是"退出即清空"**：物化可能嵌套（父级求值期间再起一次物化），
 * 清空式解绑会让外层作用域**静默失去绑定**（表现为外层后续查询全部落兜底——最难查的一类）。
 * 栈式恢复使嵌套天然正确，先例 = 属性管线的 `PushEvalStack` / `PopEvalStack`。
 *
 * **不可拷贝**（拷贝会让"谁负责恢复"失去唯一答案）。
 */
class FTcsStateSnapshotScope
{
// 构造与析构
#pragma region Lifetime

public:
	/**
	 * 进入作用域：保存壳上的上一份绑定并绑到新快照。
	 *
	 * @param InReader 目标壳（可为 `nullptr`——门面尚未建壳时作用域退化为空操作）。
	 * @param InSnapshot 本次求值要用的快照（借用，须存活过本作用域）。
	 */
	FTcsStateSnapshotScope(UTcsStateParamTableReader* InReader, const FTcsParamSnapshot* InSnapshot)
		: Reader(InReader)
		, PrevSnapshot(InReader ? InReader->GetBoundSnapshot() : nullptr)
	{
		if (Reader)
		{
			Reader->Bind(InSnapshot);
		}
	}

	// 退出作用域：恢复上一份绑定（嵌套按栈回退）
	~FTcsStateSnapshotScope()
	{
		if (Reader)
		{
			Reader->Bind(PrevSnapshot);
		}
	}

	FTcsStateSnapshotScope(const FTcsStateSnapshotScope&) = delete;
	FTcsStateSnapshotScope& operator=(const FTcsStateSnapshotScope&) = delete;

#pragma endregion


// 取用
#pragma region Query

public:
	// 取本次作用域的参数表（可直接装进 `FTcsParamEvaluateContext::ParamTable`）
	TScriptInterface<ITcsParamTableReader> GetTable() const
	{
		return Reader ? TScriptInterface<ITcsParamTableReader>(Reader) : TScriptInterface<ITcsParamTableReader>();
	}

#pragma endregion


// 内核
#pragma region Core

private:
	// 被绑定的壳（不持有所有权）
	UTcsStateParamTableReader* Reader = nullptr;

	// 进入前的绑定（退出时恢复）
	const FTcsParamSnapshot* PrevSnapshot = nullptr;

#pragma endregion
};
