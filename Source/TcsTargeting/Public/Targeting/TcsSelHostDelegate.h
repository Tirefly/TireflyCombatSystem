// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ScriptInterface.h"

#include "Host/TcsTargetSelectorHost.h"
#include "Targeting/TcsTargetSelectorStrategy.h"

#include "TcsSelHostDelegate.generated.h"



/**
 * 宿主选择器**转发器**（2026-09-24，台账 S-8）：把 `ITcsTargetSelectorHost`（UObject 反射接口）
 * 接进 `FTcsTargetSelectorStrategy`（USTRUCT 虚分派体系）——**宿主用任意 UE 脚本语言写选择逻辑，
 * 零 C++ 改动**。
 *
 * **为什么需要转发器**：选择器基类走 C++ 虚分派，而脚本定义的 struct **没有 C++ 类型**
 * ⇒ `CppStructOps == nullptr` ⇒ vtable 指针位为 0 ⇒ 调用即**野函数指针**（引擎层面无解）。
 * 转发器是 USTRUCT（进虚分派体系），内部把调用**转发**到 UObject 接口（反射分派，脚本可达）。
 *
 * **配置面**：本类型可被编辑器 picker 选中（`FTcsTargetSelectorStrategy` 的 `BaseStruct` 限定内），
 * 宿主在其 `Host` 字段上指定脚本实现对象——配置体验与普通策略一致。
 *
 * **双轨并存**：框架默认选择器（`FTcsSelSelf`）与 C++ 策略子类走虚分派快路径、原样保留；
 * 本转发器只服务宿主扩展（非热路径，多一次 `UFunction::Invoke` 开销是接受的取舍）。
 */
USTRUCT(DisplayName = "宿主脚本选择器（转发）")
struct TCSTARGETING_API FTcsSelHostDelegate : public FTcsTargetSelectorStrategy
{
	GENERATED_BODY()

// 转发目标
#pragma region Host

public:
	// 宿主实现（脚本层对象；未配置时产出空集 + Warning——见 Resolve 实现）
	UPROPERTY(EditAnywhere, Category = "Tcs|Targeting")
	TScriptInterface<ITcsTargetSelectorHost> Host;

#pragma endregion


// 解析契约（纯转发）
#pragma region Resolve

public:
	/**
	 * 转发到 `Host->ResolveTargets`（**只填充不清空**——调用方负责清空，语义同基类契约）。
	 *
	 * 降级：`Host` 未配置 / 对象无效时**产出空集 + Warning**（不崩溃、不静默通过——
	 * 空集与"配好了但选不中"在链日志里可区分）。
	 */
	virtual void Resolve(const FTcsEffectContext& Context, ITcsEntityQuery* EntityQuery, TArray<FTcsCombatEntityHandle>& OutTargets) const override;

#pragma endregion
};
