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
class UTcsDefinitionManagerSubsystem;
class UTcsRuntimeBootstrapSubsystem;
class UTcsStateInstance;
class UTcsStateComponent;


// 技能激活结果枚举
UENUM(BlueprintType)
enum class ETcsSkillActivateResult : uint8
{
	Success				UMETA(DisplayName = "Success", ToolTip = "技能成功激活"),
	NotReady			UMETA(DisplayName = "Not Ready", ToolTip = "Skill runtime 尚未 ready"),
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
	/** 在组件初始化时接入 runtime bootstrap。 */
	virtual void InitializeComponent() override;

	/** 在组件反初始化时退出 runtime bootstrap。 */
	virtual void UninitializeComponent() override;

	virtual void OnUnregister() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

#pragma endregion


#pragma region RuntimeBootstrap

public:
	/**
	 * 查询当前 SkillComponent 是否已完成 runtime prepare。
	 *
	 * @return 若当前组件已完成 runtime prepare，则返回 true
	 */
	UFUNCTION(BlueprintPure, Category = "Skill|Runtime")
	bool IsRuntimePrepared() const { return bRuntimePrepared; }

	/**
	 * 查询当前 SkillComponent 是否已满足完整 runtime-ready 条件。
	 *
	 * @return 若当前组件已满足完整 runtime-ready 条件，则返回 true
	 */
	UFUNCTION(BlueprintPure, Category = "Skill|Runtime")
	bool IsRuntimeReady() const;

	/**
	 * 显式执行 Skill runtime prepare。
	 *
	 * @return 若 prepare 成功，则返回 true
	 */
	bool PrepareSkillRuntime();

protected:
	/** 缓存的 RuntimeBootstrapSubsystem 指针。 */
	UPROPERTY(Transient)
	TObjectPtr<UTcsRuntimeBootstrapSubsystem> RuntimeBootstrapSubsystem;

	/** 当前 SkillComponent 是否已完成 runtime prepare。 */
	UPROPERTY(Transient)
	bool bRuntimePrepared = false;

	/**
	 * 懒加载获取 RuntimeBootstrapSubsystem。
	 *
	 * @return RuntimeBootstrapSubsystem 指针；失败时返回 nullptr
	 */
	UTcsRuntimeBootstrapSubsystem* ResolveRuntimeBootstrapSubsystem();

#pragma endregion


#pragma region Learned

public:
	/**
	 * 按 SkillDefId 学会一个技能（创建 Entry 并注册）。
	 *
	 * @param SkillDefId 要学习的 Skill 定义 ID。
	 * @return 成功创建并注册 SkillEntry 时返回 true；解析失败或已学会时返回 false。
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Learned")
	bool LearnSkill(FName SkillDefId);

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
	/** 已学会技能集合（Key 为稳定 SkillDefId）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill|Learned", Meta = (AllowPrivateAccess = "true"))
	TMap<FName, TObjectPtr<UTcsSkillEntry>> LearnedSkills;

private:
	/**
	 * 通过 DefinitionManager 解析 Skill 定义。
	 *
	 * @param SkillDefId 要解析的 Skill 定义 ID。
	 * @return 解析成功时返回 SkillDefinition；失败时返回 nullptr。
	 */
	const UTcsSkillDefinition* ResolveSkillDefinition(FName SkillDefId) const;

#pragma endregion


#pragma region Cooldown

protected:
	UPROPERTY()
	FTcsSkillCooldownTracker CooldownTracker;

#pragma endregion


#pragma region SkillModifier

public:
	/**
	 * 使用 SourceHandle 按 SkillModifierDefId 批量应用 SkillModifier。
	 *
	 * @param SourceHandle 当前 apply 的 authority 因果链句柄。
	 * @param SkillModifierDefIds 要应用的 SkillModifier 定义 ID 列表。
	 * @param OutRuntimeEntries 成功写入账本的运行时记录。
	 * @return 所有定义都成功解析并完整写入时返回 true；失败时不保留部分 apply。
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Modifier")
	bool ApplySkillModifiersWithSourceHandle(
		const FTcsSourceHandle& SourceHandle,
		const TArray<FName>& SkillModifierDefIds,
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
	/**
	 * 通过 DefinitionManager 解析指定 SkillModifier 定义资产。
	 *
	 * @param SkillModifierDefId 要解析的 SkillModifier 定义 ID。
	 * @return 解析成功时返回对应 Definition；失败时返回 nullptr。
	 */
	const UTcsSkillModifierDefinition* ResolveSkillModifierDefinition(FName SkillModifierDefId) const;

	/**
	 * 分配新的 SkillModifier runtime id。
	 *
	 * @return 严格单调递增且大于 0 的 runtime id。
	 */
	int32 AllocateSkillModifierRuntimeId();

	/**
	 * 为单个 SkillModifierDefId 创建展开后的 runtime records。
	 *
	 * @param SkillModifierDefId 要展开的 SkillModifier 定义 ID。
	 * @param SourceHandle 当前 apply 的 authority 因果链句柄。
	 * @param OutRuntimeEntries 输出的待写入运行时记录。
	 * @return 定义解析并至少创建一条运行时记录时返回 true。
	 */
	bool CreateSkillModifierRuntimeEntries(
		FName SkillModifierDefId,
		const FTcsSourceHandle& SourceHandle,
		TArray<FTcsSkillModifierRuntimeEntry>& OutRuntimeEntries);

	/**
	 * 将 runtime records 真正写入账本和 SkillEntry 参数实例链。
	 *
	 * 注意：这里写入的是 SkillEntry 的共享参数容器，而不是 SkillInstance 私有副本；
	 * 因此来源存活期间新增的 SkillModifier 会立即对全部读取该 SkillEntry 的调用方可见。
	 */
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
	/**
	 * 激活指定技能。
	 *
	 * @param SkillDefId 要激活的 Skill 定义 ID。
	 * @param Instigator 技能发起者。
	 * @return 返回技能激活结果。
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Activation")
	ETcsSkillActivateResult ActivateSkill(FName SkillDefId, AActor* Instigator);

private:
	/** @return Owner Actor 上的 UTcsStateComponent。 */
	UTcsStateComponent* GetOwnerStateComponent() const;

#pragma endregion
};
