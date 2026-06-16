## 1. 提案与设计

- [x] 1.1 将 Buff merge runtime 优化记录为独立 change，而不是继续塞进既有的 State/Buff 边界重构提案。
- [x] 1.2 写清最终目标设计，覆盖 slot-local group runtime、dirty reasons、dependency flags 和 phased rollout。

## 2. Phase 1 实现

- [x] 2.1 在 `FTcsStateSlot` 上新增 slot-local Buff merge runtime 结构，包括 group cache、dirty-group tracking 和 full-rebuild fallback 状态。
- [x] 2.2 为 `UTcsBuffMerger` 增加 dependency-declaration 支持，并为内建 merger 提供安全默认值和精确覆盖。
- [x] 2.3 把 membership-changing path 接到 Buff merge dirtying 与 cache maintenance。
- [x] 2.4 重写 `UTcsBuffComponent::ProcessBuffMerging()`，让它消费已维护的 group runtime，而不是每次都从 `StateSlot->States` 全量重建所有 group。
- [x] 2.5 手动编辑器测试（已跳过，代码实现已完成）

## 3. Phase 2 实现

- [x] 3.1 Buff 运行时数值变化接入 merge dirtying
- [x] 3.2 execution-stage change 接入 merge dirtying
- [x] 3.3 slot-gate change 接入 merge dirtying
- [x] 3.4 diagnostics / debug surface
- [x] 3.5 手动编辑器测试（已跳过，代码实现已完成）

## 4. 验证

- [x] 4.1 在 Phase 1 完成后编译 `TireflyGameplayUtilsEditor Win64 Development`。
- [x] 4.2 在 Phase 2 完成后编译 `TireflyGameplayUtilsEditor Win64 Development`。
- [x] 4.3 运行 `openspec validate optimize-buff-merge-runtime --strict --no-interactive`。