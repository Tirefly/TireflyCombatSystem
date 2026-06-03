## 1. 提案与规格

- [x] 1.1 新建独立 change，明确该能力是在 `BuffInstance` 单根上下文之上的桥接 evaluator，而不是恢复 `StateInstance` 根上下文。
- [x] 1.2 为 `buff-state-tree-objectref-bridge` 写清 requirement 与 scenario，固定“单 evaluator、成组输出、保持弱引用语义”的约束。

## 2. 运行时实现

- [x] 2.1 新增 `FTcsSTEvaluator_ObjectRefInstanceData`，成组暴露 `Owner / Instigator` 及对应组件输出。
- [x] 2.2 新增 `FTcsSTEvaluator_ObjectRef`，通过 `TStateTreeExternalDataHandle<UTcsStateInstance>` 读取共享执行态外部数据。
- [x] 2.3 在 `TreeStart` 时把 `UTcsStateInstance` getter 返回值同步到桥接输出，并在 `TreeStop` 时清空输出。
- [x] 2.4 保持输出为 `TWeakObjectPtr`，不把运行时弱引用桥接成额外强引用。

## 3. 文档与验证

- [x] 3.1 在 `README.md` 中补充 BuffStateTree 使用桥接 evaluator 的说明。
- [x] 3.2 在 `文档：TCS编辑器测试方案（State-Buff-Attribute）.md` 中补充桥接 evaluator 的最小使用说明。
- [x] 3.3 编译 `TireflyGameplayUtilsEditor Win64 Development`。
- [x] 3.4 执行 `openspec validate add-buff-state-objectref-bridge --strict --no-interactive`。
- [ ] 3.5 等待开发者手动在编辑器中确认：BuffStateTree 中新增 evaluator 后，`TcsSTTask_StateChangeNotify.StateComponent` 可绑定到 `Owner State Component`，且其他 Task / Evaluator / Condition 可消费同组输出。