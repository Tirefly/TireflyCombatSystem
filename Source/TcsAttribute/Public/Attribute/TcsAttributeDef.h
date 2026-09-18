// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"

#include "Attribute/TcsAttributeBounds.h"
#include "Attribute/TcsAttrModInstance.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "TcsAttributeDef.generated.h"



/**
 * 属性定义行（**唯一字段形状** + **编辑期载体**，08 §5 双轨同步的"表行轨"）：
 * 项目词表表 `DT_AttributeDefinitions` 的行类型（`FTableRowBase` 是 DataTable 行结构的 UHT 前提）。
 *
 * **身份 = RowName（= 属性名）**——行内 MUST NOT 再存一份 id（2026-09-17 用户口径：
 * DataTable 的键本就是行名，行内再放一份只会制造"行名与字段谁为准"的双真相）；
 * 运行期资产按 `[PrimaryAssetType, DefId]` 解析（资产侧 `DefId` 与行名取同一值）。
 * 定义字段（基础值/边界/值域模式/覆盖带同优先级策略）MUST 只在本结构声明一次，资产侧组合持有（不得复制字段集）。
 */
USTRUCT(BlueprintType)
struct TCSATTRIBUTE_API FTcsAttributeDefTableRow : public FTableRowBase
{
	GENERATED_BODY()

// 基础值
#pragma region BaseValue

public:
	// 基础值（单位侧实例的初值；等级成长 = 宿主升级事务改写基础值，等级 → 数值映射归项目）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Definition")
	double BaseValue = 0.0;

#pragma endregion


// 边界
#pragma region Bounds

public:
	// 值域边界（Min/Max 各自三态；Dynamic 边界先按聚合管线求值，自引用禁止）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Definition")
	FTcsAttributeBounds Bounds;

#pragma endregion


// 值域模式
#pragma region ValueDomain

public:
	// 值域模式（末端收口语义——钳制 / 自定义逃逸位 / 循环）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Definition")
	ETcsAttributeValueDomain ValueDomain = ETcsAttributeValueDomain::AVD_Clamp;

#pragma endregion


// 覆盖带口径
#pragma region OverrideTieBreak

public:
	/**
	 * Override 带的**同优先级裁决策略**（2026-09-18 用户口径，封闭四值不开放 Custom）：
	 * 本属性的 Override 修正器优先级打平时，按此策略比较数值（默认取最大值 = 历史行为）。
	 * 强弱的第一裁决者始终是修正器侧的 `OverridePriority`；本字段只在打平时生效。
	 * 只有本属性用到 `TAO_Override` 时才有意义（其余情况是死配置，不报错）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Definition")
	ETcsAttrOverrideTieBreak OverrideTieBreak = ETcsAttrOverrideTieBreak::OTB_Max;

#pragma endregion
};



/**
 * 属性定义资产（**运行期载体**，Def 族双轨制 2026-09-17 用户拍板）：
 * 一条属性一个资产，持自身身份 `DefId` 与一行定义 `Def`（字段形状单份——见 `FTcsAttributeDefTableRow`）。
 *
 * 与表行的分工（用户口径）：DataTable 是**编辑器阶段的策划编辑载体**（批量编辑方便），
 * **不作为运行期加载源**；运行期一律走资产——资产制的扩展性好（未来给定义加 Fragment 等
 * 只动资产与定义行）。两轨一致性由 08 §5 的编辑器同步器维护（M8 工具面，R3 不做）。
 *
 * 基类 = `UPrimaryDataAsset`（Def 资产族统一约定）。
 */
UCLASS(BlueprintType)
class TCSATTRIBUTE_API UTcsAttributeDef : public UPrimaryDataAsset
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
	// 属性身份（= 属性名，与词表行名取同一值；DefLibrary 按 `[PrimaryAssetType, DefId]` 解析本资产）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Definition")
	FName DefId = NAME_None;

public:
	/**
	 * 覆写主资产身份：**名取 `DefId`**（不取资产名）——于是"FName Id ↔ 资产"的解析与资产文件
	 * 叫什么无关（引擎 `UPrimaryDataAsset` 文档亦指向"要改行为就在原生类里覆写本函数"）。
	 *
	 * @return 返回本资产的主资产身份 `[PrimaryAssetType, DefId]`。
	 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#pragma endregion


// 定义行
#pragma region Def

public:
	// 定义行（字段形状的唯一声明处——资产组合持有，不复制字段集；行身份即 RowName，故行内无 id）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Definition")
	FTcsAttributeDefTableRow Def;

#pragma endregion


// 数据校验
#pragma region Validation

#if WITH_EDITOR
public:
	/**
	 * 编辑器数据校验：身份非空（`DefId` 空则 `[PrimaryAssetType, DefId]` 无意义，报错）。
	 * 说明：资产名与 `DefId` 不一致不再校验——主资产身份的名取 `DefId`，资产文件可自由命名。
	 *
	 * @param Context 校验上下文（错误/警告收集口）。
	 * @return 返回校验结果（Invalid = 存在错误）。
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

#pragma endregion
};
