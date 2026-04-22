# Phase C：Attribute 业务迁移到 AttributeComponent

> 本文档精确到代码行号，可直接定位执行。
> 文件路径缩写见 [总览文档](./00_总览与索引.md)。

---

## C-1. 需要迁移的 API 完整清单（21 个函数）

### 属性增删改（7 个）

#### 1. AddAttribute

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:64-68` |
| **实现** | `AttrMgr.cpp:145-193`（49 行） |
| **签名** | `bool AddAttribute(AActor* CombatEntity, FName AttributeName, float InitValue = 0.f)` |
| **迁移后签名** | `virtual bool AddAttribute(FName AttributeName, float InitValue = 0.f)` |
| **内部依赖** | `AttributeDefinitions.Find()` → `ResolveAttributeManager()->GetAttributeDefinitionAsset()` |
| | `GetAttributeComponent(CombatEntity)` → `this` |
| | `++GlobalAttributeInstanceIdMgr` → `ResolveAttributeManager()->AllocateAttributeInstanceId()` |
| | `ClampAttributeValueInRange()`, `EnforceAttributeRangeConstraints()` |
| **UFUNCTION** | BlueprintCallable |

#### 2. AddAttributes

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:71-74` |
| **实现** | `AttrMgr.cpp:195-242`（48 行） |
| **签名** | `void AddAttributes(AActor* CombatEntity, const TArray<FName>& AttributeNames)` |
| **迁移后签名** | `void AddAttributes(const TArray<FName>& AttributeNames)` |
| **内部依赖** | 同 `AddAttribute`，循环版本 |
| **UFUNCTION** | BlueprintCallable |

#### 3. AddAttributeByTag

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:84-88` |
| **实现** | `AttrMgr.cpp:244-275`（32 行） |
| **签名** | `bool AddAttributeByTag(AActor* CombatEntity, const FGameplayTag& AttributeTag, float InitValue = 0.f)` |
| **迁移后签名** | `bool AddAttributeByTag(const FGameplayTag& AttributeTag, float InitValue = 0.f)` |
| **内部依赖** | `TryResolveAttributeNameByTag()` — 保留在 Subsystem（Tag 映射是全局数据） |
| | 调用 `AddAttribute()` |
| **UFUNCTION** | BlueprintCallable |

#### 4. SetAttributeBaseValue

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:123-128` |
| **实现** | `AttrMgr.cpp:329-405`（77 行） |
| **签名** | `bool SetAttributeBaseValue(AActor* CombatEntity, FName AttributeName, float NewValue, bool bTriggerEvents = true)` |
| **迁移后签名** | `virtual bool SetAttributeBaseValue(FName AttributeName, float NewValue, bool bTriggerEvents = true)` |
| **内部依赖** | `GetAttributeComponent()` → `this`，`ClampAttributeValueInRange()`，`RecalculateAttributeCurrentValues()` |
| **UFUNCTION** | BlueprintCallable |

#### 5. SetAttributeCurrentValue

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:139-144` |
| **实现** | `AttrMgr.cpp:407-483`（77 行） |
| **签名** | `bool SetAttributeCurrentValue(AActor* CombatEntity, FName AttributeName, float NewValue, bool bTriggerEvents = true)` |
| **迁移后签名** | `virtual bool SetAttributeCurrentValue(FName AttributeName, float NewValue, bool bTriggerEvents = true)` |
| **内部依赖** | `GetAttributeComponent()` → `this`，`ClampAttributeValueInRange()`，`EnforceAttributeRangeConstraints()` |
| **UFUNCTION** | BlueprintCallable |

#### 6. ResetAttribute

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:153-156` |
| **实现** | `AttrMgr.cpp:485-593`（109 行） |
| **签名** | `bool ResetAttribute(AActor* CombatEntity, FName AttributeName)` |
| **迁移后签名** | `virtual bool ResetAttribute(FName AttributeName)` |
| **内部依赖** | `GetAttributeComponent()` → `this`，`RemoveModifier()`，`ClampAttributeValueInRange()` |
| **UFUNCTION** | BlueprintCallable |

