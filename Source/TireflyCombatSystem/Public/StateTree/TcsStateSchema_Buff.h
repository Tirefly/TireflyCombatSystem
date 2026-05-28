// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeSchema.h"
#include "TcsStateSchema_Buff.generated.h"



class UTcsBuffInstance;



namespace TcsBuffStateContextName
{
	static FLazyName BuffInstance = "BuffInstance";
}



/**
 * BuffStateTree 专用 schema。
 *
 * 只暴露 BuffInstance 根上下文，同时允许共享节点以 `UTcsStateInstance` 基类请求当前 Buff 运行态。
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories)
class TIREFLYCOMBATSYSTEM_API UTcsStateSchema_Buff : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	/** 构造默认的 BuffStateTree 上下文描述。 */
	UTcsStateSchema_Buff();

	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const override;
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsClassAllowed(const UClass* InClass) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;

	/**
	 * 将 BuffInstance 根上下文写入当前执行上下文。
	 *
	 * @param BuffInstance 当前正在执行的 Buff 运行时实例
	 * @param Context 当前 StateTree 执行上下文
	 * @param bLogErrors 是否输出错误日志
	 * @return 当上下文需求完整满足时返回 true
	 */
	static bool SetContextRequirements(
		UTcsBuffInstance& BuffInstance,
		FStateTreeExecutionContext& Context,
		bool bLogErrors = true);

	/**
	 * 为 BuffStateTree 收集外部数据。
	 *
	 * @param Context 当前 StateTree 执行上下文
	 * @param StateTree 当前 StateTree 资源
	 * @param BuffInstance 当前正在执行的 Buff 运行时实例
	 * @param ExternalDataDescs 当前请求的外部数据描述
	 * @param OutDataViews 输出的外部数据视图
	 * @return 所有请求都成功解析时返回 true
	 */
	static bool CollectExternalData(
		const FStateTreeExecutionContext& Context,
		const UStateTree* StateTree,
		UTcsBuffInstance* BuffInstance,
		TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs,
		TArrayView<FStateTreeDataView> OutDataViews);

protected:
	/** Helper class to set the context data on the ExecutionContext. */
	struct FTcsContextDataSetter
	{
	public:
		FTcsContextDataSetter(
			TNotNull<const UTcsBuffInstance*> InBuffInstance,
			FStateTreeExecutionContext& Context);

		TNotNull<const UTcsBuffInstance*> GetBuffInstance() const
		{
			return BuffInstance;
		}

		TNotNull<const UStateTree*> GetStateTree() const;
		TNotNull<const UTcsStateSchema_Buff*> GetSchema() const;

		bool SetContextDataByName(FName Name, FStateTreeDataView DataView);

	private:
		TNotNull<const UTcsBuffInstance*> BuffInstance;
		FStateTreeExecutionContext& ExecutionContext;
	};

	/**
	 * 根据当前 schema 将 BuffInstance 根上下文写入执行上下文。
	 *
	 * @param ContextDataSetter 当前上下文写入帮助器
	 * @param bLogErrors 是否输出错误日志
	 */
	virtual void SetContextData(FTcsContextDataSetter& ContextDataSetter, bool bLogErrors) const;

	/** BuffStateTree 使用的根上下文描述集合。 */
	UPROPERTY()
	TArray<FStateTreeExternalDataDesc> ContextDataDescs;
};