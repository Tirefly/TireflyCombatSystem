# Design: 状态槽位激活完整性重构

本文档详细描述技术设计决策和实现方案。

## 1. 合并移除统一化

### 1.1 问题分析

**当前实现**:
```cpp
// 合并逻辑中直接 Finalize
if (ShouldMergeOut(ExistingState, NewState))
{
    FinalizeStateRemoval(ExistingState);  // 直接 Finalize
}
```

**问题**:
- `FinalizeStateRemoval` 会触发 `UpdateStateSlotActivation`
- 如果在 `UpdateStateSlotActivation` 执行过程中发生合并，就会递归调用
- 递归调用导致状态不确定

### 1.2 设计方案

**核心思路**: 所有移除都走统一路径

```cpp
// 新实现
if (ShouldMergeOut(ExistingState, NewState))
{
    RequestStateRemoval(
        ExistingState,
        ETcsStateRemovalReason::Custom,
        FName("MergedOut")
    );
}
```

**优点**:
- ✅ 单一移除路径，易于维护
- ✅ StateTree 退场逻辑有机会执行
- ✅ 避免直接 Finalize 导致的再入

**缺点**:
- ❌ 移除可能延迟一帧（可接受，因为保证了正确性）

**决策**: 采用此方案，正确性优先

---

## 2. 槽位激活去再入

### 2.1 问题分析

**当前行为**:
```
UpdateStateSlotActivation(SlotA)
  └─> 状态变化触发事件
      └─> 事件处理导致另一个状态变化
          └─> UpdateStateSlotActivation(SlotB)  // 递归调用
              └─> ...
```

**问题**:
- 递归深度不确定
- 状态更新顺序不确定
- 难以保证不变量

### 2.2 设计方案

**核心思路**: 延迟请求 + 队列排空

```cpp
class UTcsStateManagerSubsystem
{
    // 标志：是否正在更新槽位激活
    bool bIsUpdatingSlotActivation = false;

    // 待处理的槽位激活更新请求
    // 注意：实际实现使用 Map<Component, Set<Tag>> 而非 Set<Tag>
    // 原因：多个 Actor 可能共享相同的 SlotTag，需要区分不同的 Component
    TMap<TWeakObjectPtr<UTcsStateComponent>, TSet<FGameplayTag>> PendingSlotActivationUpdates;

    // 请求更新槽位激活（公开接口）
    void RequestUpdateStateSlotActivation(
        UTcsStateComponent* StateComponent,
        FGameplayTag SlotTag)
    {
        if (bIsUpdatingSlotActivation)
        {
            // 正在更新，延迟请求
            PendingSlotActivationUpdates.FindOrAdd(StateComponent).Add(SlotTag);
        }
        else
        {
            // 不在更新，立即执行
            UpdateStateSlotActivation(StateComponent, SlotTag);
        }
    }

private:
    void UpdateStateSlotActivation(
        UTcsStateComponent* StateComponent,
        FGameplayTag SlotTag)
    {
        bIsUpdatingSlotActivation = true;

        // 执行槽位激活更新
        DoUpdateStateSlotActivation(StateComponent, SlotTag);

        bIsUpdatingSlotActivation = false;

        // 排空队列
        DrainPendingSlotActivationUpdates();
    }

    void DrainPendingSlotActivationUpdates()
    {
        const int32 MaxIterations = 10;
        int32 Iteration = 0;

        while (!PendingSlotActivationUpdates.IsEmpty() && Iteration < MaxIterations)
        {
            // 拷贝队列（避免遍历时修改）
            TMap<TWeakObjectPtr<UTcsStateComponent>, TSet<FGameplayTag>> ToProcess = PendingSlotActivationUpdates;
            PendingSlotActivationUpdates.Empty();

            for (const auto& Pair : ToProcess)
            {
                UTcsStateComponent* Component = Pair.Key.Get();
                const TSet<FGameplayTag>& SlotTags = Pair.Value;

                if (IsValid(Component))
                {
                    for (const FGameplayTag& Tag : SlotTags)
                    {
                        UpdateStateSlotActivation(Component, Tag);
                    }
                }
            }

            Iteration++;
        }

        if (Iteration >= MaxIterations)
        {
            UE_LOG(LogTcsState, Warning,
                TEXT("[DrainPendingSlotActivationUpdates] Max iterations reached, possible infinite loop"));
        }
    }
};
```

**数据结构设计说明**:

**方案对比**:
| 方案 | 数据结构 | 优点 | 缺点 |
|------|---------|------|------|
| ❌ 方案A | `TMap<FGameplayTag, TWeakObjectPtr<UTcsStateComponent>>` | 简单 | **多 Actor 共享相同 SlotTag 时会覆盖** |
| ✅ 方案B | `TMap<TWeakObjectPtr<UTcsStateComponent>, TSet<FGameplayTag>>` | 无覆盖风险，支持批量处理 | 稍复杂 |

**Bug 修复说明**:
- 原设计文档中简化示例使用了 `TSet<FGameplayTag>`
- 实际实现时错误地使用了 `TMap<FGameplayTag, TWeakObjectPtr<UTcsStateComponent>>`（方案A）
- **问题**: 多个 Actor 共享相同 SlotTag（如 `Combat.Attack`）时，Map key 冲突导致后来的请求覆盖前面的请求
- **修复**: 改用 `TMap<TWeakObjectPtr<UTcsStateComponent>, TSet<FGameplayTag>>`（方案B）
- **影响**: 修复了多 Actor 战斗场景下状态槽激活更新丢失的严重 bug

**优点**:
- ✅ 消除递归调用
- ✅ 状态更新顺序确定
- ✅ 有限步收敛
- ✅ 易于调试
- ✅ 支持多 Actor 场景（修复后）

**缺点**:
- ❌ 增加了一些复杂度（可接受）

**决策**: 采用方案B（已修复）

---

## 3. 同优先级 tie-break 策略化

### 3.1 问题分析

**当前实现**:
```cpp
// 硬编码的排序规则
States.Sort([](const UTcsStateInstance* A, const UTcsStateInstance* B)
{
    if (A->Priority != B->Priority)
        return A->Priority > B->Priority;

    // 同优先级：硬编码使用 ApplyTimestamp
    return A->ApplyTimestamp > B->ApplyTimestamp;
});
```

**问题**:
- 规则硬编码，无法扩展
- 不同槽位可能需要不同规则（Buff vs Skill）
- 修改规则需要修改核心代码

### 3.2 设计方案

**核心思路**: 策略对象（UObject CDO）

#### 策略接口

```cpp
// 基类
UCLASS(Abstract, BlueprintType)
class TIREFLYCOMBATSYSTEM_API UTcsStateSamePriorityPolicy : public UObject
{
    GENERATED_BODY()

public:
    /**
     * 获取排序键
     *
     * @param State 状态实例
     * @return 排序键（越大越靠前）
     */
    UFUNCTION(BlueprintNativeEvent, Category = "TireflyCombatSystem|State")
    int64 GetOrderKey(const UTcsStateInstance* State) const;
    virtual int64 GetOrderKey_Implementation(const UTcsStateInstance* State) const;

    /**
     * 是否接受新状态（可选，用于队列上限）
     *
     * @param NewState 新状态
     * @param ExistingSamePriority 现有同优先级状态列表
     * @param OutFailReason 失败原因（输出）
     * @return 是否接受
     */
    UFUNCTION(BlueprintNativeEvent, Category = "TireflyCombatSystem|State")
    bool ShouldAcceptNewState(
        const UTcsStateInstance* NewState,
        const TArray<UTcsStateInstance*>& ExistingSamePriority,
        FName& OutFailReason) const;
    virtual bool ShouldAcceptNewState_Implementation(
        const UTcsStateInstance* NewState,
        const TArray<UTcsStateInstance*>& ExistingSamePriority,
        FName& OutFailReason) const;
};
```

#### 内置策略

**UseNewest 策略**（Buff 友好）:
```cpp
UCLASS()
class UTcsStateSamePriorityPolicy_UseNewest : public UTcsStateSamePriorityPolicy
{
    GENERATED_BODY()

public:
    virtual int64 GetOrderKey_Implementation(const UTcsStateInstance* State) const override
    {
        // 使用 ApplyTimestamp，越新越靠前
        return State->ApplyTimestamp;
    }
};
```

**UseOldest 策略**（技能队列友好）:
```cpp
UCLASS()
class UTcsStateSamePriorityPolicy_UseOldest : public UTcsStateSamePriorityPolicy
{
    GENERATED_BODY()

public:
    virtual int64 GetOrderKey_Implementation(const UTcsStateInstance* State) const override
    {
        // 使用负的 ApplyTimestamp，越旧越靠前
        return -State->ApplyTimestamp;
    }
};
```