#### 7. RemoveAttribute

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:165-168` |
| **实现** | `AttrMgr.cpp:595-699`（105 行） |
| **签名** | `bool RemoveAttribute(AActor* CombatEntity, FName AttributeName)` |
| **迁移后签名** | `virtual bool RemoveAttribute(FName AttributeName)` |
| **内部依赖** | `GetAttributeComponent()` → `this`，`RemoveModifier()` |
| **UFUNCTION** | BlueprintCallable |

---

### Modifier 操作（8 个）

#### 8. CreateAttributeModifier

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:194-199` |
| **实现** | `AttrMgr.cpp:716-786`（71 行） |
| **签名** | `bool CreateAttributeModifier(FName ModifierId, AActor* Instigator, AActor* Target, FTcsAttributeModifierInstance& OutModifierInst)` |
| **迁移后签名** | `virtual bool CreateAttributeModifier(FName ModifierId, AActor* Instigator, FTcsAttributeModifierInstance& OutModifierInst)` |
| **内部依赖** | `AttributeModifierDefinitions.Find()` → `ResolveAttributeManager()->GetModifierDefinitionAsset()` |
| | `++GlobalAttributeModifierInstanceIdMgr` → `ResolveAttributeManager()->AllocateModifierInstanceId()` |
| **注意** | 移除 `Target` 参数，Component 已知自身 Owner |
| **UFUNCTION** | BlueprintCallable |

#### 9. CreateAttributeModifierWithOperands

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:211-217` |
| **实现** | `AttrMgr.cpp:788-877`（90 行） |
| **签名** | `bool CreateAttributeModifierWithOperands(FName ModifierId, AActor* Instigator, AActor* Target, const TMap<FName, float>& Operands, FTcsAttributeModifierInstance& OutModifierInst)` |
| **迁移后签名** | `virtual bool CreateAttributeModifierWithOperands(FName ModifierId, AActor* Instigator, const TMap<FName, float>& Operands, FTcsAttributeModifierInstance& OutModifierInst)` |
| **内部依赖** | 同 `CreateAttributeModifier`，额外处理 `Operands` |
| **UFUNCTION** | BlueprintCallable |

#### 10. ApplyModifier

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:220-221` |
| **实现** | `AttrMgr.cpp:879-1044`（166 行） |
| **签名** | `void ApplyModifier(AActor* CombatEntity, TArray<FTcsAttributeModifierInstance>& Modifiers)` |
| **迁移后签名** | `virtual void ApplyModifier(UPARAM(ref) TArray<FTcsAttributeModifierInstance>& Modifiers)` |
| **内部依赖** | `GetAttributeComponent()` → `this` |
| | `++GlobalAttributeModifierChangeBatchIdMgr` → `ResolveAttributeManager()->AllocateModifierChangeBatchId()` |
| | `SourceHandleIdToModifierInstIds` 维护（行 960, 1025） |
| | `ModifierInstIdToIndex` 维护（行 965, 1021） |
| | `RecalculateAttributeBaseValues()`，`RecalculateAttributeCurrentValues()` |
| **UFUNCTION** | BlueprintCallable |

#### 11. ApplyModifierWithSourceHandle

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:232-237` |
| **实现** | `AttrMgr.cpp:1780-1823`（44 行） |
| **签名** | `bool ApplyModifierWithSourceHandle(AActor* CombatEntity, const FTcsSourceHandle& SourceHandle, const TArray<FName>& ModifierIds, TArray<FTcsAttributeModifierInstance>& OutModifiers)` |
| **迁移后签名** | `bool ApplyModifierWithSourceHandle(const FTcsSourceHandle& SourceHandle, const TArray<FName>& ModifierIds, TArray<FTcsAttributeModifierInstance>& OutModifiers)` |
| **内部依赖** | `CreateAttributeModifier()` + `ApplyModifier()` 组合调用 |
| **UFUNCTION** | BlueprintCallable，非 virtual 包装器 |

#### 12. RemoveModifier

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:240-241` |
| **实现** | `AttrMgr.cpp:1046-1122`（77 行） |
| **签名** | `void RemoveModifier(AActor* CombatEntity, TArray<FTcsAttributeModifierInstance>& Modifiers)` |
| **迁移后签名** | `virtual void RemoveModifier(UPARAM(ref) TArray<FTcsAttributeModifierInstance>& Modifiers)` |
| **内部依赖** | `GetAttributeComponent()` → `this` |
| | `++GlobalAttributeModifierChangeBatchIdMgr` → `ResolveAttributeManager()->AllocateModifierChangeBatchId()` |
| | `ModifierInstIdToIndex` 维护（行 1061, 1087, 1108） |
| | `SourceHandleIdToModifierInstIds` 维护（行 1091） |
| | `RecalculateAttributeCurrentValues()` |
| **UFUNCTION** | BlueprintCallable |

