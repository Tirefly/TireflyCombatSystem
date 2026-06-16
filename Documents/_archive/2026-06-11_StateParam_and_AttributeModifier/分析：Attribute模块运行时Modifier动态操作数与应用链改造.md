# Attribute 模块运行时 Modifier 动态操作数与应用链改造分析

## 1. 文档定位

本文档记录 Attribute 模块的架构分析与改造方向。所有 Phase 已完成，详见 `Documents/设计：StateParam动态绑定与AttributeModifier自动更新.md`。

## 2. 已完成的优化

| 目标 | 方案 |
|------|------|
| DefAsset 作为模板 | `OperandBindings` 动态绑定到 StateParamInstance |
| 运行时 operand override | `RecalculateAttributeCurrentValues` 执行前自动刷新 |
| PerfCache | `CachedMergedModifiers` + `bMergedModifiersDirty` + `CachedBaseValuesSnapshot` |
| StateParam 统一运行时 Instance 化 | `FTcsStateParamInstance` 替代六容器 |
| 创建 API | `CreateAttributeModifierWithBindings` 替代 `CreateAttributeModifierWithOperands` |
| StateTree Task | `ApplyAttributeModifierToOwner` + `ApplyAttributeModifierToTarget` |
| BuffPeriod | `TcsSTEvaluator_BuffPeriod` Evaluator 替代 Task |
