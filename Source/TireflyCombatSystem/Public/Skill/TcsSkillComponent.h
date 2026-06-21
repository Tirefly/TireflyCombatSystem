// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Skill/TcsSkillCooldownTracker.h"
#include "Skill/TcsSkillModifierRuntime.h"
#include "TcsSkillComponent.generated.h"



class UTcsSkillDefinition;
class UTcsSkillEntry;
class UTcsSkillModifierDefinition;
class UTcsSkillInstance;
class UTcsStateInstance;
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
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
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


#pragma region SkillModifier

public:
	/** 使用 SourceHandle 批量应用 SkillModifier。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Modifier")
	bool ApplySkillModifiersWithSourceHandle(
		const FTcsSourceHandle& SourceHandle,
		const TArray<FName>& ModifierIds,
		TArray<FTcsSkillModifierRuntimeEntry>& OutRuntimeEntries);

	/** 按 SourceHandle 批量移除 SkillModifier。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Modifier")
	bool RemoveSkillModifiersBySourceHandle(const FTcsSourceHandle& SourceHandle);

	/** 按 SourceHandle 查询 SkillModifier 账本记录。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Modifier")
	bool GetSkillModifiersBySourceHandle(
		const FTcsSourceHandle& SourceHandle,
		TArray<FTcsSkillModifierRuntimeEntry>& OutRuntimeEntries) const;

	/** 按目标 SkillEntry 查询 SkillModifier 账本记录。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Modifier")
	bool GetSkillModifiersBySkillEntry(
		UTcsSkillEntry* SkillEntry,
		TArray<FTcsSkillModifierRuntimeEntry>& OutRuntimeEntries) const;

#pragma endregion


#pragma region SkillModifierRuntime

protected:
	/** SkillModifier 运行时账本与索引聚合结构。 */
	UPROPERTY(Transient)
	FTcsSkillModifierRuntimeIndex SkillModifierRuntimeIndex;

	/** 下一个可分配的 SkillModifier runtime id。 */
	UPROPERTY(Transient)
	int32 NextSkillModifierRuntimeId = 0;

private:
	/** 解析指定 SkillModifier Id 对应的定义资产。 */
	const UTcsSkillModifierDefinition* ResolveSkillModifierDefinition(FName ModifierId) const;

	/**
	 * 分配新的 SkillModifier runtime id。
	 *
	 * @return 严格单调递增且大于 0 的 runtime id。
	 */
	int32 AllocateSkillModifierRuntimeId();

	/** 为单个 ModifierId 创建展开后的 runtime records。 */
	bool CreateSkillModifierRuntimeEntries(
		FName ModifierId,
		const FTcsSourceHandle& SourceHandle,
		TArray<FTcsSkillModifierRuntimeEntry>& OutRuntimeEntries);

	/** 将 runtime records 真正写入账本和 SkillEntry 参数实例链。 */
	bool ApplySkillModifierRuntimeEntries(TArray<FTcsSkillModifierRuntimeEntry>& RuntimeEntries);

	/** 将单条 runtime record 写入目标 SkillEntry 的参数实例链。 */
	bool WriteRuntimeEntryToSkillEntry(FTcsSkillModifierRuntimeEntry& RuntimeEntry);

	/** 将单条 runtime record 从目标 SkillEntry 的参数实例链中移除。 */
	bool RemoveRuntimeEntryFromSkillEntry(const FTcsSkillModifierRuntimeEntry& RuntimeEntry);

	/** 将同冲突组内的真实激活状态同步回 runtime 账本。 */
	void SyncSkillModifierConflictSetActiveStates(const FTcsSkillModifierRuntimeEntry& RuntimeEntry);

	/** 按 runtime id 批量移除 runtime records。 */
	bool RemoveSkillModifierRuntimeEntriesByIds(const TArray<int32>& RuntimeModifierIds);

	/** 来源结束时统一回收对应的 SkillModifier。 */
	void HandleSkillModifierSourceEnded(const FTcsSourceHandle& SourceHandle);

	/** 技能实例结束时统一转发到 SourceHandle 清理链。 */
	void HandleSkillModifierSkillInstanceEnded(UTcsSkillInstance* SkillInstance);

	/** 按目标 SkillEntry 清理全部相关 SkillModifier。 */
	void RemoveSkillModifiersForSkillEntry(UTcsSkillEntry* SkillEntry);

	/** 绑定 Owner StateComponent 的内部生命周期桥接事件。 */
	void BindOwnerStateLifecycleEvents(UTcsStateComponent* StateComponent);

	/** 解绑 Owner StateComponent 的内部生命周期桥接事件。 */
	void UnbindOwnerStateLifecycleEvents(UTcsStateComponent* StateComponent);

	/** 处理 FinalizeRemoval 前段的 Skill 生命周期桥接。 */
	void HandleOwnerStateFinalizeRemovalStarted(UTcsStateComponent* StateComponent, UTcsStateInstance* StateInstance, FName RemovalReason);

	/** 处理 FinalizeRemoval 来源清理阶段的 Skill 生命周期桥接。 */
	void HandleOwnerStateFinalizeRemovalSourceCleanup(UTcsStateComponent* StateComponent, UTcsStateInstance* StateInstance, FName RemovalReason);

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