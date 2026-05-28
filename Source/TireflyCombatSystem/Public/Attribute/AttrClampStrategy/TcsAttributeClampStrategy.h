// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StructUtils/InstancedStruct.h"
#include "Attribute/AttrClampStrategy/TcsAttributeClampContext.h"
#include "TcsAttributeClampStrategy.generated.h"



class UTcsAttributeDefinition;


/**
 * 属性 Clamp 策略抽象基类
 *
 * 定义属性值约束（Clamp）的统一接口，允许开发者通过继承实现自定义的约束逻辑。
 *
 * 默认行为：
 * - 插件提供 UTcsAttrClampStrategy_Linear 作为默认实现，使用线性约束（FMath::Clamp）
 * - 所有属性定义默认使用线性 Clamp 策略
 *
 * 自定义策略示例（C++）：
 * @code
 * UCLASS(Meta = (DisplayName = "属性 Clamp 策略：循环约束"))
 * class UTcsAttrClampStrategy_Wrap : public UTcsAttributeClampStrategy
 * {
 *     GENERATED_BODY()
 *
 * public:
 *     virtual float Clamp_Implementation(
 *         float Value,
 *         float MinValue,
 *         float MaxValue,
 *         const FTcsAttributeClampContextBase& Context,
 *         const FInstancedStruct& Config) override
 *     {
 *         float Range = MaxValue - MinValue;
 *         if (Range <= 0.f) return MinValue;
 *         return FMath::Fmod(Value - MinValue, Range) + MinValue;
 *     }
 * };
 * @endcode
 *
 * 自定义策略示例（蓝图）：
 * - 在蓝图编辑器中创建继承自 UTcsAttributeClampStrategy 的蓝图类
 * - 重写 Clamp 事件，实现自定义约束逻辑
 * - 在属性定义数据表中选择该蓝图类作为 ClampStrategyClass
 * - 可选：定义自定义配置结构体并在属性定义中设置 ClampStrategyConfig
 */
UCLASS(Abstract, BlueprintType, Blueprintable, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsAttributeClampStrategy : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 对属性值进行约束
	 *
	 * 开发者可以重写此方法以实现自定义约束逻辑：
	 * - 简单策略：只使用 Value、MinValue、MaxValue
	 * - 复杂策略：使用 Context 访问 Owner Actor、其他属性值等
	 * - 配置化策略：使用 Config 读取用户自定义配置
	 *
	 * @param Value 待约束的值
	 * @param MinValue 最小值（可能为 -Infinity，表示无下限）
	 * @param MaxValue 最大值（可能为 +Infinity，表示无上限）
	 * @param Context 固定的基础上下文（包含 AttributeComponent、Resolver 等）
	 * @param Config 可选的用户自定义配置（FInstancedStruct，可以是任意用户定义的结构体）
	 * @return 约束后的值
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "TireflyCombatSystem|Attribute")
	float Clamp(
		float Value,
		float MinValue,
		float MaxValue,
		UPARAM(ref) const FTcsAttributeClampContextBase& Context,
		UPARAM(ref) const FInstancedStruct& Config);

	virtual float Clamp_Implementation(
		float Value,
		float MinValue,
		float MaxValue,
		const FTcsAttributeClampContextBase& Context,
		const FInstancedStruct& Config)
	{
		// 默认实现：简单的线性 Clamp
		return FMath::Clamp(Value, MinValue, MaxValue);
	}

	/**
	 * 收集当前 Clamp 策略声明式依赖的其他属性。
	 *
	 * 这里声明的是“当前 Attribute 的 Clamp 结果依赖哪些其他属性”。
	 * AttributeComponent 会据此反向构建依赖图，用于脏属性驱动的局部传播。
	 *
	 * @param AttributeName 当前正在被 Clamp 的属性名
	 * @param AttributeDef 当前属性定义
	 * @param Config 当前策略配置
	 * @param OutAttributeNames 输出的依赖属性集合
	 * 返回 false 表示该策略没有声明可用于局部传播的完整依赖信息，调用方应回退到保守的全局传播路径。
	 * @return 是否提供了完整的声明式依赖信息
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "TireflyCombatSystem|Attribute")
	bool CollectDependentAttributes(
		FName AttributeName,
		const UTcsAttributeDefinition* AttributeDef,
		UPARAM(ref) const FInstancedStruct& Config,
		UPARAM(ref) TArray<FName>& OutAttributeNames) const;

	virtual bool CollectDependentAttributes_Implementation(
		FName AttributeName,
		const UTcsAttributeDefinition* AttributeDef,
		const FInstancedStruct& Config,
		TArray<FName>& OutAttributeNames) const;
};
