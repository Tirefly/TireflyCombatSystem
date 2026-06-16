# Tasks: StateParamInstance 类型拆分

## 1. 类型定义

- [x] 1.1 新建 `FTcsNumericStateParamInstance` / `FTcsBoolStateParamInstance` / `FTcsVectorStateParamInstance` USTRUCT
- [x] 1.2 各类型实现 `Initialize` 方法（均含 CDO 校验和缓存）
- [x] 1.3 各类型实现内联求值（`PopulateStateParamInstances` 中调用对应 Evaluator）
- [x] 1.4 删除 `FTcsStateParamInstance`

## 2. UTcsStateInstance 容器拆分

- [x] 2.1 `StateParamInstances` TMap → `NumericParamInstances` / `BoolParamInstances` / `VectorParamInstances`
- [x] 2.2 新增三个类型化访问器方法（virtual）
- [x] 2.3 `PopulateStateParamInstances` 按类型分桶
- [x] 2.4 `GetNumericParamByTag` / `SetNumericParamByTag` / `GetBoolParamByTag` 系列适配新容器

## 3. UTcsSkillEntry 容器拆分

- [x] 3.1 `StateParamInstances` TMap → 三容器同构
- [x] 3.2 `InitializeFromDef` 按类型分桶
- [x] 3.3 `StartCooldown` / `GetRemainingCooldownRatio` 适配 Numeric 容器

## 4. UTcsSkillInstance 覆写适配

- [x] 4.1 覆写改为三个类型化方法指向 Entry 对应容器
- [x] 4.2 `PopulateStateParamInstances` 保持空实现

## 5. ResolveStateParamInstances 收窄

- [x] 5.1 更名为 `ResolveNumericParamInstances`，返回 Numeric TMap*
- [x] 5.2 三层回退逻辑中 `SourceSkillEntry->NumericParamInstances` 适配

## 6. 所有调用方适配

- [x] 6.1 `RecalculateAttributeCurrentValues` — Operand 刷新去掉 `CachedEvaluator` 空判
- [x] 6.2 `TcsAttributeComponent_Calculation.cpp` 中 Resolve 调用的重命名
- [x] 6.3 其他所有引用 `FTcsStateParamInstance` / `StateParamInstances` 的文件

## 7. 验证

- [x] 7.1 编译通过
- [x] 7.2 `openspec validate` 通过
