// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Handle/TcsCombatEntityHandle.h"

#include "TcsTargetScorerHost.generated.h"



// 宿主评分器插槽（TcsTargeting 反射面；宿主用任意 UE 脚本语言实现——P-A 排序相位）
UINTERFACE(MinimalAPI, Blueprintable)
class UTcsTargetScorerHost : public UInterface
{
	GENERATED_BODY()
};

/**
 * 宿主目标评分器插槽（P-A 排序相位）：**让宿主用任意 UE 脚本语言（C# / AS / Luau / TS / 蓝图）
 * 表达"谁更优"，零 C++ 改动**——"血量最低"这类**依赖宿主词汇**的排序语义的正解
 * （没有它，宿主只能去写 C++ 策略类型）。
 *
 * **为什么必须经本接口转发**：评分器基类走 **C++ 虚分派**（vtable），而脚本定义的 struct
 * **没有 C++ 类型** ⇒ `CppStructOps == nullptr` ⇒ vtable 指针位为 0 ⇒ 调用即**野函数指针**。
 * 引擎层面无解（不为脚本类型生成 C++ 代码）。故 MUST 经两层：
 * `FTcsScorerHostDelegate`（USTRUCT 转发器，进虚分派体系）→ 本接口（UObject 反射分派，脚本可达）。
 * 与选择器族 / 过滤器族同款理由（见 `TcsTargetSelectorHost.h`）。
 *
 * **性能纪律（SHOULD——写进规格与类型注释两处）**：评分器的调用频次是"**逐候选 × 逐排序项**"，
 * **高于过滤器**；而插槽走跨语言反射（`UFunction::Invoke`），**热路径上不便宜** ⇒
 * **排序评分优先用 C++ 策略，脚本插槽用于低频 / 非关键排序**。这是给宿主的判断依据，不是限制。
 *
 * **形参 MUST 全反射**（`BlueprintNativeEvent` 触发 UHT 蓝图参数校验，`UhtFunction.cs:859`/`:1043-1053`）
 * ⇒ 传**句柄**而非上下文 struct。
 */
class ITcsTargetScorerHost
{
	GENERATED_BODY()

// 评分契约
#pragma region Score

public:
	/**
	 * 求候选评分（**纯评分**——MUST NOT 改写候选或上下文）。
	 *
	 * **NaN 的语义 = 排除该候选**（与 C++ 评分器同一条浮点边界，不特殊处理）。
	 *
	 * @param Candidate 候选目标实体句柄。
	 * @param Caster 施法者实体句柄。
	 * @param Instigator 发起者实体句柄（可与 Caster 不同）。
	 * @return 返回评分（数值大小与方向的组合含义由排序项给定；NaN = 排除该候选）。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Tcs|Targeting")
	double ScoreTarget(FTcsCombatEntityHandle Candidate, FTcsCombatEntityHandle Caster, FTcsCombatEntityHandle Instigator);

#pragma endregion
};
