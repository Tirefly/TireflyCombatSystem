# ARCH-04-attribute · TcsAttribute（属性聚合层）

- **文档 ID**：`ARCH-04-attribute`
- **类型**：模块档案（现状 + 债务 + 未实现）
- **状态**：生效中
- **权威范围**：`TcsAttribute` **今天实际落地了什么**。本模块是**层 1 的唯一模块**——全部 10 个其它业务模块（除三个层 0 模块）**都依赖它**。它的对外契约一旦漂移，全仓同时受影响。
- **最后更新**：2026-10-17（首次审核）

---

## §1 模块定位与边界

**一句话**：给定「属性定义表」与「每实体的修正器条目集」，**惰性**算出当前值、并在值确实变化时广播一条事件——**不认识时间、不认识 buff、不认识任何具体属性**。

| 项 | 内容 |
|---|---|
| 代码规模 | **3 034 行**（15 个 `.h` + 9 个 `.cpp` + 1 个 `.Build.cs`）；扣掉 `Build.cs` 22 行 ⇒ 代码 **3 012 行** |
| 编译期依赖 | `Core` / `CoreUObject` / `Engine` / `GameplayTags` + **`TcsCore`** + **`TcsNotation`**（`TcsAttribute.Build.cs:12-20`，恰好六项） |
| 被谁依赖 | `TcsState` / `TcsEffect` / `TcsBuff` / `TcsSkill` / `TcsDamage` / `TcsIntegration` —— **6 个**（全仓最多的一个，与 `TcsCore` 并列） |
| **不依赖谁** | **`TcsAssetButler`**（见 §5 D-2 / §6 U-6）——这条缺失是本模块最贵的一笔账 |
| 对应可执行需求 | **4 个能力 / 27 需求 / 86 场景 / 752 行**：`attribute-types`（311 行 / 11 需求）、`attribute-pipeline`（232 / 8）、`attribute-store`（136 / 4）、`attribute-transaction`（73 / 4） |
| 旧文档族 | `SPEC-01-attributes`（`spec/02-module-attributes.md`，129 行，状态 `ACTIVE`）+ 决策 `dec-03-attribute-set` |
| 落地日期 | 2026-09-02 起分轮：词表/账本（R3）→ 管线+事务（2026-09-18）→ tag 化（2026-09-22）→ 反射面（2026-09-24）→ 解析点（2026-10-05）→ 基值读口（2026-10-11） |

### 1.1 文件组与职责【实测】

| 组 | 文件 | 行 | 职责 |
|---|---|---|---|
| 模块壳 | `TcsAttributeModule.h/.cpp` | 20 | 空体模块类 |
| 日志 | `TcsAttributeLogChannel.h/.cpp` | 15 | `LogTcsAttribute`（D0-6 v2：不 include `Module.h`） |
| **折叠核** | `Attribute/TcsAttributeBandFold.h` | **170** | `FoldTcsAttributeBands` + `IsStrongerTcsOverride` + `GetTcsOverrideTieScore`——**全仓唯一一份带式算术** |
| 值域词汇 | `Attribute/TcsAttributeBounds.h` | 122 | `ETcsAttributeBoundMode`（三态）/ `FTcsAttributeBound` / `FTcsAttributeBounds` / `ETcsAttributeValueDomain` |
| 修正器词汇 | `Attribute/TcsAttrModInstance.h` | 240 | `ETcsAttributeOp`（五带）/ `GetTcsAttributeBandWeight` / `ETcsAttrOverrideTieBreak` / `ETcsOperandKind` / 操作数双形状 / `FTcsAttrModInstance` |
| 属性定义 | `Attribute/TcsAttributeDef.h` + `.cpp` | 242 | `FTcsAttributeDefData`（字段唯一声明处）/ `FTcsAttributeDefTableRow` / `UTcsAttributeDef` |
| 修正器模板 | `Attribute/TcsAttrModDef.h` + `.cpp` | 289 | `FTcsAttrModDefTableRow` / `UTcsAttrModDef` / **`MakeFromDef` 的 inline 实现**（住在文件末，见 §2.2） |
| 账本 | `Attribute/TcsAttributeInstance.h` + `TcsAttributeStore.h` | 121 | 纯 C++ struct，**零反射** |
| **聚合管线** | `Attribute/TcsAttributePipeline.h` + 3 个 `.cpp` | **799** | 重算 / 收口 / 依赖 / 级联 / 事务（本模块最大的单块） |
| **门面** | `TcsAttributeSubsystem.h` + `.cpp` | **703** | World 子系统，全仓唯一入口 |
| 事件 | `Attribute/TcsAttributeChangedEvent.h` + `.cpp` | 76 | 原生 tag + 反射载荷 |
| 读侧契约 | `Attribute/TcsAttributeProvider.h` | 65 | `ITcsAttributeProvider`（3 个 `BlueprintNativeEvent`） |
| 参数源 | `Attribute/TcsParamSource_AttributeScaled.h` | 122 | `FTcsAttributeEvaluateContext` + `FTcsParamSource_AttributeScaled` |
| 比较词 | `Attribute/TcsAttributeComparison.h` | 28 | `ETcsAttributeComparison`（四向，无 Equal/NotEqual） |

### 1.2 本模块最反常的一点：**规格质量是全仓最好的一档，但有三处"规格更严、代码更松"**

先说好的一面：**27 需求 / 86 场景 / 752 行**，逐字段列形状、逐分支给场景，覆盖度明显高于 `primary-asset-loading`（6 需求）与 `state-def-asset`。四份规格之间还做了**主动分工**（`attribute-types` 管词、`attribute-store` 管宿主、`attribute-pipeline` 管行为、`attribute-transaction` 管事务），没有出现 `ARCH-03` 那种"规格与真实价值完全错位"。

但有三处**规格写了 MUST、代码没做**，且方向一致（**规格更严、代码更松**）：

| # | 规格要求 | 代码实况 | 严重度 |
|---|---|---|---|
| **D-1** | `attribute-transaction` 需求「唯一提交点与失败零写入」：提交失败 MUST **零写入**，不得"改了一半" | **无候选集、无回滚**。修正器在 `ApplyModifier` 当场进账本，批只推迟重算与广播 | ★★★ |
| **D-4** | `attribute-pipeline:106`「图 MUST **每轮重建不缓存**」 | 图**持久累加**，只增不减（`Store.Dependents` 从不清） | ★★ |
| **D-7** | `attribute-pipeline:199`「跨模块消费者 MUST NOT 自行 `ResolveStore` + `FindInstance`」 | `TcsEffectAttributeAccess::HasAttribute` **逐字如此** | ★★ |

**⇒ 这三条不是"文档没跟上代码"，而是"代码没跟上文档"。** 与 `ARCH-03` 的 D-1/D-4（规格零收录唯一在干活的入口）方向相反——那里是**文档欠账**，这里是**实现欠账**。

---
## §2 已落地

### 2.1 折叠核：全仓唯一一份带式算术【实测】

```
Source/TcsAttribute/Public/Attribute/TcsAttributeBandFold.h:115
inline double FoldTcsAttributeBands(double BaseValue, TConstArrayView<FTcsAttributeBandEntry> Entries,
    ETcsAttrOverrideTieBreak OverrideTieBreak = ETcsAttrOverrideTieBreak::OTB_Max)
```

**三个真实消费者**（`D5-5 v3` 的兑现，实测 grep）：

