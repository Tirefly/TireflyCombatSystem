# SourceHandle 使用指南

## 概述

SourceHandle 是 TireflyCombatSystem (TCS) 插件中用于**统一追踪效果来源**的核心机制。它解决了传统 `SourceName` 字符串无法提供完整来源信息的问题，支持效果的精确管理和生命周期控制。

**版本**: 1.0
**适用引擎**: Unreal Engine 5.6+

---

## 核心概念

### Source vs Instigator

SourceHandle 明确区分两个重要概念：

| 概念 | 含义 | 类型 | 示例 |
|------|------|------|------|
| **Source** | 效果的**定义/配置** | `FDataTableRowHandle` | 技能 Definition、装备效果 Definition |
| **Instigator** | **实际造成效果的实体** | `AActor*` (TWeakObjectPtr) | 角色、陷阱、投射物 |

**典型场景**：
- **技能直接造成伤害**: Source = 技能 Definition，Instigator = 角色
- **陷阱造成伤害**: Source = 技能 Definition（继承自创建陷阱的技能），Instigator = 陷阱

---

## 数据结构

```cpp
USTRUCT(BlueprintType)
struct FTcsSourceHandle
{
    GENERATED_BODY()

    // 全局唯一来源 ID (单调递增, -1 表示无效)
    UPROPERTY(BlueprintReadOnly)
    int32 Id = -1;

    // Source 定义的 DataTable 引用
    UPROPERTY(BlueprintReadOnly)
    FDataTableRowHandle SourceDefinition;

    // Source 名称 (冗余字段, 用于快速访问和调试)
    UPROPERTY(BlueprintReadOnly)
    FName SourceName = NAME_None;

    // Source 类型标签 (用于分类和过滤)
    UPROPERTY(BlueprintReadOnly)
    FGameplayTagContainer SourceTags;

    // 施加者 (实际造成效果的实体)
    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> Instigator;
};
```

---

## 基本使用

### 1. 创建 SourceHandle

#### C++ 完整版本

```cpp
// 获取 AttributeManagerSubsystem
UTcsAttributeManagerSubsystem* AttrMgr = GetWorld()->GetGameInstance()
    ->GetSubsystem<UTcsAttributeManagerSubsystem>();

// 创建 SourceHandle (完整版本)
FDataTableRowHandle SkillDef;
SkillDef.DataTable = SkillDataTable;
SkillDef.RowName = FName("Skill_Fireball");

FGameplayTagContainer SourceTags;
SourceTags.AddTag(FGameplayTag::RequestGameplayTag("Source.Skill"));

FTcsSourceHandle SourceHandle = AttrMgr->CreateSourceHandle(
    SkillDef,           // Source Definition
    FName("Fireball"),  // Source Name
    SourceTags,         // Source Tags
    CasterActor         // Instigator
);
```

#### C++ 简化版本（用户自定义效果）

```cpp
// 创建 SourceHandle (简化版本, 无 DataTable Definition)
FTcsSourceHandle SourceHandle = AttrMgr->CreateSourceHandleSimple(
    FName("CustomBuff"),  // Source Name
    BuffOwner             // Instigator
);
```

#### 蓝图版本

- `CreateSourceHandle`（完整版本）
- `CreateSourceHandleSimple`（简化版本）

---

### 2. 应用属性修改器

#### C++ 版本

```cpp
// 使用 SourceHandle 应用修改器
TArray<FName> ModifierIds = { FName("Mod_AttackBoost"), FName("Mod_SpeedBoost") };
TArray<FTcsAttributeModifierInstance> OutModifiers;

bool bSuccess = AttrMgr->ApplyModifierWithSourceHandle(
    TargetActor,    // 目标实体
    SourceHandle,   // 来源句柄
    ModifierIds,    // 修改器 ID 列表
    OutModifiers    // 输出创建的修改器实例
);
```

#### 蓝图版本

- `ApplyModifierWithSourceHandle`

---

### 3. 移除属性修改器

#### C++ 版本

```cpp
// 按 SourceHandle 移除修改器
bool bSuccess = AttrMgr->RemoveModifiersBySourceHandle(
    TargetActor,    // 目标实体
    SourceHandle    // 来源句柄
);
```

**特性**：
- ✅ 精确移除：只移除匹配该 SourceHandle.Id 的修改器
- ✅ 独立管理：同一技能多次触发，每次生成不同的 SourceHandle，可独立撤销

#### 蓝图版本

- `RemoveModifiersBySourceHandle`

---

### 4. 查询属性修改器

#### C++ 版本

