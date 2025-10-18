// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateManagerSubsystem.h"
#include "State/TcsState.h"
#include "TcsDeveloperSettings.h"
#include "Engine/DataTable.h"
#include "State/TcsStateComponent.h"
#include "TcsLogChannels.h"
#include "Templates/Function.h"

// 匿名命名空间：参数应用辅助函数
namespace
{
	/**
	 * 从FInstancedStruct中提取参数并应用到StateInstance
	 * 支持FName和GameplayTag两种通道，以及Numeric/Bool/Vector三种参数类型
	 */
	void ApplyParametersToStateInstance(UTcsStateInstance* StateInstance, const FInstancedStruct& Parameters)
	{
		if (!IsValid(StateInstance) || !Parameters.IsValid())
		{
			return;
		}

		const UScriptStruct* ParamStruct = Parameters.GetScriptStruct();
		if (!ParamStruct)
		{
			return;
		}

		const void* ParameterMemory = Parameters.GetMemory();
		bool bAppliedAnyParam = false;

		auto ProcessMap = [ParameterMemory](FMapProperty* MapProperty, const TFunctionRef<void(FScriptMapHelper&, int32)>& Processor)
		{
			if (!MapProperty || !ParameterMemory)
			{
				return;
			}

			void* MapData = MapProperty->ContainerPtrToValuePtr<void>(const_cast<void*>(ParameterMemory));
			if (!MapData)
			{
				return;
			}

			FScriptMapHelper MapHelper(MapProperty, MapData);
			const int32 MaxIndex = MapHelper.GetMaxIndex();
			for (int32 Index = 0; Index < MaxIndex; ++Index)
			{
				if (!MapHelper.IsValidIndex(Index))
				{
					continue;
				}

				Processor(MapHelper, Index);
			}
		};

		for (TFieldIterator<FProperty> It(ParamStruct); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property)
			{
				continue;
			}

			const FString PropertyName = Property->GetName();
			FMapProperty* MapProperty = CastField<FMapProperty>(Property);
			if (!MapProperty)
			{
				continue;
			}

			const bool bIsTagChannel = PropertyName.Contains(TEXT("Tag"));
			const bool bIsNumericChannel = PropertyName.Contains(TEXT("NumericParam"));
			const bool bIsBoolChannel = PropertyName.Contains(TEXT("BoolParam"));
			const bool bIsVectorChannel = PropertyName.Contains(TEXT("VectorParam"));

			if (bIsNumericChannel && !bIsTagChannel)
			{
				FNameProperty* KeyProperty = CastField<FNameProperty>(MapProperty->KeyProp);
				FFloatProperty* ValueProperty = CastField<FFloatProperty>(MapProperty->ValueProp);
				if (!KeyProperty || !ValueProperty)
				{
					continue;
				}

				ProcessMap(MapProperty, [&](FScriptMapHelper& MapHelper, int32 Index)
				{
					const FName* KeyPtr = reinterpret_cast<const FName*>(MapHelper.GetKeyPtr(Index));
					const float* ValuePtr = reinterpret_cast<const float*>(MapHelper.GetValuePtr(Index));
					if (!KeyPtr || !ValuePtr)
					{
						return;
					}

					StateInstance->SetNumericParam(*KeyPtr, *ValuePtr);
					UE_LOG(LogTcsState, VeryVerbose,
						TEXT("[ApplyParameters] Set numeric param: %s = %.2f"),
						*KeyPtr->ToString(), *ValuePtr);
					bAppliedAnyParam = true;
				});
				continue;
			}

			if (bIsBoolChannel && !bIsTagChannel)
			{
				FNameProperty* KeyProperty = CastField<FNameProperty>(MapProperty->KeyProp);
				FBoolProperty* ValueProperty = CastField<FBoolProperty>(MapProperty->ValueProp);
				if (!KeyProperty || !ValueProperty)
				{
					continue;
				}

				ProcessMap(MapProperty, [&](FScriptMapHelper& MapHelper, int32 Index)
				{
					const FName* KeyPtr = reinterpret_cast<const FName*>(MapHelper.GetKeyPtr(Index));
					const uint8* ValuePtr = static_cast<const uint8*>(MapHelper.GetValuePtr(Index));
					if (!KeyPtr || !ValuePtr)
					{
						return;
					}

					const bool bValue = ValueProperty->GetPropertyValue(ValuePtr);
					StateInstance->SetBoolParam(*KeyPtr, bValue);
					UE_LOG(LogTcsState, VeryVerbose,
						TEXT("[ApplyParameters] Set bool param: %s = %s"),
						*KeyPtr->ToString(), bValue ? TEXT("true") : TEXT("false"));
					bAppliedAnyParam = true;
				});
				continue;
			}

			if (bIsVectorChannel && !bIsTagChannel)
			{
				FNameProperty* KeyProperty = CastField<FNameProperty>(MapProperty->KeyProp);
				FStructProperty* ValueProperty = CastField<FStructProperty>(MapProperty->ValueProp);
				if (!KeyProperty || !ValueProperty || ValueProperty->Struct != TBaseStructure<FVector>::Get())
				{
					continue;
				}

				ProcessMap(MapProperty, [&](FScriptMapHelper& MapHelper, int32 Index)
				{
					const FName* KeyPtr = reinterpret_cast<const FName*>(MapHelper.GetKeyPtr(Index));
					const FVector* ValuePtr = reinterpret_cast<const FVector*>(MapHelper.GetValuePtr(Index));
					if (!KeyPtr || !ValuePtr)
					{
						return;
					}

					StateInstance->SetVectorParam(*KeyPtr, *ValuePtr);
					UE_LOG(LogTcsState, VeryVerbose,
						TEXT("[ApplyParameters] Set vector param: %s = [%.2f, %.2f, %.2f]"),
						*KeyPtr->ToString(), ValuePtr->X, ValuePtr->Y, ValuePtr->Z);
					bAppliedAnyParam = true;
				});
				continue;
			}

			if (bIsNumericChannel && bIsTagChannel)
			{
				FStructProperty* KeyProperty = CastField<FStructProperty>(MapProperty->KeyProp);
				FFloatProperty* ValueProperty = CastField<FFloatProperty>(MapProperty->ValueProp);
				if (!KeyProperty || !ValueProperty || KeyProperty->Struct != TBaseStructure<FGameplayTag>::Get())
				{
					continue;
				}

				ProcessMap(MapProperty, [&](FScriptMapHelper& MapHelper, int32 Index)
				{
					const FGameplayTag* KeyPtr = reinterpret_cast<const FGameplayTag*>(MapHelper.GetKeyPtr(Index));
					const float* ValuePtr = reinterpret_cast<const float*>(MapHelper.GetValuePtr(Index));
					if (!KeyPtr || !ValuePtr)
					{
						return;
					}

					StateInstance->SetNumericParamByTag(*KeyPtr, *ValuePtr);
					UE_LOG(LogTcsState, VeryVerbose,
						TEXT("[ApplyParameters] Set numeric param by tag: %s = %.2f"),
						*KeyPtr->ToString(), *ValuePtr);
					bAppliedAnyParam = true;
				});
				continue;
			}

			if (bIsBoolChannel && bIsTagChannel)
			{
				FStructProperty* KeyProperty = CastField<FStructProperty>(MapProperty->KeyProp);
				FBoolProperty* ValueProperty = CastField<FBoolProperty>(MapProperty->ValueProp);
				if (!KeyProperty || !ValueProperty || KeyProperty->Struct != TBaseStructure<FGameplayTag>::Get())
				{
					continue;
				}

				ProcessMap(MapProperty, [&](FScriptMapHelper& MapHelper, int32 Index)
				{
					const FGameplayTag* KeyPtr = reinterpret_cast<const FGameplayTag*>(MapHelper.GetKeyPtr(Index));
					const uint8* ValuePtr = static_cast<const uint8*>(MapHelper.GetValuePtr(Index));
					if (!KeyPtr || !ValuePtr)
					{
						return;
					}

					const bool bValue = ValueProperty->GetPropertyValue(ValuePtr);
					StateInstance->SetBoolParamByTag(*KeyPtr, bValue);
					UE_LOG(LogTcsState, VeryVerbose,
						TEXT("[ApplyParameters] Set bool param by tag: %s = %s"),
						*KeyPtr->ToString(), bValue ? TEXT("true") : TEXT("false"));
					bAppliedAnyParam = true;
				});
				continue;
			}

			if (bIsVectorChannel && bIsTagChannel)
			{
				FStructProperty* KeyProperty = CastField<FStructProperty>(MapProperty->KeyProp);
				FStructProperty* ValueProperty = CastField<FStructProperty>(MapProperty->ValueProp);
				if (!KeyProperty || !ValueProperty ||
					KeyProperty->Struct != TBaseStructure<FGameplayTag>::Get() ||
					ValueProperty->Struct != TBaseStructure<FVector>::Get())
				{
					continue;
				}

				ProcessMap(MapProperty, [&](FScriptMapHelper& MapHelper, int32 Index)
				{
					const FGameplayTag* KeyPtr = reinterpret_cast<const FGameplayTag*>(MapHelper.GetKeyPtr(Index));
					const FVector* ValuePtr = reinterpret_cast<const FVector*>(MapHelper.GetValuePtr(Index));
					if (!KeyPtr || !ValuePtr)
					{
						return;
					}

					StateInstance->SetVectorParamByTag(*KeyPtr, *ValuePtr);
					UE_LOG(LogTcsState, VeryVerbose,
						TEXT("[ApplyParameters] Set vector param by tag: %s = [%.2f, %.2f, %.2f]"),
						*KeyPtr->ToString(), ValuePtr->X, ValuePtr->Y, ValuePtr->Z);
					bAppliedAnyParam = true;
				});
				continue;
			}

			UE_LOG(LogTcsState, Verbose,
				TEXT("[ApplyParameters] Unsupported parameter field '%s' (Type=%s) - skipped."),
				*PropertyName, *Property->GetClass()->GetName());
		}