#### 13. RemoveModifiersBySourceHandle

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:251-253` |
| **实现** | `AttrMgr.cpp:1825-1883`（59 行） |
| **签名** | `bool RemoveModifiersBySourceHandle(AActor* CombatEntity, const FTcsSourceHandle& SourceHandle)` |
| **迁移后签名** | `virtual bool RemoveModifiersBySourceHandle(const FTcsSourceHandle& SourceHandle)` |
| **内部依赖** | `GetAttributeComponent()` → `this` |
| | `SourceHandleIdToModifierInstIds`（行 1845） |
| | `ModifierInstIdToIndex`（行 1859） |
| | `RemoveModifier()` |
| **UFUNCTION** | BlueprintCallable |

---

### 性能 TODO：Modifier 批量移除路径的 O(K²) 退化

> 迁移 #12 `RemoveModifier` 与 #13 `RemoveModifiersBySourceHandle` 到 Component 时，须在函数声明和实现中保留以下 `TODO(Perf)` 注释，避免后续维护者丢失优化上下文。

**问题**：
- `SourceHandleIdToModifierInstIds` 当前 Value 为 `TArray<int32>`，单次 `TArray::Remove(InstId)` 为 O(bucket)
- 批量移除同一 SourceHandle 下 K 个 Modifier 时，整体退化为 **O(K²)**
- 热点路径：`RemoveModifiersBySourceHandle` → 逐个委托 `RemoveModifier` → 每次都在同一桶上 `Remove`

**优化方向**（迁移时保持现状，列为 TODO）：
1. **桶类型换为 `TSet<int32>`**：单次删除 O(1)；桶内元素量通常较小，内存开销可接受（首选）
2. **提取 `RemoveModifierInternal` 无桶维护版**：`RemoveModifiersBySourceHandle` 末尾一次性 `SourceHandleIdToModifierInstIds.Remove(SourceHandle.Id)` 整桶丢弃，跳过逐个 `Remove`
3. **批量移除引入延迟紧凑化**：先标记后重建，一次性扫描 O(N)

**迁移时在 Component 头文件对应函数声明上添加注释**：

```cpp
// TODO(Perf): 批量移除同一 SourceHandle 下 K 个 Modifier 时，桶维护退化为 O(K^2)。
//   优化方向见 RemoveModifiersBySourceHandle 实现注释，以及 SourceHandleIdToModifierInstIds 成员注释。
virtual void RemoveModifier(UPARAM(ref) TArray<FTcsAttributeModifierInstance>& Modifiers);

// TODO(Perf): 当前实现逐个委托给 RemoveModifier，桶内 Remove 导致 O(K^2)。
//   优化优先级：1) 将 SourceHandleIdToModifierInstIds 桶类型改为 TSet<int32>；
//              2) 或提取 RemoveModifierInternal(无桶维护)，在末尾一次性整桶丢弃。
virtual bool RemoveModifiersBySourceHandle(const FTcsSourceHandle& SourceHandle);
```

**迁移时在 Component .cpp 实现内部热点行也加行内 `TODO(Perf)`**：

- `RemoveModifier` 内的 `InstIdsPtr->Remove(RemovedModifier.ModifierInstId)` 行上方
- `RemoveModifiersBySourceHandle` 内 "拷贝 ID 列表" 之后、收集循环之前

---

#### 14. GetModifiersBySourceHandle

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:263-267` |
| **实现** | `AttrMgr.cpp:1885-1951`（67 行） |
| **签名** | `bool GetModifiersBySourceHandle(AActor* CombatEntity, const FTcsSourceHandle& SourceHandle, TArray<FTcsAttributeModifierInstance>& OutModifiers) const` |
| **迁移后签名** | `bool GetModifiersBySourceHandle(const FTcsSourceHandle& SourceHandle, TArray<FTcsAttributeModifierInstance>& OutModifiers) const` |
| **内部依赖** | `GetAttributeComponent()` → `this`，纯读取操作 |
| **UFUNCTION** | BlueprintCallable，非 virtual 包装器 |

