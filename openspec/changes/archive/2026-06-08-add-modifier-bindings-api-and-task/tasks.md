## 1. API 改造

- [x] 1.1 新增 `CreateAttributeModifierWithBindings` — 从 DefAsset 复制默认 Operands，设置 OperandBindings，首次 Recalculate 时拉取初值
- [x] 1.2 删除 `CreateAttributeModifierWithOperands`
- [x] 1.3 清理 `TcsAttributeManagerSubsystem` 旧 API 转发（已确认无残留）

## 2. StateTree Task

- [x] 2.1 新建 `ApplyAttributeModifierToOwner` + `ApplyAttributeModifierToTarget`（两个 Task 替代单一通用版本）
- [x] 2.2 EnterState: `CreateAttributeModifierWithBindings` → `ApplyModifier`
- [x] 2.3 ExitState: `SourceHandle → RemoveModifiersBySourceHandle` 自动清理

## 3. HandleModifierUpdated

- [x] 3.1 维持"拉取"模式：Recalculate 时自动刷新
- [x] 3.2 已确认不添加额外驱动点

## 4. 验证

- [x] 4.1 编译验证（Win64 Development）✅
