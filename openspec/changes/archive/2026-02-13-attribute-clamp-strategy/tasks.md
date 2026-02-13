# Tasks: 属性 Clamp 功能策略模式化

## Phase 1: 创建策略基类和默认实现

- [x] 创建 `Source/TireflyCombatSystem/Public/Attribute/AttrClampStrategy/` 目录
- [x] 创建 `TcsAttributeClampStrategy.h` 抽象基类
  - [x] 定义 `Clamp` BlueprintNativeEvent 接口
  - [x] 添加完整的类注释和参数说明
  - [x] 标记为 `Abstract`, `BlueprintType`, `Blueprintable`
- [x] 创建 `TcsAttributeClampStrategy.cpp` 实现文件
  - [x] 实现默认的 `Clamp_Implementation`（直接返回 Value）
- [x] 创建 `TcsAttrClampStrategy_Linear.h` 默认线性策略
  - [x] 继承 `UTcsAttributeClampStrategy`
  - [x] 添加 DisplayName Meta 标签
- [x] 创建 `TcsAttrClampStrategy_Linear.cpp` 实现文件
  - [x] 实现 `Clamp_Implementation`，使用 `FMath::Clamp`
- [x] 创建 `Source/TireflyCombatSystem/Private/Attribute/AttrClampStrategy/` 目录

## Phase 2: 扩展属性定义结构

- [x] 修改 `TcsAttribute.h` 中的 `FTcsAttributeDefinition`
  - [x] 添加 `ClampStrategyClass` 字段（`TSubclassOf<UTcsAttributeClampStrategy>`）
  - [x] 设置字段为 `EditAnywhere, BlueprintReadOnly`
  - [x] 添加字段注释，说明默认使用线性 Clamp 策略
  - [x] 将字段放在 "Range" 分类下
- [x] 创建 `TcsAttribute.cpp` 实现文件
  - [x] 包含正确的文件头（Copyright Tirefly. All Rights Reserved.）
  - [x] 引入 `TcsAttrClampStrategy_Linear.h`
  - [x] 实现 `FTcsAttributeDefinition` 构造函数
  - [x] 在构造函数中设置 `ClampStrategyClass = UTcsAttrClampStrategy_Linear::StaticClass()`

## Phase 3: 更新 Clamp 函数实现

- [x] 修改 `TcsAttributeManagerSubsystem.cpp` 中的 `ClampAttributeValueInRange` 函数
  - [x] 在计算 MinValue 和 MaxValue 后，获取属性的 ClampStrategyClass
  - [x] 移除原有的硬编码 `FMath::Clamp` 逻辑
  - [x] 添加统一的策略对象调用逻辑：
    - [x] 如果 ClampStrategyClass 有效，获取 CDO 并调用 Clamp 方法
    - [x] 如果 ClampStrategyClass 为 nullptr（防御性），记录 Warning 日志并使用 `FMath::Clamp` 作为 fallback
  - [x] 添加日志（Verbose 级别）记录使用的策略类型
  - [x] 确保 OutMinValue 和 OutMaxValue 输出逻辑不变

## Phase 4: 代码规范和注释

- [x] 确保所有新文件包含正确的文件头（Copyright Tirefly. All Rights Reserved.）
- [x] 确保所有新文件遵循空行缩进规范
- [x] 为 `UTcsAttributeClampStrategy` 添加完整的类注释
  - [x] 说明策略模式的用途
  - [x] 提供自定义策略的示例
  - [x] 说明默认行为（线性 Clamp）
- [x] 为 `UTcsAttrClampStrategy_Linear` 添加类注释
  - [x] 说明这是默认实现
  - [x] 说明行为（FMath::Clamp）
- [x] 为 `ClampStrategyClass` 字段添加详细的 Tooltip
  - [x] 说明默认使用线性 Clamp 策略
  - [x] 说明可以选择其他策略或自定义策略
  - [x] 提供使用示例
- [x] 为 `FTcsAttributeDefinition` 构造函数添加注释
  - [x] 说明默认策略的设置

## Phase 5: 编译和验证

- [x] 刷新 Unreal Engine 项目文件
  - [x] 运行 UnrealBuildTool 生成项目文件
- [x] 编译项目（Development Editor 配置）
  - [x] 确保无编译错误
  - [x] 确保无编译警告
- [x] 验证向后兼容性
  - [x] 确认现有属性定义（未显式设置 ClampStrategyClass）自动使用默认线性 Clamp
  - [x] 测试属性值变更时 Clamp 行为正确
  - [x] 验证构造函数默认值生效
- [x] 验证完全策略化
  - [x] 确认所有属性都通过策略对象执行 Clamp
  - [x] 验证不存在硬编码的 `FMath::Clamp` 分支（除了防御性 fallback）
  - [x] 检查日志输出，确认策略被正确调用

## Phase 6: 文档更新（可选）

- [x] 更新 `CLAUDE.md` 中的架构说明
  - [x] 添加属性 Clamp 策略的说明
  - [x] 列出内置策略类
- [x] 更新 `Documents/` 中的相关文档（如果存在）
  - [x] 说明如何自定义 Clamp 策略（标记为不需要）
  - [x] 提供蓝图和 C++ 示例（标记为不需要）

## Dependencies

- Phase 2 依赖 Phase 1（需要先定义策略类）
- Phase 3 依赖 Phase 1 和 Phase 2（需要策略类和字段定义）
- Phase 5 依赖 Phase 1-4（需要所有代码完成）

## Validation Checklist

- [x] 所有新文件遵循项目代码规范
- [x] 所有公共 API 包含完整注释
- [x] 编译通过，无警告
- [x] 现有项目无需修改即可正常运行
- [x] 所有属性统一通过策略对象执行 Clamp
- [x] 构造函数默认值正确设置为 `UTcsAttrClampStrategy_Linear::StaticClass()`
- [x] 可以通过继承 `UTcsAttributeClampStrategy` 实现自定义策略
- [x] 自定义策略可以在编辑器中配置
- [x] 自定义策略可以在蓝图中实现
- [x] 头文件无循环引用问题
