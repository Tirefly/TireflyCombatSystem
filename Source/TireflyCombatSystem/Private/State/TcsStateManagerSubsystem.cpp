// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateManagerSubsystem.h"
#include "State/TcsState.h"
#include "TcsCombatSystemSettings.h"
#include "Engine/DataTable.h"
#include "State/TcsStateComponent.h"

FTcsStateDefinition UTcsStateManagerSubsystem::GetStateDefinition(FName StateDefId)
{
    if (StateDefId.IsNone())
    {
        return FTcsStateDefinition();
    }

    const UTcsCombatSystemSettings* Settings = GetDefault<UTcsCombatSystemSettings>();
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

    // 创建状态实例
    UTcsStateInstance* StateInstance = CreateStateInstance(TargetActor, StateDefRowId, SourceActor);
    if (!StateInstance)
    {
        return false;
    }

    // TODO: 应用 Parameters 到 StateInstance（如有需要，可解析并写入 Numeric/Bool/Vector）

    // 将状态添加到组件并分配到槽位
    if (UTcsStateComponent* StateComponent = TargetActor->FindComponentByClass<UTcsStateComponent>())
    {
        StateComponent->AddStateInstance(StateInstance);

        const FTcsStateDefinition& Def = StateInstance->GetStateDef();
        if (Def.StateSlotType.IsValid())
        {
            // 智能分配到槽位（含排序、合并、激活）
            StateComponent->AssignStateToStateSlot(StateInstance, Def.StateSlotType);
        }
        else
        {
            // 未指定槽位：直接启动其 StateTree（不纳入槽位激活管理）
            StateInstance->InitializeStateTree();
            StateInstance->StartStateTree();
            StateInstance->SetCurrentStage(ETcsStateStage::SS_Active);
        }

        return true;
    }

    // 没有状态组件，退化为直接启动（尽量不丢弃）
    StateInstance->InitializeStateTree();
    StateInstance->StartStateTree();
    StateInstance->SetCurrentStage(ETcsStateStage::SS_Active);
    return true;
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
