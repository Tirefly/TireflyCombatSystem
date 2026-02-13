# Tasks for fix-state-safety-and-api

## 实施任务列表

### 1. 修复 SetDurationRemaining 空指针风险
- [x] 在 `UTcsStateInstance::SetDurationRemaining()` 中添加 `OwnerStateCmp` 有效性检查后的 return 语句
- [x] 验证修复后不会崩溃

### 2. 加强条件验证逻辑
- [x] 修改 `UTcsStateManagerSubsystem::CheckStateApplyConditions()` 中的条件验证逻辑，当条件无效时直接返回 false
- [x] 验证修复后无效条件会阻止状态应用

### 3. 明确等级数组参数使用约定
- [x] 更新 `FTcsStateNumericParam_StateLevelArray` 结构体的注释，明确数组下标约定
- [x] 更新 `FTcsStateNumericParam_InstigatorLevelArray` 结构体的注释，明确数组下标约定
- [x] 更新 `UTcsStateNumericParamEvaluator_StateLevelArray::Evaluate_Implementation()` 的注释
- [x] 更新 `UTcsStateNumericParamEvaluator_InstigatorLevelArray::Evaluate_Implementation()` 的注释

### 4. 新增 Map 版等级参数 Evaluator
- [x] 创建 `TcsStateNumericParameter_StateLevelMap.h` 和 `.cpp`
- [x] 实现 `FTcsStateNumericParam_StateLevelMap` 结构体（使用 TMap<int32, float>）
- [x] 实现 `UTcsStateNumericParamEvaluator_StateLevelMap` 类
- [x] 创建 `TcsStateNumericParameter_InstigatorLevelMap.h` 和 `.cpp`
- [x] 实现 `FTcsStateNumericParam_InstigatorLevelMap` 结构体（使用 TMap<int32, float>）
- [x] 实现 `UTcsStateNumericParamEvaluator_InstigatorLevelMap` 类

### 5. 完善状态等级 API
- [x] 为 `UTcsStateManagerSubsystem::TryApplyStateToTarget()` 添加可选的 `StateLevel` 参数（默认值 1）
- [x] 更新函数实现，将 `StateLevel` 参数传递给 `CreateStateInstance()`
- [x] 更新函数注释，说明新参数的用途

### 6. 编译验证
- [x] 刷新 Unreal Engine 项目文件
- [x] 编译 Editor Target (Development 配置)
- [x] 解决所有编译错误和警告

### 7. 测试验证
- [x] 测试空指针修复：验证 `OwnerStateCmp` 无效时不会崩溃
- [x] 测试条件验证：验证无效条件会阻止状态应用
- [x] 测试 Map 版 Evaluator：验证等级参数正确解析
- [x] 测试状态等级 API：验证可以按等级施加状态

## 依赖关系
- 任务 1-3 可以并行执行
- 任务 4 依赖任务 3（需要先明确约定）
- 任务 5 独立，可以并行执行
- 任务 6 依赖所有代码修改任务（1-5）
- 任务 7 依赖任务 6

## 验证标准
- 所有代码符合项目的代码规范（命名、注释、空行等）
- 编译无错误和警告
- 所有测试通过
- 向后兼容性得到保证
