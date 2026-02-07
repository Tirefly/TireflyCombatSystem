# Spec: 属性动态范围约束传播

## 概述

本规范定义属性系统的动态范围约束传播机制,确保属性范围约束(如 HP <= MaxHP)在任意时刻都正确,包括多跳依赖场景。

## ADDED Requirements

### Requirement: 范围约束传播机制 MUST 在属性重算后执行

**优先级**: P0 (关键)

**描述**:
Attribute system MUST provide range constraint propagation mechanism, executed after attribute value recalculation, to ensure all attributes' BaseValue and CurrentValue satisfy their defined range constraints. 属性系统必须提供范围约束传播机制,在属性值重算后执行,确保所有属性的 BaseValue 和 CurrentValue 都满足其定义的范围约束。

#### Scenario: 重算后执行范围约束

**Given** 属性 `"HP"` 的动态上限绑定到属性 `"MaxHP"`

**And** `"MaxHP"` 的 CurrentValue 为 `100.0`

**And** `"HP"` 的 CurrentValue 为 `100.0`

**When** 移除 MaxHP Buff,导致 `"MaxHP"` 的 CurrentValue 降为 `80.0`

**And** 执行 `RecalculateAttributeCurrentValues`

**Then** 必须在重算后执行范围约束传播

**And** `"HP"` 的 CurrentValue 必须被 Clamp 到 `80.0`

**And** 不变量 `HP <= MaxHP` 必须成立

---

### Requirement: MUST 支持多跳依赖收敛

**优先级**: P0 (关键)

**描述**:
Range constraint propagation MUST support multi-hop dependencies (e.g., HP depends on MaxHP, MaxHP depends on Level) through iterative algorithm to ensure all constraints converge. 范围约束传播必须支持多跳依赖(如 HP 依赖 MaxHP,MaxHP 依赖 Level),通过迭代算法确保所有约束收敛。

#### Scenario: 多跳依赖收敛

**Given** 属性 `"Level"` 的 CurrentValue 为 `10`

**And** 属性 `"MaxHP"` 的动态上限绑定到 `"Level" * 10`,当前值为 `100.0`

**And** 属性 `"HP"` 的动态上限绑定到 `"MaxHP"`,当前值为 `100.0`

**When** `"Level"` 降为 `5`

**And** 执行范围约束传播

**Then** 第一次迭代:
- `"MaxHP"` 被 Clamp 到 `50.0`

**And** 第二次迭代:
- `"HP"` 被 Clamp 到 `50.0`

**And** 第三次迭代:
- 没有任何值变化,收敛完成

**And** 最终 `"Level" = 5`, `"MaxHP" = 50.0`, `"HP" = 50.0`

---

### Requirement: MUST 检测循环依赖

**优先级**: P1 (重要)

**描述**:
Range constraint propagation MUST detect circular dependencies to avoid infinite loops and output warning logs. 范围约束传播必须检测循环依赖,避免无限循环,并输出警告日志。

#### Scenario: 检测循环依赖

**Given** 属性 `"A"` 的动态上限绑定到 `"B"`

**And** 属性 `"B"` 的动态上限绑定到 `"A"` (循环依赖)

**When** 执行范围约束传播

**Then** 必须在达到最大迭代次数(如 8 次)后停止

**And** 必须输出 Warning 级别的日志,包含 `"Max iterations reached"` 和 `"possible circular dependency"` 信息

**And** 不应崩溃或进入无限循环

---

### Requirement: MUST 使用工作集解析 In-Flight 值

**优先级**: P0 (关键)

**描述**:
Range constraint propagation MUST use "working values" (working set) instead of "committed values" to resolve dynamic ranges, ensuring latest values can be read during iteration. 范围约束传播必须使用"工作集"(working values)而不是"已提交值"(committed values)来解析动态范围,确保在迭代过程中能读取到最新的值。

#### Scenario: 使用工作集解析动态范围

**Given** 属性 `"HP"` 的动态上限绑定到 `"MaxHP"`

**And** 在范围约束传播的第一次迭代中,`"MaxHP"` 的工作集值被更新为 `80.0`

**And** `"MaxHP"` 的已提交值仍为 `100.0`

