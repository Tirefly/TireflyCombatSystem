# Project Context

## Purpose
TireflyCombatSystem (TCS) 是为 Unreal Engine 5 设计的高度模块化战斗系统框架插件。

**核心理念**: "一切皆状态" - 提供统一的属性、状态、技能管理方案。

**主要目标**:
- 提供统一的战斗系统架构，技能、Buff、状态使用同一套定义和实例系统
- 支持 StateTree 可视化编辑，实现零代码配置
- 通过策略模式实现高度可扩展的算法系统
- 数据驱动的配置方式，减少硬编码
- 高性能设计：对象池、批量更新、智能缓存机制

**当前状态**: Beta 版本 1.0

## Tech Stack
- **引擎**: Unreal Engine 5 (UE5)
- **语言**: C++ (UE5 标准)
- **核心模块**:
  - StateTreeModule - StateTree 核心系统
  - GameplayStateTreeModule - StateTree 游戏扩展
  - GameplayTags - GameplayTag 系统
  - GameplayMessageRuntime - 消息路由系统
- **项目依赖插件**:
  - TireflyObjectPool - 对象池系统
- **开发环境**: JetBrains Rider
- **版本控制**: Git

## Project Conventions

### Code Style

**文件头部规范**:

所有代码文件必须以以下格式开头：
```cpp
// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
// ... 其他 includes
#include "FileName.generated.h"  // 如果是 UObject 相关文件


// 前置声明或代码内容
```

**空行缩进规范**:

- **文件头部**:
  - Copyright 声明后: 1 个空行
  - `#pragma once` 后: 1 个空行
  - Include 块后: 3 个空行

- **声明之间**:
  - 枚举/结构体/类声明之间: 3 个空行
  - 前置声明块和枚举/结构体之间: 3 个空行
  - 委托声明之间: 1 个空行
  - 委托声明块和类声明之间: 3 个空行

- **函数之间** (.cpp 文件):
  - 函数实现之间: 0 个空行 (函数直接相邻)
  - 函数体内部: 根据逻辑分组适当使用空行

**#pragma region 使用规范**:

用于组织代码的逻辑分区，常见分区包括：
- `#pragma region ActorComponent` - Actor 组件相关
- `#pragma region Attribute` - 属性相关
- `#pragma region State` - 状态相关
- `#pragma region Skill` - 技能相关

格式规范：
```cpp
#pragma region RegionName

public:
	// 区域内容

#pragma endregion


#pragma region NextRegion
```

空行规则：
- `#pragma region` 前: 0 个空行
- `#pragma region` 后: 1 个空行
- `#pragma endregion` 前: 0 个空行
- `#pragma endregion` 后: 2 个空行

**缩进规范**:
- 使用 **Tab** 缩进，不使用空格
- 所有代码块、函数参数、枚举值等都使用 Tab 对齐
- 长函数参数换行时，每个参数独占一行并使用 Tab 缩进

**命名规范**:

所有 TCS 类使用 `Tcs` 前缀：
- **组件**: `UTcs*Component` (如 `UTcsAttributeComponent`)
- **子系统**: `UTcs*Subsystem` (如 `UTcsStateManagerSubsystem`)
- **实例**: `UTcs*Instance` 或 `FTcs*Instance`
- **策略类**: `UTcs*Execution`、`UTcs*Merger`、`UTcs*Condition` 等
- **接口**: `ITcsEntityInterface`

**文件命名**:
- 头文件: `Tcs*.h`
- 源文件: `Tcs*.cpp`
- 蓝图类: `BP_Tcs*`
- 数据表: `DT_Tcs*`

**枚举和结构体**:
```cpp
enum class ETcs*          // 枚举前缀
struct FTcs*              // 结构体前缀
class UTcs*               // UObject 派生类前缀
```

**枚举值命名规范**:

枚举值使用**缩写前缀 + 驼峰命名**的方式：
- 前缀通常为枚举类型名称的缩写（去掉 ETcs 前缀）
- 使用下划线分隔前缀和名称
- 示例：
  - `ETcsStateType` → `ST_State`, `ST_Skill`, `ST_Buff`
  - `ETcsStateDurationType` → `SDT_None`, `SDT_Duration`, `SDT_Infinite`
  - `ETcsStateStage` → `SS_Inactive`, `SS_Active`, `SS_HangUp`

缩写前缀规则：
- 提取类型名中每个单词的首字母
- 如：StateType → ST, StateDurationType → SDT, StateStage → SS
- 特殊情况下可使用更具描述性的缩写

**枚举值格式**:
```cpp
UENUM(BlueprintType)
enum class ETcsStateType : uint8
{
	ST_State = 0	UMETA(DisplayName = "State", ToolTip = "状态"),
	ST_Skill		UMETA(DisplayName = "Skill", ToolTip = "技能"),
	ST_Buff			UMETA(DisplayName = "Buff", ToolTip = "BUFF效果"),
};
```
- 使用 Tab 对齐枚举值和 UMETA
- ToolTip 使用中文说明

**注释规范**:
- 必须为以下内容添加注释：
  - 类、结构体、枚举的声明
  - 类和结构体的成员变量和成员函数
  - 枚举的具体值
  - 超过 2 个参数的函数需要为每个参数添加注释
  - 有返回值的函数需要说明返回值含义
  - 委托的声明
- 不要为所有代码或每一行代码添加注释
- 注释使用中文，简洁明了

### Architecture Patterns

**1. "一切皆状态" 统一架构**
- 技能、Buff、普通状态使用同一套 `FTcsStateDefinition` 和 `UTcsStateInstance`
- 统一的生命周期管理和事件系统
- 减少重复代码，提高系统一致性

**2. StateTree 双层架构**
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

**3. 策略模式 (Strategy Pattern)**
- 所有算法通过 CDO (Class Default Object) 策略类实现
- 零代码扩展：创建新策略类无需修改核心引擎
- 数据驱动：在编辑器中选择不同策略
- 易于测试：每个策略独立可测

**策略扩展点**:
| 算法类型 | 基类 | 示例实现 |
|---------|------|---------|
| 属性执行 | `UTcsAttributeModifierExecution` | Add、Multiply、MultiplyAdditive |
| 属性合并 | `UTcsAttributeModifierMerger` | NoMerge、UseNewest、UseMaximum、UseAdditiveSum |
| 状态条件 | `UTcsStateCondition` | AttributeComparison、ParameterBased |
| 状态合并 | `UTcsStateMerger` | NoMerge、Stack、UseNewest、UseOldest |
| 技能修正 | `UTcsSkillModifierExecution` | AdditiveParam、CooldownMultiplier、CostMultiplier |

**4. 组件化设计**
- `UTcsAttributeComponent` - 属性管理
- `UTcsStateComponent` - 状态管理（继承 StateTreeComponent）
- `UTcsSkillComponent` - 技能管理
- 通过 `ITcsEntityInterface` 接口统一访问

**5. 数据驱动配置**
- 状态定义通过 DataTable (`FTcsStateDefinition : FTableRowBase`)
- 参数系统支持常数、等级查询、数据表查询
- GameplayTag 驱动的槽位和类型系统

### Testing Strategy

**编译验证**:
- 使用 UnrealBuildTool 进行编译验证
- 支持多种配置：Debug、DebugGame、Development、Shipping
- 支持 Game Target 和 Editor Target 编译

**测试模块**:
- 项目包含测试模块用于验证核心功能
- 重点测试：属性计算、状态合并、技能修正等核心算法

**性能测试**:
- 对象池性能验证
- 批量更新性能测试
- 大量状态实例并发测试

### Git Workflow

**分支策略**:
- 主分支: `master`
- 所有 PR 合并到 `master` 分支

