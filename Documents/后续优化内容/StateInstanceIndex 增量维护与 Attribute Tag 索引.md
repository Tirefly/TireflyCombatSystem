# StateInstanceIndex 增量维护 & Attribute 侧 Tag 索引

> 文档定位：后续独立议题的前置调研记录。不在「Manager API 迁移到 Component」议题范围内。
>
> 触发时机：迁移议题完成 + 性能基线采集之后，视性能数据决定是否推进。

---

## 优化 2：`StateInstanceIndex` 的批量清空

### 1. 当前形态

`FTcsStateInstanceIndex` 的维护粒度是**逐条**：

- `AddInstance(UTcsStateInstance*)` — 写入 `Instances` / `BySlot` / `ByName` 等多个子索引
- `RemoveInstance(UTcsStateInstance*)` — 从上述子索引里逐个移除

调用路径：

- `TryApplyStateInstance` → `AddInstance`（单次）
- `FinalizeStateRemoval` → `RemoveInstance`（单次）
- `RemoveAllStates()` → 对每个 `UTcsStateInstance` 调用一次 `FinalizeStateRemoval` → 内部一次 `RemoveInstance`

### 2. 问题

`RemoveAllStates()` 是对象池回收、场景切换、角色重置等场景的常见路径，调用时通常持有 `N` 个 State（N 可能是 10~50）：

- 每次 `RemoveInstance` 都要：
  - 从 `Instances` 数组里线性查找 + 删除（`O(N)` 每次，累计 `O(N²)`）
  - 从 `BySlot` 的 `TArray` 里 `Remove`（每个 Slot 内线性查找）
  - 从 `ByName` 的 `TMap<FName, TArray<...>>` 里 `Remove`
- 所有子索引最终都要清空，**中间过程的“部分一致性”没有意义**

典型开销：`N=30` 时，`RemoveAllStates` 在索引侧做了 `~900` 次 `TArray::Remove` 比较，其中 `~870` 次是纯粹的浪费。

### 3. 方案：提供 `ClearAll()`

```cpp
struct FTcsStateInstanceIndex
{
    TArray<TObjectPtr<UTcsStateInstance>>                    Instances;
    TMap<FGameplayTag, TArray<TObjectPtr<UTcsStateInstance>>> BySlot;
    TMap<FName, TArray<TObjectPtr<UTcsStateInstance>>>        ByName;

    void AddInstance(UTcsStateInstance* Inst);
    void RemoveInstance(UTcsStateInstance* Inst);

    /** O(1) 均摊：直接 Reset 全部子索引 */
    void ClearAll()
    {
        Instances.Reset();
        BySlot.Reset();
        ByName.Reset();
    }
};
```

### 4. 调用点改造

```cpp
int32 UTcsStateComponent::RemoveAllStates()
{
    const int32 RemovedCount = StateInstanceIndex.Instances.Num();

    // 先逐个走完 FinalizeStateRemoval（需要广播事件、清 Modifier、StopStateTree）
    // 但 FinalizeStateRemoval 里对 StateInstanceIndex.RemoveInstance 的调用改为 no-op
    // 或者引入 bSuppressIndexUpdate 参数，跳过单条维护
    for (UTcsStateInstance* Inst : CollectAllInstances())
    {
        FinalizeStateRemoval(Inst, TcsStateRemovalReasons::Removed, /*bSuppressIndexUpdate*/true);
    }

    // 最后一次性清空
    StateInstanceIndex.ClearAll();

    return RemovedCount;
}
```

### 5. 关键设计点

| 点 | 说明 |
|----|------|
| **不影响单条路径** | `AddInstance` / `RemoveInstance` 不变，常规 `TryApplyState` / `RequestStateRemoval` 路径零改动 |
| **可选抑制参数** | `FinalizeStateRemoval(..., bool bSuppressIndexUpdate = false)`，仅批量路径传 true；单条路径保持当前行为 |
| **替代方案：仅重置 BySlot/ByName** | 若 `Instances` 数组本身需要精确维护（例如 Iterator 仍在遍历），可以只 `ClearAll` 子索引，保留 `Instances` 的逐条 Remove；但 `RemoveAllStates` 结束时 `Instances` 必然是空的，没必要保留 |
| **Debug 一致性** | `GetStateDebugSnapshot` 在 `RemoveAllStates` 运行中不能被调用，否则会看到“索引已清空但 StateSlots 还有残余” |

