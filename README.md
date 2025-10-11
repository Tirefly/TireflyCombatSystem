# TireflyCombatSystem (TCS)

**现代化的UE5战斗系统插件 - 基于"一切皆状态"理念与StateTree深度集成**

---

## 🎯 核心理念

TCS是一个基于数据-行为分离思想设计的现代化战斗系统，采用"一切皆状态"的统一架构，将技能、Buff、状态通过同一套管理系统处理，并与UE5的StateTree系统深度集成，为开发者提供可视化的战斗逻辑编辑体验。

### 🏗️ 架构特色

- **统一状态管理**：技能、Buff、状态使用同一套`FTireflyStateDefinition`和`UTireflyStateInstance`
- **StateTree深度集成**：战斗实体状态管理可视化编辑，Buff/技能逻辑通过StateTree执行
- **CDO策略模式**：广泛采用ClassDefaultObject作为策略模板，保证高扩展性
- **数据驱动架构**：通过数据表配置游戏逻辑，减少硬编码
- **对象池优化**：深度集成TireflyObjectPool和TireflyActorPool，保证高性能

---

## 🌟 设计愿景：StateTree双层架构

TCS采用创新的StateTree双层架构设计，将StateTree从"执行工具"提升为"状态关系管理器"，实现状态依赖、互斥、优先级的完全可视化管理。

### 🏗️ 双层架构设计

#### 第一层：StateTree作为状态槽管理器

StateTree管理预定义的状态槽和转换规则，每个状态槽可以容纳动态的状态实例：

```
StateTree结构示例：
战斗实体状态管理Tree
├─ 主状态槽 (Selector) 
│  ├─ 死亡状态 [优先级: 最高]
│  ├─ 眩晕状态 [优先级: 高]
│  ├─ 攻击状态 [优先级: 中]
│  └─ 待机状态 [优先级: 低]
│
└─ Buff状态组 (Parallel)
   ├─ 属性增益槽 (Selector)
   │  ├─ 攻击力加成Buff
   │  └─ 移动速度加成Buff
   ├─ 属性减益槽 (Selector)
   │  ├─ 中毒Debuff
   │  └─ 减速Debuff
   └─ DOT效果槽 (Parallel, 支持多个)
      ├─ 燃烧DOT
      ├─ 流血DOT
      └─ 毒素DOT
```

#### 第二层：动态状态实例管理

```cpp
// StateTree状态槽节点 - 管理状态实例的容器
UCLASS()
class UTireflyStateSlot : public UStateTreeTaskBase
{
    GENERATED_BODY()

public:
    // 该槽位当前的状态实例
    UPROPERTY()
    TArray<UTireflyStateInstance*> StateInstances;
    
    // 槽位类型（决定互斥/并行规则）
    UPROPERTY(EditAnywhere, Category = "Slot")
    ETireflyStateSlotType SlotType;

    // 新状态应用时的判定逻辑
    virtual bool CanApplyState(const FTireflyStateDefinition& NewState) const;
    
    // 状态应用时的合并/替换逻辑
    virtual void ApplyState(UTireflyStateInstance* NewStateInstance);
};

// StateTree状态槽管理组件
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflyStateTreeComponent : public UStateTreeComponent
{
    GENERATED_BODY()

public:
    // 动态应用状态的接口
    UFUNCTION(BlueprintCallable, Category = "State Management")
    bool TryApplyState(const FTireflyStateDefinition& StateDef, AActor* Instigator);
    
    // 获取当前激活的状态槽信息
    UFUNCTION(BlueprintCallable, Category = "State Management")
    TMap<FGameplayTag, UTireflyStateInstance*> GetActiveStates() const;

protected:
    // 状态实例池 - 按槽位类型组织
    UPROPERTY()
    TMap<FGameplayTag, TArray<UTireflyStateInstance*>> StateSlots;
};
```

### ✨ 设计优势

#### 🎨 **可视化状态关系**
- 在StateTree编辑器中直观配置状态槽规则和转换条件
- 无需编程即可设置复杂的状态依赖、互斥、优先级关系
- 状态关系的图形化显示，一目了然

#### 🔄 **动态与静态完美融合**
- **静态结构**：StateTree定义状态槽的组织结构和规则
- **动态管理**：StateInstance在槽位中动态创建、执行、销毁
- **最佳兼容**：结合StateTree静态结构管理和动态状态实例需求

#### 🚀 **零代码状态管理**
- 状态间依赖/互斥通过StateTree结构天然表达
- 无需硬编码StateCondition来处理状态关系
- 策划可直接通过可视化编辑器配置复杂战斗逻辑

#### 🐞 **调试友好**
- 运行时可在StateTree调试器中查看状态关系
- 状态槽占用情况、状态实例执行状态一目了然
- 支持断点调试和状态变化追踪

### 💡 **实际应用场景**

#### 1. 状态槽管理
```cpp
// 在StateTree中配置互斥状态槽
struct FTireflyStateSlot_Exclusive : public FStateTreeTaskBase
{
    // 互斥槽：只能有一个状态，新状态会替换旧状态
    UPROPERTY(EditAnywhere, Category = "Slot Config")
    FGameplayTagContainer AcceptedStateTags; // 接受的状态类型
    
    UPROPERTY(EditAnywhere, Category = "Slot Config")
    int32 Priority = 1; // 优先级
};

// 并行状态槽
struct FTireflyStateSlot_Parallel : public FStateTreeTaskBase
{
    // 并行槽：可以有多个状态，支持叠加
    UPROPERTY(EditAnywhere, Category = "Slot Config")
    int32 MaxStackCount = -1; // -1表示无限制
};
```

#### 2. 状态应用条件
```cpp
// StateTree条件节点 - 无需硬编码即可配置复杂条件
struct FTireflyStateCondition_SlotAvailable : public FStateTreeConditionBase
{
    UPROPERTY(EditAnywhere, Category = "Input")
    FGameplayTag TargetSlot;
    
    UPROPERTY(EditAnywhere, Category = "Input") 
    FTireflyStateDefinition StateToApply;
    
    // 自动检查槽位是否可用，考虑优先级、互斥等因素
    virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
```

