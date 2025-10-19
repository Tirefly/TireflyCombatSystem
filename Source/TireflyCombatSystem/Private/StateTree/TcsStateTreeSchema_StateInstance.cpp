// Copyright Tirefly. All Rights Reserved.

#include "StateTree/TcsStateTreeSchema_StateInstance.h"

#include "StateTreeConditionBase.h"
#include "StateTreeConsiderationBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreeTaskBase.h"
#include "TcsEntityInterface.h"
#include "TcsLogChannels.h"
#include "State/TcsState.h"
#include "State/TcsStateComponent.h"
#include "Attribute/TcsAttributeComponent.h"
#include "Blueprint/StateTreeNodeBlueprintBase.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Skill/TcsSkillComponent.h"


UTcsStateTreeSchema_StateInstance::UTcsStateTreeSchema_StateInstance()
{
	// 定义Schema保证的上下文数据
	
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		TcsStateTreeContextName::Owner,
		AActor::StaticClass(),
		FGuid::NewGuid()
	));
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		TcsStateTreeContextName::Instigator, 
		AActor::StaticClass(),
		FGuid::NewGuid()
	));
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		TcsStateTreeContextName::StateInstance,
		UTcsStateInstance::StaticClass(),
		FGuid::NewGuid()
	));

	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		TcsStateTreeContextName::OwnerController,
		AController::StaticClass(),
		FGuid::NewGuid()
	));
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		TcsStateTreeContextName::OwnerStateCmp,
		UTcsStateComponent::StaticClass(),
		FGuid::NewGuid()
	));
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		TcsStateTreeContextName::OwnerAttributeCmp,
		UTcsAttributeComponent::StaticClass(), 
		FGuid::NewGuid()
	));
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		TcsStateTreeContextName::OwnerSkillCmp,
		UTcsSkillComponent::StaticClass(), 
		FGuid::NewGuid()
	));

	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		TcsStateTreeContextName::InstigatorController,
		AController::StaticClass(),
		FGuid::NewGuid()
	));
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		TcsStateTreeContextName::InstigatorStateCmp,
		UTcsStateComponent::StaticClass(),
		FGuid::NewGuid()
	));
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		TcsStateTreeContextName::InstigatorAttributeCmp,
		UTcsAttributeComponent::StaticClass(), 
		FGuid::NewGuid()
	));
	ContextDataDescs.Add(FStateTreeExternalDataDesc(
		TcsStateTreeContextName::InstigatorSkillCmp,
		UTcsSkillComponent::StaticClass(), 
		FGuid::NewGuid()
	));
}

TConstArrayView<FStateTreeExternalDataDesc> UTcsStateTreeSchema_StateInstance::GetContextDataDescs() const
{
	return ContextDataDescs;
}

bool UTcsStateTreeSchema_StateInstance::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct->IsChildOf(FStateTreeConditionCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeEvaluatorCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeTaskCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreeConsiderationCommonBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FStateTreePropertyFunctionCommonBase::StaticStruct());
}

bool UTcsStateTreeSchema_StateInstance::IsClassAllowed(const UClass* InClass) const
{
	return InClass && InClass->IsChildOf<UStateTreeNodeBlueprintBase>();
}

bool UTcsStateTreeSchema_StateInstance::IsExternalItemAllowed(const UStruct& InStruct) const
{
	// 允许Actor、组件和TCS相关类作为外部数据
	return InStruct.IsChildOf(AActor::StaticClass())
		|| InStruct.IsChildOf(UActorComponent::StaticClass())
		|| InStruct.IsChildOf(UWorldSubsystem::StaticClass())
		|| InStruct.IsChildOf(UGameInstance::StaticClass())
		|| InStruct.IsChildOf(UTcsStateInstance::StaticClass());
}

bool UTcsStateTreeSchema_StateInstance::SetContextRequirements(
	UTcsStateInstance& StateInstance,
	FStateTreeExecutionContext& Context,
	bool bLogErrors)
{
	if (!Context.IsValid())
	{
		return false;
	}

	FTcsContextDataSetter ContextDataSetter = FTcsContextDataSetter(&StateInstance, Context);
	ContextDataSetter.GetSchema()->SetContextData(ContextDataSetter, bLogErrors);

	bool bResult = Context.AreContextDataViewsValid();
	if (!bResult && bLogErrors)
	{
		UE_LOG(LogTcsState, Error, TEXT("%s Missing external data requirements. StateTree will not update."), *FString(__FUNCTION__));
	}

	return bResult;
}

