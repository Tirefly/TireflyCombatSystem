# Design: SourceHandle 机制架构设计

## 架构概览

SourceHandle 机制通过引入统一的来源句柄结构，为 TCS 的属性系统和状态系统提供精确的生命周期管理能力。

```
┌─────────────────────────────────────────────────────────────────┐
│                      FTcsSourceHandle                           │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │ - int32 Id (唯一标识)                                      │ │
│  │ - FDataTableRowHandle SourceDefinition (效果定义引用)      │ │
│  │ - FName SourceName (人类可读名称，冗余字段)                │ │
│  │ - FGameplayTagContainer SourceTags (分类标签)              │ │
│  │ - TWeakObjectPtr<AActor> Instigator (施加者)              │ │
│  └───────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                            │
                            │ 被引用
                            ▼
        ┌───────────────────────────────────────┐
        │                                       │
        ▼                                       ▼
┌──────────────────────┐          ┌──────────────────────┐
│ AttributeModifier    │          │ StateInstance        │
│ Instance             │          │ (后续阶段)            │
│                      │          │                      │
│ + SourceHandle       │          │ + SourceHandle       │
│ + SourceName (兼容)  │          │                      │
└──────────────────────┘          └──────────────────────┘

核心概念：
- Source：效果的定义/配置（技能 Definition、装备效果 Definition 等）
- Instigator：实际造成效果的实体（角色、防御塔、陷阱等）
```

## 核心数据结构

### FTcsSourceHandle

```cpp
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSourceHandle
{
    GENERATED_BODY()

    // === 核心标识 ===

    // 全局唯一来源 ID（单调递增）
    UPROPERTY(BlueprintReadOnly)
    int32 Id = -1;

    // === Source 定义引用（效果的定义/配置） ===

    // Source 定义的 DataTable 引用（技能 Definition、状态 Definition 等）
    // 例如：指向技能数据表中的某一行
    // 注意：Source 是"效果的定义"，不是运行时实例
    UPROPERTY(BlueprintReadOnly)
    FDataTableRowHandle SourceDefinition;

    // Source 名称（冗余字段，用于快速访问和调试，避免每次查表）
    UPROPERTY(BlueprintReadOnly)
    FName SourceName;

    // === Source 分类（用于统计和筛选） ===

    // Source 类型标签（Skill、Equipment、Environment 等）
    UPROPERTY(BlueprintReadOnly)
    FGameplayTagContainer SourceTags;

    // === Instigator 引用（实际造成效果的实体） ===

    // 施加者（谁造成的效果：角色、防御塔、陷阱等）
    // 注意：Instigator 是"实际造成效果的实体"，可能与 Source 不同
    // 例如：陷阱（Instigator）由技能（Source）生成
    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> Instigator;

    // === 辅助方法 ===

    // 检查句柄是否有效
    bool IsValid() const
    {
        return Id >= 0;
    }

    // 获取 Source Definition（需要时查询完整定义）
    template<typename T>
    T* GetSourceDefinition() const
    {
        if (SourceDefinition.IsNull())
            return nullptr;
        return SourceDefinition.GetRow<T>(TEXT("SourceHandle"));
    }

    // 生成调试字符串
    FString ToDebugString() const;

    // 相等性比较（基于 Id）
    bool operator==(const FTcsSourceHandle& Other) const
    {
        return Id == Other.Id;
    }

    bool operator!=(const FTcsSourceHandle& Other) const
    {
        return Id != Other.Id;
    }

    // 哈希函数（用于 TMap，基于 Id）
    friend uint32 GetTypeHash(const FTcsSourceHandle& Handle)
    {
        return GetTypeHash(Handle.Id);
    }

    // 自定义网络序列化
    bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

// 启用自定义网络序列化
template<>
struct TStructOpsTypeTraits<FTcsSourceHandle> : public TStructOpsTypeTraitsBase2<FTcsSourceHandle>
{
    enum
    {
        WithNetSerializer = true,
    };
};
```

### 设计决策

