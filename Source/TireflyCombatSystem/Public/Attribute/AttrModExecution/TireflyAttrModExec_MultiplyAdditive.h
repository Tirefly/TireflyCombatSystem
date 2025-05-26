// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/TireflyAttributeModifierExecution.h"
#include "TireflyAttrModExec_MultiplyAdditive.generated.h"



// 属性修改器：乘法结合律
UCLASS(Meta = (DisplayName = "属性修改器：乘法结合律"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModExec_MultiplyAdditive : public UTireflyAttributeModifierExecution
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(
		AActor* Instigator,
		AActor* Target,
		const FTireflyAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues) override;
};
