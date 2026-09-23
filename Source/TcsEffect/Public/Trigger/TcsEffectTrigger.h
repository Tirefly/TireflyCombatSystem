// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "TcsEffectTrigger.generated.h"



/**
 * 执行闸（D4-1 第 ⑥ 字段）：网络姿态挂点——"这行在哪种权威环境下才允许触发"。
 *
 * **R4 只实现 `TEG_Always`**（本作单机，无联网）；其余取值随网络姿态轮（R0 §9 的网络姿态规定：
 * 服务器权威拓扑 / 客户端镜像只跑 Cue 类步骤）。枚举值按 Custom 逃逸位规约排布：
 * **值 0 = 默认**，新值一律**末尾追加**（不得插队——插队会让已存资产的序列化值改变含义）。
 */
UENUM()
enum class ETcsExecutionGate : uint8
{
	// 恒通过（R4 唯一实现——单机形态下执行闸恒开）
	TEG_Always = 0		UMETA(DisplayName = "恒通过"),

	// 仅权威侧（**未实现，留位**）：客户端镜像侧不触发本行
	TEG_AuthorityOnly = 1	UMETA(DisplayName = "仅权威侧（未实现）"),
};



/**
 * 触发定义（**定义侧 = 纯配置**，D4-1 终版 10 字段去掉一个）——"事件 → 效果链"的映射行数据。
 *
 * **分层（2026-09-23 用户拍板）**：定义与运行期**分两个类型**——
 * 本类型是**纯配置**（零运行期字段 → 可进 DA 资产、可内联进 SkillDef/BuffDef）；
 * 运行期形态 `FTcsEffectTriggerInstance` 组合持有一份本类型 + 运行期簿记。
 * **理由**：触发行的"来源"（级联退订锚点）是**运行期发号**的句柄（跨会话/跨机不同，
 * MUST NOT 进内容资产），与"能存进资产的配置"是两类字段，混在一个 struct 里会让
 * "这个 struct 能不能进资产"这个问题没有干净答案。
 *
 * 职责边界（04 §1）：TcsEffect 是"事件 → 触发行 → 效果链"的触发执行引擎；定义本身**零行为**——
 * 求值（匹配/四道门/起链）由**共享 Handler** 承载（裁决 2a：事件类型 → Handler CDO 执行，
 * 事件 struct 上不自绑 delegate）。
 *
 * **全部字段独立可空、零策略类**（D4-1 原文："各自独立可空，空 = 默认行为；没有策略类"）。
 * `Scope` / `HandlerClass` 已在 D4-1 v2 砍除（目标归属由 `SelectTargets` 步骤或上下文默认目标
 * 表达；求值器本身就是全部行的共享 Handler），**MUST NOT 复活**。
 */
USTRUCT(BlueprintType)
struct TCSEFFECT_API FTcsEffectTriggerDef
{
	GENERATED_BODY()

// 触发语义（D4-1 的字段）
#pragma region Trigger

public:
	// ① 订阅哪个事件（Tag 路由——裁决 2a）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tcs|Effect|Trigger")
	FGameplayTag EventTag;

	// ② 载荷预筛（廉价的字段匹配，**先于条件求值**）——R4 只存不裁（载荷类型尚无字段可筛）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger")
	FInstancedStruct EventPayloadFilter;

	/**
	 * ③ 门禁条件（不求值即不触发；未过按 ⑧ 决定是否静默）。
	 * 求值走**条件求值器注册表**（与步骤执行器同款——按类型分派；见 `TcsTriggerCondition.h`）。
	 */
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger")
	TArray<FInstancedStruct> Conditions;

	/**
	 * ④ 触发后执行的效果链（**引用已登记的链 id**；内联链后置）。
	 * 2026-09-23 用户拍板改名：原 `Effects` → `EffectChainId`（更直观——它就是一个链 id）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tcs|Effect|Trigger")
	FGameplayTag EffectChainId;

	/**
	 * ⑤ 同 Tag 多行的触发顺序：**大者先**。
	 * 与覆盖带 `OverridePriority`（大者胜）同向——避免系统内出现两套方向相反的"谁更强"约定。
	 * 同 Priority 时按**登记序**（确定性：遍历顺序 MUST NOT 依赖容器哈希序）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tcs|Effect|Trigger")
	int32 Priority = 0;

	// ⑥ 执行闸（网络姿态挂点；R4 只有 `TEG_Always` 有实现）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tcs|Effect|Trigger")
	ETcsExecutionGate ExecutionGate = ETcsExecutionGate::TEG_Always;

	// ⑦ 可打断哪些正在跑的链——**R4 只存不裁**（链打断语义尚无实现；裁决者出现时落地）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tcs|Effect|Trigger")
	int32 InterruptPriority = 0;

	// ⑧ 行级开关 Tag（运行时点灯控制整行；**全部点亮**才通过——点灯 API 属下一批）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger")
	TArray<FGameplayTag> GateTags;

	/**
	 * ⑨ 条件未过时是否静默。
	 * `true`（默认）= 静默跳过；`false` = 记一条 `LogTcsEffect` 的 **Verbose** 行
	 * （M8 Explain 面板的数据源——条件未过是**正常业务路径**，不是故障，故 MUST NOT 用
	 * Warning/Error 级别）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tcs|Effect|Trigger")
	bool bConditionMissIsSilent = true;

#pragma endregion


// 已删字段（留痕，防复活）
#pragma region Removed

// 2026-09-23 用户拍板删除：`Cues: TArray<FGameplayTag>`（D4-1 原第 ⑨ 字段，
// "CueId 引用列表，帧末通道"）——**TcsCue 模块整体未敲定**（`Source/TcsCue` 不存在，
// `07-module-presentation.md` 只是 v1 草案），留一个填了没有任何消费者的字段 =
// 给策划一个假控件（与同批删除的修正器族死字段 `Tag` 同款理由）。
// **TcsCue 落地时加回**（台账已记）。

#pragma endregion
};
