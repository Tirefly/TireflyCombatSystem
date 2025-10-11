# TireflyCombatSystem (TCS) 插件开发指南

## 插件概述

TireflyCombatSystem是一个基于"数据-行为分离"设计理念的现代化UE5战斗系统插件，采用"一切皆状态"的统一架构思想，与StateTree深度集成，为复杂的战斗系统提供强大且灵活的解决方案。

### 核心设计理念

- **统一状态管理**: 技能、Buff、状态使用同一套`FTcsStateDefinition`和`UTcsStateInstance`
- **StateTree深度集成**: 战斗实体状态管理可视化编辑，Buff/技能逻辑通过StateTree执行
- **数据-行为分离**: 通过CDO策略模式和数据表驱动，保证高扩展性和零代码配置能力
- **对象池优化**: 深度集成TireflyObjectPool，确保高性能运行

## 目录结构

```
TireflyCombatSystem/
├── Source/TireflyCombatSystem/
│   ├── Public/
│   │   ├── Attribute/                    # 属性系统 (90%完成)
│   │   │   ├── AttrModExecution/        # 属性修改器执行算法
│   │   │   ├── AttrModMerger/           # 属性修改器合并算法
│   │   │   ├── TcsAttribute.h           # 属性定义和管理
│   │   │   ├── TcsAttributeComponent.h   # 属性组件
│   │   │   └── TcsAttributeModifier.h    # 属性修改器系统
│   │   ├── State/                       # 状态系统 (85%完成)
│   │   │   ├── StateCondition/          # 状态条件检查器
│   │   │   ├── StateMerger/             # 状态合并策略
│   │   │   ├── StateParameter/          # 状态参数解析器
│   │   │   ├── TcsState.h               # 状态定义和实例
│   │   │   ├── TcsStateComponent.h      # 状态管理组件
│   │   │   └── TcsStateManagerSubsystem.h # 状态管理子系统
│   │   ├── Skill/                       # 技能系统 (95%完成)
│   │   │   ├── TcsSkillComponent.h      # 技能组件
│   │   │   ├── TcsSkillInstance.h       # 技能实例
│   │   │   └── TcsSkillManagerSubsystem.h # 技能管理子系统
│   │   ├── StateTree/                   # StateTree集成
│   │   │   ├── TcsCombatStateTreeSchema.h # StateTree模式定义
│   │   │   └── TcsCombatStateTreeTasks.h  # 战斗专用StateTree节点
│   │   ├── TcsCombatEntityInterface.h    # 战斗实体接口
│   │   ├── TcsCombatSystemEnum.h        # 插件枚举定义
│   │   └── TcsCombatSystemSettings.h    # 插件设置
│   └── Private/                         # 对应的实现文件
├── Resources/                           # 插件资源文件
├── Document/                            # 设计文档
└── README.md                           # 插件说明文档
```

## 核心架构模块

### 1. 属性系统 (Attribute System)

**完成度**: 90%

**核心文件**:
- `TcsAttribute.h` - 属性定义和基础数据结构
- `TcsAttributeComponent.h` - 属性管理组件
- `TcsAttributeModifier.h` - 属性修改器系统

**设计特色**:
- 基于CDO策略模式的执行算法和合并算法
- 支持静态/动态范围限制和公式计算
- 完整的事件驱动属性更新机制
- 支持UI配置和实时属性查询

**关键接口**:
```cpp
// 属性修改器执行策略基类
class TIREFLYCOMBATSYSTEM_API UTcsAttributeModifierExecution : public UObject

// 属性修改器合并策略基类  
class TIREFLYCOMBATSYSTEM_API UTcsAttributeModifierMerger : public UObject

// 属性组件
class TIREFLYCOMBATSYSTEM_API UTcsAttributeComponent : public UActorComponent
```

### 2. 状态系统 (State System)

**完成度**: 85%

**核心文件**:
- `TcsState.h` - 状态定义和状态实例
- `TcsStateComponent.h` - 状态管理组件
- `TcsStateManagerSubsystem.h` - 全局状态管理

