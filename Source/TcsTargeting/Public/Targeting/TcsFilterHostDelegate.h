// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ScriptInterface.h"

#include "Host/TcsTargetFilterHost.h"
#include "Targeting/TcsTargetFilterStrategy.h"

#include "TcsFilterHostDelegate.generated.h"



/**
 * 宿主过滤器**转发器**（2026-09-24，台账 SCRIPT-8）：把 `ITcsTargetFilterHost`（UObject 反射接口）
 * 接进 `FTcsTargetFilterStrategy`（USTRUCT 虚分派体系）——宿主用任意 UE 脚本语言表达
 * "存活/敌对/阵营"这类**宿主本体论**语义，零 C++ 改动。
 *
 * **为什么需要转发器**：同 `FTcsSelHostDelegate`——过滤器基类走 C++ 虚分派，脚本定义的 struct
 * 没有 C++ 类型（vtable 位为 0，调用即野函数指针），引擎层面无解。
 *
 * **组合语义**：AND 全过 + 短路（由 `SelectTargets` 执行器保证）——本转发器只回答单个候选。
 */
USTRUCT(DisplayName = "宿主脚本过滤器（转发）")
struct TCSTARGETING_API FTcsFilterHostDelegate : public FTcsTargetFilterStrategy
{
	GENERATED_BODY()

// 转发目标
#pragma region Host

public:
	// 宿主实现（脚本层对象；未配置时判"通过"——与基类中性默认实现同口径）
	UPROPERTY(EditAnywhere, Category = "Tcs|Targeting")
	TScriptInterface<ITcsTargetFilterHost> Host;

#pragma endregion


// 判定契约（纯转发）
#pragma region Pass

public:
	/**
	 * 转发到 `Host->PassTarget`（**纯判定**——MUST NOT 改写候选或上下文）。
	 *
	 * 降级：`Host` 未配置 / 对象无效时**判"通过"**——与基类中性默认实现同口径
	 * （"框架零默认 Filter"指不提供**有语义**的默认实现，而非把"未配置"变成"淘汰全部候选"；
	 * 后者会让配置遗漏表现成"打不到人"，比放过更难排查）。
	 */
	virtual bool Pass(FTcsCombatEntityHandle Candidate, const FTcsEffectContext& Context) const override;

#pragma endregion
};