| 消费者 | 位置 | 传参 |
|---|---|---|
| M2 属性聚合 | `TcsAttributePipeline.cpp:251` | `(Instance.BaseValue, Entries, Instance.OverrideTieBreak)` |
| M5 参数链 | `TcsSkill/Private/Skill/TcsParamChain_Evaluate.cpp:98` | `(BaseValue, Entries)`——**两参**，靠默认值 |
| TcsDamage 流程属性 | `TcsDamage/Private/Flow/TcsFlowAttributes.cpp:45` | `(0.0, Entries)`——**两参**，初值恒 0 |

`TcsSkill` 侧还有一条**自我约束**写进了公开头（`TcsParamChain.h:166`）：「`Source/TcsSkill/` 内 `FoldTcsAttributeBands` 调用**仅此一处**」——即"单份"不只靠自觉，还有模块内的 grep 级纪律。**这是全仓最好的一处防重复设计。**

**顺序无关的三级全序**（`TcsAttributeBandFold.h:71-89`）是这条的核心价值：

```
优先级（大者胜）→ 同优先级策略得分（大者胜）→ 有符号值（大者胜）
```

第三级不是冗余——它补的是"策略下打平"（`OTB_MaxAbs` 下 +5 与 −5 得分相同）。没有第三级，赢家会取决于**遍历顺序**。注释明写「比较一律用**精确关系**（`!=` / `>`）而非近似相等：模糊相等不满足传递性，会破坏顺序无关」——这条比多数引擎实现更严。

### 2.2 定义的双轨与"字段只声明一次"【实测】

三条纪律同时成立，且都有代码锚点：

1. **字段形状单份**：`FTcsAttributeDefData`（`TcsAttributeDef.h:31`）是唯一声明处；`FTcsAttributeDefTableRow::Def`（`:119`）与 `UTcsAttributeDef::Def`（`:178`）都只是**组合持有**。
2. **身份分工而非双真相**：`DefTag`（`FGameplayTag`）= 内容身份；`RowName`（`FName`）= 编辑期定位。头注释明写「二者 MUST NOT 被要求同名」。
3. **主资产身份名取 tag**：`GetPrimaryAssetId()` 返回 `[PrimaryAssetType, DefTag.GetTagName()]`（`TcsAttributeDef.cpp:16`）——资产文件可自由改名/挪目录。

`UTcsAttrModDef` 同款（`TcsAttrModDef.h:110`，`TcsAttrModDef.cpp:16`）。

**一处值得单独记的工程手法**：`FTcsAttrModInstance::MakeFromDef` 的**声明**住 `TcsAttrModInstance.h:230`、**实现**住 `TcsAttrModDef.h:177`（文件末 `inline`）。原因是形参 `FTcsAttrModDefTableRow` 只在后者完备，而后者 include 前者——反向 include 会成环。注释把这个约束写在两个文件的**两头**（`:222` 与 `:173`），避免下一次有人"顺手"把它挪回声明处。

### 2.3 聚合管线：惰性重算 + 单点写入【实测】

管线是 `CachedCurrent` 的**唯一生产者**（`TcsAttributePipeline.cpp:201` 是全仓唯一写该字段的地方）。四个入口的行为差异是刻意的：

| 入口 | 批外行为 | 批内行为 |
|---|---|---|
| `EvaluateCurrent` | 脏则重算并写回 | **不重算**，返回旧缓存（事务期读旧值，D2-5） |
| `PeekPending` | 等同 `EvaluateCurrent` | 只算不写（`ComputeCandidate`，不标脏、不广播） |
| `ApplyModifier` | 立即重算 + 广播（**隐式批**） | 只标脏 |
| `SetBaseValue` | 立即重算 + 广播 | 只标脏 |

**"新建即脏"这条设计是对的，且理由写得很实**（`TcsAttributePipeline.h` 对应的 spec 场景 + `TcsAttributeSubsystem.cpp:171` 注释）：若在 `AddAttribute` 内立即结算，动态边界会读到**尚未添加**的引用属性（求值 0）→ 把原值钳成 0 且无人再标脏（**静默错值**）。代价是首次结算可能广播一次，被明文接受。

### 2.4 值域收口：三态边界 + 三态模式【实测】

`ApplyValueDomain`（`TcsAttributePipeline.cpp:256-310`）三个分支：

- `AVD_Clamp`（默认）：两侧各自判，越界钳到边界；
- `AVD_Wrap`：**两侧齐备且 `Max > Min` 才回卷**，否则**原样返回**（`:273`）——"只给一侧没有跨度可言，框架不替作者猜一个 0"；
- `AVD_Custom`：`ensureMsgf(false, …)` + `Verbose` 日志 + `[[fallthrough]]` 到 Clamp（`:288-298`）——**确定性优先于未实现策略**。

`ABM_Dynamic` 的边界走 `ResolveBound`（`:351`），**先按管线求值目标属性**并**登记依赖边**（`RegisterDependency`，`:370`）——"HP ≤ MaxHP"因此是自维护的。

### 2.5 依赖图：Tarjan SCC + 求值栈兜底【实测】

两层防护，职责分离得很干净：

| 层 | 位置 | 作用 |
|---|---|---|
| **登记期拒绝** | `RegisterDependency`（`_Dependency.cpp:95`） | 加边后跑 `HasDependencyCycle`（Tarjan），成环则**把刚加的那条边删掉** + `ensureMsgf` |
| **求值期兜底** | `PushEvalStack`（`TcsAttributePipeline.cpp:331`） | 同一属性在一次求值链上重复出现 → `ensureMsgf` + 返回 false（用缓存值终止递归） |

**轮数上限与环判定解耦**（`MaxFlushPasses = 64`，`TcsAttributePipeline.cpp:24`）——注释明写「MUST NOT 作为环判据」，修的是旧 TCS "8 轮上限把深链误判为环"的缺陷。这条在 `attribute-pipeline:105` 有对应需求。

### 2.6 冻结暂存区：整条实例进出【实测】

`RemoveAttribute` = **冻结**（`TcsAttributeSubsystem.cpp:210`，`MoveTemp` 整条进 `FrozenAttributes`）；`AddAttribute` = **解冻优先**（`:133-144`，命中则整条搬回并跳过定义解析）。

**与来源撤销的共存是真落地的**（`_Cascade.cpp:46-58`）：`RemoveBySource` 扫两个容器，命中冻结区时**同样标脏**——注释写得很准：「否则会把"已被撤销来源的旧缓存值"带回（**静默多数值**）」。

### 2.7 反射面：有边界、有理由【实测】

| 有 `UFUNCTION()` | 无 `UFUNCTION` |
|---|---|
| `RegisterEntity` / `UnregisterEntity` / `AddAttribute` / `SetBaseValue` / `EvaluateCurrent` / `PeekPending`（`TcsAttributeSubsystem.h`） | `ApplyModifier` / `RemoveBySource` / `TryGetBaseValue` / `ResolveStore` / `GetStore` |

**无反射的两个理由都成立**：
- `ApplyModifier` / `RemoveBySource`：形参 `FTcsAttrModInstance` 是纯 C++ struct（无 `USTRUCT`）——补 `UFUNCTION` **不足以**打通，UHT 会拒该形参（台账 `SCRIPT-9` 已逐条核实）；
- `TryGetBaseValue`：形参含**未反射的句柄 + 出参引用**，且它是"框架内部消费面"而非脚本面（`TcsAttributeSubsystem.h:252` 明写）。

**`UFUNCTION()` 无 specifier 是有意的**（`:86-89`）：形参/返回全反射，但同批统一口径，避免蓝图参数校验带来的隐式承诺面。这条在 `TcsAttributeSubsystem.h` 里写了三遍（每个反射方法一处），是**过度重复但方向正确**的注释。

