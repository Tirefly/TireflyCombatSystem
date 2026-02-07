# Design: 属性系统运行时完整性重构

本文档详细描述技术设计决策和实现方案。

## 1. SourceHandle 索引重构

### 1.1 问题分析

**当前实现**:
```cpp
// UTcsAttributeComponent
TMap<int32, TArray<int32>> SourceHandleIdToModifierIndices;  // SourceId -> 数组下标列表
TArray<FTcsAttributeModifierInstance> AttributeModifiers;     // Modifier 数组
```

**问题**:
- 删除 `AttributeModifiers[i]` 会导致所有 `j > i` 的下标失效
- 当前通过扫描所有桶来修正下标,O(n*m) 复杂度
- 容易出现缓存漂移,导致查询/移除失败

### 1.2 设计方案

**新数据结构**:
```cpp
// UTcsAttributeComponent
TMap<int32, TArray<int32>> SourceHandleIdToModifierInstIds;  // SourceId -> ModifierInstId 列表
TMap<int32, int32> ModifierInstIdToIndex;                    // ModifierInstId -> 当前数组下标
TArray<FTcsAttributeModifierInstance> AttributeModifiers;     // Modifier 数组(保持不变)
```

**核心思想**:
- `ModifierInstId` 是稳定的身份标识,永不改变
- `SourceHandleIdToModifierInstIds` 存储稳定 ID,不受数组操作影响
- `ModifierInstIdToIndex` 提供 O(1) 的 ID -> 下标映射
- 使用 `RemoveAtSwap` 删除,只需修正 1 个被 swap 的元素

### 1.3 变更规则

#### 插入 (Insert)
```cpp
// 在 AttributeModifiers.Add(...) 之后
int32 NewIndex = AttributeModifiers.Num() - 1;
ModifierInstIdToIndex.Add(ModifierInstId, NewIndex);
if (SourceHandle.IsValid())
{
    SourceHandleIdToModifierInstIds.FindOrAdd(SourceHandle.Id).AddUnique(ModifierInstId);
}
```

#### 更新 (Update)
```cpp
// ModifierInstId 必须保持稳定不变
// 若 SourceId 发生变化:
if (OldSourceId != NewSourceId)
{
    // 从旧桶移除
    if (TArray<int32>* OldBucket = SourceHandleIdToModifierInstIds.Find(OldSourceId))
    {
        OldBucket->Remove(ModifierInstId);
        if (OldBucket->Num() == 0)
        {
            SourceHandleIdToModifierInstIds.Remove(OldSourceId);
        }
    }
    // 加入新桶
    SourceHandleIdToModifierInstIds.FindOrAdd(NewSourceId).AddUnique(ModifierInstId);
}
// 始终确保 ModifierInstIdToIndex 正确
```

#### 删除 (Remove)
```cpp
// 1. 定位下标
int32* IndexPtr = ModifierInstIdToIndex.Find(ModifierInstId);
if (!IndexPtr) return;  // 已删除
int32 Index = *IndexPtr;

// 2. 使用 RemoveAtSwap 删除
AttributeModifiers.RemoveAtSwap(Index, 1, false);

// 3. 修正被 swap 过来的元素(如果有)
if (Index < AttributeModifiers.Num())
{
    int32 SwappedId = AttributeModifiers[Index].ModifierInstId;
    ModifierInstIdToIndex[SwappedId] = Index;
}

// 4. 从缓存中移除被删 id
ModifierInstIdToIndex.Remove(ModifierInstId);
if (TArray<int32>* Bucket = SourceHandleIdToModifierInstIds.Find(SourceId))
{
    Bucket->Remove(ModifierInstId);
    if (Bucket->Num() == 0)
    {
        SourceHandleIdToModifierInstIds.Remove(SourceId);
    }
}
```

### 1.4 查询逻辑

```cpp
TArray<FTcsAttributeModifierInstance*> GetModifiersBySourceHandle(int32 SourceId)
{
    TArray<FTcsAttributeModifierInstance*> Result;
    TArray<int32>* Bucket = SourceHandleIdToModifierInstIds.Find(SourceId);
    if (!Bucket) return Result;

    // 拷贝 ID 列表,避免遍历时修改
    TArray<int32> InstIds = *Bucket;

    for (int32 InstId : InstIds)
    {
        int32* IndexPtr = ModifierInstIdToIndex.Find(InstId);
        if (IndexPtr)
        {
            // ID 有效,返回 Modifier
            Result.Add(&AttributeModifiers[*IndexPtr]);
        }
        else
        {
            // ID 陈旧,从桶中剔除(自愈)
            Bucket->Remove(InstId);
        }
    }

    // 清理空桶
    if (Bucket->Num() == 0)
    {
        SourceHandleIdToModifierInstIds.Remove(SourceId);
    }

    return Result;
}
```