#### 15. HandleModifierUpdated

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:270-271` |
| **实现** | `AttrMgr.cpp:1124-1208`（85 行） |
| **签名** | `void HandleModifierUpdated(AActor* CombatEntity, TArray<FTcsAttributeModifierInstance>& Modifiers)` |
| **迁移��签名** | `virtual void HandleModifierUpdated(UPARAM(ref) TArray<FTcsAttributeModifierInstance>& Modifiers)` |
| **内部依赖** | `GetAttributeComponent()` → `this` |
| | `++GlobalAttributeModifierChangeBatchIdMgr` |
| | `ModifierInstIdToIndex` 维护（行 1140, 1175-1196） |
| | `SourceHandleIdToModifierInstIds` 维护 |
| | `RecalculateAttributeCurrentValues()` |
| **UFUNCTION** | BlueprintCallable |

---

### 计算与 Clamp（6 个）

#### 16. RecalculateAttributeBaseValues

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:314`（protected static） |
| **实现** | `AttrMgr.cpp:1210-1320`（111 行） |
| **签名** | `static void RecalculateAttributeBaseValues(const AActor* CombatEntity, const TArray<FTcsAttributeModifierInstance>& Modifiers)` |
| **迁移后签名** | `virtual void RecalculateAttributeBaseValues(const TArray<FTcsAttributeModifierInstance>& Modifiers)` |
| **内部依赖** | `GetAttributeComponent()` → `this`，`MergeAttributeModifiers()`，`ClampAttributeValueInRange()`，`EnforceAttributeRangeConstraints()` |
| **变化** | static → 成员方法 |

#### 17. RecalculateAttributeCurrentValues

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:317`（protected static） |
| **实现** | `AttrMgr.cpp:1322-1435`（114 行） |
| **签名** | `static void RecalculateAttributeCurrentValues(const AActor* CombatEntity, int64 ChangeBatchId = -1)` |
| **迁移后签名** | `virtual void RecalculateAttributeCurrentValues(int64 ChangeBatchId = -1)` |
| **内部依赖** | 同上 |
| **变化** | static → 成员方法 |

#### 18. MergeAttributeModifiers

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:320-323`（protected static） |
| **实现** | `AttrMgr.cpp:1437-1477`（41 行） |
| **签名** | `static void MergeAttributeModifiers(const AActor* CombatEntity, const TArray<FTcsAttributeModifierInstance>& Modifiers, TArray<FTcsAttributeModifierInstance>& MergedModifiers)` |
| **迁移后签名** | `virtual void MergeAttributeModifiers(const TArray<FTcsAttributeModifierInstance>& Modifiers, TArray<FTcsAttributeModifierInstance>& MergedModifiers)` |
| **内部依赖** | 自包含，无外部依赖 |
| **变化** | static → 成员方法 |

#### 19. ClampAttributeValueInRange

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:327-333`（protected static） |
| **实现** | `AttrMgr.cpp:1479-1625`（147 行） |
| **签名** | `static void ClampAttributeValueInRange(UTcsAttributeComponent* AttributeComponent, const FName& AttributeName, float& NewValue, float* OutMinValue = nullptr, float* OutMaxValue = nullptr, const TMap<FName, float>* WorkingValues = nullptr)` |
| **迁移后签名** | `virtual void ClampAttributeValueInRange(const FName& AttributeName, float& NewValue, float* OutMinValue = nullptr, float* OutMaxValue = nullptr, const TMap<FName, float>* WorkingValues = nullptr)` |
| **内部依赖** | 已接收 `AttributeComponent` 指针 → 迁移后直接用 `this` |
| **变化** | static → 成员方法，移除第一参数 |

#### 20. EnforceAttributeRangeConstraints

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:338`（protected static） |
| **实现** | `AttrMgr.cpp:1627-1766`（140 行） |
| **签名** | `static void EnforceAttributeRangeConstraints(UTcsAttributeComponent* AttributeComponent)` |
| **迁移后签名** | `virtual void EnforceAttributeRangeConstraints()` |
| **内部依赖** | `ClampAttributeValueInRange()` |
| **变化** | static → 成员方法，移除参数 |

#### 21. GetAttributeComponent（辅助函数）

| 项 | 详情 |
|----|------|
| **声明** | `AttrMgr.h:172`（protected static） |
| **实现** | `AttrMgr.cpp:701-714`（14 行） |
| **签名** | `static UTcsAttributeComponent* GetAttributeComponent(const AActor* CombatEntity)` |
| **处理** | 迁移后不再需要——Component 内部用 `this`，外部通过 `ITcsEntityInterface` 获取 |
| **最终** | Phase G 删除 |

---

### 设计约束：属性夹值计算的单 Component 边界

> **重要**：以下约束适用于 `ClampAttributeValueInRange`（#19）和 `EnforceAttributeRangeConstraints`（#20），迁移实现时须在代码注释中明确。

