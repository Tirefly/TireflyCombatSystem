// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TireflyStateComponent.generated.h"


UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tirefly State Comp"))
class TIREFLYCOMBATSYSTEM_API UTireflyStateComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region ActorComponent

public:
	// Sets default values for this component's properties
	UTireflyStateComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

#pragma endregion
};