		if (!bAppliedAnyParam)
		{
			UE_LOG(LogTcsState, Verbose, TEXT("[ApplyParameters] No parameters applied (struct: %s)"), *ParamStruct->GetName());
		}
		else
		{
			UE_LOG(LogTcsState, Log, TEXT("[ApplyParameters] Parameters applied to StateInstance %s"),
				*StateInstance->GetStateDefId().ToString());
		}
	}
}

FTcsStateDefinition UTcsStateManagerSubsystem::GetStateDefinition(FName StateDefId)
{
    if (StateDefId.IsNone())
    {
        return FTcsStateDefinition();
    }

    const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
    if (!Settings || !Settings->StateDefTable.IsValid())
    {
        return FTcsStateDefinition();
    }

    if (UDataTable* Table = Settings->StateDefTable.LoadSynchronous())
    {
        if (const FTcsStateDefinition* Row = Table->FindRow<FTcsStateDefinition>(StateDefId, TEXT("TCS GetStateDefinition"), true))
        {
            return *Row; // copy out
        }
    }

    return FTcsStateDefinition();
}

UTcsStateInstance* UTcsStateManagerSubsystem::CreateStateInstance(AActor* Owner, FName StateDefRowId, AActor* Instigator)
{
    if (!Owner)
    {
        return nullptr;
    }

    // 获取状态定义
    FTcsStateDefinition StateDef = GetStateDefinition(StateDefRowId);
    
    // 创建状态实例
    UTcsStateInstance* StateInstance = NewObject<UTcsStateInstance>(Owner);
    if (StateInstance)
    {
        // 初始化状态实例
        StateInstance->Initialize(StateDef, Owner, Instigator);
        StateInstance->SetStateDefId(StateDefRowId);
        // 标记应用时间戳（用于合并器等逻辑）
        StateInstance->SetApplyTimestamp(FDateTime::UtcNow().GetTicks());
    }
    
    return StateInstance;
}

