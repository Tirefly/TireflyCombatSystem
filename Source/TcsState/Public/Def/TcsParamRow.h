// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Parameter/TcsParamValue.h"
#include "TcsValueConvention.h"

#include "TcsParamRow.generated.h"



// 参数行取值模式（D5-18 v2：镜像 SkillDef 行形状，本行型住 TcsState）
// 枚举值前缀 EPM_ 是 ParamMode 的缩写
UENUM(BlueprintType)
enum class ETcsParamMode : uint8
{
	EPM_Snapshot = 0	UMETA(DisplayName = "快照", ToolTip = "施加时求值一次并冻结进实例快照（默认；buff 强度即此语义）"),
	EPM_Live = 1		UMETA(DisplayName = "实时", ToolTip = "每次读取都重新求值（技能侧的实时通道用；R5 状态层不用）"),
};



/**
 * 数值参数行（**字段形状的唯一声明处**，`FTcsStateDefBase.Params` 的元素类型）：
 * 键 = 参数表键，值 = `FTcsParamValue`（PV 系列统一载体：字面量 / 参数引用 / 等级表等）。
 *
 * **键的归属**：`Key` MUST 落 **`TcsStateParam`** 根（消费角色 = 参数表读取；形态 `TcsStateParam.<键>`，
 * 2 段）——具体键由宿主 `Config/DefaultGameplayTags.ini` 声明，插件 MUST NOT 声明任何参数表键
 * （规则与根段注册表见 `gameplay-tag-governance` 能力）；根下存在性校验归 M8 校验器，不在本行内建。
 *
 * **取值模式**：`Snapshot`（默认）= 施加瞬间求值一次、写进实例快照，实例生命周期内读快照不重算
 * （"运行中升级不追溯"的既定语义）；`Live` 是技能侧的实时通道，R5 状态层不消费。
 *
 * **约定列**：`ValueConvention` 的作用域 = **本行自己书写的数值**（不是引用来的、也不是算出来的），
 * 快照构建的写入点经 `FTcsValueConvention::ConvertToCanonical` 转规范值；可配性由数值来源自身声明
 * （能力位虚函数），校验归资产 `IsDataValid`（见 `state-def-asset` 能力）。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsNumericParamRow
{
	GENERATED_BODY()

// 键
#pragma region Key

public:
	// 参数键（`TcsStateParam` 根；快照构建时按它写入参数表）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Param")
	FGameplayTag Key;

#pragma endregion


// 值与模式
#pragma region Value

public:
	// 数值来源（默认 Literal；快照写入点求值一次并转规范值）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Param")
	FTcsParamValue Base;

	// 取值模式（快照 = 施加时冻结，默认）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Param")
	ETcsParamMode Mode = ETcsParamMode::EPM_Snapshot;

	// 本行数值的书写约定（D5-18 v3）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Param",
		Meta = (Bitmask, BitmaskEnum = "/Script/TcsNotation.ETcsValueConventionFlag"))
	ETcsValueConventionFlag ValueConvention = ETcsValueConventionFlag::VCF_None;

#pragma endregion
};