### 1.5 取舍分析

**优点**:
- ✅ 正确性: 缓存永不漂移
- ✅ 性能: 删除从 O(n*m) 降到 O(1)
- ✅ 自愈: 陈旧 ID 自动剔除
- ✅ 简单: 逻辑清晰,易于维护

**缺点**:
- ❌ 内存: 多一个 `ModifierInstIdToIndex` 映射(可接受)
- ❌ 顺序: `RemoveAtSwap` 改变数组顺序(不影响正确性,因为数值计算依赖 Priority/时间戳,不依赖物理顺序)

**决策**: 采用此方案,优点远大于缺点

---

## 2. 严格输入校验

### 2.1 校验规则

在 `CreateAttributeModifier` 和 `CreateAttributeModifierWithOperands` 中添加以下校验:

```cpp
bool UTcsAttributeManagerSubsystem::CreateAttributeModifier(...)
{
    // 1. SourceName 校验
    if (SourceName == NAME_None)
    {
        UE_LOG(LogTcsAttribute, Error, TEXT("[CreateAttributeModifier] SourceName is NAME_None"));
        return false;
    }

    // 2. Actor 有效性校验
    if (!IsValid(Instigator))
    {
        UE_LOG(LogTcsAttribute, Error, TEXT("[CreateAttributeModifier] Instigator is invalid"));
        return false;
    }
    if (!IsValid(Target))
    {
        UE_LOG(LogTcsAttribute, Error, TEXT("[CreateAttributeModifier] Target is invalid"));
        return false;
    }

    // 3. 战斗实体接口校验
    if (!Instigator->Implements<UTcsEntityInterface>())
    {
        UE_LOG(LogTcsAttribute, Error, TEXT("[CreateAttributeModifier] Instigator does not implement ITcsEntityInterface"));
        return false;
    }
    if (!Target->Implements<UTcsEntityInterface>())
    {
        UE_LOG(LogTcsAttribute, Error, TEXT("[CreateAttributeModifier] Target does not implement ITcsEntityInterface"));
        return false;
    }

    // ... 继续原有逻辑
}
```

### 2.2 错误处理策略

**原则**: Fail Fast - 尽早发现错误,避免传播

**日志级别**: Error - 这些都是严重的配置错误,必须修复

**返回值**: false - 明确告知调用方失败

---

## 3. AddAttribute 防覆盖

### 3.1 语义调整

**当前行为**:
```cpp
void AddAttribute(AActor* CombatEntity, FName AttributeName, float InitValue)
{
    // 直接添加或覆盖
    Attributes.Add(AttributeName, FTcsAttributeInstance{...});
}
```

**新行为**:
```cpp
void AddAttribute(AActor* CombatEntity, FName AttributeName, float InitValue)
{
    // 检查是否已存在
    if (Attributes.Contains(AttributeName))
    {
        UE_LOG(LogTcsAttribute, Warning,
            TEXT("[AddAttribute] Attribute '%s' already exists on '%s', skipping"),
            *AttributeName.ToString(), *CombatEntity->GetName());
        return;
    }

    // 添加新属性
    Attributes.Add(AttributeName, FTcsAttributeInstance{...});
}
```

### 3.2 AddAttributeByTag 返回值

**当前语义**: 返回 Tag 是否解析成功

**新语义**: 返回是否真的添加成功

```cpp
bool AddAttributeByTag(AActor* CombatEntity, const FGameplayTag& AttributeTag, float InitValue)
{
    // 1. 解析 Tag
    FName AttributeName;
    if (!TryResolveAttributeNameByTag(AttributeTag, AttributeName))
    {
        return false;  // Tag 无效
    }

    // 2. 检查是否已存在
    if (Attributes.Contains(AttributeName))
    {
        UE_LOG(LogTcsAttribute, Warning,
            TEXT("[AddAttributeByTag] Attribute '%s' already exists on '%s', skipping"),
            *AttributeName.ToString(), *CombatEntity->GetName());
        return false;  // 已存在,未添加
    }

    // 3. 添加新属性
    Attributes.Add(AttributeName, FTcsAttributeInstance{...});
    return true;  // 成功添加
}
```

---

## 4. 动态范围约束

### 4.1 问题分析

