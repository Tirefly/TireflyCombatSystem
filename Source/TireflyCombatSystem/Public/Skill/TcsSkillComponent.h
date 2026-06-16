// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Skill/TcsSkillCooldownTracker.h"
#include "TcsSkillComponent.generated.h"



class UTcsSkillDefinition;
class UTcsSkillEntry;
class UTcsStateComponent;


// 技能激活结果枚举
UENUM(BlueprintType)
enum class ETcsSkillActivateResult : uint8
{
	Success				UMETA(DisplayName = "Success", ToolTip = "技能成功激活"),
	NotLearned			UMETA(DisplayName = "Not Learned", ToolTip = "技能未学会"),
	OnCooldown			UMETA(DisplayName = "On Cooldown", ToolTip = "技能处于冷却中"),
	InvalidDefinition	UMETA(DisplayName = "Invalid Definition", ToolTip = "技能定义无效"),
	ApplyFailed			UMETA(DisplayName = "Apply Failed", ToolTip = "StateComponent 拒绝应用"),
};



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

#pragma endregion


#pragma region Learned

public:
	/** 学会一个技能（创建 Entry 并注册）。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Learned")
	void LearnSkill(UTcsSkillDefinition* Def);

	/** 遗忘一个技能（移除 Entry，并取消活跃实例）。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Learned")
	void ForgetSkill(FName SkillDefId);

	/** @return 是否已学会指定技能。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Learned")
	bool HasSkill(FName SkillDefId) const;

	/** @return 指定技能对应的 learned-skill 数据对象。未学会则返回 nullptr。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Learned")
	UTcsSkillEntry* GetSkillEntry(FName SkillDefId) const;

	/** @return 当前所有已学会的技能 Entry 列表。 */
	TArray<UTcsSkillEntry*> GetAllSkillEntries() const;

protected:
	/** 已学会技能集合（Key 为 SkillDef 的 AssetName）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Learned", Meta = (AllowPrivateAccess = "true"))
	TMap<FName, TObjectPtr<UTcsSkillEntry>> LearnedSkills;

#pragma endregion


#pragma region Cooldown

protected:
	UPROPERTY()
	FTcsSkillCooldownTracker CooldownTracker;

#pragma endregion


#pragma region Activation

public:
	/** 激活指定技能。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Activation")
	ETcsSkillActivateResult ActivateSkill(FName SkillDefId, AActor* Instigator);

private:
	/** @return Owner Actor 上的 UTcsStateComponent。 */
	UTcsStateComponent* GetOwnerStateComponent() const;

#pragma endregion
};