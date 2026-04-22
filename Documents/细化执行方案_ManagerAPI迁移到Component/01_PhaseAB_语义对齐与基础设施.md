# Phase A & B：StateRemoval 语义对齐 + 公共基础设施

> 本文档精确到代码行号，可直接定位执行。
> 文件路径缩写见 [总览文档](./00_总览与索引.md)。

---

## Phase A：StateRemoval 语义对齐

### A-1. 统一移除原因命名

> **执行建议：作为独立 pre-PR 先合入 main**
>
> namespace 引入 + 8 处字符串替换 + 注释补全是纯机械操作，与 Phase B~G 零耦合，可被任意 revert。
> 推荐与 A-2 / A-3 合并为同一 pre-PR，PR 名建议 `TCS: unify state removal reasons`。
> 收益：缩小 Phase C/D/E 主迁移 PR 的 diff 体积，review 专注业务迁移本身。

#### 当前散落位置

移除原因以 `FName("...")` 硬编码字符串散布在以下位置：

| 字符串 | 文件 | 行号 | 所在函数 |
|--------|------|------|---------|
| `"StackDepleted"` | `State.cpp` | 296 | `SetStackCount()` |
| `"MergedOut"` | `StateMgr.cpp` | 1669 | `RemoveUnmergedStates()` |
| `"Cancelled"` | `StateMgr.cpp` | 2170 | `CancelState()` |
| `"Expired"` | `StateMgr.cpp` | 2182 | `ExpireState()` |
| `"Removed"` | `StateMgr.cpp` | 2375 | `RemoveState()` |
| `"Removed"` | `StateMgr.cpp` | 2410 | `RemoveStatesByDefId()` |
| `"Removed"` | `StateMgr.cpp` | 2445 | `RemoveAllStatesInSlot()` |
| `"Removed"` | `StateMgr.cpp` | 2471 | `RemoveAllStates()` |

#### 文档注释中的描述（不完整）

| 文件 | 行号 | 内容 | 问题 |
|------|------|------|------|
| `StateCmp.h` | 175 | `// RemovalReason: "Expired"=自然过期, "Removed"=主动移除, "Cancelled"=被取消` | 缺少 `MergedOut` 和 `StackDepleted` |
| `StateCmp.h` | 63 | 委托注释 `(移除原因: Expired=自然过期, Removed=主动移除, Cancelled=被取消)` | 同上 |

#### 执行操作

1. **在 `State.h` 中新增常量命名空间**：

```cpp
namespace TcsStateRemovalReasons
{
    static const FName Removed(TEXT("Removed"));
    static const FName Cancelled(TEXT("Cancelled"));
    static const FName Expired(TEXT("Expired"));
    static const FName MergedOut(TEXT("MergedOut"));
    static const FName StackDepleted(TEXT("StackDepleted"));
}
```

2. **替换所有硬编码字符串**（共 8 处，见上表）

3. **补全文档注释**（`StateCmp.h:63` 和 `StateCmp.h:175`），加上 `MergedOut` 和 `StackDepleted`

---

### A-2. PendingRemoval 残影清理

#### 活跃代码中的残留

| 文件 | 行号 | 类型 | 内容 | 处理 |
|------|------|------|------|------|
| `StateCmp.h` | 330 | 注释 | `// 状态实例调试输出（按实例枚举，便于定位 PendingRemoval/Duration/Tick 等字段）` | 改为：`// 状态实例调试输出（按实例枚举，便于定位 Duration/Tick 等字段）` |

**确认**: `FinalizePendingRemovalRequest`、`RemovalFlowPolicy`、`HardTimeout` 在活跃代码中**零匹配**，已彻底清理。

---

### A-3. Debug 函数 RemovalStr 占位清理

#### GetSlotDebugSnapshot

- **位置**: `StateCmp.cpp:475-687`
- **RemovalStr 声明**: `StateCmp.cpp:515` — `FString RemovalStr = TEXT("-");`
- **RemovalStr 使用**: `StateCmp.cpp:528` — 格式化输出 `Rem=%s`
- **问题**: 变量始终为 `"-"`，从未被赋有意义的值
- **处理**: 删除 `RemovalStr` 变量及其格式化输出，或替换为当前状态阶段信息