**当前行为**:
```cpp
// ClampAttributeValueInRange 调用 GetAttributeValue
float ClampAttributeValueInRange(UTcsAttributeComponent* Component, FName AttributeName, float Value)
{
    // 解析动态 min/max
    if (RangeMin.Type == Dynamic)
    {
        float MinValue;
        Component->GetAttributeValue(RangeMin.AttributeName, MinValue);  // 读取已提交的值
        // ...
    }
}
```

**问题**:
- 在重算过程中,`GetAttributeValue` 返回的是旧值(尚未提交)
- MaxHP 下降时,HP 的 BaseValue 可能不会被重算(因为没有 base modifier 触发)

**期望不变量**:
- 如果 HP 的动态上限绑定到 MaxHP,则 **HP (base/current) <= MaxHP (current)** 必须在任意时刻成立

### 4.2 设计方案

#### Option A: In-Flight Resolver (推荐)

**核心思想**: 在重算过程中,使用"工作集"而不是"已提交值"

```cpp
// 扩展 clamp 逻辑,支持值解析器
using FAttributeValueResolver = TFunction<bool(FName, float&)>;

float ClampAttributeValueInRange(
    UTcsAttributeComponent* Component,
    FName AttributeName,
    float Value,
    const FAttributeValueResolver& Resolver = nullptr)
{
    // 解析动态 min/max
    if (RangeMin.Type == Dynamic)
    {
        float MinValue;
        if (Resolver && Resolver(RangeMin.AttributeName, MinValue))
        {
            // 优先从工作集读取
        }
        else
        {
            // Fallback 到已提交值
            Component->GetAttributeValue(RangeMin.AttributeName, MinValue);
        }
        // ...
    }
}
```

**使用场景**:
1. 重算过程中的 in-flight clamp: 传入工作集 resolver
2. 重算后的 post-pass enforcement: 传入工作集 resolver
3. 普通查询: 不传 resolver,使用已提交值

**优点**:
- ✅ 精确: 避免"短暂提交越界值"
- ✅ 灵活: 同一函数支持多种场景
- ✅ 向后兼容: 默认行为不变

**缺点**:
- ❌ 复杂: 需要维护工作集

#### Option B: Commit-First

**核心思想**: 先提交重算结果,再执行 enforcement

```cpp
void RecalculateAttributeCurrentValues(...)
{
    // 1. 重算
    TMap<FName, float> NewValues = ...;

    // 2. 提交
    for (auto& Pair : NewValues)
    {
        Attribute->CurrentValue = Pair.Value;
    }

    // 3. Enforcement
    EnforceAttributeRangeConstraints(Component);
}
```

**优点**:
- ✅ 简单: 不需要工作集
- ✅ 清晰: 提交和约束分离

**缺点**:
- ❌ 短暂越界: 在 enforcement 前,值可能越界
- ❌ 事件: 可能触发两次事件(提交时一次,enforcement 时一次)

#### 决策: 采用 Option A

**理由**:
- 精确性更重要
- 避免短暂越界状态
- 事件语义更清晰

### 4.3 范围约束传播算法

