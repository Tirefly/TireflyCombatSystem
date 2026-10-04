// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"

#include "Def/TcsBuffDef.h"

#include "TcsBuffDefTableRow.generated.h"



/**
 * Buff 定义表行（**编辑期载体**，双轨制的"表行轨"）：`FTableRowBase` 是 DataTable 行结构的 UHT 前提。
 *
 * **双轨制（2026-09-17 用户口径，全 Def 族适用）**：**表 = 编辑期载体、资产 = 运行期载体**——
 * DataTable 只服务策划批量编辑与编辑器即时响应，**运行期 MUST 零 DataTable 加载路径**；
 * 运行期一律按 `DefTag` 经定义库解析资产。两轨一致性由编辑器侧同步器维护（R8 `TOOLS-3`）。
 *
 * **身份分工**（两者分工而非双真相）：
 * - `DefTag`（`FGameplayTag`）= **内容身份**（定义库的解析键与去重键）；
 * - RowName（`FName`）= **编辑期定位**（DataTable 的键，引擎硬约束）。
 *
 * 二者 MUST NOT 被要求同名。字段形状 MUST 只在 `FTcsBuffDef` 声明一次——本行只组合持有，不复制字段集。
 *
 * 表格编辑局限（同款先例见 `FTcsAttrModDefTableRow`）：行内含 `FTcsParamValue`（`FInstancedStruct` 载荷）
 * 与 `TArray<FTcsEffectTriggerDef>`，CSV/Excel 往返**不保留**这些列——本表行只支持编辑器内表格编辑。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsBuffDefTableRow : public FTableRowBase
{
	GENERATED_BODY()

// 身份
#pragma region Identity

public:
	// 内容身份（= 资产的 `DefTag`；RowName 只是编辑期定位）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def")
	FGameplayTag DefTag;

#pragma endregion


// 定义数据
#pragma region Definition

public:
	// 定义数据（字段形状的唯一声明处住 `FTcsBuffDef`）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff Def")
	FTcsBuffDef BuffDef;

#pragma endregion
};