- `FTcsAttributeRange::MinValueAttribute` / `MaxValueAttribute` 是纯 `FName`，只能引用**同一 `AttributeComponent` 上**的属性，不支持跨 Actor 属性引用
- 迁移为成员方法后，所有动态范围依赖都在 `this` 上解析
- 自定义 Clamp 策略（`UTcsAttributeClampStrategy` 子类）接收的 `FTcsAttributeClampContextBase` 上下文绑定到单个 `AttributeComponent`
- 未来若需支持跨 Actor 属性依赖，应通过扩展 Context 或引入跨 Component Resolver 实现，**不应破坏当前单 Component 假设**

迁移后应在 `UTcsAttributeComponent` 头文件的 Clamp region 注释中明确此约束：

```cpp
// 属性夹值计算：所有动态范围依赖（ART_Dynamic）仅在本 Component 上解析。
// 不支持跨 Actor 属性引用。自定义 ClampStrategy 接收的 Context 也绑定到本 Component。
// 若未来需要跨 Actor 依赖，应扩展 FTcsAttributeClampContextBase 或引入跨 Component Resolver。
```

---

## C-2. TcsAttributeComponent 当前已有内容

### includes（`AttrCmp.h:5-10`）

```
CoreMinimal.h
Components/ActorComponent.h
TcsAttribute.h
TcsAttributeChangeEventPayload.h
TcsAttributeModifier.h
TcsAttributeComponent.generated.h
```

**需新增 include**：`TcsSourceHandle.h`、`TcsAttributeManagerSubsystem.h`

### 已有 public 方法

| 方法 | 头文件行 | 实现行 |
|------|---------|--------|
| 构造函数 | `AttrCmp.h:58` | `AttrCmp.cpp:8-11` |
| `GetAttributeValue()` | `AttrCmp.h:71-73` | `AttrCmp.cpp:18-27` |
| `GetAttributeBaseValue()` | `AttrCmp.h:77-79` | `AttrCmp.cpp:29-38` |
| `GetAttributeValues()` | `AttrCmp.h:82` | `AttrCmp.cpp:40-49` |
| `GetAttributeBaseValues()` | `AttrCmp.h:83` | `AttrCmp.cpp:51-60` |
| `BroadcastAttributeValueChangeEvent()` | `AttrCmp.h:87` | `AttrCmp.cpp:62-69` |
| `BroadcastAttributeBaseValueChangeEvent()` | `AttrCmp.h:89` | `AttrCmp.cpp:71-78` |
| `BroadcastAttributeModifierAddedEvent()` | `AttrCmp.h:91` | `AttrCmp.cpp:80-87` |
| `BroadcastAttributeModifierRemovedEvent()` | `AttrCmp.h:93` | `AttrCmp.cpp:89-96` |
| `BroadcastAttributeModifierUpdatedEvent()` | `AttrCmp.h:95` | `AttrCmp.cpp:98-105` |
| `BroadcastAttributeReachedBoundaryEvent()` | `AttrCmp.h:97-102` | `AttrCmp.cpp:107-118` |

### 已有数据成员（全部 public）

| 成员 | 类型 | 行号 |
|------|------|------|
| `Attributes` | `TMap<FName, FTcsAttributeInstance>` | `AttrCmp.h:107` |
| `AttributeModifiers` | `TArray<FTcsAttributeModifierInstance>` | `AttrCmp.h:111` |
| `SourceHandleIdToModifierInstIds` | `TMap<int32, TArray<int32>>` | `AttrCmp.h:122` |
| `ModifierInstIdToIndex` | `TMap<int32, int32>` | `AttrCmp.h:127` |

**关键观察**: 数据已经全部在 Component 上，Manager 当前直接操作 Component 的 public 成员。迁移后这些成员可以改为 protected。

### 已有委托

| 委托 | 行号 |
|------|------|
| `OnAttributeValueChanged` | `AttrCmp.h:131` |
| `OnAttributeBaseValueChanged` | `AttrCmp.h:135` |
| `OnAttributeModifierAdded` | `AttrCmp.h:139` |
| `OnAttributeModifierRemoved` | `AttrCmp.h:143` |
| `OnAttributeModifierUpdated` | `AttrCmp.h:146` |
| `OnAttributeReachedBoundary` | `AttrCmp.h:155` |

---

## C-3. 迁移替换规则汇总

### 规则 1: `CombatEntity` → `GetOwner()`

以下函数的 `AActor* CombatEntity` 参数在迁移后移除，函数体内用 `GetOwner()` 替换：

