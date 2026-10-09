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
	EPM_Snapshot = 0	UMETA(DisplayName = "快照", ToolTip = "施法/施加的那一瞬间算一次并固定下来（默认）。技能伤害、buff 强度都用它——中途被上 debuff 也不会改这一发的值"),
	EPM_Live = 1		UMETA(DisplayName = "实时", ToolTip = "每次读取都重新算。适合引导型技能的持续伤害、会随战况变化的系数这类【不该被最初那一瞬定死】的值。⚠ 目前只有技能侧的数值参数行真按本档分流"),
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
	/**
	 * **这个参数叫什么**（参数表里的键名）。
	 *
	 * 参数的用途完全由你自己定：技能伤害值、技能倍率、持续时间、每跳伤害……都行。
	 * **键名要落 `TcsStateParam` 词根**（形如 `TcsStateParam.DamageBase`），由**宿主**在
	 * `Config/DefaultGameplayTags.ini` 里声明——引擎不预设任何参数名，避免与你的命名冲突。
	 *
	 * 同一个键可以出现在多个技能/状态上，各自配各自的值，互不影响。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Param")
	FGameplayTag Key;

#pragma endregion


// 值与模式
#pragma region Value

public:
	/**
	 * **这个参数的值**。
	 *
	 * 最常用的是 `字面量`（直接写个数）。也可以让它**跟着别的值走**：
	 * `参数引用` 取本参数表里另一个键的值（做"倍率"这类中间值）；
	 * `属性换算` 取单位某属性的当前值再乘系数；`等级表` 按技能/状态等级取档
	 * （"每级 +10 血"就配这个）。
	 *
	 * 这里写的值会在**施法/施加那一刻**算一次（除非上面「取值模式」选了 `实时`）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Param")
	FTcsParamValue Base;

	/**
	 * **这个值什么时候算**（配合上面的「值」一起看）。
	 *
	 * - `快照`（默认）= 施法/施加那一瞬算一次并**固定**。技能伤害、buff 强度都用它——
	 *   按下技能后中途被上 debuff，**这一发的值不变**。
	 * - `实时` = 每次读取**重新算**。适合引导型技能的持续伤害、会随战况浮动的系数这类
	 *   **不该被最初那一瞬定死**的值。
	 *
	 * **⚠ 当前生效范围（实测，MUST NOT 误以为全都能用）**：本列**今天只在技能侧的
	 * `FTcsNumericParamRow` 上真起作用**（`TcsCastOps_Snapshot.cpp` 按本列分流）。
	 * **状态 / buff 侧与布尔开关表上的本列目前零读取者** ⇒ 在那两处填 `实时`，
	 * 行为与 `快照` 相同、**且不会有任何提示**（缺口已登记，随各自轮次闭合）。
	 * **另外**：技能侧的「布尔开关表」本列同样零读取者。
	 *
	 * ---
	 *
	 * （实现口径：`Snapshot` 在快照构建写入点求值一次并转规范值；`Live` 不进快照数值面，
	 * 实时读取走账本求值通道——D5-12 v2 / D5-18 v2。）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Param")
	ETcsParamMode Mode = ETcsParamMode::EPM_Snapshot;

	/**
	 * **上面「值」按什么约定书写**（只作用于你**这里手写**的数，引用来的值不受影响）。
	 *
	 * 最常用 `百分比`：想写"减少 25%"，就填 `25` + 勾 `百分比`，引擎自己存成 `0.25`
	 * ——不必手工换算成小数。`一减` / `取负` 供"减少类"配置。默认 `无` = 写多少就是多少。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Param",
		Meta = (Bitmask, BitmaskEnum = "/Script/TcsNotation.ETcsValueConventionFlag"))
	ETcsValueConventionFlag ValueConvention = ETcsValueConventionFlag::VCF_None;

#pragma endregion
};
