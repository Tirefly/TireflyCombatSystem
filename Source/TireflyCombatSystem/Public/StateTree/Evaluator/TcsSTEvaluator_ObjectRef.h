// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeExecutionTypes.h"
#include "TcsSTEvaluator_ObjectRef.generated.h"



class AActor;
class AController;
class UTcsAttributeComponent;
class UTcsBuffComponent;
class UTcsSkillComponent;
class UTcsStateComponent;
class UTcsStateInstance;
struct FStateTreeExecutionContext;
struct FStateTreeLinker;



/**
 * TCS StateTree ObjectRef evaluator 的实例数据。
 *
 * 用途：把 `UTcsStateInstance` 的宿主 / 发起者及对应组件引用整理成一组可绑定的 StateTree Output 属性。
 */
USTRUCT()
struct FTcsSTEvaluator_ObjectRefInstanceData
{
	GENERATED_BODY()

	/** 状态实例拥有者 Actor。 */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Owner"))
	TWeakObjectPtr<AActor> Owner;

	/** 状态实例拥有者 Controller。 */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Owner Controller"))
	TWeakObjectPtr<AController> OwnerController;

	/** 状态实例拥有者的状态组件。 */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Owner State Component"))
	TWeakObjectPtr<UTcsStateComponent> OwnerStateComponent;

	/** 状态实例拥有者的 Buff 组件。 */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Owner Buff Component"))
	TWeakObjectPtr<UTcsBuffComponent> OwnerBuffComponent;

	/** 状态实例拥有者的属性组件。 */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Owner Attribute Component"))
	TWeakObjectPtr<UTcsAttributeComponent> OwnerAttributeComponent;

	/** 状态实例拥有者的技能组件。 */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Owner Skill Component"))
	TWeakObjectPtr<UTcsSkillComponent> OwnerSkillComponent;

	/** 状态实例发起者 Actor。 */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Instigator"))
	TWeakObjectPtr<AActor> Instigator;

	/** 状态实例发起者 Controller。 */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Instigator Controller"))
	TWeakObjectPtr<AController> InstigatorController;

	/** 状态实例发起者的状态组件。 */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Instigator State Component"))
	TWeakObjectPtr<UTcsStateComponent> InstigatorStateComponent;

	/** 状态实例发起者的 Buff 组件。 */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Instigator Buff Component"))
	TWeakObjectPtr<UTcsBuffComponent> InstigatorBuffComponent;

	/** 状态实例发起者的属性组件。 */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Instigator Attribute Component"))
	TWeakObjectPtr<UTcsAttributeComponent> InstigatorAttributeComponent;

	/** 状态实例发起者的技能组件。 */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Instigator Skill Component"))
	TWeakObjectPtr<UTcsSkillComponent> InstigatorSkillComponent;
};



/**
 * 为 TCS StateTree 提供整组 ObjectRef 输出的 evaluator。
 *
 * 当前主要用于 BuffStateTree：保持 `BuffInstance` 单根上下文不变，同时为绑定面板补齐 owner / instigator 相关运行时引用。
 */
USTRUCT(meta = (DisplayName = "TcsSTEvaluator_ObjectRef"))
struct TIREFLYCOMBATSYSTEM_API FTcsSTEvaluator_ObjectRef : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTcsSTEvaluator_ObjectRefInstanceData;

	/** @return 当前 evaluator 使用的实例数据类型。 */
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	/**
	 * 绑定共享执行态外部数据。
	 *
	 * @param Linker 当前 StateTree Linker
	 * @return 绑定成功时返回 true
	 */
	virtual bool Link(FStateTreeLinker& Linker) override;

	/**
	 * 启动 StateTree 时同步当前 ObjectRef 输出。
	 *
	 * @param Context 当前执行上下文
	 */
	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;

	/**
	 * 停止 StateTree 时清空桥接输出。
	 *
	 * @param Context 当前执行上下文
	 */
	virtual void TreeStop(FStateTreeExecutionContext& Context) const override;

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