#### 1. 为什么使用 int32 而不是 FGuid？

**选择 int32 的原因**：
- ✅ 内存占用小（4 bytes vs 16 bytes），对性能友好
- ✅ 简单直观，易于调试和日志输出
- ✅ 网络复制友好，int32 可以直接复制
- ✅ 作为 TMap key 性能更好（整数哈希比 GUID 哈希快）
- ✅ 符合 UE 常见模式（如 UniqueID、InstanceID 等）

**需要注意的事项**：
- ⚠️ 需要 WorldSubsystem 维护全局计数器
- ⚠️ PIE 多世界需要隔离（每个 World 独立计数）
- ⚠️ 线程安全需要考虑（在 GameThread 生成）

**实现策略**：
- 在 `UTcsAttributeManagerSubsystem` 中维护全局计数器
- 每次创建 SourceHandle 时递增计数器
- 使用 `FAtomicInt32` 或在 GameThread 中生成以确保线程安全
- PIE 多世界天然隔离（每个 World 有独立的 Subsystem 实例）

#### 2. 为什么使用 FDataTableRowHandle 引用 Source？

**Source 的本质**：
- Source 是"效果的定义/配置"，不是运行时实例
- 例如：技能 Definition、装备效果 Definition、状态 Definition
- Source 应该是持久化的数据，不依赖运行时对象的生命周期

**为什么不使用 Actor 或 UObject**：
- ❌ Actor 实例（子弹、光环、领域）可能被回收或销毁
- ❌ UObject 实例在统计时可能已经不存在
- ❌ 运行时实例的生命周期不可控

**FDataTableRowHandle 的优势**：
- ✅ 符合 TCS 现有架构（技能、状态都用 DataTable 定义）
- ✅ 轻量级（只存储表引用 + 行名，~16 bytes）
- ✅ 网络友好（可以直接网络复制）
- ✅ 持久化（Definition 永远存在，不依赖实例）
- ✅ 类型安全（可以通过 `GetRow<T>()` 获取具体类型）
- ✅ 易于查询（需要时可以获取完整 Definition）

**对于非 DataTable 的 Source**：
- 用户自定义的装备效果等可以只使用 SourceName + SourceTags
- 建议用户也使用 DataTable 以保持架构一致性

#### 3. Source vs Instigator 的区别

**关键概念**：
- **Source**：效果的定义/配置（技能 Definition、装备效果 Definition）
- **Instigator**：实际造成效果的实体（角色、防御塔、陷阱）

**示例 1：技能直接造成伤害**
```cpp
// 角色释放技能
FTcsSourceHandle Handle;
Handle.SourceDefinition = SkillDefinitionHandle;  // Source：技能 Definition
Handle.Instigator = PlayerCharacter;              // Instigator：角色
```

**示例 2：技能生成陷阱，陷阱造成伤害**
```cpp
// 技能生成陷阱时
ATrap* Trap = SpawnTrap();
Trap->CreationSource.SourceDefinition = SkillDefinitionHandle;  // Source：技能 Definition
Trap->CreationSource.Instigator = PlayerCharacter;              // Instigator：角色

// 陷阱造成伤害时
FTcsSourceHandle DamageHandle;
DamageHandle.SourceDefinition = SkillDefinitionHandle;  // Source：仍然是技能 Definition
DamageHandle.Instigator = Trap;                         // Instigator：陷阱
DamageHandle.SourceTags.AddTag("Source.Trap");          // 标签标识是陷阱造成的
```

**为什么这样设计**：
- ✅ Source 用于统计"什么效果造成的伤害"（技能、装备、环境）
- ✅ Instigator 用于统计"谁造成的伤害"（角色、防御塔、陷阱）
- ✅ 即使 Instigator 被销毁，Source 仍然可用于统计
- ✅ 支持级联效果（技能 -> 陷阱 -> 伤害）

#### 4. 为什么使用 TWeakObjectPtr 而不是 TObjectPtr？