### 2.8 解析点与基值读口：两处"为了唯一实现而存在"的接口【实测】

**`UTcsAttributeSubsystem::Resolve`**（`TcsAttributeSubsystem.cpp:36-40`）——全仓唯一门面查找点。两个消费模块的白名单薄壳都走它：

- `FTcsBuffAttributeAccess::Resolve`（`TcsBuff/Private/State/TcsBuffOps_Modifier.cpp:23`）
- `FTcsEffectAttributeAccess::Resolve`（`TcsEffect/Private/Attribute/TcsEffectAttributeAccess.cpp:15`）

**`TryGetBaseValue`**（`TcsAttributeSubsystem.cpp:225-250`）——`TryGet` 双答案形态的理由是**实测过的一条判据**：`FTcsAttributeStore::GetBaseValue` 对"未持有"返回 `0.0`，与"确实持有且基值为 0"**不可区分**。而技能侧 `Cost` 恰恰必须区分这两件事（"属性没加/配错" vs "付不起"是两种红字）。

**这条设计的真实价值**：它把 `TcsSkill` 从"自行 `ResolveStore` + `FindInstance`"挡了回来（`attribute-pipeline:199-202`）。今天 `TcsCostOps.cpp:274/376` 两处调用都是这个口。

### 2.9 事件：结果事件而非操作事件【实测】

`Tag_TcsEvent_Attribute_ValueChanged` 由本模块**原生声明**（`TcsAttributeChangedEvent.cpp:9`，`UE_DEFINE_GAMEPLAY_TAG_COMMENT`），带 `TCSATTRIBUTE_API` 导出宏（`:26`）——注释解释了为什么必须带：`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**，宿主引用会 `LNK2001`（实测先例）。

**语义边界写得很清楚**（`TcsAttributeChangedEvent.h:35-38`）：它**不承担**"某次写操作发生过"的记账，也**不承担**"基础值变更历史"——基值改了但被值域收口吃掉 = 对外没变 = **不广播**，这是故意的。

### 2.10 `ETcsAttributeComparison`：一个"词住在生产者侧"的正确样本【实测】

`TcsAttributeComparison.h:22` 四值，**无 `Equal` / `NotEqual`**——理由写在注释里：「等值语义由"大于等于 且 小于等于"组合表达，故本枚举**不立** `Equal` / `NotEqual` 档（零消费者不预建）」。

**唯一消费者**是 `TcsEffect` 的触发条件（`TcsTriggerCondition.cpp:295-304` 四分支），且它**没有**在消费侧另立同义枚举——`TcsTriggerCondition.h:396` 明写「比较词住 `TcsAttribute`」。**这是全仓"词汇归口"做得最正的一处**：跨模块的公共词汇住在最底层模块，消费者只取词不造词。

---
## §3 对外契约

### 3.1 稳定契约（其它模块可以依赖）

| 契约 | 锚点 | 谁在用【实测】 |
|---|---|---|
| `FoldTcsAttributeBands` / `FTcsAttributeBandEntry` | `TcsAttributeBandFold.h:115` / `:15` | `TcsSkill`（参数链）+ `TcsDamage`（流程属性）——**3 处调用** |
| `FTcsAttrModInstance` + `MakeFromDef` | `TcsAttrModInstance.h:188` / `TcsAttrModDef.h:177` | `TcsBuff` 物化器（`TcsBuffModifierMaterializer.cpp:108`）、`TcsEffect` 链步骤、`TcsDamage` 流程步 |
| `ETcsAttributeOp`（五带） | `TcsAttrModInstance.h:30` | `TcsSkill` / `TcsDamage` / `TcsEffect` |
| `UTcsAttributeSubsystem`（门面） | `TcsAttributeSubsystem.h:32` | **6 个模块** + 宿主 `TcsDev`（12 处直取） |
| `UTcsAttributeSubsystem::Resolve` | `TcsAttributeSubsystem.cpp:36` | `TcsBuff` / `TcsEffect` 两个白名单薄壳 |
| `TryGetBaseValue` | `TcsAttributeSubsystem.cpp:225` | `TcsSkill` `Cost`（2 处）+ `TcsDev` 装置 |
| `ETcsAttributeComparison` | `TcsAttributeComparison.h:22` | `TcsEffect` 触发条件（唯一消费者） |
| `FTcsAttributeChangedEvent` + tag | `TcsAttributeChangedEvent.h:41` / `:26` | 总线订阅方（宿主为主） |
| `UTcsAttributeDef` / `UTcsAttrModDef` | `TcsAttributeDef.h:139` / `TcsAttrModDef.h:110` | `TcsIntegration` 定义库、物化器 |

### 3.2 MUST NOT 依赖（内部实现细节）

| 项 | 为什么是内部 |
|---|---|
| `FTcsAttributeStore` 的字段布局 | 跨模块读它会让 `BaseValue` 的载体变成**跨模块契约**（`attribute-pipeline:200-202` 明文禁止） |
| `FTcsAttributePipeline` 的 `private:` 段 | 机制面（重算内核 / 依赖登记 / 求值栈），`attribute-pipeline:145` 明写不构成消费契约 |
| `FrozenAttributes` | 冻结是**门面语义**，暂存区只是实现手段 |
| `MaxFlushPasses` 等常量 | 实现调参 |

**`FTcsAttributePipeline` 本身的定位有点微妙**：`attribute-pipeline:139` 要求它**声明住 `Public/`**（跨模块消费者需要直调它），但实测**外部消费者为零**——全仓对它的引用只有两处**注释**（`TcsCostOps.cpp:523` / `.h:144`），零调用。见 §6 U-5。

### 3.3 一条被违反的边界（D-7 的来源）

`attribute-pipeline:199-202` 写得很硬：

> **MUST NOT 让跨模块消费者直伸存储**：`TcsSkill` 等消费模块 MUST 经本口读基值，**MUST NOT** 自行 `ResolveStore` + `FindInstance`。

**实际**：`TcsEffectAttributeAccess::HasAttribute`（`TcsEffect/Private/Attribute/TcsEffectAttributeAccess.cpp:25-29`）：

```cpp
const FTcsAttributeStore* Store = AttributeSubsystem ? AttributeSubsystem->ResolveStore(Entity) : nullptr;
return Store && Store->FindInstance(Attribute) != nullptr;
```

**逐字就是被禁止的那个形态。** 展开见 §5 D-7。
---

## §4 竖切影响

### 4.1 调用方向（谁在什么时候碰它）【实测】

| 调用方 | 时机 | 用哪个口 |
|---|---|---|
| `TcsIntegration` `UTcsCombatEntityComponent` | `BeginPlay` / `EndPlay` | `RegisterEntity` / `UnregisterEntity`（**发号方**，`TcsCombatEntityComponent.cpp:79`） |
| `TcsIntegration` `UTcsDefinitionSubsystem` | `Initialize` | `DiscoverAttrModDefs` → 定义库索引（**不经门面**，走 `AssetRegistry`） |
| 宿主 `TcsDev` `UTcsDevBootstrap` | 世界装配 | `RegisterAttributeDef` ×4 + `AddAttribute` ×4（`TcsDevBootstrap.cpp:200/130`） |
| `TcsBuff` `FTcsBuffOps::MountModifiers` | 状态施加/刷新 | `BeginBatch` → `RemoveBySource` → `ApplyModifier` → `Commit`（**一个批**） |
| `TcsEffect` `FTcsStepModifyAttribute` | 链步骤执行 | 同上（**单条也开批**，`TcsStepModifyAttribute.cpp:83`） |
| `TcsEffect` `FTcsTriggerCondition_AttributeCompare` | 触发行求值 | `EvaluateCurrent`（**只读**） |
| `TcsDamage` `FlowExecute` | 流程步骤 | `BeginBatch` → `EvaluateCurrent` → `SetBaseValue` → `Commit`（`TcsFlowStepsCore.cpp:194-203`） |
| `TcsSkill` `CanAffordCost` / `CommitCost` | 施法门禁/扣费 | `TryGetBaseValue` + `SetBaseValue`（**包批前先 `ResolveStore` 探测**） |

### 4.2 时序位置（硬约束）

```
World Init
  └─ UTcsDefinitionSubsystem::Initialize        （GameInstance 级，先于世界）
       └─ DiscoverAttrModDefs → 索引 UTcsAttrModDef（**不进属性定义表**）
