// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"



/**
 * 流程黑板契约键的原生 Tag 全集（2026-09-22 用户拍板"标识体系迁移到 GameplayTag"）。
 *
 * **为什么契约键由框架原生声明**（判据 = "谁拥有那个词，谁声明"，与事件 tag 同款）：
 * 契约键是**步骤之间的接口**（`BaseDamage` 由 ⑥BaseDamage 步写、由 ⑨Execute 步与 ⑩Completed 步读）——
 * 属**框架词汇**。若让项目声明，项目漏配即导致上下游步骤对不上（**静默读到 0**），
 * 框架契约被项目配置破坏。故 MUST 插件原生声明（随模块加载生效、零项目配置依赖）。
 *
 * **命名**：`Tcs.Flow.Key.<KeyName>`（与事件 tag 的 `Tcs.Event.*` 并列，共用 `Tcs` 命名空间、域段隔离）；
 * 常量名 = tag 文本逐点换下划线（2026-09-21 约定）。
 *
 * **导出宏（2026-09-21 跨模块实证）**：`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**
 * （无 `__declspec(dllexport)`，`NativeGameplayTags.h:31`）——宿主/其他模块引用会**链接失败**
 * （实测 `LNK2001`）。这些键的设计意图正是"供宿主步骤/事件响应方读写"，故 MUST 带模块导出宏。
 *
 * **项目自定义键**：不受本头约束——由项目 `Config/DefaultGameplayTags.ini` 声明、自由命名，
 * 插件不预设、不校验其存在性（存在性校验归 M8 校验矩阵）。
 */
extern TCSDAMAGE_API FNativeGameplayTag Tag_Tcs_Flow_Key_BaseDamage;         // 伤害量唯一契约键（基值以 Add 落在它上面）
extern TCSDAMAGE_API FNativeGameplayTag Tag_Tcs_Flow_Key_Executed;           // 本次实际执行量（Override）
extern TCSDAMAGE_API FNativeGameplayTag Tag_Tcs_Flow_Key_Absorbed;           // 本次被吸收量（Override）
extern TCSDAMAGE_API FNativeGameplayTag Tag_Tcs_Flow_Key_Kill;               // 击杀记账（Override）
extern TCSDAMAGE_API FNativeGameplayTag Tag_Tcs_Flow_Key_Hit;                // 命中结果（Hit 步写）
extern TCSDAMAGE_API FNativeGameplayTag Tag_Tcs_Flow_Key_Crit;               // 暴击结果（Crit 步写）
extern TCSDAMAGE_API FNativeGameplayTag Tag_Tcs_Flow_Key_ExecuteCandidates;  // 免疫/减伤候选落点键（PreExecute ↔ Execute）
extern TCSDAMAGE_API FNativeGameplayTag Tag_Tcs_Flow_Key_HitRate;            // 命中率（宿主响应方可改写；Hit 步读回）
extern TCSDAMAGE_API FNativeGameplayTag Tag_Tcs_Flow_Key_CritRate;           // 暴击率（宿主响应方可改写；Crit 步读回）



/**
 * 官方默认流程模板的原生 Tag（**框架词汇**——插件自带、不提供就没得用）。
 * 其余模板 id 属**项目知识**（D7-5"流程阶段构成 = 项目知识"），由项目 ini 声明。
 */
extern TCSDAMAGE_API FNativeGameplayTag Tag_Tcs_Flow_Template_Default;
