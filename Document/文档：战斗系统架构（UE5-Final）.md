# 一、战斗实体

***

##  1.1 战斗实体接口

```cpp
class ITireflyCombatEntity
{
	UTireflyAttributeManagerComponent* GetAttributeManagerComponent() const;
	UTireflyStateManagerComponent* GetStateManagerComponent() const;
	UTireflyAbilityManagerComponent* GetAbilityManagerComponent() const;
	UTireflyBuffManagerComponent* GetBuffManagerComponent() const;
}
```

##  1.2 战斗实体分类
- 角色（玩家、怪物、NPC等）
- 武器（冷兵器、热兵器、玄幻类武器等）
- 投射物（子弹、炮弹、法术球等）
- 技能指示器



# 二、属性模块

***

## 2.1 属性定义
- 属性的静态配置器，使用数据表编辑
- 定义属性的名称、描述、范围、求值器、UI配置

```cpp
struct FTireflyAttributeDefinition : public FTableRowBase
{
// 属性元数据
public:
	FName AttributeId = NAME_None;// 属性名定义
	FTireflyAttributeRange AttributeRange;// 属性范围

// 属性显示设置
public:
	FText AttributeName;// 属性名称（推荐使用 StringTable）
	FText AttributeDescription;// 属性描述（推荐使用 StringTable）
	FGameplayTag AttributeCategory;// 属性类别
	
// 属性UI设置
	bool bShowInUI = true;// 是否在UI中显示
	TSoftObjectPtr<UTexture2D> Icon;// 属性图标
	bool bAsDecimal = true;// 是否作为小数显示
	bool bAsPercentage = false;// 是否作为百分数显示
};
```

### a. 属性范围
- 限制属性的值范围的设置
- 支持静态范围
- 支持动态引用属性作为属性值的边界

```cpp
// 属性范围类型
enum class ETireflyAttributeRangeType : uint8
{
	Static		UMETA(DisplayName = "Static Range", ToolTip = "固定范围"),
	Dynamic		UMETA(DisplayName = "Dynamic Range", ToolTip = "动态范围，引用其他属性作为边界"),
	Unlimited	UMETA(DisplayName = "Unlimited Range", ToolTip = "无限制，该属性没有单侧范围限制"),
};


// 属性范围
struct FTireflyAttributeRange
{
// MinValue
public:
	// 属性最小值范围类型
	ETireflyAttributeRangeType MinValueRangeType = ETireflyAttributeRangeType::Static;

	UPROPERTY(EditAnywhere, Meta = (EditCondition = "MinValueRangeType == ETireflyAttributeRangeType::Static", EditConditionHides,
	DisplayName = "属性最小值"))
	float MinValue = 0.f;

	UPROPERTY(EditAnywhere, Meta = (EditCondition = "MinValueRangeType == ETireflyAttributeRangeType::Dynamic", EditConditionHides,	DisplayName = "属性最小值属性"))
	FName MinValueAttribute = NAME_None;


// MaxValue
public:
	// 属性最小值范围类型
	ETireflyAttributeRangeType MaxValueRangeType = ETireflyAttributeRangeType::Static;

	UPROPERTY(EditAnywhere, Meta = (EditCondition = "MaxValueRangeType == ETireflyAttributeRangeType::Static", EditConditionHides,
	DisplayName = "属性最大值"))
	float MaxValue = 0.f;

	UPROPERTY(EditAnywhere, Meta = (EditCondition = "MaxValueRangeType == ETireflyAttributeRangeType::Dynamic", EditConditionHides,
	DisplayName = "属性最大值属性"))
	FName MaxValueAttribute = NAME_None;
};
```

## 2.2 属性实例
- 属性的运行时实例，是一个结构体
- 包含属性的基础值、当前值、属性修改器列表

