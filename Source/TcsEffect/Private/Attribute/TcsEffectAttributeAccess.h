// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Attribute/TcsAttrModInstance.h"
#include "GameplayTagContainer.h"
#include "Handle/TcsCombatEntityHandle.h"

class UWorld;
class UTcsAttributeSubsystem;



/**
 * 属性访问解析点（**TcsEffect 侧白名单薄壳**，R5 Task 6 落地）：本模块内部唯一的属性门面取用处
 * ——`FTcsStepModifyAttribute`（挂条目）与 `FTcsTriggerCondition_AttributeCompare`（读当前值）共用它。
 *
 * **"怎么找到门面"不在这里**：那归 `UTcsAttributeSubsystem::Resolve`（TcsAttribute 的**单一查找点**，
 * 将来若补属性访问注入契约（`ITcsAttributeAccess`，台账 `R-7`）只换那一处，本类与调用点不动）。
 * 本类只管**本模块允许触碰哪几个属性 API**——白名单是两个消费模块各自的审计面，**MUST NOT 合并**
 * （口径见 `attribute-pipeline` 能力的「属性门面的解析点」需求）。
 *
 * **暴露面即纪律**：本轮允许面 = `ApplyModifier`（挂条目）+ `EvaluateCurrent`（读当前值）
 * + 事务对（`BeginBatch` / `Commit`——单条也要开批：批内只标脏，最外层提交才重算 + 广播）
 * + 两个只读判据（`IsLedgerReady` = 账本认不认识这个单位；`HasAttribute` = 该键在不在账本上）。
 * 除这些之外 MUST NOT 暴露任何属性 API——需要新能力时先改本类（让改动可审计）。
 *
 * **依赖边界（本文件是 TcsEffect 对下层领域模块的首次真实使用）**：`TcsEffect` 允许依赖**下层**
 * 领域模块（`TcsAttribute` / `TcsCore` / `TcsNotation`），**MUST NOT** 依赖上层
 * （`TcsDamage` / `TcsTargeting` / `TcsState` / `TcsSkill`）。
 *
 * **本类是栈上值对象**：只在一次调用期间存活，持裸指针（门面寿命 > 调用），不可跨帧携带。
 */
class FTcsEffectAttributeAccess
{
// 解析
#pragma region Resolve

public:
	/**
	 * 解析属性门面（**本模块唯一取用点**，内部走 `UTcsAttributeSubsystem::Resolve`）。
	 *
	 * @param World 目标世界（可为 `nullptr`）。
	 * @return 返回访问对象；世界为空或该系统不存在时 `IsValid()` 为 false（调用方按无操作处理，不 ensure）。
	 */
	static FTcsEffectAttributeAccess Resolve(const UWorld* World);

	// 是否解析到属性门面
	bool IsValid() const
	{
		return AttributeSubsystem != nullptr;
	}

	/**
	 * 属性账本是否认识这个单位（有属性容器）。
	 *
	 * **为什么需要它**：状态/链可以作用在没有属性账本的单位上（合法），而账本侧的 `BeginBatch`
	 * 对这种单位会 **ensure**（它把"未注册单位"当契约违规）⇒ 挂载前先问一句，把"没有可改的属性"
	 * 当**配置状态**处理（`Log` 级、零红字）。
	 *
	 * @param Unit 单位实体句柄。
	 * @return 返回账本是否持有该单位的属性容器。
	 */
	bool IsLedgerReady(FTcsCombatEntityHandle Unit) const;

	/**
	 * 该属性键在账本上是否存在实例（只读判据，**不取值**）。
	 *
	 * **为什么需要它**：`EvaluateCurrent` 对"键不存在"与"值恰为 0"给同一个读数（0.0）——
	 * 条件求值要区分二者才能把"键写错"报出来（MUST NOT 静默按 0.0 比较）。
	 *
	 * @param Unit 单位实体句柄。
	 * @param Attribute 属性键。
	 * @return 返回账本上是否存在该属性的实例。
	 */
	bool HasAttribute(FTcsCombatEntityHandle Unit, const FGameplayTag& Attribute) const;

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

	// 开始变更批（嵌套计数；最外层提交才统一重算 + 广播）
	void BeginBatch(FTcsCombatEntityHandle Unit) const;

	// 提交变更批（唯一提交点）
	void Commit(FTcsCombatEntityHandle Unit) const;

#pragma endregion


// 账本读面
#pragma region Query

public:
	/**
	 * 读属性**当前值**（惰性重算后的生效值；单位或属性不存在时返回 0.0，不 ensure）。
	 *
	 * @param Unit 单位实体句柄。
	 * @param Attribute 属性键。
	 * @return 返回当前值（不存在 ⇒ 0.0）。
	 */
	double EvaluateCurrent(FTcsCombatEntityHandle Unit, const FGameplayTag& Attribute) const;

#pragma endregion


// 内核
#pragma region Core

private:
	// 属性门面（解析失败 = nullptr；本类不持有所有权）
	UTcsAttributeSubsystem* AttributeSubsystem = nullptr;

#pragma endregion
};