```cpp
// 按 SourceHandle 查询修改器
TArray<FTcsAttributeModifierInstance> OutModifiers;
bool bFound = AttrMgr->GetModifiersBySourceHandle(
    TargetActor,    // 目标实体
    SourceHandle,   // 来源句柄
    OutModifiers    // 输出查询到的修改器实例
);
```

---

## 实战示例

### 示例 1: 技能 Buff 系统

```cpp
// 技能释放时创建 SourceHandle
FTcsSourceHandle BuffSourceHandle = AttrMgr->CreateSourceHandle(
    SkillDefinition,
    FName("Skill_PowerUp"),
    SkillTags,
    CasterActor
);

// 应用 Buff 效果
TArray<FName> BuffModifiers = { FName("Mod_Attack_+50"), FName("Mod_Defense_+30") };
TArray<FTcsAttributeModifierInstance> AppliedModifiers;
AttrMgr->ApplyModifierWithSourceHandle(TargetActor, BuffSourceHandle, BuffModifiers, AppliedModifiers);

// 保存 SourceHandle 用于后续撤销
BuffComponent->SaveBuffSourceHandle(BuffSourceHandle);

// Buff 结束时统一移除
AttrMgr->RemoveModifiersBySourceHandle(TargetActor, BuffSourceHandle);
```

---

### 示例 2: 陷阱级联效果

```cpp
// 1. 技能生成陷阱时，陷阱保存技能的 SourceHandle
class ATrap : public AActor
{
    FTcsSourceHandle CreatorSourceHandle;  // 保存创建者的 SourceHandle
};

// 技能创建陷阱
FTcsSourceHandle SkillSourceHandle = AttrMgr->CreateSourceHandle(...);
ATrap* Trap = SpawnTrap();
Trap->CreatorSourceHandle = SkillSourceHandle;

// 2. 陷阱造成伤害时，创建新的 SourceHandle 继承 Source
FTcsSourceHandle TrapDamageHandle = AttrMgr->CreateSourceHandle(
    Trap->CreatorSourceHandle.SourceDefinition,  // 继承技能的 Source Definition
    Trap->CreatorSourceHandle.SourceName,        // 继承技能名称
    CombinedTags,                                // 组合标签: "Source.Skill" + "Source.Trap"
    Trap                                         // Instigator 是陷阱本身
);
```

---

### 示例 3: 死亡伤害统计

```cpp
// 监听属性变化事件
void AMyCharacter::OnHealthChanged(const TArray<FTcsAttributeChangeEventPayload>& Payloads)
{
    for (const FTcsAttributeChangeEventPayload& Payload : Payloads)
    {
        if (Payload.AttributeName == FName("Health"))
        {
            // 遍历所有伤害来源
            for (const TPair<FTcsSourceHandle, float>& SourcePair : Payload.ChangeSourceRecord)
            {
                const FTcsSourceHandle& Source = SourcePair.Key;
                float Damage = -SourcePair.Value;  // 负值表示伤害

                // 记录伤害来源
                DamageHistory.Add(FDamageRecord{
                    Source.SourceName,
                    Source.Instigator.Get(),
                    Damage,
                    FDateTime::UtcNow()
                });
            }
        }
    }
}

// 角色死亡时统计
void AMyCharacter::OnDeath()
{
    // 统计最近 10 秒的伤害来源
    FDateTime TenSecondsAgo = FDateTime::UtcNow() - FTimespan::FromSeconds(10);

    TMap<FName, float> DamageBySkill;
    TMap<AActor*, float> DamageByInstigator;

    for (const FDamageRecord& Record : DamageHistory)
    {
        if (Record.Timestamp >= TenSecondsAgo)
        {
            DamageBySkill.FindOrAdd(Record.SkillName) += Record.Damage;
            if (Record.Instigator.IsValid())
            {
                DamageByInstigator.FindOrAdd(Record.Instigator.Get()) += Record.Damage;
            }
        }
    }

    // 输出统计结果
    UE_LOG(LogTemp, Log, TEXT("Death Statistics:"));
    for (const auto& Pair : DamageBySkill)
    {
        UE_LOG(LogTemp, Log, TEXT("  Skill %s: %.2f damage"), *Pair.Key.ToString(), Pair.Value);
    }
}
```

---

## 网络同步

SourceHandle 支持自动网络同步，使用自定义 `NetSerialize` 实现：

### 同步内容
- ✅ **Id**: 全局唯一 ID
- ✅ **SourceDefinition**: DataTable 引用（DataTable + RowName）
- ✅ **SourceName**: Source 名称
- ✅ **SourceTags**: GameplayTag 容器
- ✅ **Instigator**: Actor 引用（条件复制，仅在有效时同步）