**原因**：
- ✅ 不延长对象生命周期，避免 GC 问题
- ✅ 对象销毁后自动失效，安全
- ✅ Instigator 可能被销毁（如陷阱被回收），但不影响 Source 的统计

**注意事项**：
- ⚠️ TWeakObjectPtr 需要通过自定义 NetSerialize 处理网络同步
- ⚠️ 对象销毁后 Instigator 信息会丢失（但 Source 仍然可用）
- ✅ int32 Id 可以直接网络复制，非常简单

#### 5. 为什么保留 SourceName 冗余字段？

**原因**：
- ✅ 向后兼容，旧代码继续工作
- ✅ 人类可读，便于调试和日志
- ✅ 支持按名称汇总统计
- ✅ 避免每次都查询 DataTable（性能优化）

**规则**：
- 如果提供了 `SourceDefinition`，则 `SourceName` 应该与 Definition 中的名称一致
- 如果没有 `SourceDefinition`（用户自定义），则只使用 `SourceName`

## 属性系统集成

### FTcsAttributeModifierInstance 改造

```cpp
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeModifierInstance
{
    GENERATED_BODY()

    // ... 现有字段 ...

    // 新增：来源句柄
    UPROPERTY(BlueprintReadOnly)
    FTcsSourceHandle SourceHandle;

    // 保留：来源名称（向后兼容，作为 Debug/汇总字段）
    UPROPERTY(BlueprintReadOnly)
    FName SourceName = NAME_None;

    // ... 其他字段 ...
};
```

### FTcsAttributeChangeEventPayload 升级

**方案：彻底升级（开发期可接受）**

```cpp
USTRUCT(BlueprintType)
struct FTcsAttributeChangeEventPayload
{
    GENERATED_BODY()

    // ... 现有字段 ...

    // 升级：变化来源记录（从 FName 升级为 FTcsSourceHandle）
    UPROPERTY(BlueprintReadOnly)
    TMap<FTcsSourceHandle, float> ChangeSourceRecord;

    // ... 其他字段 ...
};
```

**理由**：
- TCS 仍处开发期，可接受破坏性变更
- 提供更详细的归因信息
- 为后续功能（如按施加者筛选）奠定基础

### UTcsAttributeManagerSubsystem API 扩展

#### 新增 API

```cpp
// 应用修改器（带 SourceHandle）
UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
bool ApplyModifierWithSourceHandle(
    AActor* CombatEntity,
    const TArray<FTcsAttributeModifierDefinition>& ModifierDefs,
    const FTcsSourceHandle& SourceHandle,
    AActor* Instigator,
    AActor* Target,
    TArray<FTcsAttributeModifierInstance>& OutModifiers);

// 按 SourceHandle 移除修改器
UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
bool RemoveModifiersBySourceHandle(
    AActor* CombatEntity,
    const FTcsSourceHandle& SourceHandle);

// 按 SourceHandle 获取修改器
UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
bool GetModifiersBySourceHandle(
    AActor* CombatEntity,
    const FTcsSourceHandle& SourceHandle,
    TArray<FTcsAttributeModifierInstance>& OutModifiers);

// 创建新的 SourceHandle
UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
static FTcsSourceHandle CreateSourceHandle(
    FName SourceName,
    UObject* SourceObject = nullptr,
    AActor* Instigator = nullptr,
    const FGameplayTagContainer& SourceTags = FGameplayTagContainer());
```

#### 旧 API 兼容

```cpp
// 旧 API 保持不变，内部自动创建 SourceHandle
UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
void ApplyModifier(AActor* CombatEntity, UPARAM(ref)TArray<FTcsAttributeModifierInstance>& Modifiers);

// 实现：内部为每个 Modifier 创建新的 SourceHandle
// SourceHandle.Id = 自动递增的唯一 ID
// SourceHandle.SourceName = Modifier.SourceName
```

## 句柄生成策略

### 生成时机

1. **显式创建**：用户调用 `CreateSourceHandle` 创建
2. **隐式创建**：旧 API 内部自动创建（向后兼容）

### 生成流程

