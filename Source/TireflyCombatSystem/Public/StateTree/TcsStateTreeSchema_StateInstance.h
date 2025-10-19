// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeSchema.h"
#include "StateTreeExecutionContext.h"
#include "TcsStateTreeSchema_StateInstance.generated.h"



class UTcsStateInstance;
class UTcsStateComponent;
class UTcsAttributeComponent;



namespace TcsStateTreeContextName
{
	static FLazyName Owner = "Owner";
	static FLazyName Instigator = "Instigator";
	static FLazyName StateInstance = "StateInstance";

	static FLazyName OwnerController = "OwnerController";
	static FLazyName OwnerStateCmp = "OwnerStateCmp";
	static FLazyName OwnerAttributeCmp = "OwnerAttributeCmp";
	static FLazyName OwnerSkillCmp = "OwnerSkillCmp";
	
	static FLazyName InstigatorController = "InstigatorController";
	static FLazyName InstigatorStateCmp = "InstigatorStateCmp";
	static FLazyName InstigatorAttributeCmp = "InstigatorAttributeCmp";
	static FLazyName InstigatorSkillCmp = "InstigatorSkillCmp";
}



/**
 * StateTree Schema for TireflyCombatSystem
 * 为战斗系统提供专用的StateTree Schema，定义可用的上下文数据和节点类型
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories)
class TIREFLYCOMBATSYSTEM_API UTcsStateTreeSchema_StateInstance : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	UTcsStateTreeSchema_StateInstance();
	
	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const override;
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsClassAllowed(const UClass* InClass) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;

	static bool SetContextRequirements(
		UTcsStateInstance& StateInstance,
		FStateTreeExecutionContext& Context,
		bool bLogErrors = true);
	static bool CollectExternalData(
		const FStateTreeExecutionContext& Context,
		const UStateTree* StateTree,
		UTcsStateInstance* StateInstance,
		TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs,
		TArrayView<FStateTreeDataView> OutDataViews);

protected:
	/** Helper class to set the context data on the ExecutionContext */
	struct FTcsContextDataSetter
	{
	public:
		FTcsContextDataSetter(
			TNotNull<const UTcsStateInstance*> InStateInstance,
			FStateTreeExecutionContext& Context);
		
		TNotNull<const UTcsStateInstance*> GetStateInstance() const
		{
			return StateInstance;
		}
		
		TNotNull<const UStateTree*> GetStateTree() const;
		TNotNull<const UTcsStateTreeSchema_StateInstance*> GetSchema() const;

		bool SetContextDataByName(FName Name, FStateTreeDataView DataView);

	private:
		TNotNull<const UTcsStateInstance*> StateInstance;
		FStateTreeExecutionContext& ExecutionContext;
	};

	virtual void SetContextData(FTcsContextDataSetter& ContextDataSetter, bool bLogErrors) const;
	
	UPROPERTY()
	TArray<FStateTreeExternalDataDesc> ContextDataDescs;
};