<!-- OPENSPEC:START -->
# OpenSpec Instructions

These instructions are for AI assistants working in this project.

Always open `@/openspec/AGENTS.md` when the request:
- Mentions planning or proposals (words like proposal, spec, change, plan)
- Introduces new capabilities, breaking changes, architecture shifts, or big performance/security work
- Sounds ambiguous and you need the authoritative spec before coding

Use `@/openspec/AGENTS.md` to learn:
- How to create and apply change proposals
- Spec format and conventions
- Project structure and guidelines

Keep this managed block so 'openspec update' can refresh the instructions.

<!-- OPENSPEC:END -->

---

## ⚠️ 代码规范 - 强制要求

**在编写任何代码前，必须阅读并遵循以下规范：**

1. **项目规范**: `@/openspec/project.md` - 包含完整的代码风格规范
2. **检查清单**: `@/CODE_REVIEW_CHECKLIST.md` - 提交前必须检查的项目

### 关键规范提醒

- ✅ **使用 `#pragma region-endregion`** 组织代码区域
- ❌ **禁止使用注释** (如 `// ========== Section ==========`) 来划分区域
- ✅ 遵循空行规范: region后1空行, endregion前1空行, 区域间2空行
- ✅ 遵循文件头部格式: Copyright → pragma once → includes → 3空行

**违反规范的代码将被要求修改！**

---

## 🚀 OpenSpec 快速参考指南

> **重要提示**: 本指南用于优化 OpenSpec 工作流程，避免不必要的 token 消耗。

### 核心原则

**OpenSpec 没有 `apply` 命令！** 工作流程分为三个阶段：

1. **Stage 1: Creating Changes** - 创建提案（使用 CLI 工具）
2. **Stage 2: Implementing Changes** - 直接实施（手动编码）⭐
3. **Stage 3: Archiving Changes** - 归档（使用 `openspec archive`）

### 常用命令速查

```bash
# 查看变更列表
openspec list

# 查看规范列表
openspec list --specs

# 查看变更详情
openspec show <change-id>

# 验证变更
openspec validate <change-id> --strict --no-interactive

# 归档变更（完成后）
openspec archive <change-id> --yes
```

### 实施流程（Stage 2）⭐ 最重要

当提���已经存在时，**直接开始编码**：

1. ✅ 读取 `tasks.md` - 获取任务清单（最重要！）
2. ✅ 读取 `proposal.md` - 了解目标和背景
3. ✅ 读取 `design.md` - 了解技术决策（如果存在）
4. ✅ **直接开始实施任务** - 不需要任何 CLI 命令
5. ✅ 完成后更新 `tasks.md`，标记 `[x]`

---

# TireflyCombatSystem (TCS) 插件架构文档

> **TireflyCombatSystem** 是为 UE5 设计的高度模块化战斗系统框架。
> 核心理念："一切皆状态"，提供统一的属性、状态、技能管理方案。

**版本**: 1.0 (UE5.6) | **状态**: Beta

## 📊 完成度速览

| 模块 | 完成度 | 说明 |
|------|--------|------|
| 属性系统 (Attribute) | 95% | 核心功能完备，Clamp 策略已集成 |
| 状态系统 (State) | 90% | 核心架构完成，StateTree 集成进行中 |
| 技能系统 (Skill) | 95% | 基本功能完整，缺少少量高级特性 |
| StateTree 集成 | 85% | 基础集成完成，专用节点开发中 |
| SourceHandle 机制 | 100% | P0 全部实施完成，支持��果链追踪和自动生命周期管理 |

---

## 核心设计理念

- **统一状态管理**: 技能、Buff、状态使用同一套 `UTcsStateDefinitionAsset` 和 `UTcsStateInstance`
- **StateTree 双层架构**: 静态槽位结构 + 动态实例执行，支持可视化编辑
- **策略模式**: 通过 CDO 实现零代码扩展，所有算法都可继承和定制
- **数据驱动**: DataAsset 驱动的配置，减少硬编码
- **高性能设计**: 对象池、批量更新、智能缓存机制
- **SourceHandle 机制**: 统一的效果来源追踪，支持因果链、精确的生命周期管理和事件归因

---

## SourceHandle 机制

**SourceHandle** 是 TCS 的核心追踪机制，用于标识效果来源、构建因果链、管理 Modifier 生命周期。

### 核心结构

```cpp
struct FTcsSourceHandle
{
    int32 Id;                                    // 全局唯一 ID（单调递增）
    FGameplayTagContainer SourceTags;            // 来源标签（可选）
    TWeakObjectPtr<AActor> Instigator;           // 施加者
    TArray<FPrimaryAssetId> CausalityChain;      // 因果链（从根源到直接父级）
};
```