### 网络优化
- **条件复制**: Instigator 无效时不占用带宽
- **数据大小**: 约 40-60 bytes（取决于字段内容）
- **自动映射**: UE 自动处理 Actor 引用的 NetGUID 映射

### 注意事项
1. **DataTable 一致性**: 确保客户端和服务器拥有相同的 DataTable
2. **Instigator 映射**: 如果客户端无法映射 Instigator NetGUID，Instigator 将为空（但 SourceHandle 仍然有效）
3. **本地索引**: `SourceHandleIdToModifierIndices` 是本地优化数据，不参与网络复制

---

## 性能优化

### 索引加速

TCS 使用 `TMap<int32, TArray<int32>>` 索引加速按 SourceHandle 查询：

```cpp
// AttributeComponent 中的索引
TMap<int32, TArray<int32>> SourceHandleIdToModifierIndices;
```

**性能特性**：
- **查询复杂度**: O(1) - 通过 SourceHandle.Id 直接查找
- **内存占用**: 约 8 bytes per modifier（索引开销）
- **自动维护**: 应用/移除 Modifier 时自动更新索引

---

## 最佳实践

### ✅ 推荐做法

1. **保存 SourceHandle 用于撤销**
   ```cpp
   // 应用效果时保存 SourceHandle
   FTcsSourceHandle Handle = ApplyBuff(...);
   BuffComponent->SaveHandle(Handle);

   // 效果结束时使用保存的 Handle 撤销
   RemoveModifiersBySourceHandle(Target, Handle);
   ```

2. **使用 Source Definition 引用持久化数据**
   ```cpp
   // 推荐: 使用 DataTable 引用
   FDataTableRowHandle SkillDef;
   SkillDef.DataTable = SkillTable;
   SkillDef.RowName = FName("Skill_Fireball");
   ```

3. **级联效果继承 Source**
   ```cpp
   // 陷阱继承技能的 Source Definition
   FTcsSourceHandle TrapHandle = CreateSourceHandle(
       SkillHandle.SourceDefinition,  // 继承
       SkillHandle.SourceName,        // 继承
       CombinedTags,                  // 组合标签
       TrapActor                      // 新的 Instigator
   );
   ```

4. **使用 SourceTags 分类效果**
   ```cpp
   FGameplayTagContainer Tags;
   Tags.AddTag(FGameplayTag::RequestGameplayTag("Source.Skill"));
   Tags.AddTag(FGameplayTag::RequestGameplayTag("Source.Fire"));
   ```

### ❌ 避免的做法

1. **不要手动构造 SourceHandle**
   ```cpp
   // 错误: 手动构造会导致 ID 冲突
   FTcsSourceHandle Handle;
   Handle.Id = 123;  // ❌ 不要这样做

   // 正确: 使用 Subsystem 创建
   FTcsSourceHandle Handle = AttrMgr->CreateSourceHandle(...);  // ✅
   ```

2. **不要在 Source 中存储运行时实例**
   ```cpp
   // 错误: 使用 Actor 作为 Source（会被销毁）
   // Source 应该是持久化的 Definition，不是运行时实例
   ```

3. **不要忽略 Instigator 失效的情况**
   ```cpp
   // 错误: 直接使用 Instigator 指针
   AActor* Instigator = Handle.Instigator.Get();
   Instigator->DoSomething();  // ❌ 可能崩溃

   // 正确: 检查有效性
   if (Handle.Instigator.IsValid())
   {
       Handle.Instigator->DoSomething();  // ✅
   }
   ```

---

## 迁移指南

### 从旧 API 迁移到新 API

#### 旧代码（使用 SourceName）

```cpp
// 旧 API: 只有 SourceName
FTcsAttributeModifierInstance Modifier;
AttrMgr->CreateAttributeModifier(
    ModifierId,
    FName("MySkill"),  // SourceName
    Instigator,
    Target,
    Modifier
);
AttrMgr->ApplyModifier(Target, { Modifier });
```

#### 新代码（使用 SourceHandle）

```cpp
// 新 API: 使用 SourceHandle
FTcsSourceHandle SourceHandle = AttrMgr->CreateSourceHandle(
    SkillDefinition,
    FName("MySkill"),
    SkillTags,
    Instigator
);

TArray<FTcsAttributeModifierInstance> Modifiers;
AttrMgr->ApplyModifierWithSourceHandle(
    Target,
    SourceHandle,
    { ModifierId },
    Modifiers
);
```

### 向后兼容性

旧 API（仅 `SourceName`）仍然可用，但只会填充 `SourceName`（`SourceHandle.Id` 会保持无效值 `-1`），因此无法使用 “Remove/Get by SourceHandle” 的能力。