### 6. 收益评估

- **高频场景**：对象池回收（`TireflyActorPool` 的 `OnReturnedToPool`）、场景切换、角色死亡重置
- **单次收益有限**（索引维护本来就不是性能热点），但
- **心智收益显著**：`RemoveAllStates` 的意图从“逐条清”变成“整体清”，代码意图与实现贴合
- **风险**：低。`ClearAll` 语义简单，只要保证不在迭代 `Instances` 途中调用即可

### 7. 是否值得独立立项

**建议不单独立项，作为“后续小幅清理”一次性带入**。

原因：
- 改造面仅限 `FTcsStateInstanceIndex` + `RemoveAllStates` + `RemoveAllStatesInSlot`（可选）
- 无新设计决策、无监听者影响
- 可以和「事件广播批量化」议题（`FScopedBatchNotification`）合并到同一个 PR：两者都是“`RemoveAllStates` 路径的性能优化”

---

## 优化 3：Attribute 侧按 Tag 的快速查询索引

### 1. 当前形态

`UTcsAttributeComponent` 的属性存储：

```cpp
UPROPERTY()
TMap<FName, FTcsAttributeInstance> Attributes;
```

查询接口：

- `GetAttributeValue(FName AttributeName)` — `O(1)` `TMap` 查找
- `GetAttributesByTag(FGameplayTag Tag)` — **不存在**，需遍历 `Attributes` 对每个 `FTcsAttributeInstance` 查其 Def 的 Tags，做 `HasTag` 比较

### 2. 潜在需求场景

- **AOE 修饰**：一个 Modifier 想“对所有带 `Attribute.Combat.*` Tag 的属性生效”
- **UI 展示分组**：HUD 想展示“所有 `Attribute.Display.HUD` 的属性”
- **Clamp 策略批量应用**：自定义策略需要“对所有 `Attribute.Clamped` 属性执行夹值”
- **调试/序列化**：导出某 Tag 子集的快照

当前实现方式是线性遍历整个 `Attributes` + 每个属性查 Def 做 `HasTag`，复杂度 `O(N × M)`（`N` 属性数，`M` Tag 平均层级）。

### 3. 方案：参考 `FTcsStateInstanceIndex` 引入 `FTcsAttributeIndex`

```cpp
USTRUCT()
struct FTcsAttributeIndex
{
    GENERATED_BODY()

    /** 所有属性的 FName 列表（与 Attributes TMap 同步） */
    UPROPERTY()
    TArray<FName> AttributeNames;

    /** Tag -> 属性 FName 列表（一个属性可属于多个 Tag） */
    UPROPERTY()
    TMap<FGameplayTag, FTcsAttributeNameList> ByTag;

    void AddAttribute(FName AttrName, const UTcsAttributeDefinition* Def);
    void RemoveAttribute(FName AttrName, const UTcsAttributeDefinition* Def);
    void ClearAll();

    /** O(1) 平均查询 */
    const TArray<FName>* GetAttributesByTag(FGameplayTag Tag) const;
};

USTRUCT()
struct FTcsAttributeNameList
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FName> Names;
};
```

`UTcsAttributeComponent` 新增：

```cpp
UPROPERTY()
FTcsAttributeIndex AttributeIndex;

/** 按 Tag 查询属性 FName 列表（不修改状态） */
UFUNCTION(BlueprintCallable, Category = "Attribute")
TArray<FName> GetAttributeNamesByTag(const FGameplayTag& Tag) const;

UFUNCTION(BlueprintCallable, Category = "Attribute")
TArray<FName> GetAttributeNamesByTagQuery(const FGameplayTagQuery& Query) const;
```

### 4. 索引维护点

