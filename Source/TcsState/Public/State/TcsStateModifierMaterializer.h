// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Attribute/TcsAttrModInstance.h"

class UTcsStateSubsystem;
struct FTcsBuffDef;
struct FTcsStateInstance;



/**
 * 状态修正器物化器（D3-19 的落地执行者）：把状态定义的 `ModifierRows`（模板引用行）
 * 物化成属性账本条目（`FTcsAttrModInstance`），`Source` 取**状态实例的来源句柄**。
 *
 * **它是"数值变化"这条链的起点**：在它之前，施加一个状态只改状态自己的账；在它之后，
 * 状态真的能改属性数值——R5 第一个"肉眼可见"的信号。
 *
 * **物化点 = 定义 → 实例的边界**（MEM-20260902-20 的既定判据）：模板是**纯模板 = 默认值**，
 * 引用处零字段覆写；覆写的唯一通道是**参数传值**——施加方的 `Overrides` 进快照，
 * 物化时从**该实例的快照**解析（引用类操作数经快照读取适配器取值）。物化之后**零回查**
 * （D2-9 / D3-18）：账本条目里只有已解析的规范值。
 *
 * **值约定在物化边界转一次**（D5-18 v3）：模板行书写的数值经 `FTcsValueConvention::ConvertToCanonical`
 * 转规范值；账本不做二次猜测。能力位为假的源（引用源 / 属性换算源）不转——判据由源自身声明，
 * MUST NOT 在物化器里建"源类型 × 可配约定"的中心名单（PV-10 纪律）。
 *
 * **本类是纯函数式的**：不写账本、不广播、不产生任何世界副作用（挂载由 `FTcsStateOps` 的挂点做）。
 * 这样"求值对不对"与"挂没挂上"是两个可分别验证的问题。
 */
class FTcsStateModifierMaterializer
{
// 物化
#pragma region Materialize

public:
	/**
	 * 物化一个状态实例的全部修正器条目。
	 *
	 * 输入取**实例**而不是散字段：`Unit` / `Instigator` / `Level` / `Source` / `ParamSnapshot`
	 * 全是实例已有的信息（实例自持 `Unit` 正是为了让物化不必遍历桶找单位）——多传几个形参
	 * 只会让"某处少填一个"成为可能，而少填在源侧表现为**静默落兜底**而不是报错。
	 *
	 * 求值上下文经 `FTcsStateOps::MakeContext` **同一处装配**（与快照构建同源），且其中的
	 * `ParamTable` 位即该实例的快照（经反射壳绑定）——"本次求值的参数表"在物化点就是快照本身。
	 *
	 * @param Subsystem 状态门面（提供登记表与快照读取壳）。
	 * @param Def 状态定义内容（取 `ModifierRows`）。
	 * @param Instance 目标实例（提供主体 / 等级 / 来源 / 快照；本函数只读它）。
	 * @param Out 输出条目数组（**函数内先清空**——调用方无需自行清空）。
	 */
	static void Materialize(
		UTcsStateSubsystem& Subsystem,
		const FTcsBuffDef& Def,
		const FTcsStateInstance& Instance,
		TArray<FTcsAttrModInstance>& Out);

#pragma endregion
};