#### 槽位定义集成

```cpp
USTRUCT(BlueprintType)
struct FTcsStateSlotDefinition
{
    GENERATED_BODY()

    // ... 现有字段 ...

    /**
     * 同优先级排序策略
     * 用于 PriorityOnly 模式下，多个 state 同优先级时的排序
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
    TSubclassOf<UTcsStateSamePriorityPolicy> SamePriorityPolicy;

    FTcsStateSlotDefinition()
    {
        // 默认使用 UseNewest 策略
        SamePriorityPolicy = UTcsStateSamePriorityPolicy_UseNewest::StaticClass();
    }
};
```

#### 排序逻辑

```cpp
void SortStatesBySamePriorityPolicy(
    TArray<UTcsStateInstance*>& States,
    const FTcsStateSlotDefinition& SlotDef)
{
    if (!SlotDef.SamePriorityPolicy)
    {
        // 无策略，使用默认排序
        return;
    }

    const UTcsStateSamePriorityPolicy* Policy = SlotDef.SamePriorityPolicy->GetDefaultObject<UTcsStateSamePriorityPolicy>();

    States.Sort([Policy](const UTcsStateInstance* A, const UTcsStateInstance* B)
    {
        int64 KeyA = Policy->GetOrderKey(A);
        int64 KeyB = Policy->GetOrderKey(B);
        return KeyA > KeyB;  // 降序
    });
}
```

**优点**:
- ✅ 高度可扩展
- ✅ 支持蓝图自定义策略
- ✅ 不同槽位可使用不同策略
- ✅ 零代码扩展

**缺点**:
- ❌ 增加了一些复杂度（可接受）

**决策**: 采用此方案

---

## 4. Gate 关闭逻辑重构

### 4.1 问题分析

**当前实现**:
- Gate 关闭逻辑分散在多个函数中
- 每个函数都重复实现相同的行为
- 不变量检查不一致

### 4.2 设计方案

**核心思路**: 单一权威函数

```cpp
class UTcsStateManagerSubsystem
{
private:
    /**
     * 处理 Gate 关闭
     *
     * @param StateComponent 状态组件
     * @param SlotTag 槽位标签
     * @param SlotDef 槽位定义
     * @param Slot 槽位实例
     */
    void HandleGateClosed(
        UTcsStateComponent* StateComponent,
        FGameplayTag SlotTag,
        const FTcsStateSlotDefinition& SlotDef,
        FTcsStateSlot& Slot)
    {
        // 1. 应用 GateCloseBehavior
        switch (SlotDef.GateCloseBehavior)
        {
        case ETcsGateCloseBehavior::HangUp:
            {
                // 挂起所有 Active 状态
                for (UTcsStateInstance* State : Slot.States)
                {
                    if (State->Stage == ETcsStateStage::SS_Active)
                    {
                        State->Stage = ETcsStateStage::SS_HangUp;
                        // 触发 StateTree 挂起逻辑
                    }
                }
                break;
            }
        case ETcsGateCloseBehavior::Pause:
            {
                // 暂停所有 Active 状态
                for (UTcsStateInstance* State : Slot.States)
                {
                    if (State->Stage == ETcsStateStage::SS_Active)
                    {
                        // 触发 StateTree 暂停逻辑
                    }
                }
                break;
            }
        case ETcsGateCloseBehavior::Cancel:
            {
                // 取消所有 Active 状态
                TArray<UTcsStateInstance*> ToRemove;
                for (UTcsStateInstance* State : Slot.States)
                {
                    if (State->Stage == ETcsStateStage::SS_Active)
                    {
                        ToRemove.Add(State);
                    }
                }
                for (UTcsStateInstance* State : ToRemove)
                {
                    RequestStateRemoval(State, ETcsStateRemovalReason::Cancelled);
                }
                break;
            }
        }

        // 2. 保证不变量：Gate 关闭时，Slot 内不允许存在 Active state
        #if !UE_BUILD_SHIPPING
        for (const UTcsStateInstance* State : Slot.States)
        {
            check(State->Stage != ETcsStateStage::SS_Active);
        }
        #endif
    }
};
```

**调用点**:
- 只在 `UpdateStateSlotActivation` 中调用
- 移除所有其他重复路径

**优点**:
- ✅ 单一权威函数
- ✅ 行为一致
- ✅ 易于维护
- ✅ 不变量保证