**设计特色**:
- 统一的`FTcsStateDefinition`支持技能、Buff、状态
- 三种参数类型：Numeric(计算型)、Bool(静态型)、Vector(静态型)
- 智能参数快照机制：快照参数vs实时参数
- StateTree集成框架，支持可视化状态逻辑编辑

**状态槽系统设计**:
- **优先级驱动激活管理**: 基于状态优先级自动管理激活状态
- **多种激活模式**: PriorityOnly(互斥激活) 和 AllActive(并行激活)
- **状态槽配置**: 支持数据表驱动的槽位配置

**关键数据结构**:
```cpp
// 状态定义
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTcsStateDefinition : public FTableRowBase
{
    // StateTree资产引用
    UPROPERTY(EditAnywhere, Category = "StateTree")
    FStateTreeReference StateTreeRef;
    
    // 状态类型：技能/Buff/状态
    UPROPERTY(EditAnywhere, Category = "Meta")
    TEnumAsByte<ETcsStateType> StateType = ST_State;
    
    // 状态槽类型
    UPROPERTY(EditAnywhere, Category = "Slot")
    FGameplayTag StateSlotType;
    
    // 优先级 (数值越小优先级越高)
    UPROPERTY(EditAnywhere, Category = "Priority")
    int32 Priority = -1;
};
```

### 3. 技能系统 (Skill System)

**完成度**: 95%

**核心文件**:
- `TcsSkillInstance.h` - 技能实例管理
- `TcsSkillComponent.h` - 技能组件
- `TcsSkillManagerSubsystem.h` - 技能管理子系统

**设计特色**:
- **SkillInstance vs StateInstance分离**: 学会的技能vs运行中的状态
- **完整的技能生命周期**: 学习、升级、修正、激活、冷却
- **三种参数类型支持**: 与状态系统保持一致的参数架构
- **智能参数同步**: 快照参数和实时参数的性能优化

**技能激活流程**:
1. **SkillInstance**: 存储在SkillComponent中，记录等级、冷却、修正器
2. **StateInstance**: 激活时创建，应用到StateComponent中执行StateTree逻辑
3. **跨Actor vs 自Actor**: 技能StateTree在施法者身上执行，Buff StateTree在目标身上执行

**关键类**:
```cpp
// 技能实例 - 代表角色学会的技能
class TIREFLYCOMBATSYSTEM_API UTcsSkillInstance : public UObject

// 技能组件 - 管理角色的技能学习和激活
class TIREFLYCOMBATSYSTEM_API UTcsSkillComponent : public UActorComponent
```

### 4. StateTree集成系统

**核心文件**:
- `TcsCombatStateTreeSchema.h` - 战斗专用StateTree模式
- `TcsCombatStateTreeTasks.h` - 战斗专用StateTree节点

**StateTree双层架构设计**:

#### 第一层：StateTree作为状态槽管理器
- 管理预定义的状态槽和转换规则
- 每个状态槽可以容纳动态的状态实例
- 通过可视化编辑器配置状态关系

#### 第二层：动态状态实例管理
- StateInstance在槽位中动态创建、执行、销毁
- 每个StateInstance运行独立的StateTree执行具体逻辑
- 支持跨Actor状态应用（如Buff效果）

**专用StateTree节点**:
```cpp
// 状态应用节点
struct TIREFLYCOMBATSYSTEM_API FTcsStateTreeTask_ApplyState

// 属性修改节点  
struct TIREFLYCOMBATSYSTEM_API FTcsStateTreeTask_ModifyAttribute

// 属性比较条件节点
struct TIREFLYCOMBATSYSTEM_API FTcsStateTreeCondition_AttributeComparison
```

## 命名约定

### 类命名规范

**所有TCS插件内的类都使用`Tcs`前缀**，这是插件的统一标识：

- **组件类**: `UTcs*Component` (如 `UTcsAttributeComponent`, `UTcsStateComponent`)
- **子系统类**: `UTcs*Subsystem` (如 `UTcsStateManagerSubsystem`)
- **数据类**: `UTcs*` 或 `FTcs*` (如 `UTcsSkillInstance`, `FTcsStateDefinition`)
- **策略类**: `UTcs*` (如 `UTcsAttributeModifierExecution`)
- **接口类**: `UTcs*Interface` (如 `UTcsCombatEntityInterface`)