World BeginPlay
  └─ UTcsCombatEntityComponent::BeginPlay
       └─ RegisterEntity                        ← 实体身份在这里诞生
  └─ UTcsDevBootstrap::LoadAttributeDefs
       └─ RegisterAttributeDef × N              ← 定义表在这里填
  └─ UTcsDevBootstrap::装配实体
       └─ AddAttribute × N                      ← 实例在这里建（bDirty = true）
  ── 首次 EvaluateCurrent 才真正结算 ──         ← 值域收口/动态边界/既有修正器在这一刻生效
运行时
  └─ 任一写入口（批外）→ 立即 Recalculate + Broadcast
  └─ 批内 → 只标脏 → Commit 才 FlushDirty
```

**三条时序陷阱（都踩过，注释在案）**：

1. **`AddAttribute` 必须晚于 `RegisterAttributeDef`**，否则 `ensureMsgf(DefData != nullptr)` 红字（`TcsAttributeSubsystem.cpp:148`）。装置侧为此写了专门的幂等助手 `R65AddAttributeIfAbsent`（`TcsDevSliceRig_CooldownCost.cpp:410`）。
2. **`RegisterEntity` 必须早于任何属性写入**，否则 `BeginBatch` 的 `ensureMsgf` 红字。技能侧为此在包批前用 `ResolveStore` 探测（`TcsCostOps.cpp:537`）——注释写得很清楚：「不预判就会让**每一次施法终结**都留一条红字」。
3. **`AVD_Custom` 的 `ensure` 是每站点每进程一次**（`TcsAttributePipeline.cpp:291` 用 `ensureMsgf` 而非 `ensureAlwaysMsgf`）。这是对的：配置错误只需要报一次，不需要每帧刷屏。

### 4.3 一条竖切级教训：**批不是事务，它只推迟广播**

这是本模块**最容易被误读**的一条，且已经误导过一次真实设计：

```cpp
// TcsCostOps.cpp:523-525（注释，实测源码核实 2026-10-12）
// - **不给**：**回滚**。`FTcsAttributePipeline::SetBaseValue` 的写入是**立即**的
//   （`Instance->BaseValue = NewBaseValue;`），批只推迟 `Recalculate`。
```

**代码确实如此**：`SetBaseValue`（`_Cascade.cpp:96`）第一件事就是 `Instance->BaseValue = NewBaseValue;`，与 `BatchDepth` 无关。批只在 `Commit` 时跑 `FlushDirty`。

**⇒ 批提供的是「一次重算 + 每属性最多一次广播 + 订阅者看不到中间态」，不是「全有全无」。** 想要原子性只能靠**先判后扣**（`CanAffordCost` 在 `CommitCost` 之前）。

**这条与 `attribute-transaction` 的需求「失败零写入」直接冲突**——见 §5 D-1。

### 4.4 一条跨模块的隐性契约：**谁负责发实体号**

`FTcsCombatEntityHandleRegistry` 是**进程级**的（`TcsCombatEntityHandle.h:64`，`std::atomic<int64>`，永不复用），但**发号方是 `UTcsAttributeSubsystem::RegisterEntity`**（R3 权宜，M6 世界注册表落地后移交）。

**今天的状态**：`TcsIntegration` 的 `UTcsCombatEntityComponent` 是唯一真实发号点（`TcsCombatEntityComponent.cpp:79`），装置直接调 `RegisterEntity`。**没有任何东西阻止两个模块各自注册同一实体**——`EntityName` 只是调试名，不参与键控。

**影响**：`TcsSkill` / `TcsDamage` 都假定「句柄已在属性侧注册过」。这条假定的兜底是 `ResolveStore` 探测，但如果某模块直接 `BeginBatch` 就会红字。**这是模块边界上的一条口头契约，没有编译期或运行期保障。**
---

## §5 技术债（期望 vs 实际，逐条带证据）

### D-1 ★★★ 「唯一提交点与失败零写入」：**代码里没有候选集，也没有回滚**

| 项 | 内容 |
|---|---|
| **期望** | `attribute-transaction` 需求「唯一提交点与失败零写入」（`spec.md:27-37`）：提交 MUST **零写入**——不得出现「改了一半」的中间态；写回范围 MUST 限于本次批涉及的属性；提交失败时 MUST 保持批前状态（脏标记与缓存值回到批前语义） |
| **实际** | 批内**没有任何候选集**：修正器在 `ApplyModifier` 当场 `Instance->AttrModInstances.Add(Modifier)`（`TcsAttributePipeline.cpp:106`）；基值在 `SetBaseValue` 当场写（`_Cascade.cpp:96`）。批只推迟 `Recalculate` 与 `BroadcastChange`。**没有任何回滚代码**（全模块 grep Rollback / 回滚 = 0 命中） |
| **偏差证据** | `TcsAttributePipeline.h:113-125` 的 `BeginBatch` / `Commit` 注释只说「批内只标脏」；`Commit` 的唯一动作是 `FlushDirty`。`attribute-transaction:33` 的场景「提交失败零写入」要求「该属性值保持批前值（不出现合法那条已生效的半写入）」——**今天做不到** |
| **影响面** | ① 规格承诺的原子性**不存在**，依赖它做「多步全有全无」的调用方会静默得到半写入；② 但**实际调用方都没有依赖它**——`TcsSkill` 明文「MUST NOT 把批当作事务/回滚来依赖」（`TcsCostOps.cpp:522`）、`TcsBuff` 只借批做「摘旧与挂新之间无中间态」、`TcsDamage` 单属性写、`TcsEffect` 单条写 ⇒ **今天零实际损害**，但规格是假的 |
| **是否已登记** | **否**。台账无对应条目；`attribute-transaction` 的需求本身仍是 MUST |
| **性质判定** | 这是**实现欠账**（规格更严、代码更松），不是文档滞后。与 `ARCH-03` 的 D-1/D-4 方向相反 |

**为什么当年会这样**（【推断】，依据 `TcsCostOps.cpp:520-527` 的「源码核实 2026-10-12」追注）：批最初是按「事务」命名的，落地时只实现了「延迟重算 + 合并广播」这半边——**这半边恰好是调用方真正需要的**（避免订阅者看到中间态、避免多次广播），于是没人回头补另半边。注释里那句「批给什么、不给什么」是**事后**补的澄清，说明作者已经发现这个落差，但选择**改注释而不是改规格**。

**两条可选处置（MUST 由用户裁）**：

| 方案 | 内容 | 代价 |
|---|---|---|
| **A. 修规格**（推荐） | 把「失败零写入」降级为「失败**不广播**」，并明写「批只推迟重算与广播，不提供回滚；原子性由调用方先判后扣保障」 | 零代码；但 `attribute-transaction` 的能力名 transaction 本身有误导性 |
| **B. 修实现** | 引入候选集（`TArray<FTcsAttrModInstance>` 暂存 + 提交时合并），失败则丢弃 | 改动 `ApplyModifier` / `SetBaseValue` / `Commit` 三处 + 热路径多一层拷贝；且 `attribute-pipeline:49` 的「`PeekPending` 只算不写」语义要重新对齐 |

### D-2 ★★★ `TcsAttribute` 不依赖 `TcsAssetButler`：两个 Def 类**至今未自注册**

| 项 | 内容 |
|---|---|
| **期望** | `ARCH-03` U-1 已判定：4 个 Def 类的自注册未落，`TcsAttribute` 占 2 个（`UTcsAttributeDef` / `UTcsAttrModDef`），且性质已从「可选统一」变为「**迁移前的硬前置**」 |
| **实际** | `TcsAttribute.Build.cs:12-20` 依赖恰好六项，**无 `TcsAssetButler`**；两个 `PrimaryAssetType` 常量存在（`TcsAttributeDef.cpp:8` / `TcsAttrModDef.cpp:8`）但**无 `UE_DEFINE_PRIMARY_ASSET_TYPE`**。全仓注册宏命中仅 3 处（`TcsBuffDefAsset` / `TcsEffectChainDef` / `TcsEffectTriggerDefAsset`） |
| **偏差证据** | `grep -rn UE_DEFINE_PRIMARY_ASSET_TYPE Source/` ⇒ 3 处使用，均不含本模块 |
| **影响面** | ① `UTcsAttributeDef` **没有任何插件侧发现路径**——`TcsIntegration` 的 `Discover*` 五条里没有「属性定义」（实测：全仓 `DiscoverAttributeDefs` 零命中）；今天它只靠宿主装置 `UTcsDevBootstrap::LoadAttributeDefs` 用 `GetAssetsByClass` 直扫（`TcsDevBootstrap.cpp:168`）；② `UTcsAttrModDef` 走旧口 `DiscoverAttrModDefs`（`ARCH-03` U-2），且因未注册而**不能**改走新口 |
| **是否已登记** | **部分**——`ARCH-03` §6 U-1 已列（记作 `TcsAttribute` 域），但**未进台账** |

**注意一处不对称（新发现，ARCH-03 未记）**：`UTcsAttributeDef` **连旧口都没有**。其它四个 Def 族（链/触发/状态/技能）都至少有 `Discover*` 一条，而属性定义**只有宿主装置在扫**——插件侧零发现路径。见 §6 U-1。

### D-3 ★★ 规格引用了**不存在的**能力 `attribute-moddef`

| 项 | 内容 |
|---|---|
| **期望** | `attribute-types/spec.md:164`：「`OverridePriority` MUST 只对 `TAO_Override` 生效（其余带忽略，**编辑器侧给警告见 `attribute-moddef` 一节**）」 |
| **实际** | `openspec/specs/` 下**没有** `attribute-moddef` 能力（实测 `ls openspec/specs` ⇒ 只有 `attribute-pipeline` / -store / -transaction / -types` 四个）。该警告实际写在 `attribute-types` 自己的「修正器模板资产与约定列白名单」需求末段 |
| **偏差证据** | 该悬空引用在归档变更 `2026-09-18-add-tcsattribute-pipeline-and-transaction` 的 delta 里就已存在（`:135`），此后 `2026-09-22-switch-identifiers-to-gameplay-tags` 的 delta 逐字复制（`:212`）⇒ **两轮归档都没修** |
| **影响面** | 读者按指引去查 `attribute-moddef` **查不到**；而 `openspec validate --strict` **不校验跨能力引用**（只校验 Requirement/Scenario 结构）⇒ 这类悬空引用**永远不会被工具发现** |
| **是否已登记** | **否** |

