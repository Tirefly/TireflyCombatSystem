// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeSchema.h"
#include "TcsCombatStateTreeSchema.generated.h"

class UTcsStateInstance;
class UTcsStateComponent;
class UTcsAttributeComponent;

/**
 * StateTree Schema for TcsCombatSystem
 * 为战斗系统提供专用的StateTree Schema，定义可用的上下文数据和节点类型
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories)
class TIREFLYCOMBATSYSTEM_API UTcsCombatStateTreeSchema : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	UTcsCombatStateTreeSchema();

protected:
	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const override;
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsClassAllowed(const UClass* InClass) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;

private:
	mutable TArray<FStateTreeExternalDataDesc> ContextDataDescs;
};