#### 3. Buff效果与技能逻辑的StateTree执行

每个状态实例仍然运行独立的StateTree来执行具体的Buff效果或技能逻辑：

```cpp
// 状态实例 - StateTree逻辑执行载体
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflyStateInstance : public UObject
{
    GENERATED_BODY()

public:
    // 状态实例运行独立的StateTree实现Buff效果或技能逻辑
    UPROPERTY()
    FStateTreeInstanceData StateTreeInstanceData;
    
    // StateTree执行上下文包含战斗实体、属性、状态组件等
    bool SetupStateTreeContext(FStateTreeExecutionContext& Context);
};
```

**应用场景：**
- Buff效果的复杂逻辑（DOT伤害、属性修改、条件触发）
- 技能释放流程（前摇→判定→后摇→冷却）
- 状态间的复杂交互（如中毒状态与治疗状态的相互作用）

---

## 📋 功能模块

### 🎯 属性系统 (90%完成)
- **完整的属性定义**：支持静态/动态范围，UI配置，公式计算
- **强大的修改器系统**：基于CDO策略模式的执行算法和合并算法
- **事件驱动更新**：属性变化的完整事件系统
- **范围限制支持**：静态范围和动态属性引用

```cpp
// 属性修改器执行示例
UCLASS(Meta = (DisplayName = "属性修改器执行器：加法"))
class UTcsAttrModExec_Addition : public UTcsAttributeModifierExecution
{
    virtual void Execute_Implementation(
        const FTcsAttributeModifierInstance& ModInst,
        TMap<FName, float>& BaseValues,
        TMap<FName, float>& CurrentValues) override;
};
```

### 🔄 状态系统 (80%完成)
- **统一状态定义**：技能、Buff、状态共用`FTireflyStateDefinition`
- **灵活的状态合并**：支持同发起者/不同发起者的合并策略
- **状态条件检查**：基于属性比较、参数判定等条件系统
- **StateTree集成框架**：已预留StateTree执行接口

```cpp
// 状态定义支持StateTree引用和三种参数类型
USTRUCT()
struct FTireflyStateDefinition : public FTableRowBase
{
    // StateTree资产引用，作为状态的运行时脚本
    UPROPERTY(EditAnywhere, Category = "StateTree")
    FStateTreeReference StateTreeRef;
    
    // 状态类型：技能/Buff/状态
    UPROPERTY(EditAnywhere, Category = "Meta")
    TEnumAsByte<ETireflyStateType> StateType = ST_State;
    
    // 参数定义：支持Numeric/Bool/Vector三种类型
    UPROPERTY(EditAnywhere, Category = "Parameters")
    TMap<FName, FTireflyStateParameter> Parameters;
};

// 参数定义支持类型分化和快照机制
USTRUCT()
struct FTireflyStateParameter
{
    // 参数类型：Numeric(计算型)/Bool(静态型)/Vector(静态型)
    UPROPERTY(EditAnywhere, Category = "Parameter")
    ETireflyStateParameterType ParameterType = ETireflyStateParameterType::Numeric;
    
    // 是否为快照参数（激活时计算一次 vs 实时更新）
    UPROPERTY(EditAnywhere, Category = "Parameter")
    bool bIsSnapshot = true;
    
    // 参数解析器（仅Numeric类型使用）
    UPROPERTY(EditAnywhere, Category = "Parameter")
    TSubclassOf<UTireflyStateParamParser> ParamResolverClass;
    
    // 参数值容器（Bool/Vector类型直接存储值）
    UPROPERTY(EditAnywhere, Category = "Parameter")
    FInstancedStruct ParamValueContainer;
};
```

### ⚔️ 技能系统 (95%完成)
- **完整的技能实例系统**：UTireflySkillInstance支持运行时动态属性和参数修正
- **三种参数类型支持**：Numeric(计算型)、Bool(静态型)、Vector(静态型)
- **智能参数快照机制**：快照参数在激活时计算一次，实时参数动态更新
- **状态系统完美集成**：技能作为`ST_Skill`类型通过StateTree执行
- **完整的学习管理**：技能学习、升级、冷却、修正器全面支持
- **性能优化设计**：变化检测、批量更新、智能同步机制

