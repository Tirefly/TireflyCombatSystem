// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/StateTreeComponentSchema.h"
#include "TcsStateSchema_StateComponent.generated.h"



class UTcsStateComponent;



namespace TcsStateComponentContextName
{
	static FLazyName StateComponent = "StateComponent";
}



/**
 * Tcs StateComponent 专用 StateTree schema。
 *
 * 基于引擎的组件 schema 链路保留组件执行与 LinkSubTree 兼容性，同时将根上下文收敛为单一 StateComponent。
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Tcs State Component"))
class TIREFLYCOMBATSYSTEM_API UTcsStateSchema_StateComponent : public UStateTreeComponentSchema
{
	GENERATED_BODY()

public:
	/** 构造默认的 StateComponent 根上下文描述。 */
	UTcsStateSchema_StateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostLoad() override;

	/**
	 * 将 StateComponent 根上下文写入当前执行上下文。
	 *
	 * @param ContextDataSetter 当前上下文写入帮助器
	 * @param bLogErrors 是否输出错误日志
	 */
	virtual void SetContextData(FContextDataSetter& ContextDataSetter, bool bLogErrors) const override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

private:
	/** 确保继承链的 Actor 上下文描述不会覆盖 TCS 自定义的 StateComponent 根上下文。 */
	void RefreshContextDescriptor();
};