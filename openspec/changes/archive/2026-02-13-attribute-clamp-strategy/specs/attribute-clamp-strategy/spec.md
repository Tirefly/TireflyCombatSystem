# attribute-clamp-strategy Specification

## Purpose
提供可扩展的属性值约束（Clamp）机制，允许开发者通过策略模式自定义属性的约束行为,同时保持向后兼容性和架构一致性。

## ADDED Requirements

### Requirement: 属性 Clamp 策略抽象基类

系统 MUST 提供 `UTcsAttributeClampStrategy` 抽象基类，定义属性 Clamp 的统一接口。

#### Scenario: 策略基类定义

**Given**: 需要定义属性 Clamp 策略接口

**When**: 创建 `UTcsAttributeClampStrategy` 类

**Then**:
- 类 MUST 继承自 `UObject`
- 类 MUST 标记为 `Abstract`, `BlueprintType`, `Blueprintable`
- 类 MUST 提供 `Clamp` BlueprintNativeEvent 方法
- `Clamp` 方法 MUST 接受三个参数：`Value`, `MinValue`, `MaxValue`
- `Clamp` 方法 MUST 返回 `float` 类型的约束后的值
- 类 MUST 包含完整的注释，说明用途和使用方法

#### Scenario: 蓝图可继承

**Given**: 开发者需要在蓝图中实现自定义 Clamp 策略

**When**: 在蓝图编辑器中创建 `UTcsAttributeClampStrategy` 的子类

**Then**:
- 蓝图 MUST 能够继承 `UTcsAttributeClampStrategy`
- 蓝图 MUST 能够重写 `Clamp` 方法
- 蓝图实现的策略 MUST 能够在属性定义中使用

#### Scenario: C++ 可继承

**Given**: 开发者需要在 C++ 中实现自定义 Clamp 策略

**When**: 创建继承自 `UTcsAttributeClampStrategy` 的 C++ 类

**Then**:
- C++ 类 MUST 能够继承 `UTcsAttributeClampStrategy`
- C++ 类 MUST 能够重写 `Clamp_Implementation` 方法
- C++ 实现的策略 MUST 能够在属性定义中使用

### Requirement: 默认线性 Clamp 策略

系统 MUST 提供 `UTcsAttrClampStrategy_Linear` 作为默认的线性 Clamp 策略实现。

#### Scenario: 线性策略实现

**Given**: 需要默认的线性 Clamp 行为

**When**: 创建 `UTcsAttrClampStrategy_Linear` 类

**Then**:
- 类 MUST 继承自 `UTcsAttributeClampStrategy`
- 类 MUST 实现 `Clamp_Implementation` 方法
- `Clamp_Implementation` MUST 使用 `FMath::Clamp(Value, MinValue, MaxValue)`
- 行为 MUST 与当前硬编码的 Clamp 逻辑完全一致

#### Scenario: 线性策略作为默认行为

**Given**: 属性定义使用默认的 `ClampStrategyClass`（构造函数设置）

**When**: 执行属性值 Clamp

**Then**:
- 系统 MUST 使用 `UTcsAttrClampStrategy_Linear` 策略对象
- 行为 MUST 与当前硬编码的 `FMath::Clamp` 完全一致
- 所有属性 MUST 统一通过策略对象执行 Clamp

### Requirement: 属性定义支持 Clamp 策略配置

`FTcsAttributeDefinition` MUST 支持配置 Clamp 策略类，并通过构造函数设置默认值。

#### Scenario: 添加 ClampStrategyClass 字段

**Given**: 需要为属性指定 Clamp 策略

**When**: 修改 `FTcsAttributeDefinition` 结构体

**Then**:
- 结构体 MUST 包含 `ClampStrategyClass` 字段
- 字段类型 MUST 为 `TSubclassOf<UTcsAttributeClampStrategy>`
- 字段 MUST 标记为 `EditAnywhere, BlueprintReadOnly`
- 字段 MUST 放在 "Range" 分类下
- 字段 MUST 包含详细的 Tooltip 注释

#### Scenario: 构造函数设置默认策略

**Given**: 创建新的属性定义或加载现有属性定义

**When**: 调用 `FTcsAttributeDefinition` 构造函数

**Then**:
- 构造函数 MUST 在 .cpp 文件中实现（避免头文件引用）
- 构造函数 MUST 设置 `ClampStrategyClass = UTcsAttrClampStrategy_Linear::StaticClass()`
- 默认策略 MUST 为线性 Clamp 策略
- 现有属性定义加载时 MUST 自动应用默认策略

#### Scenario: 在编辑器中配置 Clamp 策略

**Given**: 在编辑器中编辑属性定义数据表

**When**: 选择 `ClampStrategyClass` 字段

**Then**:
- 编辑器 MUST 显示所有可用的 Clamp 策略类
- 编辑器 MUST 允许选择自定义策略类
- 编辑器 MUST 显示当前默认值为 `UTcsAttrClampStrategy_Linear`
- 编辑器 MUST 允许修改为其他策略类

### Requirement: ClampAttributeValueInRange 函数完全策略化

`ClampAttributeValueInRange` 函数 MUST 完全移除硬编码逻辑，统一使用策略对象执行 Clamp。

#### Scenario: 使用策略对象执行 Clamp

**Given**: 属性定义包含 `ClampStrategyClass`（通常为默认值或自定义值）

**When**: 调用 `ClampAttributeValueInRange` 函数

**Then**:
- 函数 MUST 从属性定义中获取 `ClampStrategyClass`
- 函数 MUST 获取策略类的 CDO（Class Default Object）
- 函数 MUST 调用 CDO 的 `Clamp` 方法
- 函数 MUST 使用 Clamp 方法返回的值更新 `NewValue`
- 函数 MUST NOT 包含硬编码的 `FMath::Clamp` 分支（除了防御性 fallback）