#### GetStateDebugSnapshot

- **位置**: `StateCmp.cpp:689-764`
- **RemovalStr 声明**: `StateCmp.cpp:712` — `FString RemovalStr = TEXT("-");`
- **RemovalStr 使用**: `StateCmp.cpp:730` — 格式化输出 `Rem=%s`
- **问题**: 同上
- **处理**: 同上

---

## Phase B：公共基础设施

### B-1. StateManagerSubsystem 暴露全局 ID 工厂

#### 当前状态

| 项 | 位置 | 当前实现 |
|----|------|---------|
| `GlobalStateInstanceIdMgr` 声明 | `StateMgr.h:168` | `int32 GlobalStateInstanceIdMgr = 0;` |
| 唯一使用点 | `StateMgr.cpp:459` | `TempStateInstance->Initialize(..., ++GlobalStateInstanceIdMgr, ...);` |
| 封装方法 | **不存在** | 需新增 |

#### 执行操作

在 `StateMgr.h` 中新增 public 方法（建议放在 `GlobalStateInstanceIdMgr` 声明附近）：

```cpp
public:
    /** 分配全局唯一的状态实例 ID */
    int32 AllocateStateInstanceId() { return ++GlobalStateInstanceIdMgr; }
```

修改 `StateMgr.cpp:459`，将 `++GlobalStateInstanceIdMgr` 替换为 `AllocateStateInstanceId()`。

---

### B-2. AttributeManagerSubsystem 暴露定义查询与全局 ID

#### 当前全局 ID 管理器

| 变量 | 声明位置 | 初始值 | 使用位置 |
|------|---------|--------|---------|
| `GlobalAttributeInstanceIdMgr` | `AttrMgr.h:177` | `-1` | `AttrMgr.cpp:178, 227` |
| `GlobalAttributeModifierInstanceIdMgr` | `AttrMgr.h:276` | `-1` | `AttrMgr.cpp:780, 871` |
| `GlobalAttributeModifierChangeBatchIdMgr` | `AttrMgr.h:280` | `-1` | `AttrMgr.cpp:891, 1056, 1135` |

**全部无封装方法**，需新增。

#### 当前定义缓存

| 数据 | 声明位置 | 类型 |
|------|---------|------|
| `AttributeDefinitions` | `AttrMgr.h:36` | `TMap<FName, const UTcsAttributeDefinitionAsset*>` |
| `AttributeModifierDefinitions` | `AttrMgr.h:39` | `TMap<FName, const UTcsAttributeModifierDefinitionAsset*>` |
| `AttributeTagToName` | `AttrMgr.h:42` | `TMap<FGameplayTag, FName>` |
| `AttributeNameToTag` | `AttrMgr.h:45` | `TMap<FName, FGameplayTag>` |

**当前无 public 的定义查询方法**（内部直接 `Find`），需新增。

#### 执行操作

在 `AttrMgr.h` 中新增 public 方法：

```cpp
public:
    /** 获取属性定义资产 */
    const UTcsAttributeDefinitionAsset* GetAttributeDefinitionAsset(FName AttributeName) const;

    /** 获取修改器定义资产 */
    const UTcsAttributeModifierDefinitionAsset* GetModifierDefinitionAsset(FName ModifierId) const;

    /** 分配全局唯一的属性实例 ID */
    int32 AllocateAttributeInstanceId() { return ++GlobalAttributeInstanceIdMgr; }

    /** 分配全局唯一的修改器实例 ID */
    int32 AllocateModifierInstanceId() { return ++GlobalAttributeModifierInstanceIdMgr; }

    /** 分配全局唯一的修改器变更批次 ID */
    int64 AllocateModifierChangeBatchId() { return ++GlobalAttributeModifierChangeBatchIdMgr; }
```

在 `AttrMgr.cpp` 中实现定义查询：

```cpp
const UTcsAttributeDefinitionAsset* UTcsAttributeManagerSubsystem::GetAttributeDefinitionAsset(FName AttributeName) const
{
    if (const auto* Found = AttributeDefinitions.Find(AttributeName))
    {
        return *Found;
    }
    return nullptr;
}

const UTcsAttributeModifierDefinitionAsset* UTcsAttributeManagerSubsystem::GetModifierDefinitionAsset(FName ModifierId) const
{
    if (const auto* Found = AttributeModifierDefinitions.Find(ModifierId))
    {
        return *Found;
    }
    return nullptr;
}
```