```cpp
// 技能实例：代表角色学会的技能，支持动态修正和参数管理
UCLASS(BlueprintType)
class TIREFLYCOMBATSYSTEM_API UTireflySkillInstance : public UObject
{
    GENERATED_BODY()

public:
    // 动态属性：技能等级、冷却修正、消耗修正
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Skill Instance")
    int32 CurrentLevel = 1;
    
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Skill Instance")
    float CooldownMultiplier = 1.0f;
    
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Skill Instance")
    float CostMultiplier = 1.0f;

    // 三种参数类型支持
    UFUNCTION(BlueprintPure, Category = "Skill Instance|Parameters")
    float CalculateNumericParameter(FName ParamName, AActor* Instigator = nullptr, AActor* Target = nullptr) const;
    
    UFUNCTION(BlueprintPure, Category = "Skill Instance|Parameters")
    bool GetBoolParameter(FName ParamName, bool DefaultValue = false) const;
    
    UFUNCTION(BlueprintPure, Category = "Skill Instance|Parameters")
    FVector GetVectorParameter(FName ParamName, const FVector& DefaultValue = FVector::ZeroVector) const;

    // 智能参数快照和同步机制
    UFUNCTION(BlueprintCallable, Category = "Skill Instance|Parameters")
    void TakeParameterSnapshot(AActor* Instigator = nullptr, AActor* Target = nullptr);
    
    UFUNCTION(BlueprintCallable, Category = "Skill Instance|Parameters")
    void SyncParametersToStateInstance(UTireflyStateInstance* StateInstance, bool bForceAll = false);
    
    UFUNCTION(BlueprintCallable, Category = "Skill Instance|Parameters")
    void SyncRealTimeParametersToStateInstance(UTireflyStateInstance* StateInstance, AActor* Instigator = nullptr, AActor* Target = nullptr);

    // 性能优化：智能更新检测
    UFUNCTION(BlueprintPure, Category = "Skill Instance|Parameters")
    bool ShouldUpdateRealTimeParameter(FName ParamName, AActor* Instigator = nullptr, AActor* Target = nullptr) const;
    
    UFUNCTION(BlueprintPure, Category = "Skill Instance|Parameters")
    bool HasPendingRealTimeParameterUpdates(AActor* Instigator = nullptr, AActor* Target = nullptr) const;

    // 参数修正器系统
    UFUNCTION(BlueprintCallable, Category = "Skill Instance|Modifiers")
    void AddParameterModifier(FName ParamName, float ModifierValue, bool bIsMultiplier = false);
    
    UFUNCTION(BlueprintCallable, Category = "Skill Instance|Modifiers")
    void RemoveParameterModifier(FName ParamName, float ModifierValue, bool bIsMultiplier = false);
};

// 技能组件：管理角色的技能学习和激活
UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent))
class TIREFLYCOMBATSYSTEM_API UTireflySkillComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // 技能学习系统
    UFUNCTION(BlueprintCallable, Category = "Skill|Learning")
    UTireflySkillInstance* LearnSkill(FName SkillDefId, int32 InitialLevel = 1);
    
    UFUNCTION(BlueprintCallable, Category = "Skill|Learning")
    bool UpgradeSkillInstance(FName SkillDefId, int32 LevelIncrease = 1);
    
    UFUNCTION(BlueprintCallable, Category = "Skill|Learning")
    void ModifySkillParameter(FName SkillDefId, FName ParamName, float Modifier, bool bIsMultiplier = false);

    // 技能激活系统（自动集成StateTree执行）
    UFUNCTION(BlueprintCallable, Category = "Skill|Casting")
    bool TryCastSkill(FName SkillDefId, AActor* TargetActor = nullptr, const FInstancedStruct& CastParameters = FInstancedStruct());
    
    // 技能查询系统
    UFUNCTION(BlueprintPure, Category = "Skill|Query")
    float GetSkillNumericParameter(FName SkillDefId, FName ParamName) const;
    
    UFUNCTION(BlueprintPure, Category = "Skill|Query")
    bool GetSkillBoolParameter(FName SkillDefId, FName ParamName, bool DefaultValue = false) const;
    
    UFUNCTION(BlueprintPure, Category = "Skill|Query")
    FVector GetSkillVectorParameter(FName SkillDefId, FName ParamName, const FVector& DefaultValue = FVector::ZeroVector) const;

protected:
    // 已学会的技能实例
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Learned Skills")
    TMap<FName, UTireflySkillInstance*> LearnedSkillInstances;
    
    // 活跃技能状态跟踪（性能优化）
    UPROPERTY(BlueprintReadOnly, Category = "Active Skills")
    TMap<FName, UTireflyStateInstance*> ActiveSkillStateInstances;
    
    // 实时参数同步系统
    virtual void UpdateActiveSkillRealTimeParameters();
};
```

### 🛡️ Buff系统 (设计完成)
- **无需独立实现**：Buff作为`ST_Buff`类型的状态，完全复用状态系统
- **类型区分**：通过`ETireflyStateType`枚举区分不同状态类型
- **辅助查询**：提供Buff专用的查询和应用函数

---

## 🔧 核心组件

### 战斗实体接口
```cpp
UINTERFACE(BlueprintType)
class UTireflyCombatEntityInterface : public UInterface
{
    GENERATED_BODY()
};

class ITireflyCombatEntityInterface
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat Entity")
    class UTireflyAttributeComponent* GetAttributeComponent() const;
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat Entity")
    class UTireflyStateComponent* GetStateComponent() const;
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat Entity")
    class UTireflySkillComponent* GetSkillComponent() const;
};
```

### StateTree状态槽管理组件
```cpp
// 核心组件：StateTree状态槽管理器 - TCS的唯一状态管理方案
UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent))
class TIREFLYCOMBATSYSTEM_API UTireflyStateComponent : public UStateTreeComponent
{
    GENERATED_BODY()

public:
    // 动态应用状态到对应槽位
    UFUNCTION(BlueprintCallable, Category = "State Management")
    bool TryApplyState(const FTireflyStateDefinition& StateDef, AActor* Instigator);
    
    // 状态槽查询接口
    UFUNCTION(BlueprintPure, Category = "State Management")
    bool IsSlotOccupied(FGameplayTag SlotTag) const;
    
    UFUNCTION(BlueprintPure, Category = "State Management")
    UTireflyStateInstance* GetSlotState(FGameplayTag SlotTag) const;
    
    UFUNCTION(BlueprintPure, Category = "State Management")
    TArray<UTireflyStateInstance*> GetActiveStatesInSlot(FGameplayTag SlotTag) const;
    
    // 获取所有激活状态（跨槽位查询）
    UFUNCTION(BlueprintPure, Category = "State Management")
    TArray<UTireflyStateInstance*> GetAllActiveStates() const;
    
    // 按类型查询状态实例
    UFUNCTION(BlueprintPure, Category = "State Management")
    TArray<UTireflyStateInstance*> GetStatesByType(ETireflyStateType StateType) const;
    
    // 按标签查询状态实例
    UFUNCTION(BlueprintPure, Category = "State Management")
    TArray<UTireflyStateInstance*> GetStatesByTags(const FGameplayTagContainer& Tags) const;

protected:
    // 状态实例槽位管理
    UPROPERTY()
    TMap<FGameplayTag, TArray<UTireflyStateInstance*>> StateSlots;
    
    // 状态实例索引（用于快速查询）
    UPROPERTY()
    TMap<int32, UTireflyStateInstance*> StateInstancesById;
    
    UPROPERTY()
    TMap<ETireflyStateType, TArray<UTireflyStateInstance*>> StateInstancesByType;
    
    // StateTree事件处理
    virtual void OnStateSlotChanged(FGameplayTag SlotTag);
    virtual bool HandleStateApplicationRequest(const FTireflyStateDefinition& StateDef, AActor* Instigator);
    
    // 状态实例生命周期管理
    virtual void OnStateInstanceAdded(UTireflyStateInstance* StateInstance);
    virtual void OnStateInstanceRemoved(UTireflyStateInstance* StateInstance);
    
    // StateTree槽位管理的Tick逻辑
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                              FActorComponentTickFunction* ThisTickFunction) override;
    
    // 内部索引维护
    void UpdateStateInstanceIndices();
};
```