**这是本档案首次发现的「跨能力悬空引用」**，性质与 `ARCH-03` G-6/G-7（假事实）不同——它不是「说错了」，而是**指向一个不存在的东西**。建议升为全局待办 G-9。

### D-4 ★★ 「依赖图每轮重建不缓存」——代码是**持久累加、只增不减**

| 项 | 内容 |
|---|---|
| **期望** | `attribute-pipeline:106`：「图 MUST **每轮重建不缓存**（脏标记已限流；热了再优化）」 |
| **实际** | `Store.Dependents`（`TcsAttributeStore.h:31`）是**持久容器**：`RegisterDependency` 只 `FindOrAdd` + `Add`（`_Dependency.cpp:104-110`），**从不清理**。全模块 `Dependents` 的 9 处引用里，唯一的移除是「成环时把刚加的那条删掉」（`:116`） |
| **偏差证据** | `grep -rn Dependents Source/TcsAttribute` ⇒ 无 `Empty()` / 无 `Reset()`；`RemoveAttribute` / `UnregisterEntity` 都不碰它 |
| **影响面** | ① 属性被冻结/移除后其依赖边**仍然在**——`MarkDependentsDirty` 会继续给一个已不存在的实例标脏（`FindInstance` 返回 null 所以无害，但白遍历）；② 长期运行的 World（尤其 PIE 反复进出）里图**单调增长**、永不收缩；③ 规格要求的「每轮重建」能自动获得的两条性质——**属性移除后自动失联**、**依赖变化后自动收敛**——今天都没有 |
| **是否已登记** | **否** |
| **性质判定** | **实现欠账**。持久化本身是个合理的性能选择（注释说「热了再优化」），但**规格写的是重建**，且没有任何地方记录「我们选了持久化」这个偏离 |

**为什么这条值得单列**：它不是「忘了做优化」，而是**语义漂移**。「每轮重建」与「持久累加」在**正确性**上不等价（前者自动清理失效边，后者不会）。选后者本身可以，但 MUST 在规格里写清并补上清理时机。

### D-5 ★★ `UTcsAttributeDef` 的定义**没有任何插件侧装载路径**

| 项 | 内容 |
|---|---|
| **期望** | `attribute-store:42`：「`RegisterAttributeDef`：宿主 / DefLibrary 加载定义资产后登记」——`DefLibrary` 即 `UTcsDefinitionSubsystem` |
| **实际** | `UTcsDefinitionSubsystem::Initialize`（`TcsDefinitionSubsystem.cpp:29-33`）只调 5 条 `Discover*`：链 / 触发 / 状态 / **修正器模板** / 技能。**没有属性定义**。全仓 `RegisterAttributeDef` 的调用点只有 3 个，**全在宿主 `TcsDev`**（`TcsDevBootstrap.cpp:200` / `TcsDevSliceRig_CooldownCost.cpp:390,1546` / `TcsDevSliceRig_Skill.cpp:252`） |
| **偏差证据** | `grep -rn DiscoverAttributeDefs` ⇒ 0 命中；`grep -rn RegisterAttributeDef Source/ Plugins/` ⇒ 插件侧仅声明与自调用 |
| **影响面** | ① **插件本身无法装载属性定义**——任何不含 `TcsDev` 的宿主都必须自己写一遍「扫 `UTcsAttributeDef` → `RegisterAttributeDef`」这段胶水（`TcsDevBootstrap.cpp:142-217` 有 76 行完整实现可抄）；② 定义库的就绪门禁（`IsRuntimeReady`）**不覆盖属性定义**——`UTcsCombatEntityComponent` 以为「定义库就绪 = 属性定义也在了」，实际不是 |
| **是否已登记** | **否**。台账 `ATTR-1` 只记了「模板身份词的根归属」，与此无关 |

