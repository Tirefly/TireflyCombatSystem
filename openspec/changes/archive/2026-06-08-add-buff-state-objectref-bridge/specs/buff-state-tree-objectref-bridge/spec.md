## ADDED Requirements

### Requirement: BuffStateTree 必须提供成组的 StateInstance ObjectRef 桥接输出
TCS SHALL 为 BuffStateTree 提供一个可复用的桥接 evaluator，用单个节点把 `UTcsStateInstance` 的 ObjectRef 运行时引用成组暴露为可绑定输出，而不是要求开发者为每个引用单独添加桥接节点。

#### Scenario: 单 evaluator 暴露整组 ObjectRef 输出
- **WHEN** 开发者在一个运行于 `UTcsBuffInstance` 的 BuffStateTree 中添加该桥接 evaluator
- **THEN** 该 evaluator 应暴露至少以下 `Output` 属性：`Owner`、`OwnerController`、`OwnerStateComponent`、`OwnerBuffComponent`、`OwnerAttributeComponent`、`OwnerSkillComponent`、`Instigator`、`InstigatorController`、`InstigatorStateComponent`、`InstigatorBuffComponent`、`InstigatorAttributeComponent`、`InstigatorSkillComponent`
- **AND** BuffStateTree 中的 Task、Evaluator 与 Condition 应可以通过常规 StateTree 绑定面板消费这组输出

### Requirement: 桥接 evaluator 不得破坏 BuffStateTree 的单根上下文契约
TCS SHALL 保持 BuffStateTree 的 root context 为单一 `BuffInstance`；ObjectRef 桥接能力必须通过 evaluator 输出提供，而不是重新引入 `StateInstance` 作为第二个 root context。

#### Scenario: BuffStateTree 继续只暴露 BuffInstance 根上下文
- **WHEN** 开发者创建或编辑一个 BuffStateTree
- **THEN** schema 根上下文仍应保持为 `BuffInstance`
- **AND** 系统不应因为引入桥接 evaluator 而重新暴露 generic `StateInstance` root context

### Requirement: 桥接输出必须保持弱引用语义
TCS SHALL 保持桥接输出与 `UTcsStateInstance` 原始 ObjectRef 字段一致的弱引用语义，不得为了绑定便利而新增额外强引用持有关系。

#### Scenario: 桥接 evaluator 同步运行时弱引用并在停止时清空
- **WHEN** BuffStateTree 启动且桥接 evaluator 完成初始化
- **THEN** evaluator 应从 `UTcsStateInstance` getter 同步当前 ObjectRef 到自身输出
- **AND** 输出应保持为弱引用类型
- **WHEN** BuffStateTree 停止
- **THEN** evaluator 应清空自身输出，避免残留旧引用的调试显示值