### 状态实例（StateTree执行载体）
```cpp
UCLASS(BlueprintType)
class TIREFLYCOMBATSYSTEM_API UTireflyStateInstance : public UObject
{
    GENERATED_BODY()

public:
    // 状态定义数据
    UPROPERTY(BlueprintReadOnly, Category = "State")
    FTireflyStateDefinition StateDef;
    
    // StateTree实例数据
    UPROPERTY()
    FStateTreeInstanceData StateTreeInstanceData;
    
    // StateTree生命周期管理
    UFUNCTION(BlueprintCallable, Category = "StateTree")
    void StartStateTree();
    
    UFUNCTION(BlueprintCallable, Category = "StateTree")
    void TickStateTree(float DeltaTime);
    
    UFUNCTION(BlueprintCallable, Category = "StateTree")
    void StopStateTree();
};
```

---

## 🎮 完整使用示例：奇幻ARPG战斗情景

### 场景设定：火焰法师vs毒系刺客的复杂战斗

以下示例展现了TCS在奇幻ARPG游戏中的真实使用场景，包含复杂的状态转换、Buff叠加、技能交互和免疫机制，**重点演示StateTree执行Buff和技能逻辑的核心设计理念**。

### 1. 战斗实体创建

```cpp
// 火焰法师角色类
UCLASS()
class AFireMage : public ACharacter, public ITireflyCombatEntityInterface
{
    GENERATED_BODY()

protected:
    // TCS核心战斗组件
    UPROPERTY(VisibleAnywhere, Category = "Combat")
    UTireflyAttributeComponent* AttributeComponent;
    
    UPROPERTY(VisibleAnywhere, Category = "Combat") 
    UTireflyStateComponent* StateComponent;
    
    UPROPERTY(VisibleAnywhere, Category = "Combat")
    UTireflySkillComponent* SkillComponent;

public:
    // 战斗实体接口实现
    virtual UTireflyAttributeComponent* GetAttributeComponent_Implementation() const override { return AttributeComponent; }
    virtual UTireflyStateComponent* GetStateComponent_Implementation() const override { return StateComponent; }
    virtual UTireflySkillComponent* GetSkillComponent_Implementation() const override { return SkillComponent; }

protected:
    virtual void BeginPlay() override
    {
        Super::BeginPlay();
        
        // 设置火焰法师的基础属性
        AttributeComponent->SetAttribute("MaxHealth", 800.0f);
        AttributeComponent->SetAttribute("CurrentHealth", 800.0f);
        AttributeComponent->SetAttribute("MaxMana", 500.0f);
        AttributeComponent->SetAttribute("CurrentMana", 500.0f);
        AttributeComponent->SetAttribute("SpellPower", 120.0f);
        AttributeComponent->SetAttribute("FireResistance", 0.8f);  // 80%火焰抗性
        AttributeComponent->SetAttribute("PoisonResistance", 0.2f); // 20%毒素抗性
    }
};

// 毒系刺客角色类
UCLASS()
class APoisonAssassin : public ACharacter, public ITireflyCombatEntityInterface
{
    GENERATED_BODY()
    
    // 类似的组件配置...
protected:
    virtual void BeginPlay() override
    {
        Super::BeginPlay();
        
        // 设置毒系刺客的基础属性
        AttributeComponent->SetAttribute("MaxHealth", 600.0f);
        AttributeComponent->SetAttribute("CurrentHealth", 600.0f);
        AttributeComponent->SetAttribute("Agility", 150.0f);
        AttributeComponent->SetAttribute("PoisonDamage", 80.0f);
        AttributeComponent->SetAttribute("FireResistance", 0.1f);   // 10%火焰抗性
        AttributeComponent->SetAttribute("PoisonResistance", 0.9f); // 90%毒素抗性
        AttributeComponent->SetAttribute("ImmuneToPoison", 0.0f);   // 初始不免疫中毒
    }
};
```

### 2. 核心设计理念：跨Actor vs 自Actor应用