---

### B-3. Component 增加 lazy resolve helper

#### TcsStateComponent 当前状态

| 项 | 位置 | 现状 |
|----|------|------|
| `StateMgr` 成员 | `StateCmp.h:292` | `TObjectPtr<UTcsStateManagerSubsystem> StateMgr;` — **已存在** |
| BeginPlay 初始化 | `StateCmp.cpp:34` | `StateMgr = World->GetGameInstance()->GetSubsystem<UTcsStateManagerSubsystem>();` |
| `ResolveStateManager` | **不存在** | 需新增 |
| `AttrMgr` 成员 | **不存在** | 需新增 |
| `ResolveAttributeManager` | **不存在** | 需新增 |

**StateMgr 的使用点**（`StateCmp.cpp` 中通过 `StateMgr->` 调用）：
- 行 43: `StateMgr->InitStateSlotMappings(GetOwner())`
- 行 201: `StateMgr->...`（DurationTracker 相关）
- 行 260: `StateMgr->ExpireState(ExpiredState)`
- 行 483, 485: Debug 相关
- 行 805, 807: `StateMgr->RequestUpdateStateSlotActivation(this, SlotTag)`
- 行 860, 862: `StateMgr->RefreshSlotsForStateChange(...)`

#### 执行操作 — TcsStateComponent

在 `StateCmp.h` 中新增：

```cpp
protected:
    /** 缓存的 AttributeManager 指针（迁移期需要） */
    UPROPERTY()
    TObjectPtr<UTcsAttributeManagerSubsystem> AttrMgr;

    /** 懒加载获取 StateManager，BeginPlay 预热但业务方法中也可自行补拉取 */
    UTcsStateManagerSubsystem* ResolveStateManager();

    /** 懒加载获取 AttributeManager */
    UTcsAttributeManagerSubsystem* ResolveAttributeManager();
```

在 `StateCmp.cpp` 中实现（**采用 BeginPlay 预热 + ensureMsgf 诊断保护策略**）：

> **设计决策**：`GameInstanceSubsystem::Initialize()` 保证在所有 Actor::BeginPlay 之前完成，
> 所以正常流程中 BeginPlay 获取 Subsystem 必然成功。Resolve helper 的价值在于**诊断**而非
> "运行时自愈"——如果 BeginPlay 时获取失败，运行时再试也不可能成功。

```cpp
UTcsStateManagerSubsystem* UTcsStateComponent::ResolveStateManager()
{
    if (!StateMgr)
    {
        if (UWorld* World = GetWorld())
        {
            if (UGameInstance* GI = World->GetGameInstance())
            {
                StateMgr = GI->GetSubsystem<UTcsStateManagerSubsystem>();
            }
        }
        ensureMsgf(StateMgr, TEXT("[%s] Failed to resolve StateManagerSubsystem for %s"),
            *FString(__FUNCTION__), *GetPathName());
    }
    return StateMgr;
}

UTcsAttributeManagerSubsystem* UTcsStateComponent::ResolveAttributeManager()
{
    if (!AttrMgr)
    {
        if (UWorld* World = GetWorld())
        {
            if (UGameInstance* GI = World->GetGameInstance())
            {
                AttrMgr = GI->GetSubsystem<UTcsAttributeManagerSubsystem>();
            }
        }
        ensureMsgf(AttrMgr, TEXT("[%s] Failed to resolve AttributeManagerSubsystem for %s"),
            *FString(__FUNCTION__), *GetPathName());
    }
    return AttrMgr;
}
```

#### TcsAttributeComponent 当前状态

| 项 | 位置 | 现状 |
|----|------|------|
| Manager 成员 | **不存在** | 无任何 Manager 缓存 |
| BeginPlay | `AttrCmp.cpp:13-16` | 仅调用 `Super::BeginPlay()`，无初始化逻辑 |
| 数据成员 | `AttrCmp.h:107-155` | `Attributes`、`AttributeModifiers`、`SourceHandleIdToModifierInstIds`、`ModifierInstIdToIndex` 等**已在 Component 上** |

