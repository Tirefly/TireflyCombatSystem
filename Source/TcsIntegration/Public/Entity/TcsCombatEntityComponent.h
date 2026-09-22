// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Attribute/TcsAttrModInstance.h"
#include "GameplayTagContainer.h"
#include "Chain/TcsChainRun.h"
#include "Handle/TcsCombatEntityHandle.h"

#include "TcsCombatEntityComponent.generated.h"



class UTcsPieEntityQuery;



/**
 * 战斗实体组件（D6-1/D6-2 终定）——**恰好三职责**，适配器零逻辑（06 §5）：
 *
 * 1. **身份锚**：`BeginPlay` 把所属 Actor 注册为战斗实体（`UTcsAttributeSubsystem::RegisterUnit`
 *    发放句柄）并记录自己的句柄；`EndPlay` 反向注销。挂组件 = 策划声明"本 Actor 是战斗单位"。
 *    **门禁**：注册前检查定义就绪（`UTcsDefinitionSubsystem::IsRuntimeReady`）——未就绪留 Warning
 *    并跳过（时序是宿主责任，不 ensure 刷屏；06 §4"就绪门禁 = IsRuntimeReady 全量判定"）。
 * 2. **查询门面**：`GetCurrent` 转发属性门面求值；`ApplyModifier` / `RemoveBySource` 供宿主与
 *    调试施加/撤销（R3 无 AttributeSet——D2-15 属 R7；属性由测试装置直接添加）。
 * 3. **手动触发 API**：`ExecuteChainById(FName)`——组 `FTcsEffectContext`（`Caster`/`Targets` = 自身
 *    句柄）→ `UTcsEffectSubsystem::ExecuteChain`。R3 无触发行，手动触发是竖切入口。
 *
 * **MUST NOT 越出三职责**：组件不是状态机、不持策略对象、不做表现（Cue/指示器归 M7/宿主）；
 * **MUST NOT 认识具体链/步骤类型**（链 id 由调用方给）。
 *
 * **不自 TickComponent**（D6-5：M0 泵唯一驱动）。
 *
 * 与未来 Mass 的关系（D3-1）：本组件只是**军官侧适配器**——战斗核心（注册表/管线/解释器/流程）
 * 只认 `FTcsCombatEntityHandle`，不认识 Actor；Mass 小兵经桶适配器实现同一契约，核心零改动（台账 T-2）。
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class TCSINTEGRATION_API UTcsCombatEntityComponent : public UActorComponent
{
	GENERATED_BODY()

// 生命期
#pragma region Lifetime

public:
	// 注册：门禁检查 → RegisterUnit 发放句柄 → 登记进实体查询映射（未就绪则 Warning + 跳过）
	virtual void BeginPlay() override;

	// 注销：从实体查询映射移除 → 反向注销单位（句柄随之失效）
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion


// 身份
#pragma region Identity

public:
	/**
	 * 本组件所属实体的句柄（未注册时无效——`IsValid()` 为 false）。
	 *
	 * @return 返回实体句柄。
	 */
	FTcsCombatEntityHandle GetEntityHandle() const
	{
		return EntityHandle;
	}

	// 是否已注册（句柄有效即已注册）
	bool IsRegistered() const
	{
		return EntityHandle.IsValid();
	}

private:
	// 本实体的身份句柄（注册期发放；注销后失效）
	FTcsCombatEntityHandle EntityHandle;

	// 实体查询实现（映射登记用；门面注入点取得——未注入时为弱引用失效态）
	TWeakObjectPtr<UTcsPieEntityQuery> ResolveEntityQuery() const;

#pragma endregion


// 职责 2：查询门面
#pragma region Facade

public:
	/**
	 * 读本实体某属性的当前值（转发属性门面求值；未注册或属性未定义返回 0）。
	 *
	 * @param Attribute 属性名。
	 * @return 返回当前值。
	 */
	double GetCurrent(const FGameplayTag& Attribute) const;

	/**
	 * 施加修正器（宿主/调试入口；R3 无 AttributeSet——D2-15 属 R7）。
	 *
	 * @param Modifier 修正器（Target 指向目标属性）。
	 * @return 返回是否挂载成功。
	 */
	bool ApplyModifier(const FTcsAttrModInstance& Modifier) const;

	/**
	 * 按来源级联摘除（宿主/调试入口）。
	 *
	 * @param Source 来源句柄。
	 * @return 返回摘除条数。
	 */
	int32 RemoveBySource(const FTcsSourceHandle& Source) const;

#pragma endregion


// 职责 3：手动触发
#pragma region Trigger

public:
	/**
	 * 手动触发一条已登记的链（R3 竖切入口——无触发行）。
	 * 上下文装配：`Caster` = `Instigator` = 自身句柄、`Targets` = 仅自身
	 * （D4-4 v2 的"默认目标 = 事件目标"在无事件时由调用方预填；R3 无 `FTcsSelSelf` 选择器）。
	 * 未注册的链 id → Error 日志 + 无效句柄（不崩溃）；全即时链在返回前已走完，句柄活性由
	 * `UTcsEffectSubsystem::IsRunActive` 判定。
	 *
	 * @param ChainId 链 id（调用方给出——组件不认识具体链）。
	 * @return 返回运行态句柄（仅在链未走完时有效）。
	 */
	FTcsChainRunHandle ExecuteChainById(FGameplayTag ChainId) const;

#pragma endregion
};