```cpp
// StateManagerSubsystem - TCS的核心状态应用管理器
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflyStateManagerSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // 核心状态应用方法 - 支持跨Actor和自Actor应用
    UFUNCTION(BlueprintCallable, Category = "State Manager")
    bool ApplyState(AActor* TargetActor, FName StateDefId, AActor* InstigatorActor, const FInstancedStruct& ApplicationData = FInstancedStruct());
    
    // Buff应用 - 跨Actor模式（Instigator → Target）
    UFUNCTION(BlueprintCallable, Category = "State Manager")
    bool ApplyBuffToTarget(AActor* TargetActor, FName BuffDefId, AActor* BuffSource, const FInstancedStruct& BuffData = FInstancedStruct());
    
    // 技能应用 - 自Actor模式（Caster → Caster）
    UFUNCTION(BlueprintCallable, Category = "State Manager")
    bool ApplySkillToCaster(AActor* CasterActor, FName SkillDefId, const FInstancedStruct& SkillData = FInstancedStruct());
};

// 应用实现展示核心设计理念
bool UTireflyStateManagerSubsystem::ApplyState(AActor* TargetActor, FName StateDefId, AActor* InstigatorActor, const FInstancedStruct& ApplicationData)
{
    // 获取目标Actor的StateComponent（重要：状态总是应用到TargetActor的StateComponent上）
    UTireflyStateComponent* TargetStateComp = TargetActor->FindComponentByClass<UTireflyStateComponent>();
    if (!TargetStateComp) return false;
    
    // 获取状态定义
    FTireflyStateDefinition StateDef = GetStateDefinition(StateDefId);
    
    // 创建StateInstance - StateTree逻辑执行载体
    UTireflyStateInstance* StateInstance = CreateStateInstance(StateDef, TargetActor, InstigatorActor);
    
    // **关键设计**：StateInstance启动独立的StateTree来执行具体逻辑
    // 无论是Buff效果还是技能逻辑，都通过StateTree在StateInstance内执行
    if (StateDef.StateTreeRef.IsValid())
    {
        StateInstance->InitializeStateTree(StateDef.StateTreeRef);
        StateInstance->StartStateTree(); // StateTree开始执行状态逻辑
    }
    
    // 应用状态到目标的StateComponent
    return TargetStateComp->TryApplyState(StateDef, InstigatorActor);
}
```

### 3. 技能应用：自Actor模式演示

```cpp
void AFireMage::CastFireball(AActor* Target)
{
    // **核心设计**：技能应用为自Actor模式
    // 火球术StateInstance应用到法师自己的StateComponent上，而不是目标上
    
    UTireflyStateManagerSubsystem* StateManager = GetWorld()->GetSubsystem<UTireflyStateManagerSubsystem>();
    
    // 技能应用：Caster → Caster的StateComponent
    bool bSkillActivated = StateManager->ApplySkillToCaster(this, TEXT("Skill_Fireball"));
    
    if (bSkillActivated)
    {
        UE_LOG(LogCombat, Log, TEXT("火焰法师开始施放火球术 - 技能StateInstance运行在法师的StateComponent中"));
        
        // **设计说明**：
        // 1. 火球术的StateInstance现在存在于法师的StateComponent中
        // 2. StateTree ST_FireballSkill 在法师身上执行技能逻辑
        // 3. StateTree负责：施法动画、法力消耗、投射物创建、目标伤害计算
        // 4. 如果需要对目标施加Buff（如燃烧），StateTree会调用StateManager跨Actor应用
    }
    else
    {
        UE_LOG(LogCombat, Warning, TEXT("火球术释放失败 - 可能被沉默或法力不足"));
    }
}

// 火球术StateTree逻辑（运行在法师的StateComponent中）
/*
StateTree: ST_FireballSkill (执行位置：法师的StateInstance)
├─ 序列器 (Sequence)
│  ├─ 施法前摇 (Task: SkillCastPreparation)
│  │  └─ 播放施法动画，消耗法力值，显示施法特效
│  │
│  ├─ 投射物发射 (Task: CreateProjectile)  
│  │  └─ 创建火球投射物，设置目标和轨迹
│  │
│  ├─ 等待命中 (Task: WaitForProjectileHit)
│  │  └─ 监听投射物命中事件
│  │
│  ├─ 伤害计算 (Task: CalculateAndApplyDamage)
│  │  └─ 直接计算并应用伤害到目标（不通过StateInstance）
│  │
│  ├─ 燃烧效果判定 (Condition: RandomChance 30%)
│  │  └─ **跨Actor应用Buff** (Task: ApplyBurningBuffToTarget)
│  │     └─ 调用StateManager->ApplyBuffToTarget(Target, "Buff_Burning", this)
│  │
│  └─ 技能后摇 (Task: SkillRecovery)
│     └─ 播放后摇动画，启动冷却，清理资源
*/

// StateTree节点实现：跨Actor应用Buff
USTRUCT()
struct FTireflySkillTask_ApplyBurningBuffToTarget : public FStateTreeTaskBase
{
    GENERATED_BODY()
    
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, 
                                         const FStateTreeTransitionResult& Transition) const override
    {
        // 从StateTree上下文获取技能施法者和目标
        AActor* SkillCaster = Context.GetExternalData<AActor>("Owner");
        AActor* SkillTarget = Context.GetExternalData<AActor>("Target");
        UTireflyStateManagerSubsystem* StateManager = Context.GetWorld()->GetSubsystem<UTireflyStateManagerSubsystem>();
        
        if (SkillCaster && SkillTarget && StateManager)
        {
            // **核心设计**：通过StateManager跨Actor应用Buff
            // Buff的StateInstance将在目标的StateComponent中运行，而不是施法者的
            bool bBuffApplied = StateManager->ApplyBuffToTarget(SkillTarget, TEXT("Buff_Burning"), SkillCaster);
            
            if (bBuffApplied)
            {
                UE_LOG(LogCombat, Log, TEXT("燃烧Buff已应用到目标 - Buff StateInstance运行在目标的StateComponent中"));
                return EStateTreeRunStatus::Succeeded;
            }
        }
        
        return EStateTreeRunStatus::Failed;
    }
};
```

### 4. Buff应用：跨Actor模式演示

