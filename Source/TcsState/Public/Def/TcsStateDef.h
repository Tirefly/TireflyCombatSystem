// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "TcsStateDef.generated.h"



/**
 * 状态定义资产（Def 资产族基类，对应 `FStateDefBase` 家族）：持**内容身份** `DefTag` 与 `IsDataValid` 挂点。
 *
 * **范围澄清**：定义库管辖的只是 `FStateDefBase` 家族（本类 → `UTcsBuffDefAsset` → R6 的 `UTcsSkillDef`）；
 * 修正器模板（`UTcsAttrModDef`）与属性词表是并列另一族，**不共用本基类**（故基类名不取 `UTcsDefAssetBase`
 * ——那会暗示覆盖全部 Def，见 `SPEC-02-states` §2）。
 *
 * **基类不持数据**：定义数据（`FTcsBuffDef` 一类）住各自的派生资产；本类只持身份与校验挂点——
 * 字段形状 MUST 只在数据 struct 声明一次，资产只组合持有。
 *
 * **基类取 `UPrimaryDataAsset`**（Def 资产族统一约定，2026-09-17）：Def 引用语义本是"身份 tag + 定义库解析"，
 * 主资产身份让"tag ↔ 资产"解析与按类型发现/加载归引擎。
 */
UCLASS(BlueprintType)
class TCSSTATE_API UTcsStateDef : public UPrimaryDataAsset
{
	GENERATED_BODY()

// 身份
#pragma region Identity

public:
	/**
	 * 主资产类型标识（Def 资产族统一约定）：**显式声明而非靠类名派生**——值取类名
	 * （本类 → `"TcsStateDef"`；派生族各自遮蔽本静态成员并覆写 `GetPrimaryAssetId`，见 `UTcsBuffDefAsset`）。
	 * 注册到 AssetManager 的 `PrimaryAssetTypesToScan` 属 M6/R7 轮（不注册也能解析 id；
	 * 当前发现走 `IAssetRegistry::GetAssetsByClass`，见台账 `INTEG-3`）。
	 */
	static const FPrimaryAssetType PrimaryAssetType;

public:
	// 定义身份（= 定义库的解析键与去重键；**内容身份**，不是资产名。词住状态词表根）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tcs|State")
	FGameplayTag DefTag;

public:
	/**
	 * 覆写主资产身份：**名取 `DefTag.GetTagName()`**（不取资产名）——于是资产文件叫什么、放哪个目录，
	 * 都与 `[PrimaryAssetType, DefTag]` 解析无关（`FPrimaryAssetId` 的 name 位是 `FName`，
	 * tag 须经 `GetTagName()` 转换，这是引擎类型约束）。
	 *
	 * @return 返回本资产的主资产身份 `[PrimaryAssetType, DefTag.GetTagName()]`。
	 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#pragma endregion


// 数据校验
#pragma region Validation

#if WITH_EDITOR
public:
	/**
	 * 编辑器数据校验（基类段：只判身份——数据级规则住各自派生资产，如 `UTcsBuffDefAsset`）。
	 *
	 * **无错时把 `NotValidated` 提升为 `Valid`**：`UObject::IsDataValid` 基类默认返回 `NotValidated`
	 * （语义是"没有规则"），不提升会让"已校验通过"在编辑器里显示为"未验证"，且装置断言 `== Valid` 必然失败。
	 * 派生类经 `Super::IsDataValid` 继承本行为，同时各自在末尾同样提升——两类**各自独立正确**，
	 * 不依赖调用顺序。
	 *
	 * @param Context 校验上下文（错误/警告收集口）。
	 * @return 返回校验结果（Invalid = 存在错误；无错 = Valid）。
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

#pragma endregion
};
