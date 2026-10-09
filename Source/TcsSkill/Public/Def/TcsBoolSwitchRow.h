// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Def/TcsParamRow.h"
#include "TcsBoolSwitchRow.generated.h"



/**
 * 布尔开关行 —— **技能上的"是非"配置项**（开 / 关）。
 *
 * **与数值参数行的分工**：数值参数行存"一个数"（伤害、时长、倍率），本表存"一个开关"
 * （这个技能是否穿透、是否可暴击、是否是位移技）。两者**各自独立**，同一个键名不会互相兜底
 * ——查开关只会在本表里找，查数值只会在数值表里找。
 *
 * **开关有什么用**：宿主脚本 / 蓝图可以按它决定行为（"如果是位移技就走另一套逻辑"）；
 * 后续的条件门禁（`GateCheck`）也会按它决定某个效果生不生效。
 *
 * **填法**：`Key` = 开关名（与数值参数一样落 `TcsStateParam` 词根、由宿主声明）；
 * `Base` = 默认值（开 / 关）。
 *
 * ---
 *
 * **以下为实现与边界（策划可略）**：当前消费者归技能层，故类型住 `TcsSkill`；取值模式复用
 * `TcsState`，不另立同义枚举。`GateCheck` 与布尔修正器归 `R6.5`。
 *
 * **⚠ 本表两处当前边界（如实，MUST NOT 被读成"配了就都生效"）**：
 * ① 本表的 `Mode` 列（下表）**零读取者**——`IsSwitchSet` 只读 `Base`，配 `实时` 与配 `快照` 行为相同；
 * ② 布尔**修正器**（`FBoolSwitchModifier`）整体归 `R6.5-e`，今天没有"改开关"的机制，
 *    本表只提供初值。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsBoolSwitchRow
{
	GENERATED_BODY()

// 参数键
#pragma region Key

public:
	/**
	 * **这个开关叫什么**（与数值参数表**独立判重复**——同一名字可以既是开关又是数值，互不影响）。
	 * 落 `TcsStateParam` 词根、由宿主在 `Config/DefaultGameplayTags.ini` 声明。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bool Switch")
	FGameplayTag Key;

#pragma endregion


// 值与模式
#pragma region Value

public:
	/**
	 * **默认是开还是关**。
	 *
	 * 注意它与数值参数表的同名项是**两个独立开关**——宿主可以按行配置，两者不互相影响。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bool Switch")
	bool Base = false;

	/**
	 * **什么时候算这个开关的值**（含义同数值参数行的同名列）。
	 *
	 * **⚠ 本列在本表上今天不起作用**（零读取者，配 `实时` 与配 `快照` 行为相同、无任何提示）；
	 * 保留字段是为了与数值参数行同形，缺口登记在案、随布尔修正器那批（`R6.5-e`）一起闭合。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bool Switch")
	ETcsParamMode Mode = ETcsParamMode::EPM_Snapshot;

#pragma endregion
};
