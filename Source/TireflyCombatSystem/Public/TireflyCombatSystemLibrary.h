// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TireflyCombatSystemLibrary.generated.h"


UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflyCombatSystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

#pragma region AttributeHelper

public:
	// 获取所有属性名称
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Attribute")
	static TArray<FName> GetAttributeNames();

#pragma endregion
};
