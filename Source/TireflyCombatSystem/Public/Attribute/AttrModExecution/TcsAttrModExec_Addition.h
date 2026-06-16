// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/AttrModExecution/TcsAttributeModifierExecution.h"
#include "TcsAttrModExec_Addition.generated.h"



// 属性修改器执行器：加法
UCLASS(Meta = (DisplayName = "属性修改器执行器：加法"))
class TIREFLYCOMBATSYSTEM_API UTcsAttrModExec_Addition : public UTcsAttributeModifierExecution
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(
		const FTcsAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues) override;
};