### D-6 ★ `ITcsAttributeProvider` **零实现者**，而规格仍写「首个实现在 plan2 Task 5 的战斗实体组件」

| 项 | 内容 |
|---|---|
| **期望** | `attribute-store:96`：「首个实现在 plan2 Task 5 的战斗实体组件」 |
| **实际** | `UTcsCombatEntityComponent`（`TcsCombatEntityComponent.h:49`）**不实现** `ITcsAttributeProvider`（实测：该头对 `ITcsAttributeProvider` / `GetBaseValue` / `GetCurrentValue` / `PeekPending` 零命中）。全仓实现者 = **0**（C++ 侧 0、C# 侧 `Script/` 下 0） |
| **偏差证据** | 台账 `STAT-11`（2026-10-11）已如实记：「接口声明在册、实现者 0；`AttributeScaled` 参数源结构体存在但**无实例化路径**」 |
| **影响面** | `FTcsParamSource_AttributeScaled`（122 行）+ `FTcsAttributeEvaluateContext`（27 行）**合计 149 行零消费**；且配了 `AttributeScaled` 的内容会**静默落兜底值**（`TcsParamSource_AttributeScaled.h:93-105` 的 checked cast 失败 → `Fallback`，不报错不 ensure） |
| **是否已登记** | **✅ 是**——台账 `STAT-11` 逐条在册，状态「待触发」 |

**与 D-2 的连带**：这条的闭合很可能与 D-2 同批——因为「属性换算」的真实用法是「读实体属性换算技能参数」，而实体属性来自属性定义资产，定义资产又需要发现路径。**三条是一条链**。

### D-7 ★★ 规格明文禁止的「直伸存储」在 `TcsEffect` 侧**正在发生**

| 项 | 内容 |
|---|---|
| **期望** | `attribute-pipeline:199-202`：「**MUST NOT** 让跨模块消费者直伸存储：…**MUST NOT** 自行 `ResolveStore` + `FindInstance`。判据 = 门面是模块的对外唯一入口：跨模块直读会让属性存储的形状变成跨模块契约」 |
| **实际** | `FTcsEffectAttributeAccess::HasAttribute`（`TcsEffectAttributeAccess.cpp:25-29`）逐字如此；它被 `TcsEffect` 的触发条件与链步骤使用 |
| **偏差证据** | `grep -rn ResolveStore Source/ | grep -v TcsAttribute` ⇒ 5 处，其中 **4 处是「存在性探测」**（合法用法，`ResolveStore` 本就是非确保查询口），**1 处是「取 Store 再 FindInstance」**（`HasAttribute`） |
| **影响面** | 与规格自述的判据**完全吻合**：今天 `FTcsAttributeInstance` 的字段布局成了 `TcsEffect` 的隐式契约。若 M2 想换 `AttrModInstances` 的载体（比如换池），必须同时改 `TcsEffect` |
| **是否已登记** | **否** |

**一个便宜的修法**：把 `HasAttribute` 改成调 `TryGetBaseValue`（它已经回答「是否持有」，且是门面口）。**语义完全等价、零新增 API**。之所以没做，大概率是因为 `TryGetBaseValue`（2026-10-11）比 `HasAttribute`（R5 Task 6，2026-10-05）**晚落地** ⇒ 又一个「执行序导致漏消费」的实例（同 `ARCH-03` D-1 的根因）。

### D-8 ★ `GetTcsAttributeBandWeight` **全仓零调用**

| 项 | 内容 |
|---|---|
| **期望** | `attribute-types:10` 要求 MUST 提供带权助手（Override 0 / Add 10 / PercentAdd 15 / Mul 20 / FlatAdd 30），用途是「带序唯一真相在 Op」 |
| **实际** | 全仓 grep ⇒ **只有定义处一处命中**（`TcsAttrModInstance.h:41`），**零调用** |
| **偏差证据** | 折叠函数 `FoldTcsAttributeBands` 用 `switch (Entry.Op)` 直接分桶（`TcsAttributeBandFold.h:130-160`），**根本不读带权**——因为五带的公式是硬编码的，不需要排序 |
| **影响面** | **零功能影响**（它本来就是个纯查询助手）。真实价值是「审计/展示」（注释自述：`账本 SortKey 仅为同带内展示/审计位`）。但它今天连审计消费者也没有 |
| **是否已登记** | **否** |
| **设计是否还成立** | **存疑**。规格把它写成 MUST，但它既非折叠所需、也无消费者。这是「零消费者不预建」这条本仓自定判据的一处**违反**——同 `ARCH-03` D-4 的加载三件 |

### D-9 ☆ `PeekPending` 的「候选值」语义在批内不完整

| 项 | 内容 |
|---|---|
| **期望** | `attribute-transaction:41-45`：批进行中 `PeekPending` MUST 返回「该属性在当前批内**候选集**上算出的值（含尚未提交的变更）」 |
| **实际** | `ComputeCandidate`（`TcsAttributePipeline.cpp:218-223`）直接复用 `ComputeFoldedValue`——**但依赖属性按缓存值算**。注释自己承认：「批内依赖也脏时以缓存值为准——预览用途足够」 |
| **偏差证据** | `:221` 的原话。而规格要求的是「候选集」（含批内对依赖的变更） |
| **影响面** | 今天**零消费者**（`PeekPending` 的外部调用只有装置 `TcsDevSliceRig_CooldownCost.cpp:1141` 的**批深度探测**——它只比较 `PeekPending == EvaluateCurrent`，不关心依赖）。**但规格承诺的比实现强** |
| **是否已登记** | **否** |

### D-10 ☆ `AVD_Wrap` 用 while 循环回卷，超大值会长时间自旋

```
TcsAttributePipeline.cpp:276-283
while (Value < MinValue) { Value += Span; }
while (Value > MaxValue) { Value -= Span; }
```

**性质**：Span 极小 + Value 极大时（如 Span=0.001、Value=1e9）会执行 **1e12 次**加法。现实中罕见（属性值域通常是人给的配置），但**没有量级保护**。`FMath::Fmod` 一行即可。

**是否已登记**：**否**（台账 `TOOLS-2` 只记了「未给跨度」，没记「跨度极小」）。**影响面**：零（无真实内容会这样配）。列在此仅为让下一次触碰该函数的人顺手改。

### D-11 ☆ 三处注释里的过时/不准引用

| 位置 | 现文 | 实况 |
|---|---|---|
| `TcsAttrModInstance.h:66` | 「策略住在**属性定义**（`FTcsAttributeDefTableRow::OverrideTieBreak`）」 | 字段实际在 `FTcsAttributeDefData`（`TcsAttributeDef.h:79`），表行只是组合持有 |
| `TcsAttributeStore.h:15-16` | 「实体适配器可长期缓存本容器指针…UE TMap 的值节点堆分配且地址稳定」 | 结论**正确但归因不准**：稳定性来自门面外层 `TUniquePtr` 间接层（`TcsAttributeSubsystem.h:324`），不是「TMap 值节点堆分配」 |
| `TcsAttributeProvider.h:22` | 「M2 对外**唯一**契约」 | 实际对外面还有门面 `UTcsAttributeSubsystem` 的 6 个 `UFUNCTION`——Provider 是「读侧契约」而非「唯一契约」 |

**性质**：纯注释瑕疵，零行为影响。**已登记**：否。

---

## §6 未实现（设计是否还成立 + 前置条件）