```cpp
struct FTireflyAttributeInstance
{
public:
	FTireflyAttributeRange AttributeDef;// 属性定义
	FName AttributeId;// 属性唯一标识

	float BaseValue = 0.f;// 属性基础值
	float CurrentValue = 0.f;// 属性当前值
	
	TWeakObjectPtr<AActor> Owner;// 属性拥有者
};
```

## 2.3 属性修改器

***

### a. 属性修改器定义
- 修改器的静态配置器，继承自`FTableRowBase`
- 定义修改器的唯一标识Id、要修改的属性、同Id修改器间的合并方式、执行优先级、修改器标签等
- 定义修改器的执行函数（纯函数）、合并函数（纯函数）0

```cpp
struct FTireflyAttributeModifierDefinition : public FTableRowBases
{
// Modifier Definition
public:
	FName ModifierName;// 修改器的唯一标识Id，调试显示用
	int Priority = 0;// 修改器执行优先级，值越小优先级越高，默认为0
	FGameplayTagContainer Tags;// 修改器标签，调试显示用

// Attribute Modifier
public:
	FName AttributeId = NAME_None;// 修改器要修改的属性
	TMap<FName, float> Operands;// 修改器执行的操作数

// Modifier Operation
public:
	TSubclassOf<UTireflyAttributeModifierExecution> ExecutionType;// 修改器执行算法
	TSubclassOf<UTireflyAttributeModifierMerger> MergerType;// 修改器合并算法
};
```

### b. 属性修改器实例

- 修改器的运行时实例，是一个结构体
- 只携带修改器计算过程中的操作值
- 通过 `修改器定义` 的 `操作函数` ，计算出最终对属性的修改值

```cpp
struct FTireflyAttributeModifierInstance
{
public:
	FTireflyAttributeModifierDefinition ModifierDef;// 修改器定义
	int AttrModInstanceId = -1;// 修改器实例唯一标识

	TMap<FName, double> Operands;// 修改器操作数

	int64 ApplyTime = 0;// 修改器应用时间
	int64 UpdateTime = 0;// 修改器更新时间
};
```

### c. 属性修改器执行算法
- 通过ClassDefaultObject作为模板，供属性修改器进行计算操作

```cpp
class UTireflyAttributeModifierExecution : public UObject
{
	virtual void Execute(
		AActor* InstigatorEntity, 
		AActor* TargetEntity,
		const TArray<FName, double>& Operands,
		TMap<FName, double>& ModifiedValues);
}
```

### d. 属性修改器合并算法
- 通过ClassDefaultObject作为模板，供属性修改器进行合并操作

```cpp
class UTireflyAttributeModifierMerger : public UObject
{
	virtual void Merge(
		UPARAM(ref)TArray<FTireflyAttributeModifierInstance>& SourceModifiers,
		TArray<FTireflyAttributeModifierInstance>& OutModifiers);
};
```

## 3.4 属性组件

```cpp
class UTireflyAttributeComponent : public UActorComponent
{
// 属性实例
public:
	UPROPERTY(BlueprintReadOnly, Category = "Attribute", Meta = (DisplayName = "属性实例容器"))
	TMap<FName, FTireflyAttributeInstance> AttributeInstances;

// 属性修改器
public:
	TMap<int, FTireflyModifierInstance> Modifiers;// 修改器列表
};
```

## 3.5 属性管理器

```cpp
class UTireflyAttributeManagerWorldSubsystem : public UWorldSubsystem
{
// 属性修改器流程
public:
	void ApplyModifier(AActor* CombatEntity, UPARAM(ref)TArray<FTireflyModifierInstance>& Modifiers);// 应用修改器
	void RemoveModifier(AActor* CombatEntity, UPARAM(ref)TArray<FTireflyModifierInstance>& Modifiers);// 移除修改器
	void HandleModifierUpdated(AActor* CombatEntity, UPARAM(ref)TArray<FTireflyModifierInstance>& Modifiers);// 修改器更新
};
```



# 三、状态模块

***

## 3.1 状态定义