```cpp
void EnforceAttributeRangeConstraints(UTcsAttributeComponent* Component)
{
    const int32 MaxIterations = 8;  // 防止无限循环
    int32 Iteration = 0;
    bool bAnyChanged = true;

    // 工作集: 当前正在处理的值
    TMap<FName, float> WorkingBaseValues;
    TMap<FName, float> WorkingCurrentValues;

    // 初始化工作集
    for (auto& Pair : Component->Attributes)
    {
        WorkingBaseValues.Add(Pair.Key, Pair.Value.BaseValue);
        WorkingCurrentValues.Add(Pair.Key, Pair.Value.CurrentValue);
    }

    // 值解析器: 优先从工作集读取
    auto Resolver = [&](FName AttributeName, float& OutValue) -> bool
    {
        if (float* Value = WorkingCurrentValues.Find(AttributeName))
        {
            OutValue = *Value;
            return true;
        }
        return false;
    };

    // 迭代直到稳定
    while (bAnyChanged && Iteration < MaxIterations)
    {
        bAnyChanged = false;
        Iteration++;

        for (auto& Pair : Component->Attributes)
        {
            FName AttributeName = Pair.Key;
            FTcsAttributeInstance& Attribute = Pair.Value;

            // Clamp BaseValue
            float OldBase = WorkingBaseValues[AttributeName];
            float NewBase = ClampAttributeValueInRange(Component, AttributeName, OldBase, Resolver);
            if (!FMath::IsNearlyEqual(OldBase, NewBase))
            {
                WorkingBaseValues[AttributeName] = NewBase;
                bAnyChanged = true;
            }

            // Clamp CurrentValue
            float OldCurrent = WorkingCurrentValues[AttributeName];
            float NewCurrent = ClampAttributeValueInRange(Component, AttributeName, OldCurrent, Resolver);
            if (!FMath::IsNearlyEqual(OldCurrent, NewCurrent))
            {
                WorkingCurrentValues[AttributeName] = NewCurrent;
                bAnyChanged = true;
            }
        }
    }

    // 检查是否收敛
    if (Iteration >= MaxIterations)
    {
        UE_LOG(LogTcsAttribute, Warning,
            TEXT("[EnforceAttributeRangeConstraints] Max iterations reached, possible circular dependency"));
    }

    // 提交工作集到组件
    TArray<FTcsAttributeChangeEventPayload> BaseChangePayloads;
    TArray<FTcsAttributeChangeEventPayload> CurrentChangePayloads;

    for (auto& Pair : Component->Attributes)
    {
        FName AttributeName = Pair.Key;
        FTcsAttributeInstance& Attribute = Pair.Value;

        // 提交 BaseValue
        float NewBase = WorkingBaseValues[AttributeName];
        if (!FMath::IsNearlyEqual(Attribute.BaseValue, NewBase))
        {
            float OldBase = Attribute.BaseValue;
            Attribute.BaseValue = NewBase;

            // 构造事件 payload
            FTcsAttributeChangeEventPayload Payload;
            Payload.AttributeName = AttributeName;
            Payload.OldValue = OldBase;
            Payload.NewValue = NewBase;
            // SourceHandle: 空(因为是 enforcement 导致的变化,不是 modifier)
            BaseChangePayloads.Add(Payload);
        }

        // 提交 CurrentValue
        float NewCurrent = WorkingCurrentValues[AttributeName];
        if (!FMath::IsNearlyEqual(Attribute.CurrentValue, NewCurrent))
        {
            float OldCurrent = Attribute.CurrentValue;
            Attribute.CurrentValue = NewCurrent;

            // 构造事件 payload
            FTcsAttributeChangeEventPayload Payload;
            Payload.AttributeName = AttributeName;
            Payload.OldValue = OldCurrent;
            Payload.NewValue = NewCurrent;
            CurrentChangePayloads.Add(Payload);
        }
    }

    // 广播事件
    if (BaseChangePayloads.Num() > 0)
    {
        Component->BroadcastAttributeBaseValueChangeEvent(BaseChangePayloads);
    }
    if (CurrentChangePayloads.Num() > 0)
    {
        Component->BroadcastAttributeValueChangeEvent(CurrentChangePayloads);
    }
}
```

### 4.4 事件语义

**问题**: 当 enforcement 导致 Clamp 时,是否触发事件?

**选项 1**: 总是触发事件
- ✅ 符合"真值变化事件"语义
- ✅ 监听者能感知所有变化
- ❌ 可能触发意外的逻辑(如 UI 更新)

**选项 2**: 不触发事件
- ✅ 避免意外触发
- ❌ 监听者无法感知 enforcement 导致的变化
- ❌ 不符合"真值变化事件"语义

**选项 3**: 可配置
- ✅ 灵活
- ❌ 增加复杂度

**决策**: 采用选项 1 - 总是触发事件

**理由**:
- 事件应该反映真实的值变化
- 监听者有责任处理所有变化
- 如果不触发事件,调试会很困难

**事件参数**:
- `AttributeName`: 变化的属性
- `OldValue`: 旧值
- `NewValue`: 新值
- `SourceHandle`: 空(因为是 enforcement 导致的,不是 modifier)

### 4.5 集成点

在以下位置调用 `EnforceAttributeRangeConstraints`:

1. `RecalculateAttributeBaseValues` 之后
2. `RecalculateAttributeCurrentValues` 之后

```cpp
void UTcsAttributeManagerSubsystem::RecalculateAttributeBaseValues(...)
{
    // ... 原有重算逻辑

    // 执行范围约束
    EnforceAttributeRangeConstraints(AttributeComponent);
}

void UTcsAttributeManagerSubsystem::RecalculateAttributeCurrentValues(...)
{
    // ... 原有重算逻辑

    // 执行范围约束
    EnforceAttributeRangeConstraints(AttributeComponent);
}
```

---

## 5. 性能考虑

### 5.1 索引重构性能

**删除操作**:
- 当前: O(n*m) (n = Modifier 数量, m = SourceHandle 桶数量)
- 新方案: O(1)

**查询操作**:
- 当前: O(k) (k = 桶内 Modifier 数量)
- 新方案: O(k) (相同,但更稳定)

