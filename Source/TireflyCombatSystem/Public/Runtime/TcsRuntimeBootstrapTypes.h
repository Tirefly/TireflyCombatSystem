// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TcsRuntimeBootstrapTypes.generated.h"



class AActor;



// Entity 完成 runtime-ready 时触发的一次性回调。
DECLARE_DYNAMIC_DELEGATE_OneParam(
	FTcsOnEntityReadyDynamicDelegate,
	AActor*, Entity);



/**
 * `RegisterEntity` 的返回结果。
 */
UENUM(BlueprintType)
enum class ETcsRegisterEntityResult : uint8
{
	/** 当前实体已经满足完整 ready 条件。 */
	ReadyNow,

	/** 当前实体已登记为 waiting，后续由 bootstrap 继续重评估。 */
	RegisteredWaiting,

	/** 当前实体已经在 waiting 集合中，拒绝重复登记。 */
	AlreadyWaiting,

	/** 输入 Actor 非法，或不满足基础注册前提。 */
	InvalidEntity,
};



/**
 * 实体当前的 runtime 状态主结论。
 */
UENUM(BlueprintType)
enum class ETcsEntityRuntimeState : uint8
{
	/** 当前实体已满足完整 runtime-ready 条件。 */
	Ready,

	/** 当前实体仍在等待某个前置条件成立。 */
	Waiting,

	/** 当前输入非法，无法进入 runtime 编排。 */
	Invalid,
};



/**
 * 实体当前被阻塞的原因。
 */
UENUM(BlueprintType)
enum class ETcsEntityRuntimeBlockReason : uint8
{
	/** 当前没有阻塞原因。 */
	None,

	/** 实体尚未通过 `RegisterEntity` 显式纳入 TCS runtime bootstrap。 */
	NotRegistered,

	/** 输入 Actor 非法。 */
	InvalidEntity,

	/** Actor 没有实现 `ITcsEntityInterface`。 */
	MissingEntityInterface,

	/** AttributeManagerSubsystem 尚未 ready。 */
	AttributeManagerNotReady,

	/** StateManagerSubsystem 尚未 ready。 */
	StateManagerNotReady,

	/** 实体缺少 AttributeComponent。 */
	MissingAttributeComponent,

	/** 实体缺少 StateComponent。 */
	MissingStateComponent,

	/** AttributeComponent 尚未满足完整 runtime-ready 条件。 */
	AttributeComponentNotReady,

	/** StateComponent 尚未满足完整 runtime-ready 条件。 */
	StateComponentNotReady,

	/** BuffComponent 尚未满足完整 runtime-ready 条件。 */
	BuffComponentNotReady,

	/** SkillComponent 尚未满足完整 runtime-ready 条件。 */
	SkillComponentNotReady,
};



/**
 * 实体 runtime 状态检测结果。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsEntityRuntimeStateResult
{
	GENERATED_BODY()

	/** 当前实体的 runtime 状态主结论。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuntimeBootstrap")
	ETcsEntityRuntimeState State = ETcsEntityRuntimeState::Invalid;

	/** 当前实体的阻塞原因；若已 ready 则为 `None`。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuntimeBootstrap")
	ETcsEntityRuntimeBlockReason BlockReason = ETcsEntityRuntimeBlockReason::InvalidEntity;
};
