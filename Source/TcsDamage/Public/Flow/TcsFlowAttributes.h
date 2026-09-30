// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Attribute/TcsAttrModInstance.h"
#include "Attribute/TcsAttributeBandFold.h"
#include "Parameter/TcsParamValue.h"

#include "TcsFlowAttributes.generated.h"



/**
 * **伤害修改器**的消耗策略（09 §2.3 / D7-4）：提交可携带"能用几次 / 多久 / 裁决座次"。
 *
 * **纯数据、可反射**（2026-09-30 用户拍板，`DEC-04` §3.3 裁定 ④）：原 `FTcsConsumePolicy`
 * 含 `TFunction<void()> OnConsumed`，而含闭包的结构**无法出现在任何 `UPROPERTY` 上**
 * （UHT 编译错误）⇒ 配置面（链步骤 / 数据步骤）一律带不上消耗策略。改造后**消费行为经事件语义表达**：
 * 消费成功时发布消费事件，"被消费时要做什么"由项目**经触发行订阅**表达——可序列化、可反射、
 * 可脚本可达，且复用项目既有的订阅通道，零新机制。
 *
 * **改名（同日）**：`FTcsConsumePolicy` → `FTcsDamageModifierConsumePolicy`。"DamageModifier"
 * 指名"**伤害修改器**"这一提交者身份（09 §2.3 的修改器通道），与走运算带聚合的**属性**修正器
 * （`FTcsAttrModInstance` / `UTcsAttrModDef`，无消耗语义）明确区分。
 *
 * **本容器只存不裁**——裁决（SortKey 选一 → 成功才消费）归 `FTcsFlowExecute` 步骤（D7-4"收集 ≠ 消费"），
 * 未选中者完全不动。消费**动作**（扣 `MaxUses` / 起 `Cooldown` / 标记已消费 / 发消费事件）归
 * 台账 `DAMAGE-4`（R5/M4a）：本结构只定形状。
 */
USTRUCT()
struct TCSDAMAGE_API FTcsDamageModifierConsumePolicy
{
	GENERATED_BODY()

	// 最多可用次数（0 = 不限）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	int32 MaxUses = 0;

	// 冷却（秒；0 = 无冷却；CD 到期堆在流程外驱动，见 09 §3）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	double Cooldown = 0.0;

	// 裁决排座次（大者优先；**不参与求值顺序**）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	int32 SortKey = 0;
};



/**
 * 黑板的一条提交（收集项）：**操作数保持提交态**（`FTcsParamValue`），读时求值——
 * 与 M2 账本同款纪律（D2-13：运行侧账本恒为已解析规范值，但求值发生在重算/读取时点，
 * 以便引用型操作数先读到最新依赖值）。
 */
struct TCSDAMAGE_API FTcsFlowAttributeSubmit
{	// 运算带（带序唯一真相）
	ETcsAttributeOp Op = ETcsAttributeOp::TAO_Add;

	// 操作数（提交态书写值/引用；读时求值）
	FTcsParamValue Operand;

	// 消耗策略（只存不裁）
	FTcsDamageModifierConsumePolicy Consume;
};



/**
 * 流程属性黑板（09 §2.1 三层值空间的第三层；09 §2.3）：键 → 修正链 + 封闭五带运算。
 * **流程的工作值**（BaseDamage / FinalDamage 一类临时变量），不是角色属性——
 * 故 R3 无值域收口（无边界/值域模式概念；`IValueDomainPolicy` 同款逃逸位随值域策略轮）。
 *
 * 折叠纪律（D5-5 v3）：`Read` MUST 调用 TcsAttribute 的共享纯函数 `FoldTcsAttributeBands`——
 * M2 属性聚合 / M5 参数链 / 本容器**三处共用，MUST NOT 私建第二份**。
 *
 * 键 = 项目词表（内建步骤读写的键名是标准步骤库的契约；项目键自由 FName，无注册表）。
 * 生命周期 = 每流程一份（随上下文释放；无跨流程状态）。
 *
 * **导出宏（2026-09-21 跨模块实证）**：本容器的 `Submit` / `Read` 是**宿主自研流程步骤的公共调用面**
 * （宿主在自己的步骤里读写黑板是既定用法）——无导出宏则宿主模块链接失败（实测 `LNK2019`）。
 * 同文件的 `FTcsFlowAttributeSubmit`（被 `FindSubmits` 返回）同此。
 */
struct TCSDAMAGE_API FTcsFlowAttributes
{
// 提交与读取
#pragma region Access

public:
	/**
	 * 提交一笔修正（落进该键的修正链；同键可多笔）。
	 *
	 * @param Key 黑板键（项目词表）。
	 * @param Op 运算带。
	 * @param Operand 操作数（提交态；读时求值）。
	 * @param Consume 消耗策略（只存不裁）。
	 * @return 返回提交是否成功（键为空 → false，不静默记账）。
	 */
	bool Submit(FGameplayTag Key, ETcsAttributeOp Op, const FTcsParamValue& Operand, const FTcsDamageModifierConsumePolicy& Consume = FTcsDamageModifierConsumePolicy());

	/**
	 * 读取键的当前值：对该键全部提交**逐一求值**后按共享折叠函数求值。
	 *
	 * @param Key 黑板键。
	 * @param EvalContext 操作数求值上下文（引用型操作数按此取依赖；Literal 源忽略）。
	 * @return 返回折叠结果；键无提交时返回 0（折叠初值）。
	 */
	double Read(FGameplayTag Key, const FTcsParamEvaluateContext& EvalContext) const;

	/**
	 * 便捷读取（无上下文的场景——如只含 Literal 源的键）。
	 *
	 * @param Key 黑板键。
	 * @return 返回折叠结果。
	 */
	double Read(FGameplayTag Key) const;

	// 收集重置（标准步骤 `CollectStart` 的落点）：清空全部键的提交
	void Reset();

#pragma endregion


// 候选访问（裁决步骤用）
#pragma region Submits

public:
	/**
	 * 取某键的提交列表（**只读**——裁决步骤按 `SortKey` 选一，不消费他人候选）。
	 *
	 * @param Key 黑板键。
	 * @return 返回该键的提交数组；键不存在返回 nullptr。
	 */
	const TArray<FTcsFlowAttributeSubmit>* FindSubmits(FGameplayTag Key) const;

#pragma endregion


// 存储
#pragma region Storage

private:
	// 键 → 提交链（每次提交追加；Reset 清空）
	TMap<FGameplayTag, TArray<FTcsFlowAttributeSubmit>> Submits;

#pragma endregion
};
