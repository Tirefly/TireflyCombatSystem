// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TcsSourceHandle.h"
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

	// 获取所有属性修改器名称
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "TireflyCombatSystem|Attribute")
	static TArray<FName> GetAttributeModifierIds();

	// 获取属性组件
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
	static class UTcsAttributeComponent* GetAttributeComponent(AActor* Actor);

#pragma endregion


#pragma region StateHelper

public:
	// 获取状态组件
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|State")
	static class UTcsStateComponent* GetStateComponent(AActor* Actor);

#pragma endregion


#pragma region BuffHelper

public:
	// 获取 Buff 组件
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Buff")
	static class UTcsBuffComponent* GetBuffComponent(AActor* Actor);

#pragma endregion


#pragma region SkillHelper

public:
	// 获取技能组件
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Skill")
	static class UTcsSkillComponent* GetSkillComponent(AActor* Actor);

#pragma endregion


	// SourceHandle 工厂转发
#pragma region SourceHandleHelper

public:
	/**
	 * 创建没有父来源的 Root SourceHandle。
	 *
	 * @param Instigator 实际造成效果的运行时 Actor。
	 * @param SourceTags Source 类型标签。
	 * @return 返回新创建的有效 SourceHandle。
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "TireflyCombatSystem|SourceHandle")
	static FTcsSourceHandle CreateRootSourceHandle(
		AActor* Instigator,
		const FGameplayTagContainer& SourceTags);

	/**
	 * 创建从父来源派生的 Child SourceHandle。
	 *
	 * @param ParentSourceHandle 父来源句柄。
	 * @param DirectParentSourceDefId 直接父来源的 Definition Id。
	 * @param Instigator 实际造成效果的运行时 Actor。
	 * @param SourceTags Source 类型标签。
	 * @return 返回新创建的有效 SourceHandle；输入无效时返回默认无效 SourceHandle。
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "TireflyCombatSystem|SourceHandle")
	static FTcsSourceHandle CreateChildSourceHandle(
		const FTcsSourceHandle& ParentSourceHandle,
		FPrimaryAssetId DirectParentSourceDefId,
		AActor* Instigator,
		const FGameplayTagContainer& SourceTags);

#pragma endregion
};