```cpp
// 状态定义
struct FTireflyStateDefinition : public FTableRowBase
{
// 状态 配置
public:
	ETireflyStateType StateType = ETireflyStateType::State;// 状态类型
	FGameplayTag StateSlotType;// 状态槽类型
	int Priority = 0;// 状态优先级

// 状态 标签
public:
	FGameplayTagContainer CategoryTags;// 状态类别标签
	FGameplayTagContainer FunctionTags;// 状态功能标签

// 状态树
public:
	FStateTreeReference StateTreeRef;// 状态树资产引用，作为状态的运行时脚本
	TArray<TSubclassOf<UTireflyStateCondition>> ActiveConditions;// 状态的激活条件
	TMap<FName, FTireflyStateParameter> Parameters;// 状态的参数集

//  状态 持续时间
public:
	ETireflyStateDurationType DurationType;// 持续时间类型
	float Duration;// 持续时间

//  状态 叠层
public:
	int MaxStackCount;// 最大叠层数
	ETireflyStateStackPolicy SameInstigatorStackPolicy;// 同一个技能者叠层处理策略
	ETireflyStateStackPolicy DiffInstigatorStackPolicy;// 不同技能者叠层处理策略
};
```

### a. 状态类型

```cpp
enum class ETireflyStateType : uint8
{
	State,// 状态
	Skill,// 技能
	Buff,// buff
};
```

### b. 状态持续时间类型

```cpp
enum class ETireflyStateDurationType : uint8
{
	None,// 无持续时间
	Duration,// 有限时间
	Infinite,// 无限制
};
```

### c. 状态叠层策略

```cpp
enum class ETireflyStateStackPolicy : uint8
{
	Parallel,// 并行执行，互不干扰
	DiscardOld,// 新旧互斥，丢弃旧的
	DiscardNew,// 新旧互斥，丢弃新的
	Stack,// 叠加层数
};
```

### d. 状态参数
- 使用结构体表达，编辑器、数据表开放编辑
- 判定是否可快照（Snapshot）
- 共有如下几个子类型：
  - 简单数值（constant numeric）
  - 公式数值（formula numeric）
  - 可伸缩数值（scalable numeric）
  - 数组数值（array numeric）
  - 范围数值（range numeric）

```cpp
struct FTireflyStateParameter
{
	FName ParamName;
	bool bSnapshot;

	virtual bool GetParamValue(AActor* Instiagtor, AActor* Target, float& OutValue);
}
```

### e. 状态条件

```cpp
class UTireflyStateCondition : public UObject
{
	virtual void CheckStateCanActive(
		AActor* InstigatorEntity,
		AActor* TargetEntity,
		UTireflyStateInstance* Instance);
}
```

## 3.2 状态实例

```cpp
class UTireflyStateInstance : public UObject
{
// 状态数据
public:
	FTireflyStateDefinition StateDef;// 状态定义
	int InstanceId = -1;// 状态实例id
	int Level = -1;// 状态等级

// 生命周期
public:
	int64 ApplyTime = 0;// 状态应用时间
	int64 UpdateTime = 0;// 状态更新时间
	ETireflyStateStage Stage = ETireflyStateStage::Inactive;// 状态阶段

// 运行时数据
public:
	TWeakObjectPtr<AActor> Owner;// 状态实例拥有者
	TMap<FName, float> RuntimeParameters;// 运行时参数
	float DurationRemaining = 0.0f;// 持续时间剩余
	int StackCount = 1;// 叠层数

// 依赖和互斥
public:
	TSet<FName> RequiredStates;// 需要战斗实体拥有这些状态才能添加状态
	TSet<FName> ImmunityStates;// 战斗实体拥有这些状态时，无法添加状态

// 状态树数据
public:
	FStateTreeInstanceData StateTreeInstanceData;// 状态树实例数据
};
```

### a. 状态阶段

```cpp
enum class ETireflyStateStage : uint8
{
	Active,// 激活
	Inactive,// 未激活
	HangingUp,// 挂起中
};
```

