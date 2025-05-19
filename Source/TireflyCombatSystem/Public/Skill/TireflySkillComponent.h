// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TireflySkillComponent.generated.h"


UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tirefly Skill Comp"))
class TIREFLYCOMBATSYSTEM_API UTireflySkillComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region ActorComponent

public:
	UTireflySkillComponent();

protected:
	virtual void BeginPlay() override;

#pragma endregion
};
