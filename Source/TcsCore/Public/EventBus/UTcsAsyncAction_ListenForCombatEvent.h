// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "StructUtils/InstancedStruct.h"

#include "EventBus/UTcsEventBusSubsystem.h"

#include "UTcsAsyncAction_ListenForCombatEvent.generated.h"



// Tag 匹配方式（枚举值前缀 EMT_ 是 EventMatchType 的缩写）
UENUM(BlueprintType)
enum class ETcsEventMatchType : uint8
{
	EMT_Exact = 0	UMETA(DisplayName = "精确", ToolTip = "仅当事件 Tag 与过滤 Tag 完全相等时触发（默认，值 0）"),
	EMT_Partial = 1	UMETA(DisplayName = "部分", ToolTip = "事件 Tag 含过滤 Tag（含相等）即触发——按 Tag 层级匹配"),
};



/**
 * 战斗事件监听节点（A' 反射面 / BP/CS 订阅入口，D0-2 的"绑定即过滤"）：绑定门面全量事件多播，
 * 在回调内按 Tag 匹配 + 载荷类型匹配过滤，命中才广播自身 OnEvent（子集流）。
 * 空 Tag 过滤 = 不限 Tag；空载荷类型 = 不限类型。
 */
UCLASS()
class TCSCORE_API UTcsAsyncAction_ListenForCombatEvent : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

// 工厂
#pragma region Factory

public:
	/**
	 * 监听战斗事件。
	 *
	 * @param WorldContextObject 世界上下文对象。
	 * @param TagFilter 事件 Tag 过滤（空 = 不限 Tag）。
	 * @param PayloadType 载荷类型过滤（可空 = 不限类型；非空时要求载荷结构为其自身或子类）。
	 * @param MatchType Tag 匹配方式（精确/部分）。
	 * @return 返回监听节点对象（未激活）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Tcs|Core|EventBus",
		Meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AdvancedDisplay = "MatchType"))
	static UTcsAsyncAction_ListenForCombatEvent* ListenForCombatEvent(
		UObject* WorldContextObject, FGameplayTag TagFilter, UScriptStruct* PayloadType = nullptr,
		ETcsEventMatchType MatchType = ETcsEventMatchType::EMT_Exact);

#pragma endregion


// 生命期
#pragma region Lifetime

public:
	// 激活：绑定门面全量事件多播（重复激活先解绑再绑定）
	virtual void Activate() override;

	// 销毁：退订门面多播（防僵尸绑定条目累积）
	virtual void BeginDestroy() override;

#pragma endregion


// 输出
#pragma region Output

public:
	// 命中过滤的事件流（子集流）
	UPROPERTY(BlueprintAssignable, Category = "Tcs|Core|EventBus")
	FTcsOnCombatEvent OnEvent;

#pragma endregion


// 内部
#pragma region Internal

private:
	// 命中过滤判定（Tag 匹配 + 载荷类型匹配）
	bool MatchesFilter(const FGameplayTag& EventTag, const FInstancedStruct& Payload) const;

	// 门面事件回调（动态多播绑定入口；签名须与 FTcsOnCombatEvent 一致）
	UFUNCTION()
	void HandleBusEvent(FGameplayTag EventTag, FInstancedStruct Payload);

private:
	// 监听目标子系统（弱引用）
	TWeakObjectPtr<UTcsEventBusSubsystem> BusSubsystem;

	// 监听世界（弱引用——Activate 时解析子系统）
	TWeakObjectPtr<UWorld> World;

	// Tag 过滤条件（空 = 不限）
	FGameplayTag TagFilter;

	// 载荷类型过滤（空 = 不限）
	UPROPERTY()
	TObjectPtr<UScriptStruct> PayloadType = nullptr;

	// Tag 匹配方式
	ETcsEventMatchType MatchType = ETcsEventMatchType::EMT_Exact;

#pragma endregion
};
