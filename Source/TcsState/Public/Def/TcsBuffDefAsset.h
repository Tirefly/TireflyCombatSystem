// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Def/TcsBuffDef.h"
#include "Def/TcsStateDef.h"

#include "TcsBuffDefAsset.generated.h"



/**
 * Buff 定义资产（本模块的定义载体，`UTcsStateDef` 家族的施加态一支）：持 `FTcsBuffDef` 定义数据。
 *
 * **类名为何带 `Asset` 后缀（UHT 硬门槛）**：UHT 按"去 `U/A/F/E/I/T` 前缀后的引擎名"判重，
 * 资产类 `UTcsBuffDef` 与数据 struct `FTcsBuffDef` 同名会在 **UHT 阶段**直接失败
 * （`Error: Class '…' shares engine name '…' with struct '…'`）——先例 `UTcsEffectTriggerDefAsset`，
 * 标准条文见 `openspec/project.md` 的 `Asset` 后缀补救条款。**补救方向 MUST 是给资产类改名**，
 * MUST NOT 反过来改已定名的数据类型。
 *
 * **发现与解析**：`UTcsDefinitionSubsystem::DiscoverStateDefs` 按类扫描本类并缓存 `BuffDef`，
 * 运行期按 `DefTag` 经 `ResolveStateDef` 取用；**不做世界装配**（状态定义按需解析，
 * 无"每世界登记一次"的语义——与链 / 触发定义两条路径不同）。
 */
UCLASS(BlueprintType)
class TCSSTATE_API UTcsBuffDefAsset : public UTcsStateDef
{
	GENERATED_BODY()

// 身份
#pragma region Identity

public:
	/**
	 * 主资产类型标识（**遮蔽基类同名静态成员**）：取值 = 本类名 `"TcsBuffDefAsset"`。
	 * 静态成员不是虚的，故只声明它不够——`GetPrimaryAssetId` 必须一并覆写（见下）。
	 */
	static const FPrimaryAssetType PrimaryAssetType;

public:
	/**
	 * 覆写主资产身份：**必须覆写**（不是可选优化）——基类实现里 `PrimaryAssetType` 是
	 * **按基类作用域编译期绑定**的静态成员，不覆写会让本族资产报出 `[TcsStateDef, …]`，
	 * 与"主资产类型取值 = 类名"的族约定不符。
	 *
	 * @return 返回本资产的主资产身份 `[PrimaryAssetType, DefTag.GetTagName()]`。
	 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#pragma endregion


// 定义数据
#pragma region Definition

public:
	/**
	 * Buff 定义数据（字段形状的唯一声明处住 `FTcsBuffDef`）。
	 *
	 * **无 `BlueprintReadOnly`**：本字段的读取面是定义库的 `ResolveStateDef`（C++ 面）；
	 * 细节面板可编辑性只看 `CPF_Edit`，与 `BlueprintType` 无关，故 `EditAnywhere` 已足够
	 * （与链资产 / 触发定义资产同款取舍）。
	 */
	UPROPERTY(EditAnywhere, Category = "Tcs|State")
	FTcsBuffDef BuffDef;

#pragma endregion


// 数据校验
#pragma region Validation

#if WITH_EDITOR
public:
	/**
	 * 编辑器数据校验（派生段：定义数据的完整性）——判据与理由见 `state-def-asset` 能力：
	 * 六条错误（身份由基类判；`Params` 键重复 / `ModifierRows` 空引用 / `Triggers` 内 `EventTag` 无效 /
	 * `Triggers` 内 `EffectChainId` 无效 / `Finite` 而 `DurationTime` 来源为空 / 描述配置项缺项）
	 * + 两条警告（`Period < 0`、`PeriodRefresh != Keep` 而 `Period == 0`）。
	 * 错误信息 MUST 点名出问题的字段，供策划可操作定位。
	 *
	 * @param Context 校验上下文（错误/警告收集口）。
	 * @return 返回校验结果（Invalid = 存在错误；无错无警 = Valid）。
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

#pragma endregion
};
