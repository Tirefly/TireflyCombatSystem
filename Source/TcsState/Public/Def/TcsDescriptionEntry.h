// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"

#include "TcsDescriptionEntry.generated.h"



/**
 * 描述视图槽位（D5-17 v3）：`SlotName` 对应 StringTable 文案里的占位符名——**文案零语法**。
 *
 * **本轮 `View` 是普通 `FInstancedStruct`**（不带 `meta = (BaseStruct = ...)`）：视图策略基类
 * `FTcsParamView` 与四个内置视图（`Value` / `Series` / `Range` / `Attribute`）住 TcsNotation、
 * 归 R8 落地。基类不存在时 MUST NOT 用 Kind 枚举之类的替代物假装有类型约束（那是旧 v2 形态的复活）；
 * 基类落地时回补 `meta`，类型限制届时由它收紧。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsDescriptionViewSlot
{
	GENERATED_BODY()

// 槽
#pragma region Slot

public:
	// 槽名（= 文案里占位符的名字，如 `"对目标造成 {Rate} 的火焰伤害"` 的 `Rate`；展示侧命名标签，非内容引用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Description")
	FName SlotName;

	// 视图载荷（本轮无类型约束——视图策略族归 R8，见类型注释）
	UPROPERTY(EditAnywhere, Category = "Description")
	FInstancedStruct View;

#pragma endregion
};



/**
 * 描述配置条目（D5-17 v3）：多描述入口（`"Tip"` / `"Codex"` / `"LevelUpPreview"`…）各有独立文本与视图槽位，
 * 支撑"简单描述 = 当前档单值、复杂描述 = 全表 + 高亮"的多对多（`FTcsStateDefBase.Descriptions` 的元素类型）。
 *
 * **三个 `FName` 是 2026-09-22 tag 化改造的明确例外**（不是漏网）：`TextKey` 是 StringTable 键
 * （`FText::FromStringTable` 的引擎约束就是 `FName`）；`DescriptionId` / `SlotName` 是展示侧命名标签，
 * 不指向 Def / 参数键 / 黑板键，不参与任何内容引用与解析。
 *
 * **住 TcsState**：`DEC-02-fold-display` 的命名批"住哪"表明写"描述视图槽位 / 配置条目 → TcsState
 * （Def 形状所在）"；TcsNotation 只拥有**词汇约定**与视图机制（`SPEC-07-notation` §1 边界）。
 *
 * **本轮零真实消费者**（渲染归 R8、技能面板归 R6）：只落字段与作者期校验（规则见 `state-def-asset` 能力）。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsDescriptionEntry
{
	GENERATED_BODY()

// 身份
#pragma region Identity

public:
	// 描述入口 id（`"Tip"` / `"Codex"` / `"LevelUpPreview"`…；展示侧标签，不参与解析）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Description")
	FName DescriptionId;

	// StringTable 键（文案原文只含槽名占位符）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Description")
	FName TextKey;

#pragma endregion


// 视图槽位
#pragma region Views

public:
	// 视图槽位列表（逐个对应文案里的占位符）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Description")
	TArray<FTcsDescriptionViewSlot> Views;

#pragma endregion
};
