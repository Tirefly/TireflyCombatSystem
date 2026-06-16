## 背景

详见 `Documents/设计：StateParam动态绑定与AttributeModifier自动更新.md`。

## 目标 / 非目标

- 目标：StateParam 运行时 Instance 化、Operand 动态绑定、"拉取"模式刷新
- 非目标：SkillModifier 实现、BaseValue Modifier 生命周期补全、StateTree Task 节点

## 决策

| 决策 | 结论 |
|------|------|
| 统一 Key | GameplayTag（删除 FName + TagParameters 冗余） |
| Instance 存储位置 | StateInstance 上（避免 Merge 丢失） |
| 刷新时机 | 唯一入口：RecalculateAttributeCurrentValues 执行前 |
| 刷新模式 | "拉取"（StateParam 变化被动，重算时主动读最新值） |
| OperandName 类型 | 保留 FName（ModifierDef 内部约定，不参与跨系统引用） |