#### Scenario: 防御性 fallback 处理

**Given**: 属性定义的 `ClampStrategyClass` 为 nullptr（理论上不应发生）

**When**: 调用 `ClampAttributeValueInRange` 函数

**Then**:
- 函数 MUST 检测到 `ClampStrategyClass` 为 nullptr
- 函数 MUST 记录 Warning 级别日志
- 函数 MUST 使用 `FMath::Clamp(NewValue, MinValue, MaxValue)` 作为 fallback
- 日志 MUST 包含属性名称和警告信息

#### Scenario: 策略执行日志记录

**Given**: 执行属性值 Clamp

**When**: 使用策略对象执行 Clamp

**Then**:
- 函数 SHOULD 在 Verbose 日志级别记录使用的策略类型
- 日志 SHOULD 包含属性名称和策略类名称
- 日志 MUST 不影响性能（仅在 Verbose 级别启用时）

### Requirement: 向后兼容性保证

系统 MUST 保证现有项目无需修改即可正常运行，通过构造函数默认值实现。

#### Scenario: 现有属性定义无需修改

**Given**: 现有项目的属性定义数据表

**When**: 升级到包含 Clamp 策略的版本

**Then**:
- 现有属性定义 MUST 正常加载
- `ClampStrategyClass` 字段 MUST 自动使用构造函数的默认值（`UTcsAttrClampStrategy_Linear::StaticClass()`）
- 属性 Clamp 行为 MUST 与升级前完全一致

#### Scenario: 现有代码无需修改

**Given**: 现有项目调用 `ClampAttributeValueInRange` 的代码

**When**: 升级到包含 Clamp 策略的版本

**Then**:
- 现有代码 MUST 无需修改即可编译
- 现有代码 MUST 无需修改即可正常运行
- Clamp 行为 MUST 与升级前完全一致

#### Scenario: 数据表序列化兼容性

**Given**: 现有属性定义数据表未包含 `ClampStrategyClass` 字段

**When**: 加载数据表

**Then**:
- 数据表 MUST 正常加载
- 新增的 `ClampStrategyClass` 字段 MUST 使用构造函数默认值
- 数据表 MUST 无需重新保存即可正常使用

### Requirement: 性能优化

系统 MUST 最小化策略模式引入的性能开销，并保证性能一致性。

#### Scenario: 使用 CDO 避免重复创建对象

**Given**: 属性定义指定了 `ClampStrategyClass`

**When**: 多次调用 `ClampAttributeValueInRange`

**Then**:
- 系统 MUST 使用策略类的 CDO
- 系统 MUST NOT 每次调用都创建新的策略对象
- CDO MUST 在整个游戏生命周期中复用

#### Scenario: 统一策略化的性能一致性

**Given**: 所有属性都通过策略对象执行 Clamp

**When**: 执行属性值 Clamp

**Then**:
- 所有属性 MUST 使用相同的代码路径（策略对象调用）
- 性能 MUST 一致且可预测
- 不存在分支逻辑导致的性能差异

#### Scenario: 防御性 fallback 的性能影响

**Given**: 属性定义的 `ClampStrategyClass` 为 nullptr（理论上不应发生）

**When**: 执行防御性 fallback

**Then**:
- fallback 逻辑 MUST 使用内联的 `FMath::Clamp`
- fallback 逻辑 MUST 记录 Warning 日志（仅执行一次）
- fallback 逻辑 MUST NOT 显著影响性能

### Requirement: 代码规范和文档

所有新增代码 MUST 遵循项目代码规范和文档要求。

#### Scenario: 文件头部规范

**Given**: 创建新的代码文件

**When**: 检查文件头部

**Then**:
- 文件 MUST 包含 `// Copyright Tirefly. All Rights Reserved.`
- 文件 MUST 遵循空行缩进规范
- 文件 MUST 包含正确的 include 顺序

#### Scenario: 类和方法注释

**Given**: 创建新的类或方法

**When**: 检查代码注释

**Then**:
- 类 MUST 包含完整的类注释，说明用途
- 公共方法 MUST 包含完整的方法注释
- 参数 MUST 包含 `@param` 注释
- 返回值 MUST 包含 `@return` 注释

#### Scenario: Meta 标签规范

**Given**: 创建新的 UCLASS 或 UPROPERTY

**When**: 检查 Meta 标签

**Then**:
- UCLASS MUST 包含 `DisplayName` Meta 标签（如果需要）
- UPROPERTY MUST 包含 `Tooltip` Meta 标签（如果需要）
- Meta 标签 MUST 使用中文（与项目约定一致）

### Requirement: 避免头文件循环引用

系统 MUST 避免头文件之间的循环引用，特别是策略类和属性定义之间。

#### Scenario: 构造函数默认值在 .cpp 文件中设置

**Given**: `FTcsAttributeDefinition` 需要设置默认策略

**When**: 实现构造函数

**Then**:
- 构造函数 MUST 在 .cpp 文件中实现
- 构造函数 MUST 引入策略类头文件（`TcsAttrClampStrategy_Linear.h`）
- 头文件（`TcsAttribute.h`）MUST NOT 引入策略类头文件
- 头文件 MUST 使用前置声明（如果需要）

#### Scenario: 编译依赖最小化

**Given**: 修改策略类实现

**When**: 重新编译项目

**Then**:
- 修改策略类 .cpp 文件 MUST NOT 触发 `TcsAttribute.h` 的重新编译
- 只有引用策略类的 .cpp 文件需要重新编译
- 编译时间 MUST 最小化
