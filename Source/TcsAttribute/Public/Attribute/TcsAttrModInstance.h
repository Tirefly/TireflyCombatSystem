// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Parameter/TcsParamValue.h"

#include "Attribute/TcsAttributeName.h"
#include "Handle/TcsSourceHandle.h"

#include "TcsAttrModInstance.generated.h"



/**
 * 属性运算带（D2-7 砍 Custom op / D2-10 增 FlatAdd，纯封闭五带）——聚合公式：
 * 存在 Override 时取 Override 组最大值直接作为结果（FlatAdd 一并被覆盖，"最强覆盖生效"）；
 * 否则 `Final = ((Base + ΣAdd) × (1 + ΣPercentAdd)) × ΠMul + ΣFlatAdd`。
 * 组内与带间顺序无关（M5 参数链同用此五带与同一折叠器——D5-5 v3）。
 * 无 Custom 位：计算在上游（施加链/伤害流程/项目代码）求值传入终值，聚合内无代码插点（D0-1）；
 * 新增运算 = 末尾追加枚举值（追加需论证与既有带的交换性）。
 * 枚举值前缀 TAO_ 是 TcsAttributeOp 的缩写。
 */
UENUM(BlueprintType)
enum class ETcsAttributeOp : uint8
{
	TAO_Add = 0			UMETA(DisplayName = "加", ToolTip = "同带求和（ΣAdd）——默认，值 0"),
	TAO_Override = 1	UMETA(DisplayName = "覆盖", ToolTip = "覆盖组存在时取组内最大值直接作为结果，其余带（含 FlatAdd）一并被覆盖"),
	TAO_PercentAdd = 2	UMETA(DisplayName = "百分比加", ToolTip = "同带求和后作整体缩放系数（1 + ΣPercentAdd）"),
	TAO_Mul = 3			UMETA(DisplayName = "乘", ToolTip = "同带连乘（ΠMul）"),
	TAO_FlatAdd = 4		UMETA(DisplayName = "平加", ToolTip = "全部缩放之后、clamp 之前的平坦加，不受 PercentAdd/Mul 缩放（GAS FixedAdd 借鉴）"),
};

// 带权助手（带序唯一真相在 Op）：账本 SortKey 仅为同带内展示/审计位，折叠 MUST NOT 依赖它——
// 按 Op 分桶即得带序，SortKey 与带权表不得互为第二真相。
FORCEINLINE int32 GetTcsAttributeBandWeight(ETcsAttributeOp Op)
{
	switch (Op)
	{
	case ETcsAttributeOp::TAO_Override:
		return 0;
	case ETcsAttributeOp::TAO_Add:
		return 10;
	case ETcsAttributeOp::TAO_PercentAdd:
		return 15;
	case ETcsAttributeOp::TAO_Mul:
		return 20;
	case ETcsAttributeOp::TAO_FlatAdd:
		return 30;
	default:
		return 10;
	}
}



/**
 * 运算数种类（D2-11，仅此两种——已考察拒绝的候选清单见决策点文档，M2 账本保持封闭）。
 * 枚举值前缀 OPK_ 是 OperandKind 的缩写。
 */
UENUM(BlueprintType)
enum class ETcsOperandKind : uint8
{
	OPK_Literal = 0			UMETA(DisplayName = "字面量", ToolTip = "数值直接来自本行书写的值（定义侧经参数源求值，运行侧为已解析规范值）——默认，值 0"),
	OPK_AttributeScaled = 1	UMETA(DisplayName = "属性换算", ToolTip = "取值 = Coefficient × Current(Attribute)：主属性→派生属性载体（收集时求值且读即登记依赖边）"),
};



/**
 * 运算数（定义侧，D2-13 双形状的定义面）：Literal 为全插件统一数值载体 FTcsParamValue
 * （可配等级表等源，模板默认值可等级化）；配值约定列时由物化边界转规范值（D5-18 v3）。
 * 两侧同形说明：OPK_AttributeScaled 不物化（live 求值），故两侧字段一致。
 */
