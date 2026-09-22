// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "GameplayTagContainer.h"

#include "TcsAttributeProvider.generated.h"



// 属性读侧契约接口（TcsAttribute 反射面；宿主/适配器可实现以承接属性读取——02 §2.3）
UINTERFACE(MinimalAPI)
class UTcsAttributeProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * 属性读侧契约（M2 对外唯一契约，02 §2.3）：计算器不关心单位载体，宿主适配在此收敛——
 * 军官组件 / Mass 存储桶适配器各自绑定自己的单位，故签名不含单位参数（单位由实现者持有）。
 *
 * 本契约只读、零副作用；实现方在事务期的读值与预览语义（D2-5）见各方法说明。
 */
class ITcsAttributeProvider
{
	GENERATED_BODY()

// 读侧契约
#pragma region Query

public:
	/**
	 * 读取属性基础值（不受修正器影响）。
	 *
	 * @param Attribute 属性名。
	 * @return 返回基础值；属性未定义时返回 0。
	 */
	UFUNCTION(BlueprintNativeEvent)
	double GetBaseValue(FGameplayTag Attribute);

	/**
	 * 读取属性当前值（聚合 + 值域收口后的权威值）。
	 * 语义：脏则惰性重算（实现方与聚合管线共同保证）——事务期返回事务前旧值（D2-5）。
	 *
	 * @param Attribute 属性名。
	 * @return 返回当前值；属性未定义时返回 0。
	 */
	UFUNCTION(BlueprintNativeEvent)
	double GetCurrentValue(FGameplayTag Attribute);

	/**
	 * 读取属性待提交候选值（事务预览，D2-5；供表现层预览用）。
	 * 语义：无进行中的事务时等于当前值；候选值只读、不落账。
	 *
	 * @param Attribute 属性名。
	 * @return 返回候选值；无事务或属性未定义时返回当前值。
	 */
	UFUNCTION(BlueprintNativeEvent)
	double PeekPending(FGameplayTag Attribute);

#pragma endregion
};