### 快速开始

```cpp
// 1. 创建 SourceHandle
UTcsAttributeManagerSubsystem* AttrMgr = GetWorld()->GetGameInstance()
    ->GetSubsystem<UTcsAttributeManagerSubsystem>();

FTcsSourceHandle SourceHandle = AttrMgr->CreateSourceHandle(
    {},             // CausalityChain（根源为空）
    CasterActor,    // Instigator
    SkillTags       // SourceTags（可选）
);

// 2. 应用修改器
TArray<FName> ModifierIds = { FName("Mod_AttackBoost") };
TArray<FTcsAttributeModifierInstance> OutModifiers;
AttrMgr->ApplyModifierWithSourceHandle(TargetActor, SourceHandle, ModifierIds, OutModifiers);

// 3. 移除修改器
AttrMgr->RemoveModifiersBySourceHandle(TargetActor, SourceHandle);
```

### 设计决策

1. **不存储 SourceDefinition**：各实例（Modifier、State）自身已持有定义引用
2. **因果链用 FPrimaryAssetId**：天然兼容所有 UPrimaryDataAsset 子类
3. **Modifier 生命周期跟随 State**：FinalizeStateRemoval 中自动清理所有通过 SourceHandle 创建的 Modifier
4. **GetModifiersBySourceHandle 纯 const 查询**：不修改内部数据，过期 ID 仅输出 Warning

### 设计规范

**跨 Actor 修改规范**：
- ❌ **禁止**：StateTree 中直接对其他 Actor 调用 `ApplyModifierWithSourceHandle`
- ✅ **正确**：在目标 Actor 上施加独立 State，由目标 State 的 SourceHandle 管理其 Modifier 生命周期

**因果链构建**：
- 根源 State：CausalityChain 为空数组
- 派生 State：CausalityChain = ParentSourceHandle.CausalityChain + ParentStateDefAsset.PrimaryAssetId

**API**：
- `CreateSourceHandle(CausalityChain, Instigator, SourceTags)` - 唯一的创建入口
- `TryApplyStateToTarget(..., ParentSourceHandle)` - 支持因果链传递
- `RemoveModifiersBySourceHandle(Owner, SourceHandle)` - 清理 Owner 上的 Modifier
- `GetModifiersBySourceHandle(CombatEntity, SourceHandle, OutModifiers)` - 纯 const 查询

### 待办

- **P1**：SourceHandle 查询 API 增强（按 Tag/Instigator 批量查询）
- **P2**：SkillModifier SourceHandle 集成（待 SkillModifier 基础架构成型）
- **P2**：对象池集成（涉及 TCS 底层逻辑）

---

## 架构概览

### 一切皆状态

```
战斗实体 (Actor)
├─ 状态 (State)  - 被击退、冰冻等
├─ 技能 (Skill)  - 攻击、法术等
└─ Buff (Buff)   - 增益、减益等
     ↓ 全部使用同一套系统管理
UTcsStateDefinitionAsset + UTcsStateInstance + StateTree
```

### StateTree 双层架构

```
Layer 1: StateTree 槽位管理 (静态)
├─ 定义状态槽位（Action、Buff、Debuff等）
├─ 定义状态转换规则
└─ 通过编辑器可视化配置

Layer 2: 动态状态实例 (动态)
├─ 每个 StateInstance 运行独立逻辑
├─ 动态创建、执行、销毁
└─ 支持跨 Actor 应用（Buff到敌人）
```

### 策略模式扩展点

| 算法类型 | 基类 | 示例实现 |
|---------|------|---------|
| 属性执行 | `UTcsAttributeModifierExecution` | Add、Multiply、MultiplyAdditive |
| 属性合并 | `UTcsAttributeModifierMerger` | NoMerge、UseNewest、UseMaximum、UseAdditiveSum |
| 属性 Clamp | `UTcsAttributeClampStrategy` | Linear（默认）、自定义策略 |
| 状态条件 | `UTcsStateCondition` | AttributeComparison、ParameterBased |
| 状态合并 | `UTcsStateMerger` | NoMerge、Stack、UseNewest、UseOldest |
| 同优先级策略 | `UTcsStateSamePriorityPolicy` | UseNewest、UseOldest |
| 技能修正 | `UTcsSkillModifierExecution` | AdditiveParam、CooldownMultiplier、CostMultiplier |

---

## 三大系统详解

### 1️⃣ 属性系统 (Attribute System) - 95%

**职责**: 管理所有数值属性（生命值、攻击力、防御力等）

