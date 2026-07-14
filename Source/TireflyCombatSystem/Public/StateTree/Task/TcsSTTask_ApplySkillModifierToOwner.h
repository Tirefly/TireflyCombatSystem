// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "Skill/TcsSkillModifierRuntime.h"
#include "TcsSTTask_ApplySkillModifierToOwner.generated.h"



class UTcsStateInstance;



/**
 * ApplySkillModifierToOwner Task 实例数据。
 *
 * 负责声明要通过 Owner 的 `UTcsSkillComponent` 应用的 SkillModifier 列表。
 */
USTRUCT()
struct FTcsSTTask_ApplySkillModifierToOwnerInstanceData
{
	GENERATED_BODY()

	/** 要应用的 SkillModifierDefId 列表。 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TArray<FName> SkillModifierDefIds;

	/** 本次应用成功后返回的运行时账本记录。 */
	UPROPERTY(EditAnywhere, Category = "Output")
	TArray<FTcsSkillModifierRuntimeEntry> AppliedRuntimeEntries;
};



/**
 * 在 State 激活时将 SkillModifier 应用到 Owner 的 SkillComponent。
 *
 * EnterState: 复用 `UTcsSkillComponent::ApplySkillModifiersWithSourceHandle()`，
 * 并把当前 `StateInstance` 的 `SourceHandle` 作为统一生命周期锚点传入。
 * ExitState: 不直接处理；后续移除统一走 `SourceHandle` 生命周期清理链。
 */
USTRUCT(meta = (DisplayName = "TcsSTTask_ApplySkillModifierToOwner"))
struct TIREFLYCOMBATSYSTEM_API FTcsSTTask_ApplySkillModifierToOwner : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTcsSTTask_ApplySkillModifierToOwnerInstanceData;

	/** 构造默认的 SkillModifier Apply Task。 */
	FTcsSTTask_ApplySkillModifierToOwner();

	/**
	 * 绑定共享执行态外部数据。
	 *
	 * @param Linker 当前 StateTree Linker
	 * @return 当父类绑定成功时返回 true
	 */
	virtual bool Link(FStateTreeLinker& Linker) override;

	/** @return 当前任务使用的实例数据类型。 */
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	/**
	 * 进入节点时把 SkillModifier 批量应用到 Owner 的 SkillComponent。
	 *
	 * @param Context 当前 StateTree 执行上下文
	 * @param Transition 当前状态切换结果
	 * @return 成功应用后返回 Running，否则返回 Failed
	 */
	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	/**
	 * 获取编辑器中显示的节点描述。
	 *
	 * @param ID 当前节点 ID
	 * @param InstanceDataView 当前实例数据视图
	 * @param BindingLookup 当前绑定查询器
	 * @param Formatting 当前节点格式化模式
	 * @return 节点描述文本
	 */
	virtual FText GetDescription(
		const FGuid& ID,
		FStateTreeDataView InstanceDataView,
		const IStateTreeBindingLookup& BindingLookup,
		EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

protected:
	/** 当前树宿主使用的共享执行态外部数据句柄。 */
	TStateTreeExternalDataHandle<UTcsStateInstance> StateInstanceHandle;
};