**内存开销**:
- 新增: `TMap<int32, int32> ModifierInstIdToIndex`
- 大小: 每个 Modifier 8 字节 (int32 key + int32 value)
- 可接受: 即使 1000 个 Modifier,也只有 8KB

### 5.2 范围约束性能

**最坏情况**:
- 属性数量: N
- 最大迭代次数: M (默认 8)
- 复杂度: O(N * M)

**优化**:
- 只在重算后执行,不在每次查询时执行
- 迭代通常 1-2 次就收敛(除非有复杂依赖)
- 可以通过配置调整最大迭代次数

**预期影响**:
- 对于典型场景(10-20 个属性),影响可忽略
- 对于极端场景(100+ 个属性),可能需要优化(如只处理有动态范围的属性)

---

## 6. 向后兼容性

### 6.1 API 兼容性

**不变**:
- 所有公开 API 签名不变
- 蓝图调用不受影响

**行为变化**:
- `CreateAttributeModifier`: 更严格的校验(可能暴露现有错误)
- `AddAttribute`: 不再静默覆盖(可能暴露重复添加)
- 属性范围约束: 更严格的约束(可能影响玩法平衡)

### 6.2 迁移策略

**Phase 1**: 修复调用方错误
- 运行测试,找出所有被拒绝的非法调用
- 修复调用方代码

**Phase 2**: 修复重复添加
- 运行测试,找出所有重复添加的属性
- 修复配置或代码

**Phase 3**: 调整玩法平衡
- 测试范围约束对玩法的影响
- 调整数值或配置

---

## 7. 测试策略

### 7.1 单元测试

**索引重构**:
- 大量插入后查询正确
- 随机删除后查询正确
- 陈旧 ID 自愈
- RemoveAtSwap 不影响正确性

**严格校验**:
- 非法输入被拒绝
- 合法输入正常工作

**防覆盖**:
- 重复添加被拒绝
- 首次添加正常工作

**范围约束**:
- 单个属性约束正确
- 多跳依赖收敛
- 循环依赖检测
- 事件语义正确

### 7.2 集成测试

**场景 1**: MaxHP Buff 移除
- 添加 MaxHP Buff (+100)
- HP 增加到 MaxHP
- 移除 MaxHP Buff
- 验证: HP 被 clamp 到新的 MaxHP

**场景 2**: 多跳依赖
- MaxHP 依赖 Level
- HP 依赖 MaxHP
- Level 下降
- 验证: MaxHP 和 HP 都被正确 clamp

**场景 3**: 大量 Modifier
- 添加 1000 个 Modifier
- 随机删除 500 个
- 验证: 查询和删除都正确

### 7.3 性能测试

**基准测试**:
- 1000 个 Modifier 的插入/删除/查询性能
- 100 个属性的范围约束性能
- 对比当前实现和新实现

**压力测试**:
- 10000 个 Modifier
- 1000 个属性
- 验证: 性能在可接受范围内

---

## 8. 风险缓解

### 8.1 高风险: 范围约束影响玩法

**缓解措施**:
- 提供配置开关,可以禁用范围约束传播
- 充分的集成测试
- 与策划沟通,调整数值

### 8.2 中风险: 索引重构引入 bug

**缓解措施**:
- 充分的单元测试
- 代码审查
- 分阶段发布

### 8.3 中风险: 严格校验暴露现有问题

**缓解措施**:
- 先在测试环境运行
- 修复所有被拒绝的调用
- 提供迁移指南

---

## 9. 未来扩展

### 9.1 范围约束优化

**当前**: 遍历所有属性

**优化**: 只处理有动态范围的属性
- 在 `FTcsAttribute` 中添加 `bHasDynamicRange` 标志
- 只对这些属性执行 enforcement
- 预期性能提升: 50-90%

### 9.2 依赖图分析

**当前**: 迭代直到稳定

**优化**: 构建依赖图,拓扑排序
- 一次遍历即可完成
- 可以检测循环依赖
- 预期性能提升: 30-50%

### 9.3 增量更新

**当前**: 每次重算后全量 enforcement

**优化**: 只对变化的属性及其依赖执行 enforcement
- 需要维护依赖关系
- 预期性能提升: 70-90%

---

## 10. 总结

本设计文档详细描述了属性系统运行时完整性重构的技术方案,包括:

1. **索引重构**: 使用稳定 ID 缓存,确保正确性
2. **严格校验**: Fail Fast,尽早发现错误
3. **防覆盖**: 明确语义,避免意外
4. **范围约束**: 确保不变量,支持多跳依赖

所有设计决策都经过充分的分析和权衡,优先考虑正确性和可维护性,同时兼顾性能和向后兼容性。
