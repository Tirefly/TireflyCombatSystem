// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TcsCombatSystemLibrary.generated.h"


UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsCombatSystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

#pragma region AttributeHelper

public:
	// 获取所有属性名称
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "TcsCombatSystem|Attribute")
	static TArray<FName> GetAttributeNames();

	// 获取属性定义总表
	UFUNCTION(BlueprintCallable, Category = "TcsCombatSystem|Attribute")
	static UDataTable* GetAttributeDefTable();

	// 获取所有属性修改器名称
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "TcsCombatSystem|Attribute")
	static TArray<FName> GetAttributeModifierIds();

	// 获取属性修改器定义总表
	UFUNCTION(BlueprintCallable, Category = "TcsCombatSystem|Attribute")
	static UDataTable* GetAttributeModifierDefTable();

#pragma endregion
};