#### 执行操作 — TcsAttributeComponent

在 `AttrCmp.h` 中新增：

```cpp
protected:
    /** 缓存的 AttributeManager 指针 */
    UPROPERTY()
    TObjectPtr<UTcsAttributeManagerSubsystem> AttrMgr;

    /** 懒加载获取 AttributeManager */
    UTcsAttributeManagerSubsystem* ResolveAttributeManager();
```

在 `AttrCmp.cpp` 的 `BeginPlay` 中增加预热（**采用 BeginPlay 预热 + ensureMsgf 诊断保护策略**）：

```cpp
void UTcsAttributeComponent::BeginPlay()
{
    Super::BeginPlay();

    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            AttrMgr = GI->GetSubsystem<UTcsAttributeManagerSubsystem>();
        }
    }
    ensureMsgf(AttrMgr, TEXT("[%s] Failed to resolve AttributeManagerSubsystem for %s"),
        *FString(__FUNCTION__), *GetPathName());
}
```

实现 `ResolveAttributeManager`（与 StateComponent 同理，包含 `ensureMsgf` 诊断）：

```cpp
UTcsAttributeManagerSubsystem* UTcsAttributeComponent::ResolveAttributeManager()
{
    if (!AttrMgr)
    {
        if (UWorld* World = GetWorld())
        {
            if (UGameInstance* GI = World->GetGameInstance())
            {
                AttrMgr = GI->GetSubsystem<UTcsAttributeManagerSubsystem>();
            }
        }
        ensureMsgf(AttrMgr, TEXT("[%s] Failed to resolve AttributeManagerSubsystem for %s"),
            *FString(__FUNCTION__), *GetPathName());
    }
    return AttrMgr;
}
```

新增 include：

```cpp
#include "Attribute/TcsAttributeManagerSubsystem.h"
```

---

### B-4. 预热自测断言（Debug/Development）

B-3 两个 Component 的预热代码落地后，立即在各自 `BeginPlay` 末尾补一条 **`checkf`** 断言验证预热链路：

```cpp
// StateCmp.cpp: UTcsStateComponent::BeginPlay尾部
#if !UE_BUILD_SHIPPING
    checkf(StateMgr, TEXT("StateMgr resolve failed in BeginPlay for %s; "
                           "GameInstanceSubsystem lifecycle broken."), *GetPathName());
#endif
```

```cpp
// AttrCmp.cpp: UTcsAttributeComponent::BeginPlay尾部
#if !UE_BUILD_SHIPPING
    checkf(AttrMgr, TEXT("AttrMgr resolve failed in BeginPlay for %s; "
                          "GameInstanceSubsystem lifecycle broken."), *GetPathName());
#endif
```

#### 为什么在 Phase B 就加断言

- Phase B 改动面极小，此时 `check` 失败的唯一来源就是预热逻辑本身有误或 `Subsystem` 生命周期破坏 —— **定位窗口最小**。
- 一旦 Phase C/D/E 业务代码涨入，再发现 `StateMgr == nullptr` 路径时，需排查的嫌疑代码规模会放大一个数量级。
- `checkf` 仅 Debug/Development/Test 生效，Shipping 零开销。

#### 为什么用 `checkf` 而不是 `ensureMsgf`

| 对比项 | `checkf`（本步） | `ensureMsgf`（运行时 Resolve） |
|--------|-------------------|----------------------------|
| 用途 | 诊断环境或配置错误，失败即代表不可恢复冲突 | 记录单次异常，继续运行 |
| 行为 | 直接 fatal error，停机 | 弹窗 + 日志 + 恢复运行 |
| 适用时机 | Phase B 刚落地时的初始发现 | Phase C/D/E 业务运行中的守护性诊断 |

B-4 的断言意图是“如果预热失败就立即暴露”；运行时的 `ensureMsgf` 意图是“记录异常但不停机”，两者定位不同、可共存。

#### 验证方式

- 运行 Editor（Development 配置），加载任一含 `StateComponent` / `AttributeComponent` 的关卡，确认无 `check` 触发
- 单元测试必须在 `FAutomationTestBase::BeforeEach` 中先创建 GameInstance，后挂 Component，否则会触发 `check`（这也正是期望行为）

