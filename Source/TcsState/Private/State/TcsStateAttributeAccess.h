// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Attribute/TcsAttrModInstance.h"
#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"

class UWorld;
class UTcsAttributeSubsystem;



/**
 * 属性访问解析点（Q-2 缓解①，R5 Task 4 落地）：**TcsState 内部唯一的属性门面取用处**。
 *
 * **为什么要有这一层**（而不是让引擎函数直接 `World->GetSubsystem<UTcsAttributeSubsystem>()`）：
 * 本轮裁定"状态层直接依赖属性门面"（Q-2 甲案——注入位是给**宿主词汇**用的，而属性账本是 TCS
 * 自家实现）。为了把"直接依赖"这件事的**影响面钉死在可替换的最小范围内**，取用点集中到本类：
 * 将来若真要补 `ITcsAttributeAccess` 一类的注入契约（台账 `R-7`），**只换这一个文件的实现**，
 * 调用点（挂载 / 摘除）一行不动。
 *
 * **暴露面同样是纪律**（本类的形状即约束）：本轮允许触碰的属性 API **上限**是
 * `ApplyModifier` / `RemoveBySource` / `EvaluateCurrent` 三个语义面 + 事务对（`BeginBatch` / `Commit`）
 * + 一个存在性判据（`IsLedgerReady`，它只是"账本认不认识这个单位"，MUST NOT 用于取值）。
 * 除这些之外 MUST NOT 暴露任何属性 API——需要新能力时先改本类（让改动可审计）。
 *
 * **事务对为什么在允许面内**：逐条挂载若不开批，N 条修正器 = N 次重算 + N 次广播（重入窗口放大 N 倍）；
 * 而"一个来源的条目一次性收口"是账本侧既有的语义（`BeginBatch` / `Commit`）。
 *
 * **本类是栈上值对象**：只在一次调用期间存活，持裸指针（门面寿命 > 调用），不可跨帧携带。
 */
class FTcsStateAttributeAccess
{
// 解析
#pragma region Resolve

public:
	/**
	 * 解析属性门面（**唯一取用点**）。
	 *
	 * @param World 目标世界（可为 `nullptr`）。
	 * @return 返回访问对象；世界为空或该系统不存在时 `IsValid()` 为 false（调用方按无操作处理，不 ensure）。
	 */
	static FTcsStateAttributeAccess Resolve(const UWorld* World);

	// 是否解析到属性门面
	bool IsValid() const
	{
		return AttributeSubsystem != nullptr;
	}

	/**
	 * 属性账本是否认识这个单位（有属性容器）。
	 *
	 * **为什么需要它**：状态与属性是**两套登记**——状态可以施加到没有属性账本的单位上（合法），
	 * 此时修正器"无处可挂"；而账本侧的 `BeginBatch` 对这种单位会 **ensure**（它把"未注册单位"
	 * 当契约违规）。故挂点先问一句，把"没有可改的属性"当**配置状态**处理（静默无操作），
	 * 而不是让它变成一条红字。
	 *
	 * @param Unit 单位实体句柄。
	 * @return 返回账本是否持有该单位的属性容器。
	 */
	bool IsLedgerReady(FTcsCombatEntityHandle Unit) const;

#pragma endregion


// 账本写面
#pragma region Ledger

public:
	/**
	 * 挂一条修正器（标脏；批内不重算）。
	 *
	 * @param Unit 单位实体句柄。
	 * @param Modifier 修正器（`Target` 指向目标属性）。
	 * @return 返回是否挂载成功（目标属性无实例 ⇒ false + 账本侧日志，不 ensure）。
	 */
	bool ApplyModifier(FTcsCombatEntityHandle Unit, const FTcsAttrModInstance& Modifier) const;

	/**
	 * 按来源级联摘除（扫描面含实例槽位与冻结暂存区）。
	 *
	 * @param Unit 单位实体句柄。
	 * @param Source 来源句柄。
	 * @return 返回摘除条数（0 = 无匹配，正常路径）。
	 */
	int32 RemoveBySource(FTcsCombatEntityHandle Unit, const FTcsSourceHandle& Source) const;

	// 开始变更批（嵌套计数；最外层提交才统一重算 + 广播）
	void BeginBatch(FTcsCombatEntityHandle Unit) const;

	// 提交变更批
	void Commit(FTcsCombatEntityHandle Unit) const;

#pragma endregion


// 内核
#pragma region Core

private:
	// 属性门面（解析失败 = nullptr；本类不持有所有权）
	UTcsAttributeSubsystem* AttributeSubsystem = nullptr;

#pragma endregion
};