bool UTcsStateManagerSubsystem::ApplyState(AActor* TargetActor, FName StateDefRowId, AActor* SourceActor, const FInstancedStruct& Parameters)
{
	if (!TargetActor)
	{
		return false;
	}

	FTcsStateDefinition StateDef = GetStateDefinition(StateDefRowId);

	// 创建状态实例
	UTcsStateInstance* StateInstance = CreateStateInstance(TargetActor, StateDefRowId, SourceActor);
	if (!StateInstance)
	{
		return false;
	}

	// 【新增】应用参数到状态实例
	// 支持FName和GameplayTag两种通道，以及Numeric/Bool/Vector三种参数类型
	if (Parameters.IsValid())
	{
		ApplyParametersToStateInstance(StateInstance, Parameters);
	}

	return ApplyStateInstanceToSlot(TargetActor, StateInstance, StateDef.StateSlotType, /*bAllowFallback=*/true);
}

bool UTcsStateManagerSubsystem::ApplyStateToSpecificSlot(AActor* TargetActor, FName StateDefRowId, AActor* SourceActor, FGameplayTag SlotTag, const FInstancedStruct& Parameters)
{
	if (!TargetActor)
	{
		return false;
	}

	UTcsStateInstance* StateInstance = CreateStateInstance(TargetActor, StateDefRowId, SourceActor);
	if (!StateInstance)
	{
		return false;
	}

	// 【新增】应用参数到状态实例
	// 支持FName和GameplayTag两种通道，以及Numeric/Bool/Vector三种参数类型
	if (Parameters.IsValid())
	{
		ApplyParametersToStateInstance(StateInstance, Parameters);
	}

	return ApplyStateInstanceToSlot(TargetActor, StateInstance, SlotTag, /*bAllowFallback=*/true);
}