| 写路径 | 维护动作 |
|--------|----------|
| `AddAttribute(FName, float)` | 查 Def → 读 Tags → `AttributeIndex.AddAttribute` |
| `RemoveAttribute(FName)` | 查 Def → 读 Tags → `AttributeIndex.RemoveAttribute` |
| `ResetAttribute(FName)` | 无需变更索引（属性仍存在） |
| `SetAttribute*Value` | 无需变更索引（Tag 不依赖值） |

### 5. 关键设计决策（需 brainstorm 时展开）

| 决策点 | 选项 A | 选项 B |
|--------|--------|--------|
| **Tag 继承语义** | 索引只存 Def 显式声明的 Tags，查询时用 `HasTagExact` | 索引存 Tag + 所有父 Tag，查询时用 `HasTag`（支持层级匹配） |
| **精确 vs 查询** | 只支持 `GetByTag`（单 Tag） | 同时支持 `GetByTagQuery`（`FGameplayTagQuery` 任意布尔组合） |
| **内存代价** | 小，只存 N 条映射 | 大，每属性展开所有父 Tag 后 ×3~5 倍 |
| **查询性能** | `O(1)` 但父 Tag 查询需走 Query 慢路径 | `O(1)` 所有层级 Tag 直达 |
| **与 `ByName` 对等** | 仅 Tag 索引，不为 FName 加索引（`TMap<FName, ...>` 已是 O(1)） | — |

推荐默认：**选项 A + Query 慢路径**——显式 Tag O(1) 命中，父 Tag 查询走 `FGameplayTagQuery::Matches` 线性遍历 `AttributeNames`。父 Tag 查询是低频场景（UI 分组、AOE 修饰），不值得用 ×3~5 内存换。

### 6. 与 State 侧索引的对称性

| 侧面 | State | Attribute |
|------|-------|-----------|
| Name 索引 | `ByName: TMap<FName, TArray<Inst*>>`（一名多实例） | `Attributes` 本身就是 `TMap<FName, Inst>`（一名一实例），无需额外 Name 索引 |
| Slot/Tag 索引 | `BySlot: TMap<FGameplayTag, TArray<Inst*>>` | `ByTag: TMap<FGameplayTag, TArray<FName>>` |
| 全集遍历 | `Instances: TArray<Inst*>` | `AttributeNames: TArray<FName>`（可从 `Attributes.GetKeys` 导出，索引里保留是为了稳定顺序 + 避免每次 `GenerateKeyArray`） |
| 清空 API | `ClearAll()` | `ClearAll()` |

### 7. 是否值得独立立项

**建议独立立项**，理由：

1. **触发条件是“出现第一个 ByTag 查询需求”**——当前代码里没有任何 `GetAttributesByTag` 调用点，纯粹是未来需求储备
2. **Tag 继承语义需要 brainstorm**（选项 A vs B 的决策会影响所有调用方）
3. **改造面涵盖所有属性写路径**：`AddAttribute` / `RemoveAttribute` / `ResetAttribute`，需要一次把事务性写清楚
4. **序列化/存档**：`FTcsAttributeIndex` 是否 `UPROPERTY()` 持久化，还是运行时从 `Attributes` 重建？取决于存档策略

**何时启动**：
- 游戏层出现第一个明确的 `GetAttributesByTag` 需求时
- 或者性能 Profiling 发现“遍历 Attributes + HasTag”是热点时

---

## 小结

| 优化 | 立项建议 | 改造面 | 前置条件 |
|------|----------|--------|----------|
| **优化 2（`StateInstanceIndex::ClearAll`）** | 不单独立项，附加到「事件广播批量化」PR 里一起做 | `StateInstanceIndex` + `RemoveAllStates` + `FinalizeStateRemoval` 可选参数 | Manager API 迁移完成（`FinalizeStateRemoval` 在 Component 内） |
| **优化 3（`FTcsAttributeIndex`）** | 独立立项，触发时启动 | `AttributeComponent` 所有属性写路径 + 新增 2 个查询 API + 存档策略决策 | 出现 `GetAttributesByTag` 实际需求 |