USTRUCT(BlueprintType)
struct TCSATTRIBUTE_API FTcsAttrModOperandDef
{
	GENERATED_BODY()

// 种类
#pragma region Kind

public:
	// 运算数种类
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operand")
	ETcsOperandKind Kind = ETcsOperandKind::OPK_Literal;

#pragma endregion


// 字面量
#pragma region Literal

public:
	// 字面量数值来源（OPK_Literal 时生效；本源书写的数值即结果——可配约定列）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operand",
		Meta = (EditCondition = "Kind == ETcsOperandKind::OPK_Literal", EditConditionHides))
	FTcsParamValue Literal;

#pragma endregion


// 属性换算
#pragma region AttributeScaled

public:
	// 被读取的属性（OPK_AttributeScaled 时生效）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operand",
		Meta = (EditCondition = "Kind == ETcsOperandKind::OPK_AttributeScaled", EditConditionHides))
	FTcsAttributeName Attribute;

	// 换算系数（OPK_AttributeScaled 时生效；百分比语义直接写小数——本种类禁配约定列）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Operand",
		Meta = (EditCondition = "Kind == ETcsOperandKind::OPK_AttributeScaled", EditConditionHides))
	double Coefficient = 1.0;

#pragma endregion
};



/**
 * 运算数（运行侧，D2-13 双形状的账本面）：纯 C++ struct——账本只为聚合热路径服务，不进反射面。
 * Literal 恒为已解析规范值（物化器单点转换保证，账本不做二次猜测——零膨胀）。
 * 导出宏纪律：全内联值类型不加模块导出宏（加了会在消费方 LNK2019——见 TcsCore 的 TcsSourceHandle 注记）。
 */
struct FTcsAttrModOperand
{
	// 运算数种类
	ETcsOperandKind Kind = ETcsOperandKind::OPK_Literal;

	// 已解析规范值（OPK_Literal）
	double Literal = 0.0;

	// 被读取的属性（OPK_AttributeScaled；取值 = Coefficient × Current(Attribute)，收集时求值）
	FTcsAttributeName Attribute;

	// 换算系数（OPK_AttributeScaled）
	double Coefficient = 1.0;
};



/**
 * 修正器实例（运行侧账本条目，D2-2 权威完整形状）：纯 C++ struct——挂在被修饰属性的
 * ModifierSlots 上，Source 为级联撤销锚点（来源注销 → 按 Source 全量摘除）。
 * 与模板 `UTcsAttrModDef` 成对（Def = 模板 / Instance = 按模板物化出的账本条目——命名与
 * 全插件"定义资产 ↔ 池化实例"的分野一致）。
 *
 * 本文件范围（2026-09-17 用户指出的整理）：只承载**修正器族自身的词汇与形状**——
 * 运算带 / 运算数（定义侧 + 运行侧）/ 修正器实例。属性定义与实例所需的**值域词汇**
 * （边界三态 `FTcsAttributeBounds` + 值域模式 `ETcsAttributeValueDomain`）已拆出到
 * `Attribute/TcsAttributeBounds.h`（一对概念同文件——2026-09-17 复评合并）。
 *
 * 命名说明：修正器族统一用 `TcsAttrMod*` 前缀（`UTcsAttrModDef` / `FTcsAttrModInstance` /
 * `FTcsAttrModOperand(Def)`），属性族用 `TcsAttribute*`（Name / Instance / Bounds / Store / …）——
 * 两族前缀不同是有意的：前者是"修正器"，后者是"属性"本身。
 */
struct FTcsAttrModInstance
{
	// 被修饰的属性
	FTcsAttributeName Target;

	// 运算带
	ETcsAttributeOp Op = ETcsAttributeOp::TAO_Add;

	// 运算数（运行侧形状；OPK_AttributeScaled 在收集时求值并读即登记依赖边）
	FTcsAttrModOperand Operand;

	// 归属来源（级联撤销锚点；系统级常驻修正器用约定的常驻来源句柄）
	FTcsSourceHandle Source;

	// 同带内展示/审计位（折叠按 Op 分桶——带序唯一真相在 Op，不得依赖本字段）
	int32 SortKey = 0;

	// 同来源内分组标签（可选）
	FName Tag = NAME_None;
};