| 函数 | CPP 参数行 | 函数体内使用行（需替换） |
|------|-----------|----------------------|
| AddAttribute | 146 | 159, 169, 173, 178, 191-192 |
| AddAttributes | 195 | 197, 209, 210, 227, 241 |
| AddAttributeByTag | 245 | 260, 262, 263, 271, 274 |
| SetAttributeBaseValue | 330 | 337, 344, 349, 362, 381, 396-402 |
| SetAttributeCurrentValue | 408 | 415, 422, 427, 440, 462, 474-480 |
| ResetAttribute | 486 | 491, 498, 503, 516, 547, 585-590 |
| RemoveAttribute | 596 | 601, 608, 613, 626, 687, 692-697 |
| ApplyModifier | 880 | 883, 936, 1043 |
| RemoveModifier | 1047 | 1050, 1121 |
| HandleModifierUpdated | 1125 | 1128, 1206 |
| ApplyModifierWithSourceHandle | 1781 | 1793, 1807, 1818 |
| RemoveModifiersBySourceHandle | 1826 | 1838, 1878 |
| GetModifiersBySourceHandle | 1886 | 1898 |

### 规则 2: `GetAttributeComponent(CombatEntity)` → `this`

所有出现位置（15 处）：

| CPP 行号 | 所在函数 |
|---------|---------|
| 159 | AddAttribute |
| 197 | AddAttributes |
| 260 | AddAttributeByTag |
| 344 | SetAttributeBaseValue |
| 422 | SetAttributeCurrentValue |
| 498 | ResetAttribute |
| 608 | RemoveAttribute |
| 883 | ApplyModifier |
| 1050 | RemoveModifier |
| 1128 | HandleModifierUpdated |
| 1214 | RecalculateAttributeBaseValues |
| 1324 | RecalculateAttributeCurrentValues |
| 1793 | ApplyModifierWithSourceHandle |
| 1838 | RemoveModifiersBySourceHandle |
| 1898 | GetModifiersBySourceHandle |

### 规则 3: 全局 ID 分配

| 原代码 | 替换为 | 出现行号 |
|--------|--------|---------|
| `++GlobalAttributeInstanceIdMgr` | `ResolveAttributeManager()->AllocateAttributeInstanceId()` | 178, 227 |
| `++GlobalAttributeModifierInstanceIdMgr` | `ResolveAttributeManager()->AllocateModifierInstanceId()` | 780, 871 |
| `++GlobalAttributeModifierChangeBatchIdMgr` | `ResolveAttributeManager()->AllocateModifierChangeBatchId()` | 891, 1056, 1135 |

### 规则 4: 定义查找

| 原代码 | 替换为 | 出现行号 |
|--------|--------|---------|
| `AttributeDefinitions.Find(...)` | `ResolveAttributeManager()->GetAttributeDefinitionAsset(...)` | 150, 217 |
| `AttributeModifierDefinitions.Find(...)` | `ResolveAttributeManager()->GetModifierDefinitionAsset(...)` | 754, 827 |

---

## C-4. 必须保留的行为顺序

以下执行顺序在迁移过程中**不可改变**：

### ApplyModifier 内部顺序

1. 验证 Component 有效性
2. 分配 BatchId（`++GlobalAttributeModifierChangeBatchIdMgr`）
3. 逐个处理 Modifier：检查重复 → 设置 ApplyTimestamp → 添加到 `AttributeModifiers` → 更新 `ModifierInstIdToIndex` → 更新 `SourceHandleIdToModifierInstIds`
4. `RecalculateAttributeBaseValues()`
5. `RecalculateAttributeCurrentValues()`
6. 广播 `BroadcastAttributeModifierAddedEvent()`

### RemoveModifier 内部顺序

1. 验证 Component 有效性
2. 分配 BatchId
3. 逐个处理：找到 Index → 移除 → 更新 `ModifierInstIdToIndex`（swap 修正） → 更新 `SourceHandleIdToModifierInstIds`
4. `RecalculateAttributeCurrentValues()`
5. 广播 `BroadcastAttributeModifierRemovedEvent()`

### EnforceAttributeRangeConstraints 调用位置

该方法作为最终收敛调用出现在以下位置，迁移后必须保持：

| 调用位置（CPP 行号） | 所在函数 |
|---------------------|---------|
| 191 | AddAttribute |
| 241 | AddAttributes |
| 459 | SetAttributeCurrentValue |
| 1319 | RecalculateAttributeBaseValues |
| 1434 | RecalculateAttributeCurrentValues |
