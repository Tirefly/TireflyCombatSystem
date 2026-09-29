// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Handle/TcsCombatEntityHandle.h"

#include "TcsTargetSelectorHost.generated.h"



// 宿主选择器插槽（TcsTargeting 反射面；宿主用任意 UE 脚本语言实现——台账 SCRIPT-8）
UINTERFACE(MinimalAPI, Blueprintable)
class UTcsTargetSelectorHost : public UInterface
{
	GENERATED_BODY()
};

/**
 * 宿主目标选择器插槽（2026-09-24，台账 SCRIPT-8）：**让宿主用任意 UE 脚本语言（C# / AS / Luau / TS / 蓝图）
 * 实现目标选择，零 C++ 改动**——客制化、只服务宿主业务、不值得进插件的选择语义的正解。
 *
 * **为什么必须经本接口转发，而不是让脚本直接继承 `FTcsTargetSelectorStrategy`**：
 * 该基类走 **C++ 虚分派**（vtable），而脚本定义的 struct **没有 C++ 类型** ⇒ `CppStructOps == nullptr`
 * ⇒ vtable 指针位为 0 ⇒ 调用即**野函数指针**。**引擎层面无解**（不为脚本类型生成 C++ 代码，连补 shim
 * 的路都没有）。故 MUST 经两层：`FTcsSelHostDelegate`（USTRUCT 转发器，进虚分派体系）→ 本接口
 * （UObject 反射分派，脚本可达）。
 *
 * **分发底座是 UE 自己的反射系统**（`UFunction::Invoke`），**不是某个脚本语言专属**——
 * AngelScript / Luau / Puerts(TS) / 蓝图全部支持 ⇒ 本插槽**天然语言无关**，兑现 D4-17。
 *
 * **形参 MUST 全反射**（`BlueprintNativeEvent` 触发 UHT 蓝图参数校验，`UhtFunction.cs:859`/`:1043-1053`）
 * ⇒ 传**句柄**而非上下文 struct（上下文是非反射纯 C++ 类型；需要更多信息时经门面按句柄访问器取，
 * 见 `UTcsEffectSubsystem::GetRunTargets` 一组）。
 *
 * **与 `ITcsEntityQuery` 的分工**：那个是"读世界"（遍历/定位/存活——宿主既有能力，框架注入）；
 * 本接口是"选目标"（本次执行要打谁——**宿主业务语义**，框架只立契约）。
 */
class ITcsTargetSelectorHost
{
	GENERATED_BODY()

// 选择契约
#pragma region Resolve

public:
	/**
	 * 解析目标集。**只填充不清空**——调用方（`FTcsSelHostDelegate` 转发器）负责清空 `OutTargets`，
	 * 本方法 MUST NOT 假设其为空（语义同 `FTcsTargetSelectorStrategy::Resolve`，保持两条路径一致）。
	 *
	 * 降级：无合适目标时留空集即可（**不产无效句柄**——消费方按句柄判空）。
	 *
	 * @param Caster 施法者实体句柄。
	 * @param Instigator 发起者实体句柄（可与 Caster 不同）。
	 * @param OutTargets 输入输出目标集（`UPARAM(ref)` 使脚本绑定保留引用语义；实体句柄无生命周期语义，需要存活/定位时经宿主自身能力询问）。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Tcs|Targeting")
	void ResolveTargets(FTcsCombatEntityHandle Caster, FTcsCombatEntityHandle Instigator, UPARAM(ref) TArray<FTcsCombatEntityHandle>& OutTargets);

#pragma endregion
};
