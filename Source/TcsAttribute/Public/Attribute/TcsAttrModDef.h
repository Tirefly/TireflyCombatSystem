// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "TcsValueConvention.h"

#include "Attribute/TcsAttrModInstance.h"
#include "GameplayTagContainer.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "TcsAttrModDef.generated.h"



/**
 * 修正器模板定义行（**唯一字段形状** + **编辑期载体**，08 §5 双轨同步的"表行轨"）：
 * 修正器模板表的行类型（`FTableRowBase` 是 DataTable 行结构的 UHT 前提）。
 *
 * **身份分工（2026-09-22 tag 化改造）**——两者**分工而非双真相**：
 * - **`TemplateTag`（`FGameplayTag`）= 内容身份**（状态/技能定义的 `ModifierRows` 按它引用模板，D3-19）；
 * - **RowName（`FName`）= 编辑期定位**（DataTable 的键，引擎硬约束的 `FName`）。
 *
 * 二者 MUST NOT 被要求同名；一致性由 M8 同步器维护。
 * 模板字段 MUST 只在本结构声明一次，资产侧组合持有（不得复制字段集）。
 *
 * 表格编辑局限（记录在案）：`Operand.Literal` 是 `FTcsParamValue`（`FInstancedStruct` 载荷），
 * CSV/Excel 往返**不会**保留该列（引擎 CSV 导入无法表达多态实例结构）——本表行只支持
 * **编辑器内表格编辑**；标量列（Target/Op/ValueConvention/SortKey）仍可表格批量编辑。
 */
USTRUCT(BlueprintType)
struct TCSATTRIBUTE_API FTcsAttrModDefTableRow : public FTableRowBase
{
	GENERATED_BODY()

// 身份
#pragma region Identity

public:
	// 模板身份 tag（内容身份；状态/技能定义按它引用，资产按 `[PrimaryAssetType, TemplateTag.GetTagName()]` 解析）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier Template")
	FGameplayTag TemplateTag;

#pragma endregion


// 模板默认值
#pragma region Defaults

public:
	// 被修饰的属性
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier Template")
	FGameplayTag Target;

	// 运算带（封闭五带；带序由 Op 决定——见 TcsAttrModInstance.h 的带权助手）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier Template")
	ETcsAttributeOp Op = ETcsAttributeOp::TAO_Add;

	// 运算数（定义侧形状：Literal 可配等级表等参数源，物化时求值并转规范值）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier Template")
	FTcsAttrModOperandDef Operand;

	/**
	 * 本行数值的书写约定（D5-18 v3）：作用域 = 本模板自己书写的数值（不是引用来的值、
	 * 不是算出来的复合结果），物化边界经 FTcsValueConvention::ConvertToCanonical 转规范值。
	 * 可配性由数值来源自身声明（能力位虚函数）——ParamRef/AttributeScaled 禁配，保存期即报。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier Template",
		Meta = (Bitmask, BitmaskEnum = "/Script/TcsNotation.ETcsValueConventionFlag"))
	ETcsValueConventionFlag ValueConvention = ETcsValueConventionFlag::VCF_None;

	// 同带内展示/审计位（折叠按 Op 分桶——带序唯一真相在 Op，不得依赖本字段）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier Template")
	int32 SortKey = 0;

	/**
	 * Override 带的强弱排座次（**仅 `Op == TAO_Override` 时有意义**，物化时原样进账本）：
	 * 同一属性上多条 Override 时**大者胜**；优先级打平才落到属性定义的 `OverrideTieBreak` 策略。
	 * 非 Override 带填了它不会参与折叠（编辑器校验会给一条警告）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier Template",
		Meta = (EditCondition = "Op == ETcsAttributeOp::TAO_Override", EditConditionHides))
	int32 OverridePriority = 0;

	// 注（2026-09-23 删除）：原有一个 `FName Tag`（"同来源内分组标签"）字段——它从旧 TCS 搬来，
	// 全库**零消费者**（折叠/物化/级联撤销都不读），却因 `EditAnywhere` 让配置者以为填了有用。
	// 用户 2026-09-23 拍板删除。若将来真需要"同来源内分组"，那时它应是 `FGameplayTag`
	// （与全系统标识体系统一），而非 FName。

#pragma endregion
};



/**
 * 修正器模板资产（D3-19，**运行期载体**）：纯模板=默认值，引用处零字段覆写——引用方（状态/技能定义）
 * 只存模板 Id，物化时按模板默认值构建修正器（D2-13 定义侧形状 → 运行侧账本形状的转换点）。
 * 物化执行器住状态模块（本模块只提供类型与校验）。
 *
 * 双轨制（2026-09-17 用户口径，全 Def 族适用）：表行 `FTcsAttrModDefTableRow` 供策划表格编辑，
 * **运行期一律走本资产**（运行期零 DataTable 加载路径）；两轨一致性由 08 §5 的编辑器同步器维护。
 * 基类 = `UPrimaryDataAsset`（Def 资产族统一约定）。
 */
UCLASS(BlueprintType)
class TCSATTRIBUTE_API UTcsAttrModDef : public UPrimaryDataAsset
{
	GENERATED_BODY()

// 身份
#pragma region Identity

public:
	/**
	 * 主资产类型标识（Def 资产族统一约定，2026-09-17 用户定案）：**显式声明而非靠类名派生**——
	 * 族语义固定、资产改名/挪目录不失联（默认实现取继承链首个原生类名与资产名）。
	 * 注册到 AssetManager 的 `PrimaryAssetTypesToScan` 属 M6 DefLibrary 轮（不注册也能解析 id）。
	 */
	static const FPrimaryAssetType PrimaryAssetType;

public:
	// 模板身份 tag（2026-09-22 tag 化改造：原 `FName TemplateId`；状态/技能定义按 `[PrimaryAssetType, TemplateTag.GetTagName()]` 引用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier Template")
	FGameplayTag TemplateTag;

public:
	/**
	 * 覆写主资产身份：**名取 `TemplateTag.GetTagName()`**（不取资产名）——"tag ↔ 资产"的解析与资产文件
	 * 叫什么无关（引擎 `UPrimaryDataAsset` 文档亦指向"要改行为就在原生类里覆写本函数"）。
	 * **`FPrimaryAssetId` 的 name 位是 `FName`**（引擎类型约束）——tag 须经 `GetTagName()` 转换。
	 *
	 * @return 返回本资产的主资产身份 `[PrimaryAssetType, TemplateTag.GetTagName()]`。
	 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#pragma endregion


// 模板定义行
#pragma region Definition

public:
	// 定义行（字段形状的唯一声明处——资产组合持有，不复制字段集；行身份即 RowName，故行内无 id）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier Template")
	FTcsAttrModDefTableRow Def;

#pragma endregion


// 数据校验
#pragma region Validation

#if WITH_EDITOR
public:
	/**
	 * 编辑器数据校验：约定列白名单（D5-18 v3）+ 配置完整性 + 身份非空——策划即配即报。
	 *
	 * @param Context 校验上下文（错误/警告收集口）。
	 * @return 返回校验结果（Invalid = 存在错误）。
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

#pragma endregion
};