bool UTcsStateManagerSubsystem::ApplyStateInstanceToSlot(AActor* TargetActor, UTcsStateInstance* StateInstance, FGameplayTag SlotTag, bool bAllowFallback)
{
	if (!TargetActor || !IsValid(StateInstance))
	{
		return false;
	}

	UTcsStateComponent* StateComponent = TargetActor->FindComponentByClass<UTcsStateComponent>();

	// 预初始化 StateTree，确保后续激活流程可正常运行
	StateInstance->InitializeStateTree();
	StateInstance->SetCurrentStage(ETcsStateStage::SS_Inactive);

	if (StateComponent)
	{
		StateComponent->AddStateInstance(StateInstance);

		const FTcsStateDefinition& Def = StateInstance->GetStateDef();
		const FGameplayTag EffectiveSlot = SlotTag.IsValid() ? SlotTag : Def.StateSlotType;
		bool bAssigned = false;

		if (EffectiveSlot.IsValid())
		{
			bAssigned = StateComponent->AssignStateToStateSlot(StateInstance, EffectiveSlot);
			if (!bAssigned && !bAllowFallback)
			{
				UE_LOG(LogTcsState, Warning, TEXT("ApplyStateInstanceToSlot: AssignStateToStateSlot failed for %s on %s (Slot %s)"),
					*StateInstance->GetStateDefId().ToString(),
					*GetNameSafe(TargetActor),
					*EffectiveSlot.ToString());
				return false;
			}
		}
		else
		{
			UE_LOG(LogTcsState, Verbose, TEXT("ApplyStateInstanceToSlot: State %s has no slot, activating directly on %s"),
				*StateInstance->GetStateDefId().ToString(),
				*GetNameSafe(TargetActor));
		}

		if (!bAssigned)
		{
			// 直接激活（与阶段2之前的行为保持一致）
			const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();
			StateInstance->StartStateTree();
			StateInstance->SetCurrentStage(ETcsStateStage::SS_Active);
			StateComponent->NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Active);
		}

		return true;
	}

	// 没有状态组件，按原行为返回失败（技能组件会记录错误）
	UE_LOG(LogTcsState, Warning, TEXT("ApplyStateInstanceToSlot: Actor %s has no TcsStateComponent, cannot apply state %s"),
		*GetNameSafe(TargetActor),
		*StateInstance->GetStateDefId().ToString());

	if (!bAllowFallback)
	{
		return false;
	}

	// 最低限度地运行 StateTree，以避免静默失败
	StateInstance->StartStateTree();
	StateInstance->SetCurrentStage(ETcsStateStage::SS_Active);
	return bAllowFallback;
}

void UTcsStateManagerSubsystem::OnStateInstanceDurationExpired(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	// 处理状态实例移除
	HandleStateInstanceRemoval(StateInstance);
}

void UTcsStateManagerSubsystem::OnStateInstanceDurationUpdated(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	// 处理状态实例持续时间更新
	HandleStateInstanceDurationUpdate(StateInstance);
}

