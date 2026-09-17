// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Object.h"
#include "TcsEventHandler.generated.h"



/**
 * 共享事件处理器基类（裁决 2a）：事件类型 → Handler CDO 执行，事件 struct 上不自绑 delegate。
 * 事件结构保持纯数据，行为集中在 Handler：
 * - C++ 侧：派生 UClass 覆写 HandleEvent_Implementation（无实例状态，通常传 CDO——共享 Handler）；
 * - BP/CS 侧：BlueprintNativeEvent 允许蓝图子类覆写同签名（A' 反射面）。
 * 事件 Tag 随载荷一并传入（2026-09-11 用户拍板；命名 EventTag——原 ActualTag 改名，更符合直觉）：
 * 同一 Handler 可订阅多个 Tag 而不失真；与 A' 动态多播 FTcsOnCombatEvent 的 (EventTag, Payload) 形状对齐。
 */
UCLASS(Abstract, Blueprintable)
class TCSCORE_API UTcsEventHandler : public UObject
{
	GENERATED_BODY()

// 处理入口
#pragma region Handle

public:
	/**
	 * 事件处理入口（共享 Handler：事件类型 → Handler CDO 执行）。
	 *
	 * @param EventTag 事件 Tag（命中订阅时发布方传入）。
	 * @param Payload 事件载荷（核心词汇事件 = 具体 FStruct 包入 FInstancedStruct）。
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Tcs|Core|EventBus")
	void HandleEvent(FGameplayTag EventTag, const FInstancedStruct& Payload);
	virtual void HandleEvent_Implementation(FGameplayTag EventTag, const FInstancedStruct& Payload);

#pragma endregion
};
