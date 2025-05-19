// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatEntityInterface.generated.h"


// This class does not need to be modified.
UINTERFACE()
class UCombatEntityInterface : public UInterface
{
	GENERATED_BODY()
};


class TIREFLYCOMBATSYSTEM_API ICombatEntityInterface
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
};
