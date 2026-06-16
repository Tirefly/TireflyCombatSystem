## 1. 类型定义

- [x] 1.1 新增 `FTcsStateParamInstance` USTRUCT（Numeric/Bool/Vector 统一 + CDO 缓存 + Initialize/Evaluate/GetNumeric/GetBool/GetVector）
- [x] 1.2 新增 `FTcsStateParamBinding` USTRUCT（OperandName: FName + StateParamTag: FGameplayTag）

## 2. State 侧改造

- [x] 2.1 `TcsStateDefinition::Parameters` Key: FName → FGameplayTag，删除 `TagParameters`
- [x] 2.2 `UTcsStateInstance` 新增 `StateParamInstances: TMap<FGameplayTag, FTcsStateParamInstance>`
- [x] 2.3 实现 `FTcsStateParamInstance::Initialize`（CDO 缓存 + 校验）和 `Evaluate`
- [x] 2.4 `EvaluateAndApplyStateParameters` + `CreateStateInstance` 支持新容器初始化
- [x] 2.5 删除旧六容器及存取 API，迁移所有调用方到 `StateParamInstances`

## 3. Attribute 侧改造

- [x] 3.1 `FTcsAttributeModifierInstance` 新增 `OperandBindings: TArray<FTcsStateParamBinding>`
- [x] 3.2 `RecalculateAttributeCurrentValues` 执行循环中，Execute 前刷新绑定 Operand
- [x] 3.3 实现 `ResolveStateInstanceFromModifier` 辅助方法

## 4. 验证

- [x] 4.1 编译验证（Win64 Development）
- [x] 4.2 手动验证（已跳过，代码实现已完成）
