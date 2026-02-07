# Proposal: 简化状态持续时间计算策略

## Why

`ETcsDurationTickPolicy` 枚举的 5 个枚举值与 HangUp/Pause 的语义存在严重冲突，导致配置复杂度高、维护成本高、用户难以理解。HangUp 和 Pause 的语义已经足够表达持续时间计算的需求，不需要额外的枚举配置。

## What Changes

删除 `ETcsDurationTickPolicy` 枚举及相关配置项，简化状态持续时间计算逻辑为：
- Active 和 HangUp 阶段：继续计时
- Pause 阶段：完全冻结
- 更新枚举注释，明确 HangUp 和 Pause 的语义（包括叠层合并行为）

## 动机

### 当前问题

1. **语义冲突严重**：
   - `ETcsDurationTickPolicy` 的 5 个枚举值与 HangUp/Pause 的语义存在多处冲突
   - `DTP_Always` 承诺"始终计时"，但 Pause 状态默认不计时（违背语义）
   - HangUp 的注释说"仍然计算持续时间"，但在 `DTP_ActiveOnly` 下不计时（注释与实际不符）

2. **配置复杂度高**：
   - 需要理解 5 个枚举值的含义和差异
   - 需要配合 `bFreezeDurationWhenPaused` 使用，增加理解成本
   - HangUp 在不同策略下行为不一致，用户难以预测

3. **维护成本高**：
   - 持续时间计算逻辑复杂（~70 行代码）
   - 每次修改都要考虑 5 种策略的组合
   - 注释与实际行为不符，容易误导开发者

4. **过度设计**：
   - 大多数场景只需要简单的"是否计时"判断
   - 5 个枚举值覆盖的场景中，90% 只需要 2-3 种配置

### 核心洞察

**HangUp 和 Pause 的语义已经足够表达持续时间计算的需求**：
- **HangUp**：逻辑暂停，时间继续（适用于技能 CD、持续 Buff 等）
- **Pause**：完全冻结（时间 + 逻辑）（适用于需要完全暂停的场景）

用户通过 `GateCloseBehavior` 和 `PreemptionPolicy` 选择 HangUp 或 Pause，已经隐式决定了持续时间计算行为，不需要额外的 `ETcsDurationTickPolicy` 配置。

## 提议的变更

### 删除的内容

1. **删除枚举定义**：
   - `ETcsDurationTickPolicy` 枚举（5 个枚举值）

2. **删除配置字段**：
   - `FTcsStateSlotDefinition::DurationTickPolicy`
   - `FTcsStateSlotDefinition::bFreezeDurationWhenPaused`

### 简化的逻辑

**新的持续时间计算规则**（极简）：
```
状态阶段 → 持续时间计算
├─ Active:   始终计时 ✅
├─ HangUp:   始终计时 ✅
├─ Pause:    始终冻结 ❌
└─ Inactive: 不计时 ❌
```

**代码简化**：
- 持续时间计算逻辑从 ~70 行减少到 ~20 行
- 删除所有 switch-case 分支判断
- 只需要判断状态阶段即可

### 更新的注释

更新 `ETcsStateSlotGateClosePolicy` 和 `ETcsStatePreemptionPolicy` 的注释，明确说明：
- **持续时间**：
  - HangUp：继续计时
  - Pause：完全冻结
- **叠层合并**：
  - HangUp 和 Pause 都**正常参与叠层合并**（保持叠层数准确）
  - 修正之前注释中"叠层计算暂停"的错误说法
- **逻辑执行**：
  - HangUp 和 Pause 都暂停 StateTree 执行

## 影响范围

### 受影响的文件

1. **TcsStateSlot.h**：
   - 删除 `ETcsDurationTickPolicy` 枚举定义
   - 删除 `DurationTickPolicy` 和 `bFreezeDurationWhenPaused` 字段
   - 更新 `ETcsStateSlotGateClosePolicy` 和 `ETcsStatePreemptionPolicy` 的注释

2. **TcsStateComponent.cpp**：
   - 简化 `TickDurationTracker` 函数中的持续时间计算逻辑
   - 删除所有与 `DurationTickPolicy` 相关的判断
   - 删除 `bFreezeDurationWhenPaused` 的判断

3. **调试输出**：
   - 更新 `GetStateSlotDebugString` 函数，移除 `DurationTickPolicy` 的输出

### 向后兼容性

**无需迁移**：
- 编辑器中没有任何现有数据引用这些配置项
- 可以直接删除，不需要数据迁移逻辑

### 用户体验改进

**配置简化**：
- 用户只需要理解 HangUp 和 Pause 的区别
- 通过 `GateCloseBehavior` 和 `PreemptionPolicy` 直接选择行为
- 不需要额外配置持续时间计算策略

**行为可预测**：
- HangUp 始终计时（无例外）
- Pause 始终冻结（无例外）
- 语义清晰，无歧义

## 实施计划

### Phase 1: 删除枚举和配置字段
- 删除 `ETcsDurationTickPolicy` 枚举定义
- 删除 `DurationTickPolicy` 和 `bFreezeDurationWhenPaused` 字段

### Phase 2: 简化持续时间计算逻辑
- 重写 `TickDurationTracker` 函数
- 删除所有 switch-case 分支
- 只保留状态阶段判断

### Phase 3: 更新注释和文档
- 更新枚举注释，明确 HangUp 和 Pause 的语义
- 更新相关文档

### Phase 4: 验证和测试
- 编译验证
- 运行现有测试
- 手动测试典型场景

## 风险评估

### 低风险

1. **无数据迁移风险**：
   - 编辑器中没有现有数据引用
   - 可以直接删除

2. **逻辑简化风险低**：
   - 新逻辑更简单、更清晰
   - 减少了出错的可能性

3. **测试覆盖充分**：
   - 现有测试可以验证基本功能
   - 简化后的逻辑更容易测试

### 潜在影响

1. **行为变更**：
   - 之前使用 `DTP_ActiveOnly` 的槽位，HangUp 状态将开始计时
   - 但由于没有现有数据，这不是问题

2. **灵活性降低**：
   - 无法实现"仅 Gate 开启时计时，Active 和 HangUp 都不计时"的场景
   - 但这种场景在实际中极少见，可以接受

## 成功标准

1. **代码简化**：
   - 持续时间计算逻辑减少到 20 行以内
   - 删除所有枚举和配置项

2. **语义清晰**：
   - HangUp 和 Pause 的行为明确且一致
   - 注释与实际行为完全一致

3. **编译通过**：
   - 所有配置（Debug、Development、Shipping）编译通过
   - 无编译警告

4. **测试通过**：
   - 所有现有测试通过
   - 手动测试验证基本功能

## 替代方案

### 方案 A：保留部分枚举值
- 保留 `DTP_ActiveOnly` 和 `DTP_Always` 两个枚举值
- **缺点**：仍然存在语义冲突，维护成本降低有限

### 方案 B：添加布尔配置
- 添加 `bTickDurationWhenHangUp` 和 `bTickWhenGateClosed` 配置
- **缺点**：增加配置复杂度，与 HangUp/Pause 的语义重叠

### 方案 C：完全删除（推荐）
- 删除所有枚举和配置，只保留状态阶段判断
- **优点**：最简洁、最清晰、维护成本最低

## 相关工作

- 依赖于已完成的 `refactor-state-slot-activation-integrity` 重构
- 与 `state-gate-close-refactor` 规范保持一致
- 为未来的状态系统优化奠定基础
