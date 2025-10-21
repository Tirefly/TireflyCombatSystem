// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TcsSkillComponent.generated.h"



UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tirefly Skill Comp"))
class TIREFLYCOMBATSYSTEM_API UTcsSkillComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region ActorComponent

public:
	UTcsSkillComponent();

protected:
	virtual void BeginPlay() override;
	
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion
};