bool UTcsStateTreeSchema_StateInstance::CollectExternalData(
	const FStateTreeExecutionContext& Context,
	const UStateTree* StateTree,
	UTcsStateInstance* StateInstance,
	TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs,
	TArrayView<FStateTreeDataView> OutDataViews)
{
	checkf(ExternalDataDescs.Num() == OutDataViews.Num(), TEXT("The execution context failed to fill OutDataViews with empty values."));

	// 验证状态实例和状态树
	if (!StateInstance || !StateTree)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to get StateInstance or StateTree for StateTree external data"),
			*FString(__FUNCTION__));
		return false;
	}
	
	// 获取World引用
	UWorld* World = Context.GetWorld();
	if (!World)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to get World for StateInstance's StateTree external data: %s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());
		return false;
	}

	// 获取Owner引用
	const FString StateDefId = StateInstance->GetStateDefId().ToString();
	AActor* OwnerActor = StateInstance->GetOwner();
	AActor* InstigatorActor = StateInstance->GetInstigator();
	if (!OwnerActor || !InstigatorActor)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to get Owner or Instigator for StateInstance's StateTree external data: %s"),
			*FString(__FUNCTION__),
			*StateDefId);
		return false;
	}

	// 验证Owner和Instigator实现TcsEntityInterface
	if (!OwnerActor->Implements<UTcsEntityInterface>() || !InstigatorActor->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsState, Error,
			TEXT("[%s] Owner or Instigator does not implement TcsEntityInterface for StateInstance's StateTree external data: %s"),
			*FString(__FUNCTION__),
			*StateDefId);
		return false;
	}

	// 遍历所有外部数据需求
	int32 IssuesFoundCounter = 0;
	for (int32 Index = 0; Index < ExternalDataDescs.Num(); Index++)
	{
		const FStateTreeExternalDataDesc& DataDesc = ExternalDataDescs[Index];
		if (DataDesc.Struct != nullptr)
		{
			// 世界子系统
			if (DataDesc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
			{
				UWorldSubsystem* Subsystem = World->GetSubsystemBase(Cast<UClass>(const_cast<UStruct*>(DataDesc.Struct.Get())));
				OutDataViews[Index] = FStateTreeDataView(Subsystem);
				UE_CVLOG(Subsystem == nullptr, StateInstance, LogTcsState, Error,
					TEXT("[%s] StateTree %s: Could not find required subsystem %s"),
					*FString(__FUNCTION__),
					*GetNameSafe(Context.GetStateTree()),
					*GetNameSafe(DataDesc.Struct));
				IssuesFoundCounter += Subsystem != nullptr ? 0 : 1;
			}
			// 游戏实例
			else if (DataDesc.Struct->IsChildOf(UGameInstanceSubsystem::StaticClass()))
			{
				UGameInstance* GameInstance = World->GetGameInstance();
				OutDataViews[Index] = FStateTreeDataView(GameInstance);
				UE_CVLOG(GameInstance == nullptr, StateInstance, LogTcsState, Error,
					TEXT("[%s] StateTree %s: Could not find required game instance"),
					*FString(__FUNCTION__),
					*GetNameSafe(Context.GetStateTree()));
				IssuesFoundCounter += GameInstance != nullptr ? 0 : 1;
			}
			// Controller
			else if (DataDesc.Struct->IsChildOf(AController::StaticClass()))
			{
				// 根据名称提供Controller数据
				if (DataDesc.Name == TcsStateTreeContextName::OwnerController
					&& StateInstance->GetOwnerController())
				{
					OutDataViews[Index] = FStateTreeDataView(StateInstance->GetOwnerController());
				}
				else if (DataDesc.Name == TcsStateTreeContextName::InstigatorController
					&& StateInstance->GetInstigatorController())
				{
					OutDataViews[Index] = FStateTreeDataView(StateInstance->GetInstigatorController());
				}
			}
			// Actor
			else if (DataDesc.Struct->IsChildOf(AActor::StaticClass()))
			{
				// 根据名称提供Actor数据
				if (DataDesc.Name == TcsStateTreeContextName::Owner
					&& OwnerActor)
				{
					OutDataViews[Index] = FStateTreeDataView(OwnerActor);
				}
				else if (DataDesc.Name == TcsStateTreeContextName::Instigator
					&& InstigatorActor)
				{
					OutDataViews[Index] = FStateTreeDataView(InstigatorActor);
				}
			}
			// 状态组件
			else if (DataDesc.Struct->IsChildOf(UTcsStateComponent::StaticClass()))
			{
				if (DataDesc.Name == TcsStateTreeContextName::OwnerStateCmp)
				{
					UTcsStateComponent* OwnerStateCmp = StateInstance->GetOwnerStateComponent();
					OutDataViews[Index] = OwnerStateCmp;
					UE_CVLOG(OwnerStateCmp == nullptr, StateInstance, LogTcsState, Error,
						TEXT("[%s] StateTree %s: Could not find required owner state component"),
						*FString(__FUNCTION__),
						*GetNameSafe(Context.GetStateTree()));
					IssuesFoundCounter += OwnerStateCmp != nullptr ? 0 : 1;
				}
				else if (DataDesc.Name == TcsStateTreeContextName::InstigatorStateCmp)
				{
					UTcsStateComponent* InstigatorStateCmp = StateInstance->GetInstigatorStateComponent();
					OutDataViews[Index] = InstigatorStateCmp;
					UE_CVLOG(InstigatorStateCmp == nullptr, StateInstance, LogTcsState, Error,
						TEXT("[%s] StateTree %s: Could not find required instigator state component"),
						*FString(__FUNCTION__),
						*GetNameSafe(Context.GetStateTree()));
					IssuesFoundCounter += InstigatorStateCmp != nullptr ? 0 : 1;
				}
			}
			// 属性组件
			else if (DataDesc.Struct->IsChildOf(UTcsAttributeComponent::StaticClass()))
			{
				if (DataDesc.Name == TcsStateTreeContextName::OwnerAttributeCmp)
				{
					UTcsAttributeComponent* OwnerAttributeCmp = StateInstance->GetOwnerAttributeComponent();
					OutDataViews[Index] = OwnerAttributeCmp;
					UE_CVLOG(OwnerAttributeCmp == nullptr, StateInstance, LogTcsState, Error, TEXT("[%s] StateTree %s: Could not find required owner attribute component"),
						*FString(__FUNCTION__),
						*GetNameSafe(Context.GetStateTree()));
					IssuesFoundCounter += OwnerAttributeCmp != nullptr ? 0 : 1;
				}
				else if (DataDesc.Name == TcsStateTreeContextName::InstigatorAttributeCmp)
				{
					UTcsAttributeComponent* InstigatorAttributeCmp = StateInstance->GetInstigatorAttributeComponent();
					OutDataViews[Index] = InstigatorAttributeCmp;
					UE_CVLOG(InstigatorAttributeCmp == nullptr, StateInstance, LogTcsState, Error, TEXT("[%s] StateTree %s: Could not find required instigator attribute component"),
						*FString(__FUNCTION__),
						*GetNameSafe(Context.GetStateTree()));
					IssuesFoundCounter += InstigatorAttributeCmp != nullptr ? 0 : 1;
				}
			}
			// 技能组件
			else if (DataDesc.Struct->IsChildOf(UTcsSkillComponent::StaticClass()))
			{
				if (DataDesc.Name == TcsStateTreeContextName::OwnerSkillCmp)
				{
					UTcsSkillComponent* OwnerSkillCmp = StateInstance->GetOwnerSkillComponent();
					OutDataViews[Index] = OwnerSkillCmp;
					UE_CVLOG(OwnerSkillCmp == nullptr, StateInstance, LogTcsState, Error, TEXT("[%s] StateTree %s: Could not find required owner Skill component"),
						*FString(__FUNCTION__),
						*GetNameSafe(Context.GetStateTree()));
					IssuesFoundCounter += OwnerSkillCmp != nullptr ? 0 : 1;
				}
				else if (DataDesc.Name == TcsStateTreeContextName::InstigatorSkillCmp)
				{
					UTcsSkillComponent* InstigatorSkillCmp = StateInstance->GetInstigatorSkillComponent();
					OutDataViews[Index] = InstigatorSkillCmp;
					UE_CVLOG(InstigatorSkillCmp == nullptr, StateInstance, LogTcsState, Error, TEXT("[%s] StateTree %s: Could not find required instigator skill component"),
						*FString(__FUNCTION__),
						*GetNameSafe(Context.GetStateTree()));
					IssuesFoundCounter += InstigatorSkillCmp != nullptr ? 0 : 1;
				}
			}
			// 状态实例
			else if (DataDesc.Struct->IsChildOf(UTcsStateInstance::StaticClass()))
			{
				OutDataViews[Index] = StateInstance;
			}
		}
	}

	return IssuesFoundCounter == 0;
}

