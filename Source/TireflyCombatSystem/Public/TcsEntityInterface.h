// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "TcsEntityInterface.generated.h"


// This class does not need to be modified.
UINTERFACE(Blueprintable)
class UTcsEntityInterface : public UInterface
{
	GENERATED_BODY()
};


class TIREFLYCOMBATSYSTEM_API ITcsEntityInterface
{
	GENERATED_BODY()

public:
	// 获取战斗实体的状态组件
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "TireflyCombatSystem")
	class UTcsStateComponent* GetStateComponent() const;

	// 获取战斗实体的技能组件
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "TireflyCombatSystem")
	class UTcsSkillComponent* GetSkillComponent() const;

	// 获取战斗实体的属性组件
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "TireflyCombatSystem")
	class UTcsAttributeComponent* GetAttributeComponent() const;

	// 获取战斗实体的类型
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "TireflyCombatSystem")
	FGameplayTag GetCombatEntityType() const;

	// 获取战斗实体的等级
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "TireflyCombatSystem")
	int32 GetCombatEntityLevel() const;
};
