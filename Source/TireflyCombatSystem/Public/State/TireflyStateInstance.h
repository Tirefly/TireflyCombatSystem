// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "StateTreeTypes.h"
#include "TireflyStateInstance.generated.h"

// 状态阶段
UENUM(BlueprintType)
enum class ETireflyStateStage : uint8
{
	Inactive = 0	UMETA(DisplayName = "Inactive", ToolTip = "未激活"),
	Active			UMETA(DisplayName = "Active", ToolTip = "已激活"),
	Expired			UMETA(DisplayName = "Expired", ToolTip = "已过期"),
};

// 状态实例
UCLASS(BlueprintType)
class TIREFLYCOMBATSYSTEM_API UTireflyStateInstance : public UObject
{
	GENERATED_BODY()

public:
	// 状态数据
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State")
	FTireflyStateDefinition StateDef;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State")
	int32 InstanceId = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State")
	int32 Level = -1;

	// 生命周期
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifecycle")
	int64 ApplyTimestamp = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifecycle")
	int64 UpdateTimestamp = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifecycle")
	ETireflyStateStage Stage = ETireflyStateStage::Inactive;

	// 运行时数据
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime")
	TWeakObjectPtr<AActor> Owner;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime")
	TMap<FName, float> RuntimeParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime")
	float DurationRemaining = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime")
	int32 StackCount = 1;

	// 依赖和互斥
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dependency")
	TSet<FName> RequiredStates;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dependency")
	TSet<FName> ImmunityStates;

	// 状态树数据
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StateTree")
	FStateTreeInstanceData StateTreeInstanceData;

public:
	// 初始化状态实例
	void Initialize(const FTireflyStateDefinition& InStateDef, AActor* InOwner, int32 InLevel = -1);

	// 更新状态实例
	void Update(float DeltaTime);

	// 检查状态是否过期
	bool IsExpired() const;

	// 检查状态是否可以叠加
	bool CanStack() const;

	// 获取状态剩余时间
	float GetRemainingTime() const;

	// 获取状态总持续时间
	float GetTotalDuration() const;

	// 获取状态等级
	int32 GetLevel() const { return Level; }

	// 获取状态实例ID
	int32 GetInstanceId() const { return InstanceId; }

	// 获取状态拥有者
	AActor* GetOwner() const { return Owner.Get(); }

	// 获取状态阶段
	ETireflyStateStage GetStage() const { return Stage; }

	// 获取状态叠层数
	int32 GetStackCount() const { return StackCount; }

	// 设置状态叠层数
	void SetStackCount(int32 InStackCount);

	// 增加状态叠层数
	void AddStack(int32 Count = 1);

	// 减少状态叠层数
	void RemoveStack(int32 Count = 1);

	// 获取运行时参数
	float GetRuntimeParameter(FName ParameterName) const;

	// 设置运行时参数
	void SetRuntimeParameter(FName ParameterName, float Value);

	// 检查是否依赖指定状态
	bool HasRequiredState(FName StateName) const;

	// 检查是否免疫指定状态
	bool HasImmunityState(FName StateName) const;

	// 添加依赖状态
	void AddRequiredState(FName StateName);

	// 添加免疫状态
	void AddImmunityState(FName StateName);

	// 移除依赖状态
	void RemoveRequiredState(FName StateName);

	// 移除免疫状态
	void RemoveImmunityState(FName StateName);
}; 