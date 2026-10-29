# Change: 实体组件「定义未就绪」不再永久失效——订阅就绪单出口后补注册

## Why

`UTcsCombatEntityComponent::BeginPlay` 在定义库未就绪时**直接 `return`**，此后该组件的 `EntityHandle` 永远是 `Id == 0`，**没有任何补救路径**：

```cpp
// TcsCombatEntityComponent.cpp:64-71（现状）
if (!DefinitionSubsystem->IsRuntimeReady())
{
    UE_LOG(..., Warning, TEXT("...定义库未就绪（%s）——跳过注册（时序是宿主责任）"), *Owner->GetName());
    return;        // ← 到此为止，再无后续
}
```

而 `UTcsDefinitionSubsystem` **已经提供了**单出口 `OnDefinitionsReady`（`R-8` 补的），**却零订阅者**（本轮实测 `grep` 全仓 + 宿主 `Source/`：零命中）——即 `R-8` 只补了「通知」，没接「用通知补注册」的那一端。

**用户的裁定（2026-10-26）**：`ARCH-11` `U-1` 走**丙**——框架**不建五态实体状态机**，只补「就绪编排」。判据（本轮实测）：

| 旧设计的五态 | 今天的实际情况 |
|---|---|
| `Unregistered→Registered` | ✅ 已有（`RegisterEntity`） |
| `Loading` | ❌ **无内容**——定义加载是 GameInstance 级，不是实体级 |
| `Ready` | ✅ 已有替身（`IsRuntimeReady` + 宿主实现的 `IsEntityReady`） |
| `TornDown` | ✅ 已有（`EndPlay` 四段级联） |
| `Failed` | ❌ **无消费者**（失败清单查 `GetFailureList()` 即可，不必每实体各存一份） |

⇒ **五态里只有一态缺、而且是「注册时机对」这件事，不是状态机。**

## What Changes

### 一、把注册逻辑抽成可重入的私有方法

`BeginPlay` 里内联的注册逻辑抽为 `RegisterAsCombatEntity()`，可被**多次调用**（`OnDefinitionsReady` 可能因多世界各触发一次）。

### 二、`BeginPlay`：未就绪时**订阅**而非放弃

```
BeginPlay():
  if (IsRuntimeReady())  → RegisterAsCombatEntity()
  else:
    订阅 OnDefinitionsReady
    订阅后**二次复查** IsRuntimeReady()   ← 填补「查-订」之间的竞态窗口
    if (已就绪) → RegisterAsCombatEntity() 并退订
```

### 三、回调后**立即退订**（单次信号）

`OnDefinitionsReady` 是**单次**信号（`Initialize` 内置位后广播恰好一次），不是每帧事件 ⇒ 回调里 MUST 先退订再做事，避免多世界场景下重复触发。

**这与组件头注释已有的口径一致**（`TcsDefinitionSubsystem.h:196-201`：「未就绪则订阅本委托、在回调里补做，**并在回调后立即退订**」）——本变更只是把这条**从未被任何订阅者执行过的说明**兑现。

### 四、`EndPlay`：补退订（防御性）

回调前组件就被销毁时，订阅仍在 ⇒ `EndPlay` MUST 退订。**顺序 = 退订先于注销级联**（否则注销过程中若触发广播会重入）。

### 五、`OnUnregister`：补齐「反订阅 + 注销级联 + 句柄重置」三件事

**现状实测**：组件**没有** `OnUnregister` 覆写。

这条是本轮核对**新发现**的漏洞（防哨兵说谎）：`RegisterAsCombatEntity` 用 `EntityHandle.IsValid()` 当幂等哨兵。若组件走「`OnUnregister`（移除但 Actor 未销毁）→ 再 `RegisterComponent`」这条路，而 `OnUnregister` 里**不注销也不重置句柄**，则：

| 时刻 | `EntityHandle` | 属性账本 |
|---|---|---|
| `OnUnregister` 后 | `Id = 1`（仍有效） | 已删 `Id=1` |
| 重新注册后再 `BeginPlay` | `Id = 1` | 空 |

⇒ `IsValid()` 会说「已注册」而**跳过补注册** ⇒ **哨兵与事实脱钩**。

⇒ `OnUnregister` MUST 做三件事：**退订 → 四段注销级联 → `EntityHandle` 置回默认构造**。

**为什么 `EntityHandle` 不会「失效后自动变有效」**（防误读，MUST 记）：`FTcsCombatEntityHandleRegistry::Allocate()` 的 `Id` 从 1 起**原子递增、永不复用**（`TcsCombatEntityHandle.h:67-72`），`Id == 0` **只表示「从未调过 `RegisterEntity`」**。让句柄有效的唯一手段就是调 `RegisterEntity`。

## Impact