```cpp
void APoisonAssassin::AttackWithPoisonBlade(AActor* Target)
{
    UTireflyStateManagerSubsystem* StateManager = GetWorld()->GetSubsystem<UTireflyStateManagerSubsystem>();
    
    // **技能应用**：自Actor模式 - 毒刃技能StateInstance运行在刺客身上
    bool bSkillActivated = StateManager->ApplySkillToCaster(this, TEXT("Skill_PoisonBlade"));
    
    if (bSkillActivated)
    {
        UE_LOG(LogCombat, Log, TEXT("毒系刺客使用毒刃攻击 - 技能StateInstance运行在刺客的StateComponent中"));
        
        // 计算物理伤害（由技能StateTree执行）
        // ... 伤害计算逻辑 ...
        
        // **Buff应用**：跨Actor模式 - 中毒Buff StateInstance将运行在目标身上
        if (ShouldApplyPoison()) // 70%概率
        {
            bool bPoisonApplied = StateManager->ApplyBuffToTarget(Target, TEXT("Buff_Poison"), this);
            
            if (bPoisonApplied)
            {
                UE_LOG(LogCombat, Log, TEXT("中毒Buff已应用到目标 - Buff StateInstance运行在目标的StateComponent中"));
                // **重要**：中毒的DOT伤害、扩散逻辑等都将在目标身上的StateTree中执行
            }
        }
    }
}

// 中毒Buff的StateTree逻辑（运行在目标的StateComponent中）
/*
StateTree: ST_PoisonBuff (执行位置：目标的StateInstance)
├─ 并行器 (Parallel)
│  ├─ DOT伤害循环 (Task: PoisonDOTDamage)
│  │  └─ 每2秒对自身（目标）造成毒素伤害
│  │
│  ├─ 毒素扩散检测 (Task: PoisonSpreadCheck)
│  │  └─ 30%概率向附近敌人扩散毒素（跨Actor应用新的毒素Buff）
│  │
│  ├─ 视觉效果管理 (Task: PoisonVFXManager)
│  │  └─ 管理中毒的视觉特效和音效
│  │
│  └─ 叠层效果计算 (Task: PoisonStackCalculation)
│     └─ 根据叠层数量调整伤害倍率
*/

// StateTree节点实现：毒素DOT伤害（运行在目标身上）
USTRUCT()
struct FTireflyBuffTask_PoisonDOTDamage : public FStateTreeTaskBase
{
    GENERATED_BODY()
    
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override
    {
        // **关键**：这个StateTree运行在中毒目标的StateComponent中
        AActor* PoisonedTarget = Context.GetExternalData<AActor>("Owner"); // 中毒的目标
        AActor* PoisonSource = Context.GetExternalData<AActor>("Instigator"); // 施毒者
        UTireflyStateInstance* PoisonState = Context.GetExternalData<UTireflyStateInstance>();
        
        if (PoisonedTarget && PoisonSource && PoisonState)
        {
            // 检查是否到了伤害时机
            float TickInterval = PoisonState->GetParameter("TickInterval"); // 2.0秒
            if (ShouldDealDamage(TickInterval))
            {
                // 计算毒素伤害
                float DamagePerSecond = PoisonState->GetParameter("DamagePerSecond");
                int32 StackCount = PoisonState->GetStackCount();
                float FinalDamage = DamagePerSecond * StackCount;
                
                // 对自身（中毒目标）造成伤害
                UTireflyAttributeComponent* TargetAttr = PoisonedTarget->FindComponentByClass<UTireflyAttributeComponent>();
                if (TargetAttr)
                {
                    float CurrentHealth = TargetAttr->GetAttribute("CurrentHealth");
                    TargetAttr->SetAttribute("CurrentHealth", CurrentHealth - FinalDamage);
                    
                    UE_LOG(LogCombat, Log, TEXT("中毒DOT伤害: %.1f (叠层: %d) - StateTree在目标身上执行"), FinalDamage, StackCount);
                }
            }
        }
        
        return EStateTreeRunStatus::Running;
    }
};
```

### 5. SkillInstance vs StateInstance 的区别演示

```cpp
// UTireflySkillComponent - 管理可学习的技能（SkillInstance）
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflySkillComponent : public UActorComponent
{
public:
    // SkillInstance：代表角色学会的技能，包含等级、冷却等信息
    UPROPERTY(BlueprintReadOnly, Category = "Learned Skills")
    TMap<FName, UTireflySkillInstance*> LearnedSkills;
    
    // 学习技能 - 创建SkillInstance
    UFUNCTION(BlueprintCallable, Category = "Skill Learning")
    bool LearnSkill(FName SkillDefId, int32 InitialLevel = 1);
    
    // 激活技能 - 将SkillInstance转换为StateInstance并应用到StateComponent
    UFUNCTION(BlueprintCallable, Category = "Skill Activation")  
    bool ActivateSkill(FName SkillDefId, const FInstancedStruct& ActivationData = FInstancedStruct());
};

// UTireflySkillInstance - 可学习的技能实例
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflySkillInstance : public UObject
{
    GENERATED_BODY()

public:
    // 技能定义引用
    UPROPERTY(BlueprintReadOnly, Category = "Skill")
    FTireflyStateDefinition SkillDefinition;
    
    // 技能等级和数据
    UPROPERTY(BlueprintReadOnly, Category = "Skill")
    int32 SkillLevel = 1;
    
    UPROPERTY(BlueprintReadOnly, Category = "Skill")
    float RemainingCooldown = 0.0f;
    
    // **关键方法**：将SkillInstance转换为StateInstance
    UFUNCTION(BlueprintCallable, Category = "Skill Activation")
    UTireflyStateInstance* CreateStateInstanceForActivation(AActor* SkillOwner);
};

// 技能激活的完整流程
bool UTireflySkillComponent::ActivateSkill(FName SkillDefId, const FInstancedStruct& ActivationData)
{
    // 1. 获取学会的技能（SkillInstance）
    UTireflySkillInstance* SkillInstance = LearnedSkills.FindRef(SkillDefId);
    if (!SkillInstance || SkillInstance->RemainingCooldown > 0.0f)
    {
        return false; // 技能未学会或在冷却中
    }
    
    // 2. 将SkillInstance转换为StateInstance
    UTireflyStateInstance* SkillStateInstance = SkillInstance->CreateStateInstanceForActivation(GetOwner());
    if (!SkillStateInstance)
    {
        return false;
    }
    
    // 3. **关键**：将StateInstance应用到技能拥有者的StateComponent（自Actor模式）
    UTireflyStateManagerSubsystem* StateManager = GetWorld()->GetSubsystem<UTireflyStateManagerSubsystem>();
    bool bActivated = StateManager->ApplySkillToCaster(GetOwner(), SkillDefId);
    
    if (bActivated)
    {
        // 4. 启动冷却
        SkillInstance->RemainingCooldown = SkillStateInstance->GetParameter("Cooldown");
        
        UE_LOG(LogCombat, Log, TEXT("技能激活成功：%s - StateInstance开始在角色身上执行"), *SkillDefId.ToString());
        return true;
    }
    
    return false;
}

// 演示完整的技能激活流程
void AFireMage::DemonstrateSkillActivationFlow()
{
    UTireflySkillComponent* MySkillComp = GetSkillComponent();
    
    // 场景1：学习火球术技能（创建SkillInstance）
    bool bLearned = MySkillComp->LearnSkill(TEXT("Skill_Fireball"), 1);
    if (bLearned)
    {
        UE_LOG(LogCombat, Log, TEXT("学会了火球术 - SkillInstance已创建，存储在SkillComponent中"));
    }
    
    // 场景2：激活火球术（SkillInstance → StateInstance → StateComponent）
    bool bActivated = MySkillComp->ActivateSkill(TEXT("Skill_Fireball"));
    if (bActivated)
    {
        UE_LOG(LogCombat, Log, TEXT("火球术激活成功"));
        UE_LOG(LogCombat, Log, TEXT("- SkillInstance：保存在SkillComponent中，记录技能等级、冷却等"));
        UE_LOG(LogCombat, Log, TEXT("- StateInstance：创建并应用到StateComponent中，执行技能逻辑"));
        UE_LOG(LogCombat, Log, TEXT("- StateTree：在StateInstance内运行，处理施法、伤害、效果等"));
    }
}
```