**提交规范**:
- 使用中文提交信息
- 提交格式示例：
  - `【MOD】修复部分程序逻辑问题`
  - `【MOD】修改事件GameplayTag`
  - `【MOD】State外部移除改为向StateMgr和StateTree发送申请事件`

**提交类型标记**:
- `【MOD】` - 修改/优化
- `【ADD】` - 新增功能
- `【FIX】` - 修复问题
- `【DEL】` - 删除功能

## Domain Context

**战斗系统核心概念**:

**1. 三大系统**:
- **属性系统 (Attribute System)**: 管理所有数值属性（生命值、攻击力、防御力等）
- **状态系统 (State System)**: 管理所有状态（技能、Buff、普通状态等）
- **技能系统 (Skill System)**: 管理角色学习和释放的技能

**2. 状态类型** (`ETcsStateType`):
- `ST_State` - 普通状态（如被击退、冰冻）
- `ST_Skill` - 技能状态（如攻击、法术）
- `ST_Buff` - Buff效果（如增益、减益）

**3. 状态阶段** (`ETcsStateStage`):
- `SS_Inactive` - 未激活
- `SS_Active` - 已激活
- `SS_HangUp` - 挂起
- `SS_Expired` - 已过期

**4. GameplayTag 约定**:
```
StateSlot.Action       // 行动状态槽
StateSlot.Buff         // 增益状态槽
StateSlot.Debuff       // 减益状态槽
StateSlot.Mobility     // 移动状态槽

State.Type.Skill       // 技能状态
State.Type.Buff        // 增益状态
State.Type.Debuff      // 减益状态
```

**5. 关键设计决策**:
- **SkillInstance vs StateInstance**: 技能实例代表已学会的技能（持久），状态实例代表技能释放时的执行状态（动态）
- **参数系统**: 支持快照参数和实时参数，优化性能
- **合并策略**: 不同的状态可以选择不同的合并策略（不合并、叠加、使用最新等）

**6. 战斗实体接口** (`ITcsEntityInterface`):
所有参与战斗的 Actor 都应实现此接口，提供：
- 获取属性组件
- 获取状态组件
- 获取技能组件
- 获取战斗实体类型和等级

## Important Constraints

**技术约束**:
- 必须兼容 Unreal Engine 5
- 必须遵循 UE5 的对象生命周期管理（UObject、GC等）
- 必须使用 UE5 的反射系统（UCLASS、UPROPERTY、UFUNCTION）
- StateTree 集成必须遵循 StateTreeModule 的规范

**性能约束**:
- 使用对象池减少频繁的内存分配
- 批量更新机制减少单帧开销
- 智能缓存机制避免重复计算
- 避免在 Tick 中进行大量计算

**设计约束**:
- 保持"一切皆状态"的核心理念
- 所有算法必须通过策略模式扩展，不能硬编码
- 数据驱动优先，减少代码配置
- 保持组件的独立性和可组合性

**兼容性约束**:
- 插件必须能够独立工作，不依赖特定项目
- 必须与 TireflyObjectPool 插件协同工作
- 必须支持蓝图和 C++ 双重使用方式

## External Dependencies

**Unreal Engine 模块**:
- `StateTreeModule` - StateTree 核心功能
- `GameplayStateTreeModule` - StateTree 游戏扩展
- `GameplayTags` - GameplayTag 系统，用于状态槽位和类型标识
- `GameplayMessageRuntime` - 消息路由系统，用于事件通知

**项目插件**:
- `TireflyObjectPool` - 对象池系统，用于状态实例的高效管理

**开发工具**:
- UnrealBuildTool - 项目编译工具
- JetBrains Rider - 推荐的 IDE

**数据依赖**:
- DataTable - 状态定义数据表 (`FTcsStateDefinition`)
- GameplayTag 配置 - 状态槽位和类型标签定义

**无外部服务依赖**: 本插件为纯客户端插件，不依赖任何外部服务或 API。