**When** 在第二次迭代中解析 `"HP"` 的动态上限

**Then** 必须读取 `"MaxHP"` 的工作集值 `80.0`,而不是已提交值 `100.0`

**And** `"HP"` 必须被 Clamp 到 `80.0`

---

### Requirement: MUST 同时约束 BaseValue 和 CurrentValue

**优先级**: P0 (关键)

**描述**:
Range constraint propagation MUST enforce constraints on both BaseValue and CurrentValue to ensure both satisfy range definitions. 范围约束传播必须同时对 BaseValue 和 CurrentValue 执行约束,确保两者都满足范围定义。

#### Scenario: 同时约束 BaseValue 和 CurrentValue

**Given** 属性 `"HP"` 的动态上限绑定到 `"MaxHP"`

**And** `"MaxHP"` 的 CurrentValue 降为 `80.0`

**And** `"HP"` 的 BaseValue 为 `100.0`,CurrentValue 为 `100.0`

**When** 执行范围约束传播

**Then** `"HP"` 的 BaseValue 必须被 Clamp 到 `80.0`

**And** `"HP"` 的 CurrentValue 必须被 Clamp 到 `80.0`

---

### Requirement: 范围约束 MUST 触发属性变化事件

**优先级**: P1 (重要)

**描述**:
When range constraint causes attribute value change, MUST trigger corresponding attribute change events to ensure listeners can perceive all changes. 当范围约束导致属性值变化时,必须触发相应的属性变化事件,确保监听者能感知所有变化。

#### Scenario: BaseValue 变化触发事件

**Given** 属性 `"HP"` 的 BaseValue 为 `100.0`

**When** 范围约束将 `"HP"` 的 BaseValue Clamp 到 `80.0`

**Then** 必须触发 `OnAttributeBaseValueChanged` 事件

**And** 事件 payload 必须包含:
- `AttributeName = "HP"`
- `OldValue = 100.0`
- `NewValue = 80.0`
- `SourceHandle` 为空或无效(因为是 enforcement 导致的,不是 modifier)

#### Scenario: CurrentValue 变化触发事件

**Given** 属性 `"HP"` 的 CurrentValue 为 `100.0`

**When** 范围约束将 `"HP"` 的 CurrentValue Clamp 到 `80.0`

**Then** 必须触发 `OnAttributeValueChanged` 事件

**And** 事件 payload 必须包含:
- `AttributeName = "HP"`
- `OldValue = 100.0`
- `NewValue = 80.0`
- `SourceHandle` 为空或无效(因为是 enforcement 导致的,不是 modifier)

---

### Requirement: MUST 集成到属性重算流程

**优先级**: P0 (关键)

**描述**:
Range constraint propagation MUST be integrated into attribute recalculation flow, automatically executed after `RecalculateAttributeBaseValues` and `RecalculateAttributeCurrentValues`. 范围约束传播必须集成到属性重算流程中,在 `RecalculateAttributeBaseValues` 和 `RecalculateAttributeCurrentValues` 之后自动执行。

#### Scenario: BaseValue 重算后执行约束

**Given** 调用 `RecalculateAttributeBaseValues`

**When** 重算完成

**Then** 必须自动调用范围约束传播

**And** 确保所有 BaseValue 满足范围约束

#### Scenario: CurrentValue 重算后执行约束

**Given** 调用 `RecalculateAttributeCurrentValues`

**When** 重算完成

**Then** 必须自动调用范围约束传播

**And** 确保所有 CurrentValue 满足范围约束

---

## 相关规范

- `source-handle-attribute-integration` - SourceHandle 与属性系统集成
- `attribute-sourcehandle-stable-indexing` - 稳定索引(依赖)

## 实施注意事项

1. **性能**: 迭代算法的复杂度为 O(N * M),N 为属性数量,M 为最大迭代次数
2. **优化**: 可以只对有动态范围的属性执行 enforcement,减少开销
3. **测试**: 必须测试单跳、多跳、循环依赖等场景
4. **玩法影响**: 此变更可能影响现有玩法平衡,需要充分测试
5. **配置**: 可以考虑提供配置开关,允许禁用范围约束传播(用于调试或兼容性)