**核心类**:
- `UTcsAttributeComponent` - 属性管理组件
- `UTcsAttributeManagerSubsystem` - 全局属性管理（SourceHandle API 所在）
- `UTcsAttributeDefinitionAsset` - 属性定义（UPrimaryDataAsset）
- `UTcsAttributeModifierDefinitionAsset` - 修改器定义（UPrimaryDataAsset）
- `FTcsAttribute` / `FTcsAttributeInstance` - 属性定义/实例
- `FTcsAttributeModifierInstance` - 修改器实例
- `FTcsSourceHandle` - 来源句柄
- `FTcsAttributeChangeEventPayload` - 属性变化事件

**执行流程**:
```
添加属性 → 应用修改器 → 执行算法 → 合并结果 → Clamp → 计算最终值 → 触发事件
```

**执行算法** (`AttrModExecution/`):
- `UTcsAttrModExec_Addition` - 加法
- `UTcsAttrModExec_MultiplyAdditive` - 乘法加法
- `UTcsAttrModExec_MultiplyContinued` - 连续乘法

**合并策略** (`AttrModMerger/`):
- `UTcsAttrModMerger_NoMerge` - 全部应用
- `UTcsAttrModMerger_UseNewest` / `UseOldest` - 使用最新/最旧
- `UTcsAttrModMerger_UseMaximum` / `UseMinimum` - 取最大/最小值
- `UTcsAttrModMerger_UseAdditiveSum` - 加法求和

**Clamp 策略** (`AttrClampStrategy/`):
- `UTcsAttributeClampStrategy` - 基类
- `UTcsAttrClampStrategy_Linear` - 线性约束（默认，FMath::Clamp）
- `FTcsAttributeClampContext` - Clamp 上下文数据
- `UTcsAttributeClampContextLibrary` - Clamp 上下文蓝图库

---

### 2️⃣ 状态系统 (State System) - 90%

**职责**: 管理所有状态（技能、Buff、普通状态等）

**核心类**:
- `UTcsStateComponent` - 状态管理组件（继承 StateTreeComponent）
- `UTcsStateManagerSubsystem` - 全局状态管理
- `UTcsStateDefinitionAsset` - 状态定义（UPrimaryDataAsset）
- `UTcsStateSlotDefinitionAsset` - 状态槽定义（UPrimaryDataAsset）
- `UTcsStateInstance` - 状态实例（携带 SourceHandle）
- `FTcsStateSlot` - 状态槽位

**状态类型** (`ETcsStateType`): State、Skill、Buff

**状态阶段** (`ETcsStateStage`): Inactive → Active → HangUp/Pause → Expired

**参数系统** (`StateParameter/`):
- 三种类型：Numeric (Float)、Bool、Vector
- `UTcsStateParameter_ConstNumeric` - 常数参数
- `UTcsStateParameter_InstigatorLevelArray/Table` - 根据施法者等级查询
- `UTcsStateParameter_StateLevelArray/Table` - 根据状态等级查询

**状态条件** (`StateCondition/`):
- `UTcsStateCondition_AttributeComparison` - 属性比较
- `UTcsStateCondition_ParameterBased` - 参数基础条件

**合并策略** (`StateMerger/`):
- `UTcsStateMerger_NoMerge` - 不合并，可并存
- `UTcsStateMerger_UseNewest` / `UseOldest` - 使用最新/最旧
- `UTcsStateMerger_Stack` - 叠加合并

**同优先级策略** (`SamePriorityPolicy/`):
- `UTcsStateSamePriorityPolicy` - 基类
- `UTcsStateSamePriorityPolicy_UseNewest` - 使用最新
- `UTcsStateSamePriorityPolicy_UseOldest` - 使用最旧

**状态移除流程**:
```
RequestStateRemoval → FinalizePendingRemovalRequest → FinalizeStateRemoval
  Step 1: Stop StateTree
  Step 2: Mark Expired
  Step 3: Remove from containers
  Step 3.5: 清理 SourceHandle 关联的 Modifier ← SourceHandle 自动清理
  Step 4: Broadcast events
  Step 5: Slot cleanup
  Step 6: MarkPendingGC
```

---

### 3️⃣ 技能系统 (Skill System) - 95%

**职责**: 管理角色学习和释放的技能

**核心类**:
- `UTcsSkillComponent` - 技能管理组件
- `UTcsSkillInstance` - 技能实例（已学会的技能）
- `UTcsSkillManagerSubsystem` - 全局技能管理

**技能修正系统**:

修正条件 (`Modifiers/Conditions/`):
- `UTcsSkillModCond_AlwaysTrue` - 总是真
- `UTcsSkillModCond_SkillHasTags` - 拥有标签
- `UTcsSkillModCond_SkillLevelInRange` - 等级在范围

修正执行 (`Modifiers/Executions/`):
- `UTcsSkillModExec_AdditiveParam` - 参数加法
- `UTcsSkillModExec_MultiplicativeParam` - 参数乘法
- `UTcsSkillModExec_CooldownMultiplier` - 冷却修正
- `UTcsSkillModExec_CostMultiplier` - 消耗修正

修正过滤器 (`Modifiers/Filters/`):
- `UTcsSkillFilter_ByDefIds` - 按ID过滤
- `UTcsSkillFilter_ByQuery` - 按查询过滤

修正合并 (`Modifiers/Mergers/`):
- `UTcsSkillModMerger_NoMerge` - 不合并
- `UTcsSkillModMerger_CombineByParam` - 按参数合并

---

### 4️⃣ StateTree 集成 - 85%

**专用节点**:
- `UTcsStateTreeSchema_StateInstance` - StateInstance 专用 Schema
- `FTcsStateChangeNotifyTask` - 状态变化通知 Task
- `FTcsStateSlotDebugEvaluator` - 状态槽调试 Evaluator
- `FTcsStateRemovalConfirmTask` - 状态移除确认 Task

---

## 命名规范

### 类命名

所有 TCS 类使用 `Tcs` 前缀：

- **组件**: `UTcs*Component`（如 `UTcsAttributeComponent`）
- **子系统**: `UTcs*Subsystem`（如 `UTcsStateManagerSubsystem`）
- **实例**: `UTcs*Instance` 或 `FTcs*Instance`
- **策略**: `UTcs*Execution`、`UTcs*Merger`、`UTcs*Strategy` 等
- **定义资产**: `UTcs*DefinitionAsset`（如 `UTcsStateDefinitionAsset`）
- **接口**: `ITcsEntityInterface`

### 文件命名

- 头文件: `Tcs*.h`
- 源文件: `Tcs*.cpp`
- 蓝图类: `BP_Tcs*`

### 枚举和结构体

```cpp
enum class ETcs*          // 枚举前缀
struct FTcs*              // 结构体前缀
class UTcs*               // UObject 前缀
```

### GameplayTag 约定

```
StateSlot.Action       // 行动状态槽
StateSlot.Buff         // 增益状态槽
StateSlot.Debuff       // 减益状态槽
StateSlot.Mobility     // 移动状态槽

State.Type.Skill       // 技能状态
State.Type.Buff        // 增益状态
State.Type.Debuff      // 减益状态
```

---

## 目录结构

```
TireflyCombatSystem/
├── Source/TireflyCombatSystem/
│   ├── Public/
│   │   ├── Attribute/                    # 属性系统
│   │   │   ├── AttrModExecution/        # 执行算法
│   │   │   ├── AttrModMerger/           # 合并策略
│   │   │   ├── AttrClampStrategy/       # Clamp 策略
│   │   │   ├── TcsAttribute.h
│   │   │   ├── TcsAttributeComponent.h
│   │   │   ├── TcsAttributeModifier.h
│   │   │   ├── TcsAttributeDefinitionAsset.h
│   │   │   ├── TcsAttributeModifierDefinitionAsset.h
│   │   │   ├── TcsAttributeManagerSubsystem.h
│   │   │   └── TcsAttributeChangeEventPayload.h
│   │   │
│   │   ├── State/                       # 状态系统
│   │   │   ├── StateCondition/          # 条件检查
│   │   │   ├── StateMerger/             # 合并策略
│   │   │   ├── StateParameter/          # 参数解析
│   │   │   ├── SamePriorityPolicy/      # 同优先级策略
│   │   │   ├── TcsState.h
│   │   │   ├── TcsStateComponent.h
│   │   │   ├── TcsStateSlot.h
│   │   │   ├── TcsStateManagerSubsystem.h
│   │   │   ├── TcsStateDefinitionAsset.h
│   │   │   └── TcsStateSlotDefinitionAsset.h
│   │   │
│   │   ├── Skill/                       # 技能系统
│   │   │   ├── Modifiers/
│   │   │   │   ├── Conditions/          # 修正条件
│   │   │   │   ├── Executions/          # 修正执行
│   │   │   │   ├── Filters/             # 过滤器
│   │   │   │   └── Mergers/             # 修正合并
│   │   │   ├── TcsSkillComponent.h
│   │   │   ├── TcsSkillInstance.h
│   │   │   └── TcsSkillManagerSubsystem.h
│   │   │
│   │   ├── StateTree/                   # StateTree 集成
│   │   │   ├── TcsStateChangeNotifyTask.h
│   │   │   ├── TcsStateSlotDebugEvaluator.h
│   │   │   ├── TcsStateRemovalConfirmTask.h
│   │   │   └── TcsStateTreeSchema_StateInstance.h
│   │   │
│   │   ├── TcsEntityInterface.h          # 战斗实体接口
│   │   ├── TcsSourceHandle.h             # SourceHandle 结构体
│   │   ├── TcsGenericEnum.h              # 枚举定义
│   │   ├── TcsGenericLibrary.h           # 通用库
│   │   ├── TcsGenericMacro.h             # 宏定义
│   │   ├── TcsGameplayTags.h             # GameplayTag 定义
│   │   ├── TcsDeveloperSettings.h        # 开发者设置
│   │   └── TcsLogChannels.h              # 日志通道
│   │
│   └── Private/                         # 实现文件（镜像 Public 结构）
│
├── Source/TireflyCombatSystemTests/      # 测试代码
├── Documents/                           # 技术文档和调研
├── Config/DefaultTireflyCombatSystem.ini
└── TireflyCombatSystem.uplugin
```