## 3.3 状态组件

```cpp
class UTireflyStateComponent : public UActorComponent
{
// 状态数据管理
	// ***
	TMap<FGameplayTag, FTireflyStateInstance> ActiveStateSlots;
	TMap<FGameplayTag, FTireflyStateInstance> StateSlots;
	TMap<int, FTireflyStateInstance> States;
};
```

## 3.4 状态管理器

```cpp
class UTireflyStateManagerWorldSubsystem : public UWorldSubsystem
{
// 状态增删管理
public:
	bool AddState(AActor* CombatEntity, const FName& StateId, FTireflyStateInstance& OutState);
	bool RemoveState(AActor* CombatEntity, const FTireflyStateInstance& InState);
	bool RemoveStates(AActor* CombatEntity, const FName& StateId);

// 状态查询
public:
	TArray<UTireflyStateInstance*> FindStatesById(AActor* CombatEntity, const FName& StateId);
	TArray<UTireflyStateInstance*> FindStatesBySlotTag(AActor* CombatEntity, const FGameplayTag& StateSlotTag);
	TArray<UTireflyStateInstance*> FindStatesByTagQuery(AActor* CombatEntity, const FGameplayEffectQuery& TagQuery);

// 状态的执行
public:
	void ActivateState(AActor* CombatEntity, UTireflyStateInstance* StateInstance);// 激活状态
	void DeactivateState(AActor* CombatEntity, UTireflyStateInstance* StateInstance);// 停止状态
	void HangUpState(AActor* CombatEntity, UTireflyStateInstance* StateInstance);// 挂起状态
	void MergeState(AActor* CombatEntity, UTireflyStateInstance* OldStateInstance, UTireflyStateInstance* NewStateInstance);// 合并状态

// 状态的辅助函数
public:
	bool ContainsHigherPriorityState(AActor* CombatEntity, UTireflyStateInstance* StateInstance);// 是否存在优先级更高的状态
	bool ContainsLowerPriorityState(AActor* CombatEntity, UTireflyStateInstance* StateInstance);// 是否存在优先级更低的状态
	bool CheckIsImmunable(AActor* CombatEntity, const FName& StateId);// 检查是否免疫某状态
	bool CheckStatesCanMerge(AActor* CombatEntity, UTireflyStateInstance* OldStateInstance, UTireflyStateInstance* NewStateInstance);// 检查状态是否可以合并
};
```



# 四、技能模组

***

## 4.1 技能实例

```cpp
class UTireflySkillInstance : public UObject
{
//  技能数据
public:
	FTireflyStateDefinition SkillDef;// 技能定义
	int InstanceId = -1;// 技能实例id
	int Level = -1;// 技能等级

// 技能运行时
public:
	TWeakObjectPtr<AActor> Owner;// 技能实例拥有者

// 技能参数
public:
	TMap<FName, float> RuntimeParameters;// 基于静态配置生成的状态参数集，可被修改器修改值
	TMap<int32, UTireflySkillModifier*> SkillModifiers;// 正在生效的技能修改器
	float Cooldown = 0.0f;// 技能冷却时间
};
```

## 4.2 技能修改器

### a. 技能修改器定义

```cpp
struct FTireflySkillModifierDefinition : public FTableRowBase
{
// Modifier Definition
public:
	FName ModifierName = NAME_None;// 修改器的唯一标识Id，调试显示用
	int Priority = 0;// 修改器执行优先级，值越小优先级越高，默认为0

// Skill Modifier
public:
	TSubclassOf<UTireflySkillFilter> SkillFilter;// 技能查询器
	TArray<TSubclassOf<UTireflySkillModifierCondition>> ActiveConditions;// 修改器的激活条件
	FTireflyStateParameter ModifierParameter;// 修改器参数

// Modifier Operation
public:
	TSubclassOf<UTireflySkillModifierExecution> ExecutionType;// 修改器的执行类型
	TSubclassOf<UTireflySkillModifierMerger> MergeType;// 修改器的合并类型
}
```