```cpp
// 在 UTcsAttributeManagerSubsystem 中
class UTcsAttributeManagerSubsystem : public UWorldSubsystem
{
protected:
    // 全局 SourceHandle ID 管理器（单调递增）
    UPROPERTY()
    int32 GlobalSourceHandleIdCounter = 0;

public:
    // 创建 SourceHandle（推荐方式：提供 DataTable 引用）
    FTcsSourceHandle CreateSourceHandle(
        const FDataTableRowHandle& SourceDefinition,
        FName SourceName,
        AActor* Instigator,
        const FGameplayTagContainer& SourceTags)
    {
        FTcsSourceHandle Handle;
        Handle.Id = ++GlobalSourceHandleIdCounter;  // 生成唯一 ID（单调递增）
        Handle.SourceDefinition = SourceDefinition;
        Handle.SourceName = SourceName;
        Handle.Instigator = Instigator;
        Handle.SourceTags = SourceTags;
        return Handle;
    }

    // 创建 SourceHandle（简化方式：仅提供名称，用于用户自定义）
    FTcsSourceHandle CreateSourceHandleSimple(
        FName SourceName,
        AActor* Instigator,
        const FGameplayTagContainer& SourceTags)
    {
        FTcsSourceHandle Handle;
        Handle.Id = ++GlobalSourceHandleIdCounter;
        Handle.SourceDefinition = FDataTableRowHandle();  // 空引用
        Handle.SourceName = SourceName;
        Handle.Instigator = Instigator;
        Handle.SourceTags = SourceTags;
        return Handle;
    }
};
```

**线程安全说明**：
- Subsystem 的方法在 GameThread 中调用，天然线程安全
- 如需在多线程环境使用，可以使用 `FAtomicInt32` 替代 `int32`

**使用示例**：

```cpp
// 示例 1：技能系统创建 SourceHandle
void ApplySkillEffect(const FTcsStateDefinition& SkillDef, AActor* Caster, AActor* Target)
{
    // 创建 SourceHandle
    FDataTableRowHandle SkillDefHandle;
    SkillDefHandle.DataTable = SkillDataTable;
    SkillDefHandle.RowName = SkillDef.RowName;

    FTcsSourceHandle Handle = AttributeManager->CreateSourceHandle(
        SkillDefHandle,
        SkillDef.StateName,
        Caster,
        FGameplayTagContainer(FGameplayTag::RequestGameplayTag("Source.Skill")));

    // 应用效果
    ApplyModifierWithSourceHandle(Target, ModifierDefs, Handle, Caster, Target, OutModifiers);
}

// 示例 2：用户自定义装备效果
void ApplyEquipmentEffect(FName EffectName, AActor* EquipmentOwner, AActor* Target)
{
    // 创建简化的 SourceHandle（无 DataTable 引用）
    FTcsSourceHandle Handle = AttributeManager->CreateSourceHandleSimple(
        EffectName,
        EquipmentOwner,
        FGameplayTagContainer(FGameplayTag::RequestGameplayTag("Source.Equipment")));

    // 应用效果
    ApplyModifierWithSourceHandle(Target, ModifierDefs, Handle, EquipmentOwner, Target, OutModifiers);
}
```
```

### 生命周期管理

```
创建 SourceHandle
    ↓
应用到 AttributeModifier/State
    ↓
存储在实例中
    ↓
按需撤销（通过 SourceHandle）
    ↓
实例销毁，SourceHandle 随之释放
```

## 与 State 系统的统一（后续阶段）

### 统一来源管理

```cpp
// 技能系统创建统一的 SourceHandle
FTcsSourceHandle SkillHandle = CreateSourceHandle(
    TEXT("Skill_Fireball"),
    SkillInstance,
    Instigator,
    SkillTags);

// 应用到 State
ApplySkillState(Target, StateDefinition, SkillHandle);

// 应用到 AttributeModifier
ApplyModifierWithSourceHandle(Target, ModifierDefs, SkillHandle, Instigator, Target, OutModifiers);