### U-1 ★★★ 属性定义的**插件侧发现路径**不存在

| 项 | 内容 |
|---|---|
| 现状 | `TcsIntegration` 的 5 条 `Discover*`（链/触发/状态/修正器模板/技能）**不含属性定义**。`UTcsAttributeDef` 今天只有宿主 `TcsDev` 在扫（`TcsDevBootstrap.cpp:142-217`，76 行） |
| **设计是否还成立** | **成立，而且比当年更必要**。旧 `SPEC-01-attributes` 写「运行期一律走资产」（双轨制），但**没有任何一条规格要求「谁负责发现资产」**——这是设计里的一处空白，不是被推翻 |
| **当初依据今天是否仍有效** | 「宿主/DefLibrary 加载后登记」这条分工**仍成立**（`attribute-store:42`）。但 `DefLibrary` 的 5 条 `Discover*` 里没有它 ⇒ 分工写了、执行方没写 |
| **前置条件** | 与 **D-2 同批**：先 `UE_DEFINE_PRIMARY_ASSET_TYPE(UTcsAttributeDef, …)`（需先给 `Build.cs` 加 `TcsAssetButler` 依赖），再加 `DiscoverAttributeDefs()`。**MUST NOT 只加发现不加注册**（`GetAssetsForDefClass` 对未登记类型直接返回失败） |
| **已登记** | **否** |

**一处必须先裁的设计问题**：属性定义的消费面是 `UTcsAttributeSubsystem::RegisterAttributeDef`（**World 级**），而 `UTcsDefinitionSubsystem` 是 **GameInstance 级**、且它的 5 条 `Discover*` 都是「建索引」不是「登记进域门面」。若属性定义照抄，则「发现」与「登记」之间还需要一次 World 级装配——**这个装配点今天也是宿主在承担**（`TcsDevBootstrap`）。**⇒ 补 U-1 之前，必须先回答「属性定义的 GameInstance 索引 ↔ World 定义表之间由谁搬」**。这不是实现细节，是设计决策。
### U-2 ★★ `AttributeSet`（D2-15）**整体未建**

| 项 | 内容 |
|---|---|
| 现状 | 全仓 grep `AttributeSet` ⇒ 只有 **4 处**，全是注释/规格里的「R3 不做 AttributeSet——D2-15 属 R7」（`TcsCombatEntityComponent.h:29,109` / `integration-entity/spec.md:13`）。**类型、资产、施加入口全无** |
| **设计是否还成立** | **需要重新核对**。旧 `SPEC-01` 的 D2-15 定了完整形态（B1a+B2+B3a：实体侧引用 / GameInstance 级资产 + World 级施加 / diff 替换），但**它依赖的三件事今天都变了**：① `DefTag` 从 `FName` 改成了 `FGameplayTag`（2026-09-22）；② 属性定义资产**没有发现路径**（U-1）；③ 冻结/解冻机制（2026-09-18）落地后，`RemoveAttribute` 已不是销毁而是冻结 ⇒ **`ClearAttributeSet` 的语义要重定**（清空 = 全冻结？还是真删？暂存区会不会无限涨？） |
| **当初依据今天是否仍有效** | 「属性 = 结构，修正器 = 动态」这条边界**仍有效**（推荐用法），但「框架能力 = 允许运行期增删」这半句与冻结机制叠加后语义未重新表述 |
| **前置条件** | U-1 先落（Set 内容是 `TArray<FGameplayTag>`，需要定义表能解析它们）；且 MUST 先裁「Clear 与冻结的关系」 |
| **已登记** | **部分**——`integration-entity:13` 与 `TcsCombatEntityComponent.h:29` 都写了「R3 不做，属 R7」，但**没有台账条目**，也没有记录上面三条变化 |
### U-3 ★ `IValueDomainPolicy`（`AVD_Custom` 的策略接口）**未建**

| 项 | 内容 |
|---|---|
| 现状 | `AVD_Custom` 命中时 `ensureMsgf(false, ...)` + 按 Clamp 收口（`TcsAttributePipeline.cpp:288-298`）。接口 `IValueDomainPolicy` **全仓零命中** |
| **设计是否还成立** | **需要重新核对**。旧 `SPEC-01` 写「CustomFragment(IValueDomainPolicy 只接管值域函数，时序/级联/事务由引擎守护)」——但 **Fragment 这个词本身已过时**（现在的逃逸位约定是「UINTERFACE + BlueprintNativeEvent」，见台账 `COST-3` 的裁定）。若今天要建，形态应是**宿主插槽接口**而非 USTRUCT 策略基类（后者脚本不可达，同 `FTcsParamValueSource` 的实测判据） |
| **当初依据今天是否仍有效** | 「自定义值域只接管值域函数」**仍有效**；「枚举值固定 1」也仍在（`TcsAttributeBounds.h:120`）。但「何时需要它」的依据从未成立——**没有任何真实内容配过 `AVD_Custom`**（全仓 grep 只有定义处与管线分支） |
| **前置条件** | 真实需求出现。**MUST NOT 预建**——它与 `COST-3` 是同一类「扩展点形状」问题，可复用同一次裁定 |
| **已登记** | **否** |

### U-4 ★ 门面**没有遍历口**，`GetStore` 是唯一后门且零消费者

| 项 | 内容 |
|---|---|
| 现状 | `UTcsAttributeSubsystem` 无任何「遍历全部实体/全部属性」的入口。要遍历只能 `GetStore(Entity)` 后自己走 `Store->Attributes`——而这正是 D-7 禁止的「直伸存储」 |
| **设计是否还成立** | **存疑**。规格从未要求遍历口（`attribute-store` 4 条需求里没有）。但屏显/存档/网络同步这类真实需求一定会需要它 |
| **前置条件** | 出现第一个真实消费者 |
| **已登记** | **否** |
| **附带发现** | `GetStore`（`TcsAttributeSubsystem.cpp:313`）**全仓零外部消费者**（插件内 0、宿主 0）。它是「零消费者不预建」的又一处违反，且它是 D-7 那条红线的**后门**——红线的正式口没建，后门却开着 |
### U-5 ★ `FTcsAttributePipeline` 的「跨模块直调」面：规格要求、**零消费者**

| 项 | 内容 |
|---|---|
| 现状 | `attribute-pipeline:139-146` 专门为「跨模块消费者直调」把 `FTcsAttributePipeline` 的声明从 `Private/` 移到 `Public/`（2026-09-18 用户拍板），并要求带导出宏。**实测外部消费者 = 0**（全仓只有两处注释引用它） |
| **设计是否还成立** | **存疑**。规格给的用途是「离屏/工具侧求值、无门面引用等场景」——但**这类场景至今一个都没出现**。同 `ARCH-03` D-4（加载三件）是同款：为一个假想的消费者付了 173 行头文件的维护成本 |
| **当初依据今天是否仍有效** | 「确认未来有跨模块消费者」（2026-09-18 用户拍板）**未兑现**。反证：今天所有消费都经门面，且 `TcsSkill` 甚至明文**拒绝**用管线做事务（`TcsCostOps.cpp:522`） |
| **前置条件** | 出现「无门面可用」的真实场景（离屏工具 / 非 UWorld 宿主） |
| **已登记** | **否** |
| **处置建议** | 不必删（它确实是一份合理的公开面），但 `attribute-pipeline` 里那条「跨模块消费者需要直调」的**依据陈述应当订正**为「预留逃生口」 |

### U-6 ☆ `TcsAttribute` 对 `TcsAssetButler` 的依赖边未建（D-2 的结构面，单列以便追踪）

