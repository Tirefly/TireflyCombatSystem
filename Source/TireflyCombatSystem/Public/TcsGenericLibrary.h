// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TcsGenericLibrary.generated.h"



// 战斗系统通用库
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsGenericLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

#pragma region AttributeHelper

public:
	// 获取所有属性名称
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "TireflyCombatSystem|Attribute")
	static TArray<FName> GetAttributeNames();

	// 获取属性定义总表
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
	static UDataTable* GetAttributeDefTable();

	// 获取所有属性修改器名称
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "TireflyCombatSystem|Attribute")
	static TArray<FName> GetAttributeModifierIds();

	// 获取属性修改器定义总表
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
	static UDataTable* GetAttributeModifierDefTable();

	// 获取属性组件
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
	static class UTcsAttributeComponent* GetAttributeComponent(AActor* Actor);

#pragma endregion


#pragma region StateHelper

public:
	// 获取所有状态定义名称
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "TireflyCombatSystem|State")
	static TArray<FName> GetStateDefNames();

	// 获取状态定义总表
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|State")
	static UDataTable* GetStateDefTable();

	// 获取状态槽定义总表
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|State")
	static UDataTable* GetStateSlotDefTable();

	// 获取状态组件
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|State")
	static class UTcsStateComponent* GetStateComponent(AActor* Actor);

#pragma endregion

	
#pragma region SkillHelper

public:
	// 获取所有技能修改器名称
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "TireflyCombatSystem|Skill")
	static TArray<FName> GetSkillModifierIds();

	// 获取技能修改器定义总表
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Skill")
	static UDataTable* GetSkillModifierDefTable();

	// 获取技能组件
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Skill")
	static class UTcsSkillComponent* GetSkillComponent(AActor* Actor);

#pragma endregion
};
