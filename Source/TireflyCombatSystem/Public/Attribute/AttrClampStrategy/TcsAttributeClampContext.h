// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TcsAttributeClampContext.generated.h"

class UTcsAttributeComponent;
class UTcsAttributeManagerSubsystem;
struct FTcsAttributeDefinition;
struct FTcsAttributeInstance;


/**
 * 属性 Clamp 上下文（固定结构）
 *
 * 提供 Clamp 策略执行时需要的基础上下文信息。
 * 此结构体是固定的，不需要用户继承或扩展。
 *
 * 字段说明：
 * - AttributeComponent: 属性组件，可以通过 GetOwner() 获取 Owner Actor
 * - AttributeName: 当前正在 Clamp 的属性名称
 * - AttributeDef: 属性定义（只读，C++ 访问）
 * - AttributeInst: 属性实例（只读，C++ 访问）
 * - Resolver: 值解析器，用于读取工作集中的其他属性值（C++ 访问）
 *
 * 蓝图访问：
 * - 使用 UTcsAttributeClampContextLibrary 提供的辅助函数访问上下文信息
 * - 例如：GetOwnerActor、GetOtherAttributeValue、OwnerHasTag 等
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeClampContextBase
{
	GENERATED_BODY()

	// 属性组件（包含 Owner Actor）
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	TObjectPtr<UTcsAttributeComponent> AttributeComponent = nullptr;

	// 属性名称
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	FName AttributeName = NAME_None;

	// 属性定义（只读指针，不序列化，不暴露给蓝图）
	// 在 C++ 中使用，蓝图中通过辅助库函数访问
	const FTcsAttributeDefinition* AttributeDef = nullptr;

	// 属性实例（只读指针，不序列化，不暴露给蓝图）
	// 在 C++ 中使用，蓝图中通过辅助库函数访问
	const FTcsAttributeInstance* AttributeInst = nullptr;

	// 工作集（用于两段式 Clamp 时读取其他属性的临时值）
	// 在 C++ 中使用，蓝图中通过辅助库函数 GetOtherAttributeValue 访问
	const TMap<FName, float>* WorkingValues = nullptr;

	// 默认构造函数
	FTcsAttributeClampContextBase() = default;

	// 便捷构造函数
	FTcsAttributeClampContextBase(
		UTcsAttributeComponent* InComponent,
		const FName& InAttributeName,
		const FTcsAttributeDefinition* InAttributeDef,
		const FTcsAttributeInstance* InAttributeInst,
		const TMap<FName, float>* InWorkingValues = nullptr)
		: AttributeComponent(InComponent)
		, AttributeName(InAttributeName)
		, AttributeDef(InAttributeDef)
		, AttributeInst(InAttributeInst)
		, WorkingValues(InWorkingValues)
	{}
};
