// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/TireflyAttributeModifierExecution.h"
#include "TireflyAttrModExec_MultiplyContinued.generated.h"



// 属性修改器：乘法连乘
UCLASS(Meta = (DisplayName = "属性修改器：乘法连乘"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttrModExec_MultiplyContinued : public UTireflyAttributeModifierExecution
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(
		AActor* Instigator,
		AActor* Target,
		UObject* SourceObject,
		const FTireflyAttributeModifierInstance& ModInst,
		UPARAM(ref) TMap<FName, float>& BaseValues,
		UPARAM(ref) TMap<FName, float>& CurrentValues) override;
};
