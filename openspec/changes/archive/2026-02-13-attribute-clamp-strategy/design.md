# Design: 属性 Clamp 功能策略模式化

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                  FTcsAttributeDefinition                     │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ FTcsAttributeRange AttributeRange                      │ │
│  │ TSubclassOf<UTcsAttributeClampStrategy> ClampStrategy  │ │ <- 新增字段
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ 引用
                            ▼
┌─────────────────────────────────────────────────────────────┐
│          UTcsAttributeClampStrategy (抽象基类)               │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ virtual float Clamp(float Value,                       │ │
│  │                     float MinValue,                    │ │
│  │                     float MaxValue)                    │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                            △
                            │ 继承
        ┌───────────────────┼───────────────────┐
        │                   │                   │
        ▼                   ▼                   ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│   Linear     │  │   Wrap       │  │   Step       │
│  (默认实现)   │  │  (循环模式)   │  │  (阶梯模式)   │
└──────────────┘  └──────────────┘  └──────────────┘
```

## Component Design

### 1. UTcsAttributeClampStrategy (抽象基类)

**职责**：定义属性 Clamp 的统一接口

**关键设计决策**：
- 使用 `BlueprintNativeEvent` 支持蓝图和 C++ 双重实现
- 接口简单，只传递必要参数（Value, MinValue, MaxValue）
- 返回 Clamp 后的值

**接口设计**：
```cpp
UCLASS(BlueprintType, Blueprintable, Abstract)
class UTcsAttributeClampStrategy : public UObject
{
    GENERATED_BODY()

public:
    /**
     * 对属性值进行约束
     *
     * @param Value 待约束的值
     * @param MinValue 最小值（可能为 -Infinity）
     * @param MaxValue 最大值（可能为 +Infinity）
     * @return 约束后的值
     */
    UFUNCTION(BlueprintNativeEvent, Category = "TireflyCombatSystem|Attribute")
    float Clamp(float Value, float MinValue, float MaxValue);
    virtual float Clamp_Implementation(float Value, float MinValue, float MaxValue) { return Value; }
};
```

**为什么不传递更多上下文**：
- 保持接口简单，易于实现
- 大多数 Clamp 逻辑只需要值和范围
- 如果未来需要更多上下文（如属性名、组件引用），可以添加重载版本

### 2. UTcsAttrClampStrategy_Linear (默认实现)

**职责**：实现当前的线性 Clamp 行为

**实现**：
```cpp
UCLASS(Meta = (DisplayName = "属性 Clamp 策略：线性约束"))
class UTcsAttrClampStrategy_Linear : public UTcsAttributeClampStrategy
{
    GENERATED_BODY()

public:
    virtual float Clamp_Implementation(float Value, float MinValue, float MaxValue) override
    {
        return FMath::Clamp(Value, MinValue, MaxValue);
    }
};
```

**为什么作为默认实现**：
- 覆盖 90% 的使用场景
- 与当前行为完全一致，保证向后兼容
- 性能最优（简单的 min/max 比较）

### 3. FTcsAttributeDefinition 扩展

**新增字段**：
```cpp
// Clamp 策略类（默认使用线性 Clamp）
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Range")
TSubclassOf<UTcsAttributeClampStrategy> ClampStrategyClass;
```

**构造函数设置默认值**（在 .cpp 文件中）：
```cpp
// TcsAttribute.cpp
#include "Attribute/AttrClampStrategy/TcsAttrClampStrategy_Linear.h"

FTcsAttributeDefinition::FTcsAttributeDefinition()
{
    // 设置默认 Clamp 策略为线性 Clamp
    ClampStrategyClass = UTcsAttrClampStrategy_Linear::StaticClass();
}
```

**设计决策**：
- 使用 `TSubclassOf` 而非实例指针，避免序列化问题
- 字段默认值在构造函数中设置，避免头文件引用策略类（防止循环依赖）
- 默认使用 `UTcsAttrClampStrategy_Linear`，保证向后兼容
- 放在 "Range" 分类下，与 `AttributeRange` 相关
- 现有数据表加载时，如果字段为空，会自动使用构造函数的默认值

### 4. ClampAttributeValueInRange 函数更新

**当前签名**：
```cpp
static void ClampAttributeValueInRange(
    UTcsAttributeComponent* AttributeComponent,
    const FName& AttributeName,
    float& NewValue,
    float* OutMinValue = nullptr,
    float* OutMaxValue = nullptr,
    const FAttributeValueResolver* Resolver = nullptr);
```

**更新逻辑**（完全策略化）：
```cpp
// 1. 计算 MinValue 和 MaxValue（保持不变）
float MinValue = ...;
float MaxValue = ...;

// 2. 获取 Clamp 策略（必定有值，因为构造函数设置了默认值）
const FTcsAttributeInstance* Attribute = AttributeComponent->Attributes.Find(AttributeName);
TSubclassOf<UTcsAttributeClampStrategy> StrategyClass = Attribute->AttributeDef.ClampStrategyClass;

// 3. 执行 Clamp（统一使用策略对象）
if (StrategyClass)
{
    UTcsAttributeClampStrategy* StrategyCDO = StrategyClass->GetDefaultObject<UTcsAttributeClampStrategy>();
    NewValue = StrategyCDO->Clamp(NewValue, MinValue, MaxValue);
}
else
{
    // 防御性代码：理论上不应该走到这里，因为构造函数设置了默认值
    // 但为了安全，仍然提供 fallback
    UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] ClampStrategyClass is null for attribute %s, using FMath::Clamp as fallback"),
        *FString(__FUNCTION__), *AttributeName.ToString());
    NewValue = FMath::Clamp(NewValue, MinValue, MaxValue);
}