这是 D-2 的**结构面**：`TcsAttribute` 住层 1，而 `TcsAssetButler` 住层 0 ⇒ 加依赖**不会成环**（`TcsEffect` / `TcsBuff` / `TcsIntegration` 三个上层模块都已这么做了）。所以这条**纯属未做**，不是被架构挡住。

**为什么它值得单列**：`ARCH-03` §3.2 那条红线是「各域向下注册」——而 `TcsAttribute` 是**层级最低的业务模块**。`TcsAssetButler.Build.cs:12` 的注释甚至点名了它：「本模块住得比所有注册者都低，各域（**含层级最低的 TcsAttribute**）」。**注释写了、代码没做**——这是全仓「注释先行于实现」的典型一例。
---

## §7 文档资产对照

| 层 | 载体 | 状态 |
|---|---|---|
| **可执行需求** | `attribute-types`（311）/ `attribute-store`（136）/ `attribute-pipeline`（232）/ `attribute-transaction`（73）= **752 行 / 27 需求 / 86 场景** | **质量最高的一档**；但有 3 处规格更严（D-1/D-4/D-7）+ 1 处悬空引用（D-3） |
| **旧模块规格** | `spec/02-module-attributes.md`（129 行，`SPEC-01-attributes`，状态 `ACTIVE`） | 内容与 `openspec` 高度重合（后者基本是它的结构化改写）。**但它含两处已过时内容**：§2.2a 的 `ModifierSlots`（已改名 `AttrModInstances`，文中有就地划改）、§5 的网络姿态（NET-1/2，`openspec` 零对应需求） |
| **决策** | `dec-03-attribute-set`（D2-15 AttributeSet） | **未落地的决策**，见 U-2 |
| **台账** | `STAT-11`（`AttributeScaled` 零实现者）/ `SCRIPT-9`（属性写入面缺反射）/ `ATTR-1`（模板身份词根归属，已闭合）/ `TOOLS-2`（`AVD_Wrap` 未给跨度 + 动态边界引用缺失）/ `CHAIN-8`（链缺直写基值原语） | 5 条在册，**本档案新增的 11 条债均不在册** |

**一处值得记的观察**：旧 `SPEC-01` 的 §2.2a 有一句「实施状态（2026-09-18 更新）：本条与冻结暂存区**已随 plan1 Task 5 落地**」——这是全仓**唯一一处**在旧文档里主动维护「落地状态」的地方。绝大多数旧文档只写设计不写状态，这正是 `ARCH-INDEX` §1.3「声称未落地」缺陷类型的主要来源。**如果每份旧 SPEC 都有这样一行，本档案的价值会小很多。**
---

## §8 审核记录

| 项 | 内容 |
|---|---|
| 审核日期 | 2026-10-17 |
| 审核范围 | `Source/TcsAttribute/` 全部 **25 个文件 / 3 034 行**；`openspec/specs/attribute-*` 四份规格 **752 行**；旧 `spec/02-module-attributes.md` **129 行**；跨模块消费面 grep 全仓 |
| 已读文件 | 25 个源码文件（逐个通读，非抽样）+ 4 份规格全文 + 旧 SPEC 全文 |
| 交叉验证的符号 | `FoldTcsAttributeBands` / `FTcsAttributeBandEntry` / `FTcsAttrModInstance` / `MakeFromDef` / `ETcsAttributeOp` / `ETcsAttributeComparison` / `GetTcsAttributeBandWeight` / `ITcsAttributeProvider` / `FTcsParamSource_AttributeScaled` / `FTcsAttributePipeline` / `ResolveStore` / `TryGetBaseValue` / `GetStore` / `PeekPending` / `GetSubsystem` / `UE_DEFINE_PRIMARY_ASSET_TYPE` / `RegisterAttributeDef` / `AddAttribute` / `Dependents` |
| 未做的事 | **未编译、未跑 PIE**——本轮是**只读审核**，全部结论来自源码与规格的静态对照。凡涉及运行期行为的结论均已标【实测】（读代码）或【推断】（多证据推理） |
| 置信度分布 | 【实测】约 90%（全部代码锚点、全部 grep 计数）；【推断】约 10%（D-1 的历史成因、U-1 的设计前置） |
---

## §9 结论

**`TcsAttribute` 的代码质量属「干净」一档**：

- 折叠核**真正单份**（三个消费者 + `TcsSkill` 的自我约束注释），三级全序的比较逻辑比多数引擎实现更严；
- 惰性重算的**唯一写入点**守得住（`CachedCurrent` 全仓只有一个赋值处）；
- 冻结/解冻与来源级联撤销的**共存**处理到位（含扫暂存区这条容易漏的）；
- 依赖环用 **Tarjan SCC** 严格拒绝，且**轮数上限与环判定解耦**（修了旧 TCS 的误判缺陷）；
- 反射面**有边界、有理由**，无反射的两处都给了实测依据；
- 词汇归口做得正：`ETcsAttributeComparison` 住生产者侧、消费者只取词不造词。

**但它是本档案目前审出的「规格更严、代码更松」最多的一处**——11 条债里有 4 条属于这一类（D-1 零写入 / D-4 图不重建 / D-7 直伸存储 / D-9 候选值不完整）。这与 `ARCH-03` 的方向**正好相反**（那里是规格漏收录、代码在干活）。

**最贵的三条（建议优先处理）**：

| 优先级 | 条目 | 一句话 |
|---|---|---|
| **1** | **D-1** | `attribute-transaction` 承诺的「失败零写入」**代码里不存在**，且没有任何回滚机制。要么改规格（推荐，零代码），要么改实现（动三个写入口） |
| **2** | **D-2 + U-1** | 属性定义**没有插件侧发现路径**，两个 Def 类未自注册；任何不含 `TcsDev` 的宿主都得自己抄 76 行胶水 |
| **3** | **D-7** | 规格明文禁止的「直伸存储」在 `TcsEffect` 侧正在发生，且有一个**语义等价的零成本修法**（改用 `TryGetBaseValue`） |

**最值得记住的一句话**：

> **本模块的债几乎全是「落地时只做了调用方真正需要的那半边」**——批只做延迟重算不做回滚（因为调用方只要前者）、图只做累加不做重建（因为累加更快）、管线只做公开不做消费（因为当时以为会有人直调）。**每一条单独看都是合理取舍，合起来就是规格与实现两张皮。**

**给下一模块 `TcsState` 的四个关注点**：

1. `TcsState` **不依赖 `TcsEffect`**（已兑现），但**依赖 `TcsAttribute`**——它是否也犯了 D-7 那种「直伸存储」？`FTcsStateDefBase.ModifierRows` 是 `TSoftObjectPtr` 的资产直引用，物化点却住 `TcsBuff`，这条跨模块分工是否清楚？
2. `attribute-store` 的「事务纪律」要求 `AddAttribute` / `RemoveAttribute` / `SetBaseValue` **走与修正器写入同一路径**——`TcsState` 侧是否有绕过门面直改账本的路径？
3. `TcsState` 与 `TcsBuff` 的关系是本仓最微妙的一处（**`TcsBuff` 依赖 `TcsState`，反之不成立**）——`FTcsStateDefBase` 在 `TcsState` 而 `UTcsBuffDefAsset` 在 `TcsBuff`，这条边是否被正确使用？
4. `STAT-4`（`CategoryTags` 的 tag 根归属）与 `R5.5-e1` 的关系表族刚落，**它的第一个解析消费者**（关系表检查器）是否真的接上了？