// 技能结束时统一清理
RemoveStateBySourceHandle(Target, SkillHandle);
RemoveModifiersBySourceHandle(Target, SkillHandle);
```

### 移除策略

**由调用方决定**：
- TCS 提供按句柄删除能力，但不强制什么时候删
- 对于"Buff 类 StateTree 停止时移除 Buff 效果"，建议在 StateTree 中显式调用
- 提供可复用的 StateTreeTask 触发"按 SourceHandle 撤销"

## 性能考虑

### 内存占用

```
FTcsSourceHandle 大小估算：
- int32 Id: 4 bytes
- FDataTableRowHandle: ~16 bytes (TObjectPtr + FName)
- FName SourceName: 8 bytes
- FGameplayTagContainer: ~24 bytes (取决于标签数量)
- TWeakObjectPtr Instigator: 8 bytes
总计：~60 bytes

每个 AttributeModifierInstance 增加：~60 bytes
```

**影响评估**：
- 假设 100 个并发 Modifier：增加 ~6 KB
- 相比纯 FName 方案增加约 5 KB（每个句柄增加 ~52 bytes）
- 可接受的内存开销，换取了完整的追踪和统计能力

### 查询性能

**按 SourceHandle 查询**：
- 使用 `TMap<int32, TArray<FTcsAttributeModifierInstance>>` 索引
- O(1) 查询复杂度
- int32 哈希性能优于 FGuid（整数哈希更快）
- 需要在 AttributeComponent 中维护索引

**实现建议**：
```cpp
// 在 UTcsAttributeComponent 中增加索引
UPROPERTY()
TMap<int32, TArray<int32>> SourceHandleToModifierIndices;
```

## 调试支持

### 日志输出

```cpp
FString FTcsSourceHandle::ToDebugString() const
{
    FString Result = FString::Printf(TEXT("[%s|%d]"),
        *SourceName.ToString(),
        Id);

    if (Instigator.IsValid())
    {
        Result += FString::Printf(TEXT(" Instigator=%s"),
            *Instigator->GetName());
    }

    return Result;
}
```

### 编辑器可视化

**后续可扩展**：
- 在编辑器中显示 SourceHandle 的详细信息
- 提供按 SourceHandle 筛选的调试工具
- 可视化来源关系图

## 网络同步实现

### NetSerialize 实现（借鉴 GAS）

```cpp
bool FTcsSourceHandle::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
    // 序列化核心字段（必需）
    Ar << Id;

    // 序列化 SourceDefinition（FDataTableRowHandle）
    // UE 会自动处理 TObjectPtr 和 FName 的序列化
    Ar << SourceDefinition.DataTable;
    Ar << SourceDefinition.RowName;

    // 序列化 SourceName（冗余字段，用于快速访问）
    Ar << SourceName;

    // 序列化 SourceTags（使用内置方法）
    SourceTags.NetSerialize(Ar, Map, bOutSuccess);

    // 条件序列化 Instigator（优化：只在有效时才序列化）
    uint8 bHasInstigator = 0;
    if (Ar.IsSaving())
    {
        bHasInstigator = Instigator.IsValid() ? 1 : 0;
    }
    Ar.SerializeBits(&bHasInstigator, 1);

    if (bHasInstigator)
    {
        // UE 会自动处理 Actor 引用的网络同步（通过 NetGUID）
        Ar << Instigator;
    }
    else if (Ar.IsLoading())
    {
        // 加载时清空 Instigator
        Instigator = nullptr;
    }

    bOutSuccess = true;
    return true;
}
```

### 网络同步机制说明

**UE 的 Actor 引用复制原理**：
1. **服务器端**：Actor 有唯一的 NetGUID
2. **序列化时**：`Ar << Actor` 会转换为 NetGUID
3. **客户端接收**：UE 会自动映射 NetGUID 到本地 Actor 实例
4. **TWeakObjectPtr 支持**：虽然是弱指针，但可以通过 `Ar <<` 序列化

**字段复制说明**：
- ✅ `Id`：int32，直接复制
- ✅ `SourceDefinition.DataTable`：TObjectPtr，UE 自动处理
- ✅ `SourceDefinition.RowName`：FName，直接复制
- ✅ `SourceName`：FName，直接复制
- ✅ `SourceTags`：FGameplayTagContainer 有内置 NetSerialize
- ✅ `Instigator`：TWeakObjectPtr<AActor>，通过自定义逻辑处理

**优化策略**：
- 使用 RepBits 条件复制（只在 Instigator 有效时才序列化）
- 减少网络带宽占用

### 网络同步示例

```cpp
// 服务器端：创建 SourceHandle 并应用效果
void AMyCharacter::Server_ApplySkillEffect_Implementation(AActor* Target)
{
    // 创建 SourceHandle
    FTcsSourceHandle Handle = AttributeManager->CreateSourceHandle(
        SkillDefinitionHandle,
        TEXT("Skill_Fireball"),
        this,  // Instigator
        SkillTags);

    // 应用效果（会自动复制到客户端）
    ApplyModifierWithSourceHandle(Target, ModifierDefs, Handle, this, Target, OutModifiers);
}