### 6. 状态查询和管理

```cpp
void AFireMage::AnalyzeCombatState()
{
    UTireflyStateComponent* MyStateComp = GetStateComponent();
    UTireflySkillComponent* MySkillComp = GetSkillComponent();
    
    UE_LOG(LogCombat, Log, TEXT("=== 战斗状态分析 ==="));
    
    // 查询学会的技能（SkillInstance）
    UE_LOG(LogCombat, Log, TEXT("已学会的技能："));
    for (auto& SkillPair : MySkillComp->GetLearnedSkills())
    {
        FName SkillName = SkillPair.Key;
        UTireflySkillInstance* SkillInstance = SkillPair.Value;
        
        UE_LOG(LogCombat, Log, TEXT("- %s: 等级%d, 冷却%.1f秒"), 
               *SkillName.ToString(), 
               SkillInstance->SkillLevel,
               SkillInstance->RemainingCooldown);
    }
    
    // 查询活跃的状态（StateInstance）
    UE_LOG(LogCombat, Log, TEXT("当前活跃状态："));
    
    // 活跃的技能状态（自Actor模式 - 运行在自己身上）
    TArray<UTireflyStateInstance*> ActiveSkills = MyStateComp->GetStatesByType(ST_Skill);
    UE_LOG(LogCombat, Log, TEXT("正在执行的技能: %d个"), ActiveSkills.Num());
    for (auto* SkillState : ActiveSkills)
    {
        UE_LOG(LogCombat, Log, TEXT("- %s: StateTree执行中，剩余%.1f秒"), 
               *SkillState->GetStateDef().RowName.ToString(),
               SkillState->GetRemainingDuration());
    }
    
    // 影响自己的Buff状态（跨Actor模式 - 别人施加给我的）
    TArray<UTireflyStateInstance*> ActiveBuffs = MyStateComp->GetStatesByType(ST_Buff);
    UE_LOG(LogCombat, Log, TEXT("身上的Buff效果: %d个"), ActiveBuffs.Num());
    for (auto* BuffState : ActiveBuffs)
    {
        AActor* BuffSource = BuffState->GetInstigator();
        FString SourceName = BuffSource ? BuffSource->GetName() : TEXT("Unknown");
        
        UE_LOG(LogCombat, Log, TEXT("- %s: 来源[%s], StateTree执行中, 剩余%.1f秒"), 
               *BuffState->GetStateDef().RowName.ToString(),
               *SourceName,
               BuffState->GetRemainingDuration());
    }
}
```

### 7. 实战总结

通过以上完整的奇幻ARPG战斗示例，TCS展现了以下核心设计理念：

#### 7.1 跨Actor vs 自Actor应用模式
- **Buff效果**：跨Actor模式（Instigator → Target的StateComponent）
- **技能逻辑**：自Actor模式（Caster → Caster的StateComponent）
- **StateTree执行位置**：无论Buff还是技能，StateTree都在StateInstance所属的StateComponent中执行

#### 7.2 SkillInstance vs StateInstance 清晰分离
- **SkillInstance**：代表角色学会的技能，存储在SkillComponent中，包含等级、冷却等静态信息
- **StateInstance**：代表活跃的战斗状态，存储在StateComponent中，通过StateTree执行动态逻辑
- **转换关系**：激活技能时，SkillInstance动态创建StateInstance并应用到StateComponent

#### 7.3 StateTree作为逻辑执行载体
- **Buff效果逻辑**：通过StateTree在目标身上的StateInstance中执行（DOT伤害、扩散、视觉效果等）
- **技能流程逻辑**：通过StateTree在施法者身上的StateInstance中执行（施法、伤害计算、效果应用等）
- **跨Actor交互**：StateTree通过StateManagerSubsystem实现跨Actor的状态应用

#### 7.4 统一的状态管理架构
- **StateManagerSubsystem**：核心状态应用管理器，支持跨Actor和自Actor模式
- **StateComponent**：统一管理所有类型的StateInstance，无论来源如何
- **StateTree双层架构**：状态槽管理 + StateInstance逻辑执行的完美结合

#### 7.5 设计优势总结
- **逻辑清晰**：Buff在目标身上执行，技能在施法者身上执行，符合直觉
- **扩展性强**：通过StateTree可视化编辑复杂逻辑，无需硬编码
- **性能优化**：StateInstance对象池化，StateTree高效执行
- **调试友好**：StateTree调试器可视化状态执行流程