### 文件命名规范

- 头文件: `Tcs*.h`
- 源文件: `Tcs*.cpp`
- 蓝图类: `BP_Tcs*`
- 数据表: `DT_Tcs*`

### 枚举和结构体

```cpp
// 枚举使用ETcs前缀
enum class ETcsStateType : uint8
enum class ETcsAttributeCheckTarget : uint8
enum class ETcsNumericComparison : uint8

// 结构体使用FTcs前缀  
struct FTcsStateDefinition : public FTableRowBase
struct FTcsAttributeModifierInstance
```

### GameplayTag约定

```cpp
// 状态槽标签
"StateSlot.Action"      // 行动状态槽 (PriorityOnly模式)
"StateSlot.Buff"        // 增益状态槽 (AllActive模式)  
"StateSlot.Debuff"      // 减益状态槽 (AllActive模式)
"StateSlot.Mobility"    // 移动状态槽 (PriorityOnly模式)

// 状态类型标签
"State.Type.Skill"      // 技能状态
"State.Type.Buff"       // 增益状态
"State.Type.Debuff"     // 减益状态
```

## 开发指南

### 1. 创建新的属性修改器

```cpp
// 1. 继承执行策略基类
UCLASS(Meta = (DisplayName = "属性修改器执行器：自定义算法"))
class TIREFLYCOMBATSYSTEM_API UTcsAttrModExec_Custom : public UTcsAttributeModifierExecution
{
    GENERATED_BODY()

public:
    virtual void Execute_Implementation(
        const FTcsAttributeModifierInstance& ModInst,
        TMap<FName, float>& BaseValues,
        TMap<FName, float>& CurrentValues) override;
};

// 2. 继承合并策略基类
UCLASS(Meta = (DisplayName = "属性修改器合并器：自定义策略"))
class TIREFLYCOMBATSYSTEM_API UTcsAttrModMerger_Custom : public UTcsAttributeModifierMerger
{
    GENERATED_BODY()

public:
    virtual bool ShouldMerge_Implementation(
        const FTcsAttributeModifierInstance& ExistingMod,
        const FTcsAttributeModifierInstance& NewMod) const override;
        
    virtual FTcsAttributeModifierInstance Merge_Implementation(
        const FTcsAttributeModifierInstance& ExistingMod,
        const FTcsAttributeModifierInstance& NewMod) const override;
};
```

### 2. 创建新的状态合并策略

```cpp
UCLASS(Meta = (DisplayName = "状态合并器：自定义策略"))
class TIREFLYCOMBATSYSTEM_API UTcsStateMerger_Custom : public UTcsStateMerger
{
    GENERATED_BODY()

public:
    virtual bool ShouldMerge_Implementation(
        const UTcsStateInstance* ExistingState,
        const UTcsStateInstance* NewState) const override;
        
    virtual UTcsStateInstance* Merge_Implementation(
        UTcsStateInstance* ExistingState,
        UTcsStateInstance* NewState) const override;
};
```

### 3. 创建新的StateTree节点

```cpp
// StateTree任务节点
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTcsStateTreeTask_CustomAction : public FStateTreeTaskBase
{
    GENERATED_BODY()
    
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
                                         const FStateTreeTransitionResult& Transition) const override;
                                         
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, 
                                   float DeltaTime) const override;
};

// StateTree条件节点
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTcsStateTreeCondition_CustomCheck : public FStateTreeConditionBase
{
    GENERATED_BODY()
    
    virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
```

### 4. 状态槽配置最佳实践

#### 数据表配置方式 (推荐)

1. **创建槽位配置数据表**:
   - 创建基于`FTcsSlotConfigurationRow`的数据表
   - 在项目设置中指定数据表路径

2. **配置常用槽位**:
```cpp
// 数据表内容示例
Row Name     | Slot Tag              | Activation Mode
-------------|----------------------|------------------
ActionSlot   | StateSlot.Action     | Priority Only
BuffSlot     | StateSlot.Buff       | All Active
DebuffSlot   | StateSlot.Debuff     | All Active
MobilitySlot | StateSlot.Mobility   | Priority Only
```

