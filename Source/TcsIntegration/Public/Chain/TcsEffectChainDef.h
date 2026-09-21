// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "Chain/TcsEffectChain.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "TcsEffectChainDef.generated.h"



/**
 * 效果链资产（**运行期载体**，2026-09-21 用户拍板"资产轨"）：
 * 一条链一个资产，供策划在编辑器里创作链（顺序、步骤、参数）——**零 C++ 加链**（验收剧本检查点 6）。
 *
 * 与表行的关系：链**不做双轨**（否决理由：链带嵌套 `FInstancedStruct` 步骤数组，DataTable 单元格内嵌
 * 多态结构的编辑体验差；行轨留给扁平词表）。故本类**没有** `FTableRowBase` 对应行类型。
 *
 * 基类 = `UPrimaryDataAsset`（Def 资产族统一约定 2026-09-17）——主资产身份让"FName Id ↔ 资产"的解析
 * 与按类型发现/加载归引擎。
 *
 * 步骤数组**无编辑器类型收窄**（`TArray<FInstancedStruct>`，D4-16 有意为之）：作者侧校验归 M8 校验矩阵；
 * 运行期由执行器注册表拒绝未注册类型（未注册 → 断链 + Error，见 `TcsEffectStepExecutor.h`）。
 */
UCLASS(BlueprintType)
class TCSINTEGRATION_API UTcsEffectChainDef : public UPrimaryDataAsset
{
	GENERATED_BODY()

// 身份
#pragma region Identity

public:
	/**
	 * 主资产类型标识（Def 资产族统一约定，2026-09-17 定案）：**显式声明而非靠类名派生**——
	 * 族语义固定、资产改名/挪目录不失联（默认实现取继承链首个原生类名与资产名）。
	 * 注册到 AssetManager 的 `PrimaryAssetTypesToScan` 属 M6 DefLibrary 轮（不注册也能解析 id；
	 * R3 的发现走 `IAssetRegistry::GetAssetsByClass`，见台账 R7-3）。
	 */
	static const FPrimaryAssetType PrimaryAssetType;

public:
	// 链身份（= 链登记表键；与 `Chain.ChainId` 必须一致——双真相禁令）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tcs|Effect Chain")
	FName ChainId = NAME_None;

public:
	/**
	 * 覆写主资产身份：**名取 `ChainId`**（不取资产名）——于是"FName Id ↔ 资产"的解析与资产文件
	 * 叫什么无关（引擎 `UPrimaryDataAsset` 文档亦指向"要改行为就在原生类里覆写本函数"）。
	 *
	 * @return 返回本资产的主资产身份 `[PrimaryAssetType, ChainId]`。
	 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#pragma endregion


// 链数据
#pragma region Chain

public:
	/**
	 * 链数据（链的 `ChainId` 字段与上方身份字段必须一致——不一致则拒绝登记，不静默取其一）。
	 * **无 `BlueprintReadOnly`**：`FTcsEffectChain` 是 `USTRUCT()`（非 BlueprintType，TcsEffect 侧有意），
	 * 而 BlueprintType 类的 `BlueprintReadOnly` 属性要求类型可蓝图化（UHT 编译实证 2026-09-21）。
	 * `EditAnywhere` 已足够——细节面板可编辑性只看 `CPF_Edit`（`PropertyEditorHelpers.cpp:407`），
	 * 与 BlueprintType 无关（2026-09-20 已核）。
	 */
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect Chain")
	FTcsEffectChain Chain;

#pragma endregion


// 数据校验
#pragma region Validation

#if WITH_EDITOR
public:
	/**
	 * 编辑器数据校验（双真相禁令 + 身份非空）：
	 * - `ChainId` 为空 → Invalid（`[PrimaryAssetType, ChainId]` 失去意义，且登记必被拒）；
	 * - `Chain.ChainId` 与 `ChainId` 不一致 → Invalid（双真相：读者无从判断以哪个为准，故要求作者消歧）。
	 *
	 * @param Context 校验上下文（错误/警告收集口）。
	 * @return 返回校验结果（Invalid = 存在错误）。
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

#pragma endregion
};