- Affected specs: `integration-entity`（战斗实体组件需求补「未就绪不是永久失效」）
- Affected code: `TcsIntegration/Public/Entity/TcsCombatEntityComponent.h`（+ 私有方法 / 委托句柄 / `OnUnregister` 覆写）、`TcsIntegration/Private/Entity/TcsCombatEntityComponent.cpp`
- 零新依赖边（组件已依赖 `TcsIntegration` 内部的 `UTcsDefinitionSubsystem`）
- **BREAKING**：无
- 关联但**不实施**：`refactor-asset-load-policy-as-contract`（批次 B）——它让「未就绪」变成**正常且必然经历**的中间态（异步预载期间），故本变更 MUST 先落

## 已裁的三条（2026-10-26，用户逐条拍板）

| # | 疑点 | 裁定 |
|---|---|---|
| **D-1** | 「回调后立即退订」与「`EndPlay` 退订」是一套还是两套 | **两者都要**，共用同一份 `FDelegateHandle` 判据（回调路径退订正常路；未收回调即销毁走 `EndPlay` 防御路） |
| **D-2** | `OnUnregister` 的四段级联是否与 `EndPlay` 共用实现 | **MUST 共用**——把「两者都必须执行的逻辑」提炼成一个私有函数，`EndPlay` 与 `OnUnregister` 都调它。理由：**宿主项目极可能用对象池**，那时两条路径都会被真实走到 |
| **D-3** | 组件是否真会走「`OnUnregister` → 再注册」 | **会**（同样因为对象池）。但**再注册 MUST 发一个新的 `EntityHandle`**，MUST NOT 复用旧句柄——用户明确表示复用「存有很大疑虑」，我按「发新号」写 |

---

## 🔴 一个必须由裁 D-3 派生的连带问题（本轮核对发现，**未裁**）

裁定「再注册发新句柄」推出一个**必须同时回答**的问题：

> **旧的 `EntityHandle` 已经流到哪些地方去了？发新号之后，那些地方怎么办？**

实测「跨时持有实体句柄」的位置（`grep` 取证）：

| 持有者 | 位置 | 语义 |
|---|---|---|
| `FTcsStateInstance::Entity` | `TcsState/Public/State/TcsStateInstance.h:78` | 状态实例所属实体（**基底字段，Buff 与 Skill 共用**） |
| `FTcsLearnedSkillEntry::Entity` | `TcsSkill/Public/Skill/TcsSkillRegistry.h:65` | 技能账本条目的实体键 |
| `FTcsAttributeChangedEvent::Entity` | `TcsAttribute/Public/Attribute/TcsAttributeChangedEvent.h:51` | **事件载荷**（会进事件总线，可能跨帧存活） |
| `FTcsParamValueSource::Subject` / `Instigator` | `TcsCore/Public/Parameter/TcsParamValueSource.h:62,66` | 参数求值上下文（链运行态可能持有） |
| `FTcsEffectContext::Caster` / `Instigator` / `Targets` | `TcsEffect/Public/Chain/TcsEffectContext.h:32,35,67` | 链运行态上下文 |

**⇒ 只要「注销级联」被执行（它会把上面这些按旧句柄清干净），旧句柄就变成「查不到的悬空值」。**

而本仓的判据是：**实体句柄无代际段、永不复用**（`TcsCombatEntityHandle.h:15`）⇒ 悬空表现为「**查不到**」而非「**误命中别人的槽**」。这正是「发新号」安全的前提。

**但有一处 MUST 确认**：**`FTcsAttributeChangedEvent` 是已发出的事件**。若某个订阅者**跨帧**保留了这个事件、事后再按 `Entity` 查账本，则会拿到「查不到」——这是**正确的**（实体确实已不存在），但订阅者 MUST 能处理它。

### 由此派生的选项（需你裁）

| 选项 | 做法 | 代价 |
|---|---|---|
| **甲** | 发新号 + **不**处理旧句柄（依赖「永不复用 ⇒ 悬空即查不到」这条既有保证） | 零改动；但订阅者若假设「事件里的 Entity 一定可查」会静默拿到空 |
| **乙** | 发新号 + 在组件里**记录旧句柄历史**（`TArray<FTcsCombatEntityHandle> RetiredHandles`）供诊断 | 多一个容器；但排障时能回答「这个旧句柄是谁的」 |
| **丙** | **不改发号**，改为「再注册时若句柄已有效则复用」 | 与你的裁定相反，**不采纳**（列出以备对照） |

**我倾向甲**：本仓的「永不复用」设计**就是为这种情况准备的**——它把「悬空句柄」的失败形态从「误命中」降级为「查不到」，而「查不到」是可检测的。乙的诊断价值在有了对象池之后才会显现，届时再加不迟（零消费者不预建）。

