// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buff/TcsBuffTypes.h"
#include "State/TcsStateInstance.h"
#include "TcsBuffInstance.generated.h"



class UTcsBuffDefinition;
class UTcsBuffComponent;
class UTcsBuffMerger;



/**
 * Buff 运行时实例。
 *
 * 用途：承载 Buff 专属的持续时间、叠层与合并语义，避免这些 API 继续固化在共享的 StateInstance 基类上。
 */
UCLASS(BlueprintType, Blueprintable)
class TIREFLYCOMBATSYSTEM_API UTcsBuffInstance : public UTcsStateInstance
{
	GENERATED_BODY()

	friend class UTcsBuffComponent;


#pragma region Meta

public:
	/** @return 当前实例对应的 Buff 定义；如果定义不是 Buff，则返回 nullptr。 */
	UFUNCTION(BlueprintPure, Category = "Buff|Meta")
	const UTcsBuffDefinition* GetBuffDef() const;

#pragma endregion


#pragma region Duration

public:
	/** @return 当前 Buff 的持续时间类型；如果定义无效，则返回 SDT_None。 */
	UFUNCTION(BlueprintPure, Category = "Buff|Duration")
	ETcsBuffDurationType GetDurationType() const;

	/** @return 如果当前 Buff 使用固定时长，则返回 true。 */
	UFUNCTION(BlueprintPure, Category = "Buff|Duration")
	bool HasFiniteDuration() const;

	/** @return 如果当前 Buff 为无限时长，则返回 true。 */
	UFUNCTION(BlueprintPure, Category = "Buff|Duration")
	bool HasInfiniteDuration() const;

	/** @return 剩余时长；无限时长返回 -1.0f。 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Duration")
	float GetDurationRemaining() const;

	/**
	 * 将剩余时长重置为当前实例持有的 TotalDuration。
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Duration")
	void ResetRemainingDuration();

	UFUNCTION(BlueprintCallable, Category = "Buff|Duration")
	void SetDurationRemaining(float InDurationRemaining);

	/** @return Buff 的总时长；无限时长返回 -1.0f。 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Duration")
	float GetTotalDuration() const;

	/**
	 * 设置 Buff 的总时长。
	 *
	 * @param InTotalDuration 新的总时长；仅对固定时长 Buff 生效
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Duration")
	void SetTotalDuration(float InTotalDuration);

protected:
	/** 当前实例持有的总时长配置；仅对固定时长 Buff 生效。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Buff|Duration", Meta = (AllowPrivateAccess = "true"))
	float TotalDuration = 0.f;

	/** 当前实例持有的剩余时长；仅对固定时长 Buff 生效。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Buff|Duration", Meta = (AllowPrivateAccess = "true"))
	float RemainingDuration = 0.f;

#pragma endregion


#pragma region Period

public:
	/** @return 如果当前 Buff 声明了周期语义，则返回 true。 */
	UFUNCTION(BlueprintPure, Category = "Buff|Period")
	bool HasPeriod() const;

	/** @return 当前 Buff 的周期触发间隔；未声明周期时返回 0。 */
	UFUNCTION(BlueprintPure, Category = "Buff|Period")
	float GetPeriod() const;

	/**
	 * 设置 Buff 的周期触发间隔。
	 *
	 * @param InPeriod 新的周期触发间隔；小于 0 时会被钳制为 0
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Period")
	void SetPeriod(float InPeriod);

protected:
	/** 当前实例持有的周期触发间隔；0 表示未启用周期语义。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Buff|Period", Meta = (AllowPrivateAccess = "true"))
	float Period = 0.f;

#pragma endregion


#pragma region Stack

public:
    /** @return Buff 合并策略类；如果未配置则返回空。 */
	UFUNCTION(BlueprintPure, Category = "Buff|Stack")
	TSubclassOf<UTcsBuffMerger> GetMergerType() const;

	/** @return 如果当前层数小于最大叠层数，则返回 true。 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Stack")
	bool CanStack() const;

	/** @return 当前叠层数；如果未初始化叠层，则返回 -1。 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Stack")
	int32 GetStackCount() const;

	/** @return 当前 Buff 实例持有的最大叠层数；如果未初始化，则返回 0。 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Stack")
	int32 GetMaxStackCount() const;

	/**
	 * 设置 Buff 的最大叠层数。
	 *
	 * @param InMaxStackCount 新的最大叠层数；小于 1 时会被钳制为 1
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Stack")
	void SetMaxStackCount(int32 InMaxStackCount);

	/**
	 * 将 Buff 的最大叠层数重置为定义资产上的 MaxStackCount 配置值。
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Stack")
	void ResetMaxStackCount();

	UFUNCTION(BlueprintCallable, Category = "Buff|Stack")
	void SetStackCount(int32 InStackCount);

	UFUNCTION(BlueprintCallable, Category = "Buff|Stack")
	void AddStack(int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Buff|Stack")
	void RemoveStack(int32 Count = 1);

protected:
	/** 当前实例持有的最大叠层数。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Buff|Stack", Meta = (AllowPrivateAccess = "true"))
	int32 MaxStackCount = 1;

	/** 当前实例持有的叠层数；未初始化时为 -1。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Buff|Stack", Meta = (AllowPrivateAccess = "true"))
	int32 StackCount = -1;

#pragma endregion

#pragma region Runtime

protected:
	/** 为 BuffStateTree 写入 BuffInstance 根上下文。 */
	virtual bool SetContextRequirements(FStateTreeExecutionContext& Context) override;

	/** 为 BuffStateTree 收集外部数据。 */
	virtual bool CollectExternalData(
		const FStateTreeExecutionContext& Context,
		const UStateTree* StateTree,
		TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs,
		TArrayView<FStateTreeDataView> OutDataViews) override;

	/** 初始化 Buff 运行时参数缓存。 */
	virtual void InitializeRuntimeParameters() override;

#pragma endregion


#pragma region Internal

private:
	/**
	 * 解析当前实例所属 Actor 上的 Buff 宿主组件。
	 *
	 * 如果宿主尚未挂载 BuffComponent，则会在 Buff 模块内部完成创建与初始化。
	 *
	 * @return 当前实例对应的 Buff 宿主组件；失败时返回 nullptr
	 */
	UTcsBuffComponent* ResolveOwnerBuffComponent() const;

#pragma endregion
};