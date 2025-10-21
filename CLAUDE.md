# TireflyCombatSystem (TCS) 插件架构文档

> **TireflyCombatSystem** 是为 UE5 设计的高度模块化战斗系统框架。
> 核心理念："一切皆状态"，提供统一的属性、状态、技能管理方案。

**版本**: 1.0 (UE5) | **状态**: Beta

## 📊 完成度速览

| 模块 | 完成度 | 说明 |
|------|--------|------|
| 属性系统 (Attribute) | 90% | 核心功能完备，缺少高级优化 |
| 状态系统 (State) | 85% | 核心架构完成，StateTree集成进行中 |
| 技能系统 (Skill) | 95% | 基本功能完整，缺少少量高级特性 |
| StateTree集成 | 80% | 基础集成完成，专用节点开发中 |

---

## 核心设计理念

- **统一状态管理**: 技能、Buff、状态使用同一套 `FTcsStateDefinition` 和 `UTcsStateInstance`
- **StateTree双层架构**: 静态槽位结构 + 动态实例执行，支持可视化编辑
- **策略模式**: 通过CDO实现零代码扩展，所有算法都可继承和定制
- **数据驱动**: 数据表驱动的配置，减少硬编码
- **高性能设计**: 对象池、批量更新、智能缓存机制

---

## 架构概览

### 一切皆状态