#### 代码配置方式 (兼容模式)

```cpp
void UTcsStateComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // 配置常用槽位
    ConfigureCommonSlots();
    
    // 或者手动配置特殊槽位
    SetSlotConfiguration(
        FGameplayTag::RequestGameplayTag("StateSlot.Custom"), 
        ETcsSlotActivationMode::PriorityOnly
    );
}
```

### 5. 战斗实体接口实现

```cpp
UCLASS()
class AMyCharacter : public ACharacter, public ITcsCombatEntityInterface
{
    GENERATED_BODY()

protected:
    UPROPERTY(VisibleAnywhere, Category = "Combat")
    UTcsAttributeComponent* AttributeComponent;
    
    UPROPERTY(VisibleAnywhere, Category = "Combat")
    UTcsStateComponent* StateComponent;
    
    UPROPERTY(VisibleAnywhere, Category = "Combat")
    UTcsSkillComponent* SkillComponent;

public:
    // 实现战斗实体接口
    virtual UTcsAttributeComponent* GetAttributeComponent_Implementation() const override 
    { return AttributeComponent; }
    
    virtual UTcsStateComponent* GetStateComponent_Implementation() const override 
    { return StateComponent; }
    
    virtual UTcsSkillComponent* GetSkillComponent_Implementation() const override 
    { return SkillComponent; }
};
```

## API使用指南

### 属性系统使用

```cpp
// 获取属性值
float CurrentHealth = AttributeComponent->GetAttribute("CurrentHealth");

// 设置属性值
AttributeComponent->SetAttribute("CurrentHealth", 100.0f);

// 添加属性修改器
FTcsAttributeModifierInstance Modifier;
Modifier.AttributeName = "AttackPower";
Modifier.Value = 50.0f;
Modifier.ExecutionType = UTcsAttrModExec_Addition::StaticClass();
AttributeComponent->AddModifier(Modifier);

// 移除属性修改器
AttributeComponent->RemoveModifier(ModifierId);
```

### 状态系统使用

```cpp
// 应用状态到角色
UTcsStateManagerSubsystem* StateManager = GetWorld()->GetSubsystem<UTcsStateManagerSubsystem>();
bool bSuccess = StateManager->ApplyState(TargetActor, "State_Poison", InstigatorActor);

// 查询状态
UTcsStateInstance* ActiveState = StateComponent->GetHighestPriorityActiveState(
    FGameplayTag::RequestGameplayTag("StateSlot.Action"));

// 获取槽位中所有激活状态
TArray<UTcsStateInstance*> ActiveDebuffs = StateComponent->GetActiveStatesInSlot(
    FGameplayTag::RequestGameplayTag("StateSlot.Debuff"));
```

### 技能系统使用

```cpp
// 学习技能
UTcsSkillInstance* LearnedSkill = SkillComponent->LearnSkill("Skill_Fireball", 1);

// 升级技能
bool bUpgraded = SkillComponent->UpgradeSkillInstance("Skill_Fireball", 1);

// 释放技能
bool bCasted = SkillComponent->TryCastSkill("Skill_Fireball", TargetActor);

// 查询技能参数
float Damage = SkillComponent->GetSkillNumericParameter("Skill_Fireball", "BaseDamage");
```

## 架构决策记录

### 1. 为什么采用Tcs前缀而非Tirefly前缀？

**决策**: 使用`Tcs`作为类前缀，而不是完整的`Tirefly`

**理由**:
- **简洁性**: `Tcs`比`Tirefly`更简短，减少代码冗长
- **专用性**: `Tcs`明确标识这是TireflyCombatSystem插件的类
- **一致性**: 与现有代码保持一致，避免混合命名

### 2. 为什么StateTree采用双层架构？

**决策**: StateTree既作为状态槽管理器，又作为状态逻辑执行器

**理由**:
- **可视化管理**: 状态关系通过StateTree图形化编辑
- **动静结合**: 静态槽位结构 + 动态状态实例
- **零代码配置**: 策划可直接配置复杂状态逻辑

