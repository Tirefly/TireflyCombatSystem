// Copyright Tirefly. All Rights Reserved.

#include "StateTree/Evaluator/TcsSTEvaluator_ObjectRef.h"

#include "State/TcsStateInstance.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"



namespace
{
	/**
	 * 将共享执行态上的 ObjectRef getter 结果同步到 evaluator 输出。
	 *
	 * @param InstanceData 当前 evaluator 实例数据
	 * @param StateInstance 当前共享执行态实例
	 */
	void SyncOutputs(
		FTcsSTEvaluator_ObjectRefInstanceData& InstanceData,
		const UTcsStateInstance& StateInstance)
	{
		InstanceData.Owner = StateInstance.GetOwner();
		InstanceData.OwnerController = StateInstance.GetOwnerController();
		InstanceData.OwnerStateComponent = StateInstance.GetOwnerStateComponent();
		InstanceData.OwnerBuffComponent = StateInstance.GetOwnerBuffComponent();
		InstanceData.OwnerAttributeComponent = StateInstance.GetOwnerAttributeComponent();
		InstanceData.OwnerSkillComponent = StateInstance.GetOwnerSkillComponent();

		InstanceData.Instigator = StateInstance.GetInstigator();
		InstanceData.InstigatorController = StateInstance.GetInstigatorController();
		InstanceData.InstigatorStateComponent = StateInstance.GetInstigatorStateComponent();
		InstanceData.InstigatorBuffComponent = StateInstance.GetInstigatorBuffComponent();
		InstanceData.InstigatorAttributeComponent = StateInstance.GetInstigatorAttributeComponent();
		InstanceData.InstigatorSkillComponent = StateInstance.GetInstigatorSkillComponent();
	}

	/**
	 * 清空 evaluator 的全部输出。
	 *
	 * @param InstanceData 当前 evaluator 实例数据
	 */
	void ResetOutputs(FTcsSTEvaluator_ObjectRefInstanceData& InstanceData)
	{
		InstanceData = FTcsSTEvaluator_ObjectRefInstanceData();
	}
}



bool FTcsSTEvaluator_ObjectRef::Link(FStateTreeLinker& Linker)
{
	const bool bResult = Super::Link(Linker);
	Linker.LinkExternalData(StateInstanceHandle);
	return bResult;
}

void FTcsSTEvaluator_ObjectRef::TreeStart(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	const UTcsStateInstance& StateInstance = Context.GetExternalData(StateInstanceHandle);
	SyncOutputs(InstanceData, StateInstance);
}

void FTcsSTEvaluator_ObjectRef::TreeStop(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	ResetOutputs(InstanceData);
}

#if WITH_EDITOR
FText FTcsSTEvaluator_ObjectRef::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Expose StateInstance owner/instigator object refs as bindable outputs"));
}
#endif