---

## 关键接口和类

### 战斗实体接口

```cpp
class ITcsEntityInterface : public IInterface
{
    virtual UTcsAttributeComponent* GetAttributeComponent() const;
    virtual UTcsStateComponent* GetStateComponent() const;
    virtual UTcsSkillComponent* GetSkillComponent() const;
    virtual ETcsCombatEntityType GetCombatEntityType() const;
    virtual int32 GetCombatEntityLevel() const;
};
```

### 数据结构速览

| 结构体 | 说明 |
|--------|------|
| `FTcsAttribute` | 属性定义 |
| `FTcsAttributeInstance` | 属性实例数据 |
| `FTcsAttributeModifierInstance` | 属性修改器实例 |
| `FTcsSourceHandle` | 来源句柄（Id + CausalityChain + Instigator + SourceTags） |
| `FTcsStateSlot` | 状态槽位 |
| `FTcsStateRemovalRequest` | 状态移除请求 |
| `FTcsAttributeChangeEventPayload` | 属性变化事件载荷 |
| `FTcsAttributeClampContext` | 属性 Clamp 上下文 |

### 枚举速览

| 枚举 | 说明 | 值 |
|-----|------|-----|
| `ETcsStateType` | 状态类型 | State, Skill, Buff |
| `ETcsStateStage` | 状态阶段 | Inactive, Active, HangUp, Pause, Expired |
| `ETcsStateDurationType` | 持续时间类型 | None, Duration, Infinite |
| `ETcsStateApplyFailReason` | 应用失败原因 | InvalidInput, NoStateComponent, ... |
| `ETcsStateParameterKeyType` | 参数键类型 | ... |
| `ETcsAttributeModifierMode` | 修改器模式 | BaseValue, CurrentValue |

---

## 重要设计决策

### 1. 为什么 "一切皆状态"？

技能、Buff、状态使用统一的 `UTcsStateDefinitionAsset` 和 `UTcsStateInstance`。
- 架构统一，减少重复代码
- 扩展方便，新增类型无需修改核心
- 所有行为都遵循同一套规则

### 2. 为什么 StateTree 双层架构？

StateTree 既管理槽位，又执行逻辑。
- 可视化编辑，状态关系图形配置
- 静动结合，静态结构 + 动态实例
- 零代码配置，策划直接编辑 StateTree

### 3. 为什么分离 SkillInstance 和 StateInstance？

技能实例和状态实例分开管理。
- 职责清晰：学会状态 vs 执行状态
- 性能优化：学会的技能持久，运行的状态动态
- 数据完整：技能等级、冷却与执行状态解耦

### 4. 为什么采用策略模式？

所有算法通过 CDO 策略类实现。
- 零代码扩展，创建新类无需修改引擎
- 数据驱动，编辑器选择不同策略
- 易于测试，每个策略独立可测

### 5. 为什么 SourceHandle 不存储 SourceDefinition？

各实例（Modifier、State、Skill）自身已持有定义引用。
- 避免职责重复
- 减少 SourceHandle 体积
- 因果链用 FPrimaryAssetId 天然兼容所有 UPrimaryDataAsset 子类

---

## 依赖关系

### 引擎模块
- `StateTreeModule` - StateTree 核心
- `GameplayStateTreeModule` - StateTree 游戏扩展
- `GameplayTags` - GameplayTag 系统
- `GameplayMessageRuntime` - 消息路由

### 项目插件
- `TireflyObjectPool` - 对象池系统

---