### 3. 为什么技能和状态使用统一定义？

**决策**: 技能、Buff、状态共用`FTcsStateDefinition`

**理由**:
- **架构统一**: "一切皆状态"的设计理念
- **代码复用**: 减少重复的系统实现
- **扩展便利**: 新增状态类型无需修改核心架构

### 4. 为什么区分SkillInstance和StateInstance？

**决策**: 技能实例和状态实例分离设计

**理由**:
- **职责分离**: SkillInstance管理学习状态，StateInstance管理执行状态
- **性能优化**: 学会的技能持久存在，运行的状态动态创建
- **数据完整**: 技能等级、冷却等信息与运行状态解耦

## 性能考虑

### 1. 对象池集成

- **StateInstance对象池**: 频繁创建销毁的状态实例使用对象池管理
- **AttributeModifier对象池**: 属性修改器的复用机制
- **StateTree实例池**: StateTree执行上下文的池化管理

### 2. 批量更新机制

- **属性批量更新**: 多个属性修改器同时生效时的批量计算
- **状态槽批量激活**: 槽位状态变化时的批量激活更新
- **实时参数同步**: 只同步有变化的参数，避免不必要的计算

### 3. 智能缓存策略

- **参数计算缓存**: 复杂参数计算结果的缓存机制
- **状态查询缓存**: 高频查询操作的结果缓存
- **StateTree执行缓存**: StateTree节点执行结果的缓存

## 调试和测试

### 1. 调试工具

- **StateTree调试器**: 利用UE内置StateTree调试器查看状态执行
- **属性变化追踪**: 属性修改的完整变化链路追踪
- **状态槽可视化**: 槽位状态占用情况的实时显示

### 2. 测试用例

- **单元测试**: 核心算法和策略的单元测试
- **集成测试**: 多系统协作的集成测试
- **性能测试**: 大规模状态管理的性能基准测试

### 3. 日志系统

```cpp
// 使用插件专用日志类别
UE_LOG(LogTcsState, Log, TEXT("State applied: %s"), *StateName);
UE_LOG(LogTcsAttribute, Warning, TEXT("Attribute not found: %s"), *AttributeName);
UE_LOG(LogTcsSkill, Error, TEXT("Skill casting failed: %s"), *SkillName);
```

## 依赖关系

### 引擎模块依赖

- `StateTreeModule` - StateTree核心功能
- `GameplayStateTreeModule` - StateTree游戏扩展
- `GameplayTags` - GameplayTag系统
- `GameplayMessageRuntime` - 消息路由系统

### 项目插件依赖

- `TireflyObjectPool` - 对象池系统
- `TireflyActorPool` - Actor对象池 (可选)
- `TireflyBlueprintGraphUtils` - 蓝图编辑器增强 (可选)

## 最佳实践

### 1. 状态设计原则

- **单一职责**: 每个状态只负责一种明确的游戏逻辑
- **数据驱动**: 尽量通过数据表配置状态行为，减少硬编码
- **可组合性**: 状态应该能够与其他状态灵活组合

### 2. 性能优化原则

- **延迟计算**: 非必要的计算延迟到真正需要时进行
- **批量操作**: 多个相关操作尽量批量执行
- **智能同步**: 只在数据真正变化时触发更新

### 3. 扩展开发原则

- **策略模式**: 新功能优先考虑通过策略类扩展
- **接口隔离**: 不同模块通过接口交互，降低耦合度
- **向后兼容**: 新功能不应破坏现有API的兼容性

## 版本历史

- **v1.0** - 基础属性系统和状态系统实现
- **v1.1** - StateTree集成和状态槽系统
- **v1.2** - 技能系统完善和参数系统重构  
- **v1.3** - 性能优化和对象池集成
- **v2.0** - StateTree双层架构和状态槽优先级系统 (当前版本)

---

**注意**: 这是插件级别的开发指南，专注于TCS插件内部的开发约定。如需了解整个TireflyGameplayUtils项目的开发指南，请参考项目根目录的CLAUDE.md文件。