void UTcsStateManagerSubsystem::HandleStateInstanceRemoval(UTcsStateInstance* StateInstance)
{
	/*
	 * 状态实例移除逻辑伪代码：
	 * 
	 * 1. 检查状态实例是否有效
	 *    - 如果无效，直接返回
	 * 
	 * 2. 设置状态实例阶段为过期
	 *    - StateInstance->SetCurrentStage(ETcsStateStage::SS_Expired)
	 * 
	 * 3. 执行状态移除前的清理工作
	 *    - 停止状态树执行（如果有）
	 *    - 清理状态相关的定时器
	 *    - 移除状态相关的效果
	 * 
	 * 4. 通知相关系统状态即将被移除
	 *    - 发送状态移除事件
	 *    - 通知属性系统移除状态相关的属性修改器
	 *    - 通知技能系统状态已过期
	 * 
	 * 5. 从状态管理器中移除状态实例
	 *    - 从状态列表/映射中移除
	 *    - 从状态槽中移除
	 * 
	 * 6. 执行状态移除后的清理工作
	 *    - 清理状态实例的引用
	 *    - 标记状态实例为待GC
	 *    - StateInstance->MarkPendingGC()
	 * 
	 * 7. 触发状态移除的回调/事件
	 *    - 通知UI系统更新状态显示
	 *    - 播放状态移除的音效/特效
	 *    - 记录状态移除的日志
	 */

	// TODO: 实现具体的状态实例移除逻辑
	UE_LOG(LogTemp, Log, TEXT("State instance %s duration expired, handling removal..."), 
		StateInstance ? *StateInstance->GetStateDefId().ToString() : TEXT("Invalid"));
}

void UTcsStateManagerSubsystem::HandleStateInstanceDurationUpdate(UTcsStateInstance* StateInstance)
{
	/*
	 * 状态实例持续时间更新逻辑伪代码：
	 *
	 * 1. 检查状态实例是否有效
	 *    - 如果无效，直接返回
	 *
	 * 2. 验证持续时间更新的合法性
	 *    - 检查状态是否处于可更新持续时间的阶段
	 *    - 验证新的持续时间值是否合理
	 *
	 * 3. 更新状态相关的系统
	 *    - 更新状态树的持续时间参数
	 *    - 更新属性修改器的持续时间
	 *    - 更新技能效果的持续时间
	 *
	 * 4. 通知相关系统持续时间已更新
	 *    - 发送状态持续时间更新事件
	 *    - 通知UI系统更新持续时间显示
	 *    - 更新状态图标/进度条
	 *
	 * 5. 记录持续时间更新日志
	 *    - 记录更新的原因（手动刷新/强制设置）
	 *    - 记录更新前后的持续时间值
	 *    - 记录更新的时间戳
	 *
	 * 6. 触发持续时间更新的回调
	 *    - 播放持续时间更新的音效/特效
	 *    - 执行持续时间更新相关的游戏逻辑
	 */

	// TODO: 实现具体的状态实例持续时间更新逻辑
	UE_LOG(LogTemp, Log, TEXT("State instance %s duration updated"),
		StateInstance ? *StateInstance->GetStateDefId().ToString() : TEXT("Invalid"));
}