**缺点**:
- 无明显缺点

**决策**: 采用此方案

---

## 5. 性能考虑

### 5.1 延迟请求机制性能

**最坏情况**:
- 槽位数量: N
- 最大迭代次数: M (默认 10)
- 复杂度: O(N * M)

**优化**:
- 使用 TSet 避免重复请求
- 迭代通常 1-2 次就收敛
- 可以通过配置调整最大迭代次数

**预期影响**:
- 对于典型场景（5-10 个槽位），影响可忽略
- 对于极端场景（100+ 个槽位），可能需要优化

### 5.2 策略排序性能

**复杂度**:
- 排序: O(N log N)，N 为同优先级状态数量

**优化**:
- 只对同优先级组排序，不是所有状态
- 使用 CDO 避免每次创建策略对象

**预期影响**:
- 对于典型场景（每组 2-5 个状态），影响可忽略

---

## 6. 向后兼容性

### 6.1 API 兼容性

**不变**:
- 所有公开 API 签名不变
- 蓝图调用不受影响

**行为变化**:
- 合并移除可能延迟一帧（更安全）
- 槽位激活更新顺序更确定（更可预测）
- 同优先级排序可配置（需要为现有槽位选择策略）

### 6.2 迁移策略

**Phase 1**: 修复合并移除
- 运行测试，验证 StateTree 退场逻辑
- 修复任何依赖直接 Finalize 的代码

**Phase 2**: 去再入
- 运行测试，验证状态更新顺序
- 修复任何依赖特定时序的代码

**Phase 3**: 策略化
- 为现有槽位选择合适的默认策略
- 测试不同策略的影响

---

## 7. 测试策略

### 7.1 单元测试

**合并移除**:
- 合并淘汰使用 RequestStateRemoval
- StateTree 退场逻辑执行
- 不会导致递归调用

**去再入**:
- 嵌套调用被延迟
- 队列正确排空
- 收敛保护生效

**策略化**:
- UseNewest 策略正确
- UseOldest 策略正确
- 自定义策略可扩展

**Gate 关闭**:
- Gate 关闭行为正确
- 不变量保持
- 所有路径使用权威函数

### 7.2 集成测试

**场景 1**: 合并移除
- 添加多个同类型 Buff
- 触发合并
- 验证: 淘汰的 Buff 走 RequestStateRemoval

**场景 2**: 嵌套激活更新
- 状态 A 激活触发事件
- 事件导致状态 B 变化
- 验证: 不会递归调用，队列正确排空

**场景 3**: 同优先级排序
- 添加多个同优先级状态
- 使用不同策略
- 验证: 排序结果符合策略

**场景 4**: Gate 关闭
- 关闭 Gate
- 验证: 所有 Active 状态按 GateCloseBehavior 处理
- 验证: 不变量保持

---

## 8. 风险缓解

### 8.1 高风险: 槽位激活时序变化

**缓解措施**:
- 充分的集成测试
- 与策划沟通，调整玩法
- 提供配置开关（如果需要）

### 8.2 中风险: 合并移除延迟

**缓解措施**:
- 验证 StateTree 退场逻辑
- 测试延迟对玩法的影响
- 文档说明行为变化

### 8.3 中风险: 策略选择

**缓解措施**:
- 提供清晰的策略说明文档
- 为常见场景推荐默认策略
- 支持蓝图自定义策略

---

## 9. 未来扩展

### 9.1 策略增强

**当前**: 只支持排序

**扩展**: 支持队列上限
- `ShouldAcceptNewState` 接口
- 可以拒绝新状态或溢出处理

### 9.2 性能优化

**当前**: 每次都排序

**扩展**: 增量更新
- 只对变化的状态重新排序
- 维护排序缓存

### 9.3 调试工具

**当前**: 基本日志

**扩展**: 可视化调试
- 显示槽位激活更新队列
- 显示策略排序结果
- 显示 Gate 状态

---

## 10. 总结

本设计文档详细描述了状态槽位激活完整性重构的技术方案，包括:

1. **合并移除统一化**: 所有移除走 RequestStateRemoval
2. **槽位激活去再入**: 延迟请求 + 队列排空
3. **同优先级 tie-break 策略化**: UObject CDO 策略
4. **Gate 关闭逻辑重构**: 单一权威函数

所有设计决策都经过充分的分析和权衡，优先考虑正确性和可维护性，同时兼顾性能和向后兼容性。
