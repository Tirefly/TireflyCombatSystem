## 1. 共享参数 Evaluator 默认值
- [ ] 1.1 将 `UTcsStateBoolParamEvaluator` 与 `UTcsStateVectorParamEvaluator` 调整为可直接 authoring 的 constant evaluator
- [ ] 1.2 统一 Numeric / Bool / Vector 三类 shared evaluator 的 payload 命名与 constant payload 解析语义

## 2. SkillModifier typed evaluator 扩展
- [ ] 2.1 将 `UTcsSkillModifierDefinition` 从单一 `EvaluatorClass` 扩展为 Numeric / Bool / Vector 三个类型化字段
- [ ] 2.2 将 `FTcsSkillModifierDefRow` 与同步映射更新为同构的三字段结构
- [ ] 2.3 为 SkillModifierDef 的三类 evaluator 默认指向 `Addition` / `SetBool` / `SetVector`

## 3. DefAsset 策略字段默认值与校验
- [ ] 3.1 为支持默认值的 DefAsset 策略字段补齐 concrete 默认类
- [ ] 3.2 在保存、同步与校验路径上对缺失 / 抽象 / 类型不匹配的策略字段输出勘误
- [ ] 3.3 保持 `AttributeModifierDef.ModifierType` 与 `ActiveConditions` 等非默认项不被强行补值

## 4. DataTable ↔ DefAsset 同步
- [ ] 4.1 DataTableRow → DefAsset 同步时对缺失的默认策略字段进行归一化补值
- [ ] 4.2 DefAsset → DataTableRow 同步时把归一化后的 concrete 默认类回写到对应 RowStruct
- [ ] 4.3 明确 DataTableRow 不承担 DefAsset 级别的有效性校验、错误日志或通知提示

## 5. 验证
- [ ] 5.1 执行 `openspec validate add-def-strategy-defaults-and-validation --strict --no-interactive`
- [ ] 5.2 编译受影响模块并验证新增字段、默认值与同步描述符不破坏现有 authoring 流程