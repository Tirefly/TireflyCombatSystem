// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "TireflyCombatEntityInterface.generated.h"


// This class does not need to be modified.
UINTERFACE(Blueprintable)
class UTireflyCombatEntityInterface : public UInterface
{
	GENERATED_BODY()
};


class TIREFLYCOMBATSYSTEM_API ITireflyCombatEntityInterface
{
	GENERATED_BODY()

public:
	// 获取战斗实体的状态组件
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = TireflyCombatSystem)
	class UTireflyStateComponent* GetStateComponent() const;

	// 获取战斗实体的技能组件
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = TireflyCombatSystem)
	class UTireflySkillComponent* GetSkillComponent() const;

	// 获取战斗实体的属性组件
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = TireflyCombatSystem)
	class UTireflyAttributeComponent* GetAttributeComponent() const;

	// 获取战斗实体的类型
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = TireflyCombatSystem)
	FGameplayTag GetCombatEntityType() const;
};
