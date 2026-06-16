// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "Attribute/TcsAttributeModifier.h"
#include "TcsSTTask_ApplyAttributeModifierToOwner.generated.h"


class UTcsAttributeComponent;
class UTcsStateInstance;



// ApplyAttributeModifierToOwner Task 实例数据
USTRUCT()
struct FTcsSTTask_ApplyAttributeModifierToOwnerInstanceData
{
	GENERATED_BODY()

	// 要应用的 AttributeModifier Id
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName ModifierId;

	// Operand 到 StateParam 的绑定列表
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TArray<FTcsStateParamBinding> OperandBindings;
};


/**
 * 在 State 激活时将 AttributeModifier 应用到 Owner，操作数从 StateParam 动态绑定。
 *
 * EnterState: 创建 Modifier → 应用到 Owner 的 AttributeComponent → ApplyModifier。
 * ExitState: SourceHandle → RemoveModifiersBySourceHandle 自动清理。
 */
USTRUCT(meta = (DisplayName = "TcsSTTask_ApplyAttributeModifierToOwner"))
struct TIREFLYCOMBATSYSTEM_API FTcsSTTask_ApplyAttributeModifierToOwner : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTcsSTTask_ApplyAttributeModifierToOwnerInstanceData;

	FTcsSTTask_ApplyAttributeModifierToOwner();

	virtual bool Link(FStateTreeLinker& Linker) override;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
	                                       const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID,
	                             FStateTreeDataView InstanceDataView,
	                             const IStateTreeBindingLookup& BindingLookup,
	                             EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

protected:
	TStateTreeExternalDataHandle<UTcsStateInstance> StateInstanceHandle;
};