FTcsStateApplyResult UTcsStateManagerSubsystem::ApplyStateWithDetails(AActor* TargetActor, FName StateDefId, AActor* SourceActor, const FInstancedStruct& Parameters)
{
	FTcsStateApplyResult Result;
	Result.bSuccess = false;

	// 检查Owner有效性
	if (!IsValid(TargetActor))
	{
		Result.FailureReason = ETcsStateApplyFailureReason::SAFR_InvalidOwner;
		Result.FailureMessage = TEXT("Target actor is invalid");

		UE_LOG(LogTcsState, Warning, TEXT("[ApplyStateWithDetails] Target actor is invalid"));
		return Result;
	}

	// 获取TcsStateComponent
	UTcsStateComponent* StateComponent = TargetActor->FindComponentByClass<UTcsStateComponent>();
	if (!StateComponent)
	{
		Result.FailureReason = ETcsStateApplyFailureReason::SAFR_ComponentMissing;
		Result.FailureMessage = TEXT("TcsStateComponent not found on target actor");

		UE_LOG(LogTcsState, Warning, TEXT("[ApplyStateWithDetails] Actor [%s] missing TcsStateComponent"), *TargetActor->GetName());

		// 广播失败事件
		return Result;
	}

	// 获取状态定义
	FTcsStateDefinition StateDef = GetStateDefinition(StateDefId);
	if (!StateDef.StateSlotType.IsValid() && StateDef.StateType == 0 && StateDef.Priority == -1)
	{
		// 检查是否实际存在于数据表中（简单启发式检查）
		const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
		if (!Settings || !Settings->StateDefTable.IsValid())
		{
			Result.FailureReason = ETcsStateApplyFailureReason::SAFR_InvalidState;
			Result.FailureMessage = TEXT("State definition not found");

			UE_LOG(LogTcsState, Warning, TEXT("[ApplyStateWithDetails] State definition not found: %s"), *StateDefId.ToString());

			// 广播失败事件
			StateComponent->OnStateApplyFailed.Broadcast(TargetActor, StateDefId, Result.FailureReason, Result.FailureMessage);
			return Result;
		}

		if (UDataTable* Table = Settings->StateDefTable.LoadSynchronous())
		{
			if (!Table->FindRow<FTcsStateDefinition>(StateDefId, TEXT(""), false))
			{
				Result.FailureReason = ETcsStateApplyFailureReason::SAFR_InvalidState;
				Result.FailureMessage = TEXT("State definition not found in data table");

				UE_LOG(LogTcsState, Warning, TEXT("[ApplyStateWithDetails] State definition not found: %s"), *StateDefId.ToString());

				// 广播失败事件
				StateComponent->OnStateApplyFailed.Broadcast(TargetActor, StateDefId, Result.FailureReason, Result.FailureMessage);
				return Result;
			}
		}
	}

	// 创建状态实例
	UTcsStateInstance* StateInstance = CreateStateInstance(TargetActor, StateDefId, SourceActor);
	if (!StateInstance)
	{
		Result.FailureReason = ETcsStateApplyFailureReason::SAFR_Unknown;
		Result.FailureMessage = TEXT("Failed to create state instance");

		UE_LOG(LogTcsState, Error, TEXT("[ApplyStateWithDetails] Failed to create state instance for %s"), *StateDefId.ToString());

		// 广播失败事件
		StateComponent->OnStateApplyFailed.Broadcast(TargetActor, StateDefId, Result.FailureReason, Result.FailureMessage);
		return Result;
	}

	// 应用参数到状态实例
	if (Parameters.IsValid())
	{
		ApplyParametersToStateInstance(StateInstance, Parameters);
	}

	// 尝试应用到槽位
	const FGameplayTag EffectiveSlot = StateDef.StateSlotType.IsValid() ? StateDef.StateSlotType : FGameplayTag();

	if (StateComponent->AssignStateToStateSlot(StateInstance, EffectiveSlot))
	{
		// 成功应用
		Result.bSuccess = true;
		Result.CreatedStateInstance = StateInstance;
		Result.TargetSlot = EffectiveSlot;
		Result.AppliedStage = StateInstance->GetCurrentStage();

		UE_LOG(LogTcsState, Log, TEXT("[ApplyStateWithDetails] State %s successfully applied to actor %s at slot %s, stage: %d"),
			*StateDefId.ToString(), *TargetActor->GetName(), *EffectiveSlot.ToString(), (int32)Result.AppliedStage);

		// 广播成功事件
		StateComponent->OnStateApplySuccess.Broadcast(TargetActor, StateDefId, StateInstance, EffectiveSlot, Result.AppliedStage);
		return Result;
	}

	// 应用失败 - 槽位被占用或其他原因
	Result.FailureReason = ETcsStateApplyFailureReason::SAFR_SlotOccupied;
	Result.FailureMessage = TEXT("Failed to assign state to slot (slot occupied or assignment failed)");

	UE_LOG(LogTcsState, Warning, TEXT("[ApplyStateWithDetails] Failed to apply state %s to actor %s at slot %s"),
		*StateDefId.ToString(), *TargetActor->GetName(), *EffectiveSlot.ToString());

	// 广播失败事件
	StateComponent->OnStateApplyFailed.Broadcast(TargetActor, StateDefId, Result.FailureReason, Result.FailureMessage);
	return Result;
}