```
战斗实体 (Actor)
├─ 状态 (State)  - 被击退、冰冻等
├─ 技能 (Skill)  - 攻击、法术等
└─ Buff (Buff)   - 增益、减益等
     ↓ 全部使用同一套系统管理
FTcsStateDefinition + UTcsStateInstance + StateTree
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
| 状态条件 | `UTcsStateCondition` | AttributeComparison、ParameterBased |
| 状态合并 | `UTcsStateMerger` | NoMerge、Stack、UseNewest、UseOldest |
| 技能修正 | `UTcsSkillModifierExecution` | AdditiveParam、CooldownMultiplier、CostMultiplier |

---

## 目录结构

```
TireflyCombatSystem/
├── Source/TireflyCombatSystem/
│   ├── Public/
│   │   ├── Attribute/                    # 属性系统 (90%)
│   │   │   ├── AttrModExecution/        # 执行算法
│   │   │   ├── AttrModMerger/           # 合并策略
│   │   │   ├── TcsAttribute.h
│   │   │   ├── TcsAttributeComponent.h
│   │   │   └── TcsAttributeModifier.h
│   │   │
│   │   ├── State/                       # 状态系统 (85%)
│   │   │   ├── StateCondition/          # 条件检查
│   │   │   ├── StateMerger/             # 合并策略
│   │   │   ├── StateParameter/          # 参数解析
│   │   │   ├── TcsState.h
│   │   │   ├── TcsStateComponent.h
│   │   │   └── TcsStateManagerSubsystem.h
│   │   │
│   │   ├── Skill/                       # 技能系统 (95%)
│   │   │   ├── Modifiers/
│   │   │   │   ├── Conditions/          # 修正条件
│   │   │   │   ├── Executions/          # 修正执行
│   │   │   │   ├── Filters/             # 过滤器
│   │   │   │   └── Mergers/             # 修正合并
│   │   │   ├── TcsSkillComponent.h
│   │   │   ├── TcsSkillInstance.h
│   │   │   └── TcsSkillManagerSubsystem.h
│   │   │
│   │   ├── StateTree/                   # StateTree集成 (80%)
│   │   │   ├── TcsStateChangeNotifyTask.h
│   │   │   ├── TcsStateSlotDebugEvaluator.h
│   │   │   └── TcsStateTreeSchema_StateInstance.h
│   │   │
│   │   ├── TcsEntityInterface.h          # 战斗实体接口
│   │   ├── TcsGenericEnum.h              # 枚举定义
│   │   ├── TcsGenericLibrary.h           # 通用库
│   │   ├── TcsGenericMacro.h             # 宏定义
│   │   ├── TcsDeveloperSettings.h        # 开发者设置
│   │   └── TcsLogChannels.h              # 日志通道
│   │
│   └── Private/                         # 实现文件
│
├── Config/DefaultTireflyCombatSystem.ini
├── TireflyCombatSystem.uplugin
└── CLAUDE.md (本文档)
```

---

## 三大系统详解

### 1️⃣ 属性系统 (Attribute System) - 90%

**职责**: 管理所有数值属性（生命值、攻击力、防御力等）

**核心类**:
- `UTcsAttributeComponent` - 属性管理组件
- `FTcsAttribute` - 属性定义
- `FTcsAttributeInstance` - 属性实例
- `FTcsAttributeModifierInstance` - 修改器实例

**执行流程**:
```
添加属性 → 应用修改器 → 执行算法 → 合并结果 → 计算最终值 → 触发事件
```

**执行算法** (`AttrModExecution/`):
- `UTcsAttrModExec_Addition` - 加法
- `UTcsAttrModExec_MultiplyAdditive` - 乘法加法
- `UTcsAttrModExec_MultiplyContinued` - 连续乘法

**合并策略** (`AttrModMerger/`):
- `UTcsAttrModMerger_NoMerge` - 全部应用
- `UTcsAttrModMerger_UseNewest` - 使用最新
- `UTcsAttrModMerger_UseOldest` - 使用最旧
- `UTcsAttrModMerger_UseMaximum` - 取最大值
- `UTcsAttrModMerger_UseMinimum` - 取最小值
- `UTcsAttrModMerger_UseAdditiveSum` - 加法求和

---

### 2️⃣ 状态系统 (State System) - 85%

**职责**: 管理所有状态（技能、Buff、普通状态等）

**核心类**:
- `UTcsStateComponent` - 状态管理组件（继承 StateTreeComponent）
- `UTcsStateInstance` - 状态实例
- `FTcsStateDefinition` - 状态定义（TableRowBase）
- `UTcsStateManagerSubsystem` - 全局状态管理
- `UTcsStateSlot` - 状态槽位

**状态类型** (`ETcsStateType`):
```cpp
ST_State = 0    // 普通状态
ST_Skill        // 技能
ST_Buff         // Buff效果
```

**状态阶段** (`ETcsStateStage`):
```cpp
SS_Inactive = 0 // 未激活
SS_Active       // 已激活
SS_HangUp       // 挂起
SS_Expired      // 已过期
```

**参数系统** (`StateParameter/`):
- 支持三种类型：Numeric (Float)、Bool、Vector
- `UTcsStateParameter_ConstNumeric` - 常数参数
- `UTcsStateParameter_InstigatorLevelArray` - 根据施法者等级查询
- `UTcsStateParameter_InstigatorLevelTable` - 根据施法者等级查表
- `UTcsStateParameter_StateLevelArray` - 根据状态等级查询
- `UTcsStateParameter_StateLevelTable` - 根据状态等级查表

**状态条件** (`StateCondition/`):
- `UTcsStateCondition_AttributeComparison` - 属性比较
- `UTcsStateCondition_ParameterBased` - 参数基础条件

**合并策略** (`StateMerger/`):
- `UTcsStateMerger_NoMerge` - 不合并，可并存
- `UTcsStateMerger_UseNewest` - 使用最新
- `UTcsStateMerger_UseOldest` - 使用最旧
- `UTcsStateMerger_Stack` - 叠加合并

---

### 3️⃣ 技能系统 (Skill System) - 95%

**职责**: 管理角色学习和释放的技能

**核心类**:
- `UTcsSkillComponent` - 技能管理组件
- `UTcsSkillInstance` - 技能实例（已学会的技能）
- `UTcsSkillManagerSubsystem` - 全局技能管理

**核心概念**:
- **SkillInstance**: 代表角色已学会的技能，存储等级、冷却等信息
- **StateInstance**: 技能释放时动态创建，在目标上执行
- **参数同步**: 快照参数 vs 实时参数的性能优化

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

## 命名规范

### 类命名

所有TCS类使用 `Tcs` 前缀：

- **组件**: `UTcs*Component`（如 `UTcsAttributeComponent`）
- **子系统**: `UTcs*Subsystem`（如 `UTcsStateManagerSubsystem`）
- **实例**: `UTcs*Instance` 或 `FTcs*Instance`
- **策略**: `UTcs*Execution`、`UTcs*Merger` 等
- **接口**: `ITcsEntityInterface`

### 文件命名

- 头文件: `Tcs*.h`
- 源文件: `Tcs*.cpp`
- 蓝图类: `BP_Tcs*`
- 数据表: `DT_Tcs*`

### 枚举和结构体

```cpp
enum class ETcs*          // 枚举前缀
struct FTcs*              // 结构体前缀
struct UObject : UTcs*    // 对象前缀
```

### GameplayTag约定

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

## 关键接口和类

### 战斗实体接口

```cpp
class ITcsEntityInterface : public IInterface
{
    // 获取属性组件
    virtual UTcsAttributeComponent* GetAttributeComponent() const;
    // 获取状态组件
    virtual UTcsStateComponent* GetStateComponent() const;
    // 获取技能组件
    virtual UTcsSkillComponent* GetSkillComponent() const;
    // 获取战斗实体类型
    virtual ETcsCombatEntityType GetCombatEntityType() const;
    // 获取战斗实体等级
    virtual int32 GetCombatEntityLevel() const;
};
```

### 数据结构速览

| 结构体 | 说明 |
|--------|------|
| `FTcsAttribute` | 属性定义 |
| `FTcsAttributeInstance` | 属性实例数据 |
| `FTcsAttributeModifierInstance` | 属性修改器实例 |
| `FTcsStateDefinition` | 状态定义（继承 FTableRowBase） |
| `FTcsStateDurationData` | 状态持续时间数据 |
| `FTcsStateApplyResult` | 状态应用结果 |
| `FTcsSkillModifierEffect` | 技能修正效果 |

### 枚举速览

| 枚举 | 说明 | 值 |
|-----|------|-----|
| `ETcsStateType` | 状态类型 | State, Skill, Buff |
| `ETcsStateStage` | 状态阶段 | Inactive, Active, HangUp, Expired |
| `ETcsStateParameterType` | 参数类型 | Numeric, Bool, Vector |
| `ETcsNumericComparison` | 数值比较 | Equal, NotEqual, >, >=, <, <= |
| `ETcsAttributeCheckTarget` | 属性检查目标 | Owner, Instigator |

---

## 重要设计决策

### 1. 为什么 "一切皆状态"？

**决策**: 技能、Buff、状态使用统一的定义和实例

**好处**:
- ✅ 架构统一，减少重复代码
- ✅ 扩展方便，新增类型无需修改核心
- ✅ 系统灵活，所有行为都遵循同一套规则

---

### 2. 为什么 StateTree 双层架构？

**决策**: StateTree既管理槽位，又执行逻辑

**优势**:
- ✅ 可视化编辑，状态关系图形配置
- ✅ 静动结合，静态结构 + 动态实例
- ✅ 零代码配置，策划直接编辑StateTree

---

### 3. 为什么分离 SkillInstance 和 StateInstance？

**决策**: 技能实例和状态实例分开管理

**原因**:
- ✅ 职责清晰，学会状态 vs 执行状态
- ✅ 性能优化，学会的技能持久，运行的状态动态
- ✅ 数据完整，技能等级、冷却与执行状态解耦

---

### 4. 为什么采用策略模式？

**决策**: 所有算法通过CDO策略类实现

**收益**:
- ✅ 零代码扩展，创建新类无需修改引擎
- ✅ 数据驱动，编辑器选择不同策略
- ✅ 易于测试，每个策略独立可测

---

## 文件位置索引

### 核心接口
- `Source/TireflyCombatSystem/Public/TcsEntityInterface.h` - 战斗实体接口

### 属性系统
- `Source/TireflyCombatSystem/Public/Attribute/TcsAttributeComponent.h`
- `Source/TireflyCombatSystem/Public/Attribute/TcsAttribute.h`
- `Source/TireflyCombatSystem/Public/Attribute/TcsAttributeModifier.h`
- `Source/TireflyCombatSystem/Public/Attribute/AttrModExecution/`
- `Source/TireflyCombatSystem/Public/Attribute/AttrModMerger/`

### 状态系统
- `Source/TireflyCombatSystem/Public/State/TcsStateComponent.h`
- `Source/TireflyCombatSystem/Public/State/TcsState.h`
- `Source/TireflyCombatSystem/Public/State/TcsStateSlot.h`
- `Source/TireflyCombatSystem/Public/State/TcsStateManagerSubsystem.h`
- `Source/TireflyCombatSystem/Public/State/StateCondition/`
- `Source/TireflyCombatSystem/Public/State/StateMerger/`
- `Source/TireflyCombatSystem/Public/State/StateParameter/`

### 技能系统
- `Source/TireflyCombatSystem/Public/Skill/TcsSkillComponent.h`
- `Source/TireflyCombatSystem/Public/Skill/TcsSkillInstance.h`
- `Source/TireflyCombatSystem/Public/Skill/TcsSkillManagerSubsystem.h`
- `Source/TireflyCombatSystem/Public/Skill/Modifiers/`

### StateTree集成
- `Source/TireflyCombatSystem/Public/StateTree/TcsStateChangeNotifyTask.h`
- `Source/TireflyCombatSystem/Public/StateTree/TcsStateTreeSchema_StateInstance.h`

### 枚举与配置
- `Source/TireflyCombatSystem/Public/TcsGenericEnum.h` - 所有枚举定义
- `Source/TireflyCombatSystem/Public/TcsGenericMacro.h` - 宏定义
- `Source/TireflyCombatSystem/Public/TcsDeveloperSettings.h` - 开发者设置
- `Source/TireflyCombatSystem/Public/TcsLogChannels.h` - 日志通道

### 插件配置
- `TireflyCombatSystem.uplugin` - 插件清单
- `Config/DefaultTireflyCombatSystem.ini` - 默认配置

---

## 依赖关系

### 引擎模块
- `StateTreeModule` - StateTree核心
- `GameplayStateTreeModule` - StateTree游戏扩展
- `GameplayTags` - GameplayTag系统
- `GameplayMessageRuntime` - 消息路由

### 项目插件
- `TireflyObjectPool` - 对象池系统

---

**最后更新**: 2025年10月

**相关文档**: 项目根目录 CLAUDE.md 包含整体项目指导