### b. 技能修改器实例

```cpp
class FTireflySkillModifierInstance
{
	FTireflySkillModifierDefinition ModifierDef;// 技能修改器定义
	int SkillModInstanceId = = -1;// 技能修改器实例Id

	TObjectPtr<UTireflySkillInstance> SkillInstance;// 技能实例

	int64 ApplyTime = 0;// 应用时间
	int64 UpdateTime = 0;// 更新时间
};
```

### c. 技能修改器执行算法

```cpp
class UTireflySkillModifierExecution : public UObject
{
	virtual void Execute(
		UTireflySkillInstance* SkillInstance,
		UPARAM(ref)FTireflySkillModifierInstance& SkillModInstance);
};
```

### d. 技能修改器合并算法

```cpp
class UTireflySkillModifierMerger : public UObject
{
	virtual void Merge(
		UPARAM(ref)TArray<FTireflySkillModifierInstance>& SourceModifiers,
		TArray<FTireflySkillModifierInstance>& OutModifiers);
};
```

### e. 技能筛选器
- 技能筛选器用于筛选出符合特定条件的技能实例，有如下子结构体
  - 基于技能配置Id
  - 基于技能配置的类别标签、功能标签
  - 基于技能配置的状态类型
  - 基于技能配置的状态槽类型
  - 基于技能配置的优先级
  - 基于技能实例的等级

```cpp
class UTireflySkillFilter : public UObject
{
	virtual void Filter(AActor* SkillOwner, TArray<UTireflySkillInstance>& OutSkills);
};
```

## 4.3 技能组件

```cpp
class UTireflySkillComponent : public UActorComponent
{
	TMap<int, UTireflySkillInstance*> SkillsInstances;// 技能实例
	TMap<int, float> SkillsCooldowns;// 统一的技能冷却
	TArray<FTireflySkillModifierInstance> SkillModifiers;// 所有生效中的技能修改器
};
```

##  4.4 技能管理器

```cpp
class UTireflySkillManagerWorldSubsystem : public UTickableWorldSubsystem
{
	void LearnSkillById(AActor* Actor, const FName& SkillId, UTireflySkillInstance*& OutSkillInstance);// 学习技能（根据技能Id）
	void LearnSkillByDef(AActor* Actor, const FTireflySkillDefinition& SkillDefinition, UTireflySkillInstance*& OutSkillInstance);// 学习技能（根据技能定义）

	void ForgetSkillById(AActor* Actor, const FName& SkillId);// 忘掉技能（根据技能Id）
	void ForgetSkillByDe(AActor* Actor, const FTireflySkillDefinition& SkillDefinition);// 忘掉技能（根据技能定义）
	void ForgetSkillByInstance(AActor* Actor, UTireflySkillInstance* SkillInstance);// 忘掉技能（根据技能实例）

	bool ActivateSkill(AActor* Actor, UTireflySkillInstance* SkillInstance);// 激活技能
	void CancelSkill(AActor* Actor, UTireflySkillInstance* SkillInstance, bool bForce);// 取消技能

	bool ApplySkillModifier(AActor* CombatEntity, UTireflySkillInstance* SkillInstance, UPARAM(ref)TArray<FTireflySkillModifier>& Modifier);// 应用技能修改器
	void RemoveSkillModifier(AActor* CombatEntity, UTireflySkillInstance* SkillInstance, UPARAM(ref)TArray<FTireflySkillModifier>& Modifier);// 移除技能修改器
	bool HandleSkillModifierUpdated(AActor* CombatEntity, UTireflySkillInstance* SkillInstance, UPARAM(ref)TArray<FTireflySkillModifier>& Modifier);// 处理技能修改器更新

	void UpdateCombatEntity(AActor* CombatEntity, float DeltaTime);// 更新战斗实体
}
```