// 客户端：接收到复制的 SourceHandle
// - Id：正确复制
// - SourceDefinition：正确复制（DataTable 引用）
// - SourceName：正确复制
// - SourceTags：正确复制
// - Instigator：正确映射到客户端的 Actor 实例
```

## 完整使用示例

### 示例 1：技能直接造成伤害

```cpp
// 角色释放技能
void AMyCharacter::CastSkill(const FTcsStateDefinition& SkillDef, AActor* Target)
{
    // 准备 SourceDefinition
    FDataTableRowHandle SkillDefHandle;
    SkillDefHandle.DataTable = SkillDataTable;
    SkillDefHandle.RowName = SkillDef.RowName;

    // 创建 SourceHandle
    FTcsSourceHandle Handle = AttributeManager->CreateSourceHandle(
        SkillDefHandle,
        SkillDef.StateName,
        this,  // Instigator：角色自己
        FGameplayTagContainer(FGameplayTag::RequestGameplayTag("Source.Skill")));

    // 应用伤害
    TArray<FTcsAttributeModifierInstance> OutModifiers;
    AttributeManager->ApplyModifierWithSourceHandle(
        Target, DamageModifierDefs, Handle, this, Target, OutModifiers);
}
```

### 示例 2：技能生成陷阱，陷阱造成伤害（级联效果）

```cpp
// 陷阱类
class ATrap : public AActor
{
public:
    // 陷阱保存生成它的 SourceHandle
    UPROPERTY()
    FTcsSourceHandle CreationSource;

    void ApplyDamage(AActor* Target)
    {
        // 创建新的 SourceHandle（陷阱造成的伤害）
        FTcsSourceHandle DamageHandle;
        DamageHandle.Id = AttributeManager->GenerateNewSourceHandleId();

        // Source 继承自创建它的技能
        DamageHandle.SourceDefinition = CreationSource.SourceDefinition;
        DamageHandle.SourceName = CreationSource.SourceName;

        // Instigator 是陷阱本身
        DamageHandle.Instigator = this;

        // 标签组合：技能 + 陷阱
        DamageHandle.SourceTags = CreationSource.SourceTags;
        DamageHandle.SourceTags.AddTag(FGameplayTag::RequestGameplayTag("Source.Trap"));

        // 应用伤害
        TArray<FTcsAttributeModifierInstance> OutModifiers;
        AttributeManager->ApplyModifierWithSourceHandle(
            Target, DamageModifierDefs, DamageHandle, this, Target, OutModifiers);
    }
};

