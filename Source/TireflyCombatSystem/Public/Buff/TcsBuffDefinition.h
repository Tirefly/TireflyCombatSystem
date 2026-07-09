// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buff/TcsBuffTypes.h"
#include "State/TcsStateDefinition.h"
#include "TcsBuffDefinition.generated.h"



class UTcsBuffInstance;



/**
 * Buff 叠层上涨后的持续时间刷新策略。
 */
UENUM(BlueprintType)
enum class ETcsBuffDurationRefreshPolicy : uint8
{
	/** 不刷新持续时间。 */
	None,

	/** 将剩余持续时间刷新为总持续时间。 */
	RefreshRemainingToTotal,
};



/**
 * Buff 持续时间耗尽后的处理策略。
 */
UENUM(BlueprintType)
enum class ETcsBuffStackExpirationPolicy : uint8
{
	/** 直接移除整个 Buff。 */
	ClearEntireBuff,

	/** 仅移除一层叠层。 */
	RemoveSingleStack,

	/** 移除一层叠层，并刷新剩余持续时间。 */
	RemoveSingleStackAndRefreshDuration,
};



/**
 * Buff 叠层上涨后的增量反应配置。
 */
USTRUCT(BlueprintType)
struct FTcsBuffOnStackIncreasePolicy
{
	GENERATED_BODY()

	/** 叠层上涨后的持续时间刷新策略。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack")
	ETcsBuffDurationRefreshPolicy DurationPolicy = ETcsBuffDurationRefreshPolicy::None;
};



/**
 * Buff 持续时间耗尽后的增量反应配置。
 */
USTRUCT(BlueprintType)
struct FTcsBuffOnDurationExpiredPolicy
{
	GENERATED_BODY()

	/** 持续时间耗尽后的处理策略。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duration")
	ETcsBuffStackExpirationPolicy ExpirationPolicy = ETcsBuffStackExpirationPolicy::ClearEntireBuff;
};



/**
 * Buff 定义资产
 *
 * 用途: 承载 Buff 专属的持续时间、叠层与合并配置
 */
UCLASS(BlueprintType, Const)
class TIREFLYCOMBATSYSTEM_API UTcsBuffDefinition : public UTcsStateDefinition
{
	GENERATED_BODY()

public:
	/** 构造默认 Buff 合并策略。 */
	UTcsBuffDefinition();

	/** Buff Definition 的 PrimaryAssetType 标识符。 */
	static const FPrimaryAssetType PrimaryAssetType;

	/** @return Buff Definition 的 PrimaryAssetId。 */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;


#pragma region Duration

public:
	/**
	 * 持续时间类型
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duration")
	TEnumAsByte<ETcsBuffDurationType> DurationType = SDT_None;

	/**
	 * 持续时间
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duration",
		Meta = (EditConditionHides, EditCondition = "DurationType == ETcsBuffDurationType::SDT_Duration"))
	float Duration = 0.f;

#pragma endregion


#pragma region Period

public:
	/**
	 * 周期触发间隔；0 表示当前 Buff 不声明周期语义。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Period", 
		Meta = (ClampMin = "0.0", EditConditionHides, EditCondition = "DurationType != ETcsBuffDurationType::SDT_None"))
	float Period = 0.f;

#pragma endregion


#pragma region Stack

public:
	/**
	 * 最大叠层数
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack", Meta = (ClampMin = "1"))
	int32 MaxStackCount = 1;

	/**
	 * Buff 合并策略
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack")
	TSubclassOf<class UTcsBuffMerger> MergerType;

	/**
	 * 叠层上涨时的增量反应配置。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack",
		Meta = (EditConditionHides, EditCondition = "MaxStackCount > 1"))
	FTcsBuffOnStackIncreasePolicy OnStackIncrease;

	/**
	 * 持续时间耗尽时的增量反应配置。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack",
		Meta = (EditConditionHides, EditCondition = "MaxStackCount > 1 && DurationType == ETcsBuffDurationType::SDT_Duration"))
	FTcsBuffOnDurationExpiredPolicy OnDurationExpired;

#pragma endregion


#pragma region Runtime

public:
	/** Buff 激活时创建的运行时实例类。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime")
	TSubclassOf<UTcsBuffInstance> BuffInstanceClass;

	/** @return Buff 定义对应的运行时实例类。 */
	virtual UClass* ResolveStateInstanceClass() const override;

#pragma endregion


#pragma region Editor

public:
#if WITH_EDITOR
	/**
	 * 编辑器验证：属性值变更时的验证
	 *
	 * @param PropertyChangedEvent 变更事件
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/**
	 * 编辑器验证：数据有效性检查
	 *
	 * @param Context 验证上下文
	 * @return 数据验证结果
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

#pragma endregion
};
