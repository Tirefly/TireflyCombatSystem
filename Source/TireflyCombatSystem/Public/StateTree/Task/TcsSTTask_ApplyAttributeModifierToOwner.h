// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "Attribute/TcsAttributeModifierApplication.h"
#include "TcsSTTask_ApplyAttributeModifierToOwner.generated.h"


class UTcsAttributeComponent;
class UTcsStateInstance;



// ApplyAttributeModifierToOwner Task 实例数据
USTRUCT()
struct FTcsSTTask_ApplyAttributeModifierToOwnerInstanceData
{
	GENERATED_BODY()

	// 要应用的 AttributeModifier Definition Id。
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName ModifierDefId = NAME_None;
};


/**
 * 在 State 激活时以 Ongoing 模式将 AttributeModifier 应用到 Owner。
 *
 * EnterState: 构造 Application Request → ApplyAttributeModifier。
 * ExitState: State 生命周期按 SourceHandle 自动清理 Ongoing 父实例。
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