TCS通过这种创新的设计理念，为现代化战斗系统提供了强大而优雅的解决方案。

---

## 🚀 开发计划

### ✅ 已完成
- [x] **属性系统完整实现（90%）** - 属性定义、修改器系统、事件驱动更新
- [x] **状态系统核心架构（85%）** - 统一状态定义、StateTree集成、三种参数类型支持
- [x] **技能实例系统（95%）** - 完整的SkillInstance、参数快照机制、智能同步优化
- [x] **战斗实体接口设计** - ITireflyCombatEntityInterface标准化接口
- [x] **CDO策略模式应用** - 广泛的策略模式实现和扩展性设计
- [x] **数据表驱动配置** - 完整的数据驱动架构支持
- [x] **StateTree状态槽基础节点** - FTireflyStateSlot_Exclusive/Parallel节点实现
- [x] **参数系统重大升级** - Numeric/Bool/Vector三种类型、快照vs实时机制

### 🔄 开发中
- [ ] **StateTree双层架构完善** - 状态槽管理器的高级功能和优化
- [ ] **StateTree专用节点扩展** - 更多战斗相关的Task和Condition节点
- [ ] **可视化编辑工具集成** - StateTree编辑器中的状态关系配置界面
- [ ] **大规模性能测试** - 100+并发状态的性能验证和优化

### 📋 计划中（基于双层架构）
- [ ] **StateTree状态槽预设模板** - 常用状态槽配置的预设模板
- [ ] **性能优化与CopyOnWrite** - 高频状态变化的性能优化机制
- [ ] **调试工具完善** - StateTree调试器的TCS专用功能扩展
- [ ] **Buff系统辅助函数** - 基于StateTree槽管理的Buff查询和操作
- [ ] **对象池深度集成** - StateInstance对象池与StateTree的无缝集成
- [ ] **大规模状态管理测试** - 100+并发状态的StateTree性能验证

### 🎯 StateTree双层架构实施计划

#### 阶段1：基础状态槽系统 ✅ 已完成
- ✅ **UTireflyStateTreeComponent** 基础功能实现
- ✅ **FTireflyStateSlot_Exclusive** 和 **FTireflyStateSlot_Parallel** 节点
- ✅ **FTireflyStateCondition_SlotAvailable** 条件判定

#### 阶段2：技能实例系统与参数优化 ✅ 已完成
- ✅ **UTireflySkillInstance** 完整实现 - 学习、升级、修正器管理
- ✅ **参数系统重大升级** - Numeric/Bool/Vector三种类型支持
- ✅ **智能参数快照机制** - 快照vs实时参数的性能优化设计
- ✅ **StateParameter使用流程修复** - UTireflyStateParamParser正确集成
- ✅ **实时参数同步系统** - 变化检测、批量更新、智能同步

#### 阶段3：StateTree节点体系扩展 🔄 进行中
- ✅ 战斗核心Task节点：ApplyState、ModifyAttribute
- ✅ 战斗核心Condition节点：AttributeComparison、HasState
- ⏳ 状态槽类型的完整节点体系扩展
- ⏳ 复杂战斗逻辑的专用节点开发

#### 阶段4：可视化编辑工具与性能优化 📋 计划中
- ⏳ StateTree编辑器插件扩展
- ⏳ 状态关系的图形化显示
- ⏳ CopyOnWrite性能优化
- ⏳ 大规模状态管理的压力测试（目标：100+并发状态）

### 🏆 技能系统完成度总结
经过完整的实现和优化，TCS技能系统现已达到95%完成度：

**✅ 核心功能完备**
- 技能学习、升级、遗忘的完整生命周期管理
- 三种参数类型（Numeric/Bool/Vector）的全面支持
- 快照与实时参数的智能同步机制
- 技能修正器的动态添加和移除
- StateTree完美集成的技能执行流程

**✅ 性能优化到位**
- 参数变化智能检测，避免不必要的重复计算
- 批量参数更新机制，提升大规模技能管理效率
- 活跃技能状态跟踪，精确控制同步范围
- 缓存机制和时间戳管理，优化实时计算性能

**✅ 扩展性设计完善**
- UTireflyStateParamParser策略模式，支持复杂参数计算逻辑
- 修正器系统支持加法和乘法修正，满足各种游戏需求
- 事件系统完备，支持参数变化和等级变化的响应式编程
- Blueprint友好的接口设计，策划可直接使用

**🎯 剩余5%主要为高级优化功能**
- 更复杂的参数依赖关系处理
- 技能组合和连携技能支持
- 更精细的性能监控和调试工具

---

## 🔗 依赖插件

TCS依赖项目中的其他核心插件：

- **GameplayMessageRouter**: 松耦合消息传递系统
- **TireflyObjectPool**: UObject对象池系统
- **TireflyActorPool**: Actor对象池系统  
- **TireflyBlueprintGraphUtils**: 蓝图编辑器增强（参数智能提示）

---

## 📖 设计文档

详细的设计文档位于`Document`文件夹：

- `文档：战斗系统架构（UE5-Final）.md` - 完整的系统架构设计
- `文档：战斗系统架构（Design）.md` - 设计理念和实现思路
- `笔记：数据-行为分离思想下的GameObject架构.md` - 架构理论基础
- `笔记：数据-行为分离思想下的战斗系统架构.md` - 战斗系统专项设计

---

## 🤝 贡献

TCS是TireflyGameplayUtils项目的核心战斗系统组件，采用现代化的UE5开发理念，欢迎：

- 🐛 Bug报告和修复
- 💡 新功能建议
- 📚 文档完善
- 🧪 测试用例添加

---

## 📄 许可证

本项目遵循与TireflyGameplayUtils主项目相同的许可证。

---

**让战斗系统开发变得简单而强大 - TireflyCombatSystem** ⚔️