```cpp
// 旧 API 仍然工作（但无法按 SourceHandle 撤销/查询）
AttrMgr->CreateAttributeModifier(...);  // ✅ 只填 SourceName
AttrMgr->ApplyModifier(...);            // ✅ 兼容

// 若希望可撤销/可追踪：请使用 SourceHandle 版本 API
FTcsSourceHandle Handle = AttrMgr->CreateSourceHandleSimple(FName("MySkill"), Instigator);
AttrMgr->ApplyModifierWithSourceHandle(Target, Handle, { ModifierId }, OutModifiers);
```

---

## API 参考

### UTcsAttributeManagerSubsystem

#### CreateSourceHandle
```cpp
UFUNCTION(BlueprintCallable)
FTcsSourceHandle CreateSourceHandle(
    const FDataTableRowHandle& SourceDefinition,
    FName SourceName,
    const FGameplayTagContainer& SourceTags,
    AActor* Instigator
);
```
创建完整的 SourceHandle。

#### CreateSourceHandleSimple
```cpp
UFUNCTION(BlueprintCallable)
FTcsSourceHandle CreateSourceHandleSimple(
    FName SourceName,
    AActor* Instigator
);
```
创建简化的 SourceHandle（用于用户自定义效果）。

#### ApplyModifierWithSourceHandle
```cpp
UFUNCTION(BlueprintCallable)
bool ApplyModifierWithSourceHandle(
    AActor* CombatEntity,
    const FTcsSourceHandle& SourceHandle,
    const TArray<FName>& ModifierIds,
    TArray<FTcsAttributeModifierInstance>& OutModifiers
);
```
使用 SourceHandle 应用属性修改器。

#### RemoveModifiersBySourceHandle
```cpp
UFUNCTION(BlueprintCallable)
bool RemoveModifiersBySourceHandle(
    AActor* CombatEntity,
    const FTcsSourceHandle& SourceHandle
);
```
按 SourceHandle 移除属性修改器。

#### GetModifiersBySourceHandle
```cpp
UFUNCTION(BlueprintCallable)
bool GetModifiersBySourceHandle(
    AActor* CombatEntity,
    const FTcsSourceHandle& SourceHandle,
    TArray<FTcsAttributeModifierInstance>& OutModifiers
) const;
```
按 SourceHandle 查询属性修改器。

---

## 常见问题

### Q: SourceHandle 的 ID 会重复吗？
**A**: 不会。ID 由 `UTcsAttributeManagerSubsystem` 的单调递增计数器生成，保证全局唯一（在同一 World 内）。

### Q: Instigator 被销毁后 SourceHandle 还有效吗？
**A**: 有效。SourceHandle 使用 `TWeakObjectPtr` 存储 Instigator，即使 Instigator 被销毁，SourceHandle 仍然有效，只是 `Instigator.IsValid()` 返回 false。你仍然可以通过 `SourceName` 和 `SourceTags` 追踪来源。

### Q: 如何在网络游戏中使用 SourceHandle？
**A**: SourceHandle 已实现 `NetSerialize`，因此当它作为 **已复制（Replicated）属性** 的一部分时，可以进行网络同步。注意：
1. 是否会同步，取决于你把包含 SourceHandle 的字段标记为 `Replicated/ReplicatedUsing`，并在 `GetLifetimeReplicatedProps` 中注册
2. `SourceDefinition.DataTable` 需要客户端可加载（或允许 SourceDefinition 为空，仅依赖 `Id/SourceName/Tags`）
3. `Instigator` 只有在可复制且在序列化时有效的情况下才会同步；否则客户端可能为空（`Instigator.IsValid()==false`）

### Q: 性能开销如何？
**A**:
- **内存**: 每个 SourceHandle 约 60 bytes
- **查询**: O(1) 复杂度（使用索引）
- **网络**: 约 40-60 bytes per SourceHandle

### Q: 可以序列化 SourceHandle 吗？
**A**: 核心信息（Id、SourceName、SourceDefinition）可以序列化。Instigator 是 `TWeakObjectPtr`，恢复时可能失效。

---

## 相关文档

- [TCS 插件架构文档](../CLAUDE.md)
- [SourceHandle 机制说明（现行实现）](./文档：SourceHandle机制设计与落地方案.md)
- [TCS 后续改进细节（Backlog）](./文档：后续改进细节.md)
- [OpenSpec: SourceHandle 变更提案](../openspec/changes/implement-source-handle/proposal.md)
- [OpenSpec: SourceHandle 网络同步 Spec](../openspec/changes/implement-source-handle/specs/source-handle-network-sync/spec.md)

---

**最后更新**: 2026-01-23
**作者**: TCS 开发团队
