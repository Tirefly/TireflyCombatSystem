// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"



/**
 * 状态域参数表的**框架契约键**（等级根）。
 *
 * **为什么本键住 `TcsState`**（2026-10-09 用户提出后核实订正；判据三条，缺一都会误判）：
 *
 * ① **语义归属**：`Level` 的**本体论在本模块**——D3-11「Level 终定」的家是 `SPEC-02-states` §3.5，
 *    `LevelBase` / `MaxLevel` 两个字段声明在 `FTcsStateDefBase`（`Def/TcsStateDefBase.h:55/:59`），
 *    而技能 Def **继承**它（`FTcsSkillDefData : FTcsStateDefBase`）。⇒ **M5 没有自己的"等级定义"层**，
 *    它只是复用状态侧定的等级语义；
 * ② **依赖方向（决定性）**：`TcsSkill` 的 `Build.cs` 已 `PublicDependencyModuleNames` 含 `TcsState`
 *    ⇒ **声明在 TcsState 时 M5 消费零成本**；反过来若声明在 TcsSkill，则 TcsState 将来要消费
 *    （状态侧等级修正）就会**反向依赖 TcsSkill = 成环**；宿主若要拿状态侧要用的词也得依赖 TcsSkill
 *    ——那是错误的模块边界。"同一个词被多域消费"是**常规情形**（M2/M5/状态三域共用同一套五带即先例），
 *    故声明方 MUST 取**语义所有者 + 依赖上游**的那一侧；
 * ③ **声明方判据 = "谁解释那个词"**：`EffectiveLevel = clamp(0, LevelBase + Σ)` 这条公式写在
 *    D3-11 里、由等级域解释，宿主只是"往里挂修正器"。⇒ 按 `gameplay-tag-governance`
 *    「框架契约词 MUST 由插件模块原生声明」，它 MUST 由**拥有该语义的模块**声明。
 *
 * **为什么不归宿主 ini**（尽管 `TcsStateParam` 根的一般键归宿主——见该能力的根段注册表）：
 * 一般键是**内容词**（具体参数键，含义由宿主规定）；`Level` 是**框架自己解释的契约词**。
 * 下放给宿主配置，宿主漏配即 `Level` 修正**静默失效**（数值照旧进账本、等级永不动、零报错）
 * ——那正是"契约词不可下放"要防的静默破坏。同一根下"契约词 + 内容词"并存是既有约定，
 * 拆的只是**声明方**（`gameplay-tag-governance` 的「同一根共享」）。
 *
 * **复用面（本键的消费者，跨模块）**：M5 技能参数链的 `Level` 修正键（`FTcsNumericParamModifier.ParamKey`
 * 取本键时改生效等级）、装置读数（`TcsDev` 经本导出符号引用）。**状态侧参数链（若将来落地）复用同一枚词**。
 *
 * **导出宏（跨模块实证）**：`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**
 * （`NativeGameplayTags.h:31`，无 `__declspec(dllexport)`）⇒ 其它模块引用本变量会**链接失败**
 * （`LNK2001`）。本键的设计意图正是"供 M5 与本模块消费/宿主引用" ⇒ MUST 带模块导出宏。
 */
extern TCSSTATE_API FNativeGameplayTag Tag_TcsStateParam_Level;
