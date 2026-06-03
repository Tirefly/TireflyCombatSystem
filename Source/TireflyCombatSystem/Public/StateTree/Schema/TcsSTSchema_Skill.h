// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeSchema.h"
#include "TcsSTSchema_Skill.generated.h"



class UTcsSkillEntry;
class UTcsSkillInstance;
class UTcsStateInstance;



namespace TcsSkillStateContextName
{
	static FLazyName SkillInstance = "SkillInstance";
	static FLazyName SkillEntry = "SkillEntry";
}



/**
 * Skill 相关运行时树的专用 schema。
 *
 * 该 schema 明确区分本次技能激活执行态与 learned-skill 数据对象。
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "TcsSTSchema_Skill"))
class TIREFLYCOMBATSYSTEM_API UTcsSTSchema_Skill : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	/** 构造默认的 Skill 双上下文描述。 */
	UTcsSTSchema_Skill();

	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const override;
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsClassAllowed(const UClass* InClass) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;

	/**
	 * 将 SkillInstance / SkillEntry 上下文写入当前执行上下文。
	 *
	 * @param SkillInstance 当前技能激活对应的运行态对象
	 * @param SkillEntry 当前技能激活对应的 learned-skill 数据对象
	 * @param Context 当前 StateTree 执行上下文
	 * @param bLogErrors 是否输出错误日志
	 * @return 当上下文需求完整满足时返回 true
	 */
	static bool SetContextRequirements(
		UTcsSkillInstance& SkillInstance,
		UTcsSkillEntry& SkillEntry,
		FStateTreeExecutionContext& Context,
		bool bLogErrors = true);

	/**
	 * 为 SkillStateTree 收集外部数据。
	 *
	 * @param Context 当前 StateTree 执行上下文
	 * @param StateTree 当前 StateTree 资源
	 * @param SkillInstance 当前技能激活对应的运行态对象
	 * @param SkillEntry 当前技能激活对应的 learned-skill 数据对象
	 * @param ExternalDataDescs 当前请求的外部数据描述
	 * @param OutDataViews 输出的外部数据视图
	 * @return 所有请求都成功解析时返回 true
	 */
	static bool CollectExternalData(
		const FStateTreeExecutionContext& Context,
		const UStateTree* StateTree,
		UTcsSkillInstance* SkillInstance,
		UTcsSkillEntry* SkillEntry,
		TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs,
		TArrayView<FStateTreeDataView> OutDataViews);

protected:
	/** Helper class to set the context data on the ExecutionContext. */
	struct FTcsContextDataSetter
	{
	public:
		FTcsContextDataSetter(
			TNotNull<const UTcsSkillInstance*> InSkillInstance,
			TNotNull<const UTcsSkillEntry*> InSkillEntry,
			FStateTreeExecutionContext& Context);

		TNotNull<const UTcsSkillInstance*> GetSkillInstance() const
		{
			return SkillInstance;
		}

		TNotNull<const UTcsSkillEntry*> GetSkillEntry() const
		{
			return SkillEntry;
		}

		TNotNull<const UStateTree*> GetStateTree() const;
		TNotNull<const UTcsSTSchema_Skill*> GetSchema() const;

		bool SetContextDataByName(FName Name, FStateTreeDataView DataView);

	private:
		TNotNull<const UTcsSkillInstance*> SkillInstance;
		TNotNull<const UTcsSkillEntry*> SkillEntry;
		FStateTreeExecutionContext& ExecutionContext;
	};

	/**
	 * 根据当前 schema 将 SkillInstance / SkillEntry 写入执行上下文。
	 *
	 * @param ContextDataSetter 当前上下文写入帮助器
	 * @param bLogErrors 是否输出错误日志
	 */
	virtual void SetContextData(FTcsContextDataSetter& ContextDataSetter, bool bLogErrors) const;

	/** SkillStateTree 使用的根上下文描述集合。 */
	UPROPERTY()
	TArray<FStateTreeExternalDataDesc> ContextDataDescs;
};