FTcsStateApplyResult UTcsStateManagerSubsystem::ApplyStateToSpecificSlotWithDetails(AActor* TargetActor, FName StateDefId, AActor* SourceActor, FGameplayTag SlotTag, const FInstancedStruct& Parameters)
{
	FTcsStateApplyResult Result;
	Result.bSuccess = false;

	// 检查Owner有效性
	if (!IsValid(TargetActor))
	{
		Result.FailureReason = ETcsStateApplyFailureReason::SAFR_InvalidOwner;
		Result.FailureMessage = TEXT("Target actor is invalid");

		UE_LOG(LogTcsState, Warning, TEXT("[ApplyStateToSpecificSlotWithDetails] Target actor is invalid"));
		return Result;
	}

	// 获取TcsStateComponent
	UTcsStateComponent* StateComponent = TargetActor->FindComponentByClass<UTcsStateComponent>();
	if (!StateComponent)
	{
		Result.FailureReason = ETcsStateApplyFailureReason::SAFR_ComponentMissing;
		Result.FailureMessage = TEXT("TcsStateComponent not found on target actor");

		UE_LOG(LogTcsState, Warning, TEXT("[ApplyStateToSpecificSlotWithDetails] Actor [%s] missing TcsStateComponent"), *TargetActor->GetName());
		return Result;
	}

	// 获取状态定义
	FTcsStateDefinition StateDef = GetStateDefinition(StateDefId);
	if (!StateDef.StateSlotType.IsValid() && StateDef.StateType == 0 && StateDef.Priority == -1)
	{
		Result.FailureReason = ETcsStateApplyFailureReason::SAFR_InvalidState;
		Result.FailureMessage = TEXT("State definition not found");

		UE_LOG(LogTcsState, Warning, TEXT("[ApplyStateToSpecificSlotWithDetails] State definition not found: %s"), *StateDefId.ToString());

		// 广播失败事件
		StateComponent->OnStateApplyFailed.Broadcast(TargetActor, StateDefId, Result.FailureReason, Result.FailureMessage);
		return Result;
	}

	// 创建状态实例
	UTcsStateInstance* StateInstance = CreateStateInstance(TargetActor, StateDefId, SourceActor);
	if (!StateInstance)
	{
		Result.FailureReason = ETcsStateApplyFailureReason::SAFR_Unknown;
		Result.FailureMessage = TEXT("Failed to create state instance");

		UE_LOG(LogTcsState, Error, TEXT("[ApplyStateToSpecificSlotWithDetails] Failed to create state instance for %s"), *StateDefId.ToString());

		// 广播失败事件
		StateComponent->OnStateApplyFailed.Broadcast(TargetActor, StateDefId, Result.FailureReason, Result.FailureMessage);
		return Result;
	}

	// 应用参数到状态实例
	if (Parameters.IsValid())
	{
		ApplyParametersToStateInstance(StateInstance, Parameters);
	}

	// 尝试应用到指定槽位
	if (StateComponent->AssignStateToStateSlot(StateInstance, SlotTag))
	{
		// 成功应用
		Result.bSuccess = true;
		Result.CreatedStateInstance = StateInstance;
		Result.TargetSlot = SlotTag;
		Result.AppliedStage = StateInstance->GetCurrentStage();

		UE_LOG(LogTcsState, Log, TEXT("[ApplyStateToSpecificSlotWithDetails] State %s successfully applied to actor %s at slot %s, stage: %d"),
			*StateDefId.ToString(), *TargetActor->GetName(), *SlotTag.ToString(), (int32)Result.AppliedStage);

		// 广播成功事件
		StateComponent->OnStateApplySuccess.Broadcast(TargetActor, StateDefId, StateInstance, SlotTag, Result.AppliedStage);
		return Result;
	}

	// 应用失败 - 槽位被占用或其他原因
	Result.FailureReason = ETcsStateApplyFailureReason::SAFR_SlotOccupied;
	Result.FailureMessage = TEXT("Failed to assign state to slot (slot occupied or assignment failed)");

	UE_LOG(LogTcsState, Warning, TEXT("[ApplyStateToSpecificSlotWithDetails] Failed to apply state %s to actor %s at slot %s"),
		*StateDefId.ToString(), *TargetActor->GetName(), *SlotTag.ToString());

	// 广播失败事件
	StateComponent->OnStateApplyFailed.Broadcast(TargetActor, StateDefId, Result.FailureReason, Result.FailureMessage);
	return Result;
}