// 技能生成陷阱
void AMyCharacter::SpawnTrap(const FTcsStateDefinition& SkillDef, FVector Location)
{
    // 创建技能的 SourceHandle
    FDataTableRowHandle SkillDefHandle;
    SkillDefHandle.DataTable = SkillDataTable;
    SkillDefHandle.RowName = SkillDef.RowName;

    FTcsSourceHandle SkillHandle = AttributeManager->CreateSourceHandle(
        SkillDefHandle,
        SkillDef.StateName,
        this,
        FGameplayTagContainer(FGameplayTag::RequestGameplayTag("Source.Skill")));

    // 生成陷阱
    ATrap* Trap = GetWorld()->SpawnActor<ATrap>(TrapClass, Location, FRotator::ZeroRotator);
    Trap->CreationSource = SkillHandle;  // 陷阱保存技能的 SourceHandle
}
```

### 示例 3：死亡前伤害统计（您的用例）

```cpp
// 角色死亡时统计伤害来源
void AMyCharacter::OnDeath()
{
    // 获取过去 10 秒内的所有属性变化记录
    TArray<FTcsAttributeChangeEventPayload> RecentDamages = GetRecentDamageEvents(10.0f);

    // 统计数据结构
    TMap<FName, float> DamageBySkill;           // 按技能名统计
    TMap<AActor*, float> DamageByInstigator;    // 按施加者统计
    TMap<FGameplayTag, float> DamageByType;     // 按类型统计
    TArray<FDamageSourceInfo> DetailedDamages;  // 详细记录

    for (const auto& DamageEvent : RecentDamages)
    {
        for (const auto& [SourceHandle, DamageAmount] : DamageEvent.ChangeSourceRecord)
        {
            // ✅ 按技能名统计（即使陷阱被销毁，技能名仍然可用）
            DamageBySkill.FindOrAdd(SourceHandle.SourceName) += DamageAmount;

            // ✅ 按施加者统计（如果还活着）
            if (SourceHandle.Instigator.IsValid())
            {
                DamageByInstigator.FindOrAdd(SourceHandle.Instigator.Get()) += DamageAmount;
            }

            // ✅ 按类型统计
            if (SourceHandle.SourceTags.HasTag(FGameplayTag::RequestGameplayTag("Source.Skill")))
            {
                DamageByType.FindOrAdd(FGameplayTag::RequestGameplayTag("Source.Skill")) += DamageAmount;
            }
            else if (SourceHandle.SourceTags.HasTag(FGameplayTag::RequestGameplayTag("Source.Environment")))
            {
                DamageByType.FindOrAdd(FGameplayTag::RequestGameplayTag("Source.Environment")) += DamageAmount;
            }

            // ✅ 查询完整的技能 Definition（如果需要）
            if (auto* SkillDef = SourceHandle.GetSourceDefinition<FTcsStateDefinition>())
            {
                UE_LOG(LogTemp, Log, TEXT("Damaged by skill: %s (Level %d, Damage: %.2f)"),
                    *SkillDef->StateName.ToString(),
                    SkillDef->DefaultLevel,
                    DamageAmount);
            }

            // 记录详细信息
            FDamageSourceInfo Info;
            Info.SourceHandle = SourceHandle;
            Info.DamageAmount = DamageAmount;
            Info.Timestamp = DamageEvent.Timestamp;
            DetailedDamages.Add(Info);
        }
    }

    // ✅ 现在您可以知道：
    // 1. 谁杀了我（DamageByInstigator 中伤害最高的）
    AActor* Killer = nullptr;
    float MaxDamage = 0.0f;
    for (const auto& [Instigator, Damage] : DamageByInstigator)
    {
        if (Damage > MaxDamage)
        {
            MaxDamage = Damage;
            Killer = Instigator;
        }
    }

    // 2. 什么技能造成了最多伤害
    FName DeadliestSkill = NAME_None;
    float MaxSkillDamage = 0.0f;
    for (const auto& [SkillName, Damage] : DamageBySkill)
    {
        if (Damage > MaxSkillDamage)
        {
            MaxSkillDamage = Damage;
            DeadliestSkill = SkillName;
        }
    }

    // 3. 伤害类型分布
    UE_LOG(LogTemp, Log, TEXT("Death Summary:"));
    UE_LOG(LogTemp, Log, TEXT("  Killer: %s"), Killer ? *Killer->GetName() : TEXT("Unknown"));
    UE_LOG(LogTemp, Log, TEXT("  Deadliest Skill: %s (%.2f damage)"), *DeadliestSkill.ToString(), MaxSkillDamage);
    UE_LOG(LogTemp, Log, TEXT("  Damage by Type:"));
    for (const auto& [Type, Damage] : DamageByType)
    {
        UE_LOG(LogTemp, Log, TEXT("    %s: %.2f"), *Type.ToString(), Damage);
    }
}
```

### 示例 4：用户自定义装备效果

```cpp
// 装备系统应用效果
void AEquipmentComponent::ApplyEquipmentEffect(FName EffectName, AActor* Target)
{
    // 创建简化的 SourceHandle（无 DataTable 引用）
    FTcsSourceHandle Handle = AttributeManager->CreateSourceHandleSimple(
        EffectName,
        GetOwner(),  // Instigator：装备拥有者
        FGameplayTagContainer(FGameplayTag::RequestGameplayTag("Source.Equipment")));

    // 应用效果
    TArray<FTcsAttributeModifierInstance> OutModifiers;
    AttributeManager->ApplyModifierWithSourceHandle(
        Target, EffectModifierDefs, Handle, GetOwner(), Target, OutModifiers);

    // 保存 Handle 以便后续撤销
    ActiveEffectHandles.Add(Handle);
}