// 4. 输出 MinValue 和 MaxValue（保持不变）
if (OutMinValue) *OutMinValue = MinValue;
if (OutMaxValue) *OutMaxValue = MaxValue;
```

**关键变化**：
- **移除硬编码分支**：不再有 `if (StrategyClass) ... else FMath::Clamp` 的双路径逻辑
- **统一使用策略对象**：所有属性都通过策略对象执行 Clamp
- **保留防御性代码**：虽然理论上 `StrategyClass` 不会为 nullptr，但仍保留 fallback 逻辑以防万一

**性能优化**：
- 使用 CDO（Class Default Object）避免重复创建策略对象
- 策略类指针在属性定义中缓存，无需每次查找
- 所有属性统一使用策略对象，性能一致且可预测

## Data Flow

```
1. 属性值变更触发
   ↓
2. ClampAttributeValueInRange 被调用
   ↓
3. 从 FTcsAttributeRange 计算 MinValue 和 MaxValue
   ↓
4. 检查 FTcsAttributeDefinition.ClampStrategyClass
   ├─ nullptr → 使用默认线性 Clamp (FMath::Clamp)
   └─ 有值 → 获取 CDO 并调用 Clamp_Implementation
   ↓
5. 返回 Clamp 后的值
```

## File Structure

```
Source/TireflyCombatSystem/
├── Public/
│   └── Attribute/
│       ├── TcsAttribute.h                          (修改：添加 ClampStrategyClass 字段)
│       └── AttrClampStrategy/                      (新增目录)
│           ├── TcsAttributeClampStrategy.h         (新增：抽象基类)
│           └── TcsAttrClampStrategy_Linear.h       (新增：默认线性策略)
└── Private/
    └── Attribute/
        ├── TcsAttribute.cpp                        (新增：实现 FTcsAttributeDefinition 构造函数，设置默认策略)
        ├── TcsAttributeManagerSubsystem.cpp        (修改：更新 ClampAttributeValueInRange，移除硬编码逻辑)
        └── AttrClampStrategy/                      (新增目录)
            ├── TcsAttributeClampStrategy.cpp       (新增：基类实现)
            └── TcsAttrClampStrategy_Linear.cpp     (新增：线性策略实现)
```

## Trade-offs

### 性能 vs 灵活性
- **Trade-off**：策略模式引入虚函数调用开销
- **决策**：使用 CDO 最小化开销；所有属性统一使用策略对象，性能一致
- **理由**：灵活性和架构一致性的价值远超微小的性能开销；统一的代码路径更易维护

### 完全策略化 vs 混合模式
- **Trade-off**：完全策略化（所有属性都用策略对象）vs 混合模式（未指定时用硬编码）
- **决策**：选择完全策略化
- **理由**：代码更简洁，无分支逻辑；与合并器设计完全一致；性能可预测

### 默认值位置 vs 头文件简洁性
- **Trade-off**：在头文件中设置默认值 vs 在 .cpp 构造函数中设置
- **决策**：在 .cpp 构造函数中设置
- **理由**：避免头文件引用策略类，防止循环依赖；保持头文件简洁

## Testing Strategy

### 单元测试
1. **默认行为测试**：未指定策略时，行为与当前实现完全一致
2. **自定义策略测试**：指定自定义策略后，正确调用策略的 Clamp 方法
3. **边界测试**：MinValue = MaxValue、MinValue > MaxValue 等边界情况
4. **性能测试**：验证 CDO 缓存有效，未指定策略时无额外开销

### 集成测试
1. **属性系统集成**：在完整的属性修改流程中验证 Clamp 功能
2. **蓝图集成**：验证蓝图可以继承和实现自定义策略
3. **数据表集成**：验证数据表可以正确配置 ClampStrategyClass

## Migration Path

### 现有项目
1. **无需修改**：`ClampStrategyClass` 默认为 nullptr，使用默认线性 Clamp
2. **可选升级**：如果需要自定义 Clamp 逻辑，在数据表中指定 `ClampStrategyClass`

### 新项目
1. **默认使用线性 Clamp**：大多数属性无需配置
2. **按需自定义**：对于特殊属性（如角度、等级），指定自定义策略

## Future Extensions

### 可能的内置策略
1. **UTcsAttrClampStrategy_Wrap**：循环 Clamp（如角度）
2. **UTcsAttrClampStrategy_Step**：阶梯 Clamp（如等级，只能是整数）
3. **UTcsAttrClampStrategy_Soft**：软 Clamp（超出范围时衰减）
4. **UTcsAttrClampStrategy_Asymmetric**：非对称 Clamp（不同的正负值范围）

### 可能的接口扩展
如果未来需要更多上下文，可以添加重载版本：
```cpp
UFUNCTION(BlueprintNativeEvent)
float ClampWithContext(
    float Value,
    float MinValue,
    float MaxValue,
    const FTcsAttributeInstance& Attribute,
    UTcsAttributeComponent* AttributeComponent);
```

## Open Questions

1. **是否需要在初始版本中提供额外的内置策略？**
   - **建议**：先实现基础框架和线性策略，后续根据需求添加
   - **理由**：避免过度设计；让社区反馈驱动功能扩展

2. **是否需要支持策略的参数配置？**
   - **示例**：循环 Clamp 可能需要配置是否允许负值
   - **建议**：暂不支持，如果需要可以通过继承实现
   - **理由**：保持接口简单；参数配置会增加复杂度

3. **是否需要支持策略的运行时切换？**
   - **建议**：暂不支持，策略在属性定义时确定
   - **理由**：简化实现；大多数场景不需要运行时切换