UTcsStateTreeSchema_StateInstance::FTcsContextDataSetter::FTcsContextDataSetter(
	TNotNull<const UTcsStateInstance*> InStateInstance,
	FStateTreeExecutionContext& Context)
		: StateInstance(InStateInstance),
		ExecutionContext(Context)
{}

TNotNull<const UStateTree*> UTcsStateTreeSchema_StateInstance::FTcsContextDataSetter::GetStateTree() const
{
	return ExecutionContext.GetStateTree();
}

TNotNull<const UTcsStateTreeSchema_StateInstance*> UTcsStateTreeSchema_StateInstance::FTcsContextDataSetter::
GetSchema() const
{
	return Cast<UTcsStateTreeSchema_StateInstance>(ExecutionContext.GetStateTree()->GetSchema());
}

bool UTcsStateTreeSchema_StateInstance::FTcsContextDataSetter::SetContextDataByName(
	FName Name,
	FStateTreeDataView DataView)
{
	return ExecutionContext.IsValid() ? ExecutionContext.SetContextDataByName(Name, DataView) : false;
}

void UTcsStateTreeSchema_StateInstance::SetContextData(
	FTcsContextDataSetter& ContextDataSetter,
	bool bLogErrors) const
{
	const UTcsStateTreeSchema_StateInstance* Schema = ContextDataSetter.GetSchema();
	const UTcsStateInstance* StateInstance = ContextDataSetter.GetStateInstance();

	// 状态树和状态实例
	if (!Schema || !StateInstance)
	{
		if (bLogErrors)
		{
			UE_LOG(LogTcsState, Error,
				TEXT("%s Expected StateTree asset to contain StateTreeSchema and StateInstance. StateTree will not update."),
				*FString(__FUNCTION__));
		}
		return;
	}
	ContextDataSetter.SetContextDataByName(
		TcsStateTreeContextName::StateInstance,
		FStateTreeDataView(const_cast<UTcsStateInstance*>(StateInstance)));

	// 状态Owner和状态Instigator
	AActor* OwnerActor = StateInstance->GetOwner();
	AActor* InstigatorActor = StateInstance->GetInstigator();
	if (!OwnerActor || !InstigatorActor)
	{
		if (bLogErrors)
		{
			UE_LOG(LogTcsState, Error,
				TEXT("%s Expected StateInstance to have valid owner and instigator. StateTree will not update."),
				*FString(__FUNCTION__));
		}
		return;
	}
	ContextDataSetter.SetContextDataByName(
		TcsStateTreeContextName::Owner,
		FStateTreeDataView(OwnerActor));
	ContextDataSetter.SetContextDataByName(
		TcsStateTreeContextName::Instigator,
		FStateTreeDataView(InstigatorActor));

	// 状态Owner和状态Instigator的控制器
	ContextDataSetter.SetContextDataByName(
		TcsStateTreeContextName::OwnerController,
		FStateTreeDataView(StateInstance->GetOwnerController()));
	ContextDataSetter.SetContextDataByName(
		TcsStateTreeContextName::InstigatorController,
		FStateTreeDataView(StateInstance->GetInstigatorController()));

	// 状态Owner和状态Instigator的状态组件
	ContextDataSetter.SetContextDataByName(
		TcsStateTreeContextName::OwnerStateCmp,
		FStateTreeDataView(StateInstance->GetOwnerStateComponent()));
	ContextDataSetter.SetContextDataByName(
		TcsStateTreeContextName::InstigatorStateCmp,
		FStateTreeDataView(StateInstance->GetInstigatorStateComponent()));

	// 状态Owner和状态Instigator的属性组件
	ContextDataSetter.SetContextDataByName(
		TcsStateTreeContextName::OwnerAttributeCmp,
		FStateTreeDataView(StateInstance->GetOwnerAttributeComponent()));
	ContextDataSetter.SetContextDataByName(
		TcsStateTreeContextName::InstigatorAttributeCmp,
		FStateTreeDataView(StateInstance->GetInstigatorAttributeComponent()));

	// 状态Owner和状态Instigator的技能组件
	ContextDataSetter.SetContextDataByName(
		TcsStateTreeContextName::OwnerSkillCmp,
		FStateTreeDataView(StateInstance->GetOwnerSkillComponent()));
	ContextDataSetter.SetContextDataByName(
		TcsStateTreeContextName::InstigatorSkillCmp,
		FStateTreeDataView(StateInstance->GetInstigatorSkillComponent()));
}