// 装备卸下时撤销效果
void AEquipmentComponent::RemoveEquipmentEffect(const FTcsSourceHandle& Handle)
{
    AttributeManager->RemoveModifiersBySourceHandle(GetOwner(), Handle);
    ActiveEffectHandles.Remove(Handle);
}
```

### 单元测试

1. **SourceHandle 创建和验证**
   - 测试 `CreateSourceHandle` 生成唯一 ID
   - 测试 `IsValid()` 正确性
   - 测试 `ToDebugString()` 输出

2. **AttributeModifier 应用和撤销**
   - 测试按 SourceHandle 应用修改器
   - 测试按 SourceHandle 撤销修改器
   - 测试同一 SourceName 多次应用的独立性

3. **事件归因**
   - 测试 `ChangeSourceRecord` 正确记录 SourceHandle
   - 测试按 SourceHandle 查询变化来源

### 集成测试

1. **技能系统集成**
   - 测试技能应用和撤销的完整流程
   - 测试同一技能多次触发的独立管理

2. **Buff 系统集成**
   - 测试 Buff 应用和移除
   - 测试 Buff 叠加和独立管理

### 性能测试

1. **大量 Modifier 并发**
   - 测试 1000+ Modifier 的性能
   - 测试按 SourceHandle 查询的性能

2. **内存占用**
   - 测试 SourceHandle 的内存开销
   - 测试索引结构的内存开销

## 迁移指南

### 对于新代码

**推荐使用新 API**：
```cpp
// 创建 SourceHandle
FTcsSourceHandle Handle = UTcsAttributeManagerSubsystem::CreateSourceHandle(
    TEXT("Buff_Strength"),
    BuffInstance,
    Instigator,
    BuffTags);

// 应用修改器
TArray<FTcsAttributeModifierInstance> OutModifiers;
AttributeManager->ApplyModifierWithSourceHandle(
    Target, ModifierDefs, Handle, Instigator, Target, OutModifiers);

// 撤销修改器
AttributeManager->RemoveModifiersBySourceHandle(Target, Handle);
```

### 对于旧代码

**无需修改，继续工作**：
```cpp
// 旧代码继续工作
TArray<FTcsAttributeModifierInstance> Modifiers;
// ... 填充 Modifiers ...
AttributeManager->ApplyModifier(Target, Modifiers);
```

**可选升级**：
- 如需精确撤销，迁移到新 API
- 如需详细归因，使用 SourceHandle

## 总结

SourceHandle 机制通过引入统一的来源句柄，为 TCS 提供了精确的生命周期管理能力，同时保持了向后兼容性和扩展性。该设计为后续与 State 系统的统一奠定了坚实的基础。