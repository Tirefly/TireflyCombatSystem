# Tasks: 状态槽位激活完整性重构

本文档列出实施本提案所需的所有任务，按执行顺序排列。

## Phase 1: 合并移除统一化

### Task 1.1: 修改合并逻辑使用 RequestStateRemoval
- [x] 在 `UTcsStateManagerSubsystem` 中找到所有直接 Finalize 合并淘汰 state 的代码
- [x] 将这些代码改为调用 `RequestStateRemoval`
  - Reason = `Custom`
  - CustomReason = `"MergedOut"`
- [x] 确保 StateTree 退场逻辑有机会执行

**验证**: ✅
- 合并淘汰的 state 走 RequestStateRemoval 路径
- StateTree 退场逻辑正确执行
- 不会导致递归激活更新

### Task 1.2: 添加合并移除测试
- [x] 添加单元测试: 合并淘汰使用 RequestStateRemoval
- [x] 添加单元测试: StateTree 退场逻辑执行
- [x] 添加单元测试: 不会导致递归调用

**验证**: ✅ 所有测试通过

---

## Phase 2: 槽位激活去再入

### Task 2.1: 添加延迟请求机制
- [x] 在 `UTcsStateManagerSubsystem` 中添加以下成员:
  - `bool bIsUpdatingSlotActivation`
  - `TMap<TWeakObjectPtr<UTcsStateComponent>, TSet<FGameplayTag>> PendingSlotActivationUpdates`
- [x] 实现 `RequestUpdateStateSlotActivation(StateComponent, SlotTag)` 函数
  - 若当前正在更新，则仅把 SlotTag 入队
  - 若当前不在更新，则执行更新
- [x] **Bug 修复**: 修改数据结构避免多 Actor 覆盖问题
  - 原设计: `TMap<FGameplayTag, TWeakObjectPtr<UTcsStateComponent>>`（错误）
  - 修复后: `TMap<TWeakObjectPtr<UTcsStateComponent>, TSet<FGameplayTag>>`（正确）
  - 原因: 多个 Actor 共享相同 SlotTag 时，Map key 会冲突导致请求被覆盖
  - 影响文件:
    - `TcsStateManagerSubsystem.h:163` (数据结构定义)
    - `TcsStateManagerSubsystem.cpp:673` (入队逻辑)
    - `TcsStateManagerSubsystem.cpp:685-720` (出队和处理逻辑)

**验证**: ✅
- 编译通过
- 延迟请求机制正确工作
- 多 Actor 共享相同 SlotTag 不会互相覆盖

### Task 2.2: 修改 UpdateStateSlotActivation 使用延迟机制
- [x] 在 `UpdateStateSlotActivation` 开头设置 `bIsUpdatingSlotActivation = true`
- [x] 在 `UpdateStateSlotActivation` 结尾:
  - 设置 `bIsUpdatingSlotActivation = false`
  - Drain `PendingSlotActivationUpdates` 队列
- [x] 所有触发槽位激活更新的地方改为调用 `RequestUpdateStateSlotActivation`

**验证**: ✅
- 不会递归调用 UpdateStateSlotActivation
- 队列在有限步内收敛
- 状态更新顺序确定

### Task 2.3: 添加收敛保护
- [x] 添加最大迭代次数限制（10 次）
- [x] 若达到最大迭代次数，输出 warning 日志
- [x] 防止无限循环

**验证**: ✅
- 正常情况下在 1-3 次迭代内收敛
- 异常情况下不会无限循环

### Task 2.4: 添加去再入测试
- [x] 添加单元测试: 嵌套调用被延迟
- [x] 添加单元测试: 队列正确排空
- [x] 添加单元测试: 收敛保护生效

**验证**: ✅ 所有测试通过

---

## Phase 3: 同优先级 tie-break 策略化

### Task 3.1: 设计策略接口
- [x] 创建基类 `UTcsStateSamePriorityPolicy`
- [x] 定义接口:
  - `int64 GetOrderKey(const UTcsStateInstance* State) const`
- [x] 添加必要的注释和文档

**验证**: ✅ 编译通过

### Task 3.2: 实现内置策略
- [x] 实现 `UTcsStateSamePriorityPolicy_UseNewest`
  - key = ApplyTimestamp
  - 适用于 Buff 槽位
- [x] 实现 `UTcsStateSamePriorityPolicy_UseOldest`
  - key = -ApplyTimestamp
  - 适用于技能队列槽位
- [x] 添加策略注释说明适用场景

**验证**: ✅
- 编译通过
- 策略逻辑正确

### Task 3.3: 在槽位定义中添加策略字段
- [x] 在 `FTcsStateSlotDefinition` 中添加:
  - `TSubclassOf<UTcsStateSamePriorityPolicy> SamePriorityPolicy`
- [x] 设置默认值为 `UTcsStateSamePriorityPolicy_UseNewest`

**验证**: ✅ 编译通过

### Task 3.4: 集成策略到槽位激活流程
- [x] 在 `ProcessPriorityOnlyMode` 中:
  - 对每个同优先级组，应用策略排序
  - 选择排序后的第一个作为 Active
- [x] 确保策略正确应用

**验证**: ✅
- 同优先级排序确定性
- 不同策略产生不同结果

### Task 3.5: 添加策略测试
- [x] 添加单元测试: UseNewest 策略正确
- [x] 添加单元测试: UseOldest 策略正确
- [x] 添加单元测试: 自定义策略可扩展

**验证**: ✅ 所有测试通过

---

## Phase 4: Gate 关闭逻辑重构

### Task 4.1: 创建权威函数 HandleGateClosed
- [x] 使用现有的 `EnforceSlotGateConsistency` 作为权威函数
- [x] 实现统一的 Gate 关闭逻辑:
  - 应用 `GateCloseBehavior`（HangUp/Pause/Cancel）
  - 保证不变量: Gate 关闭时该 Slot 内不允许存在 Active state

**验证**: ✅
- 编译通过
- 逻辑正确

### Task 4.2: 移除重复路径
- [x] 找到所有处理 Gate 关闭的代码路径
- [x] 移除 `ProcessStateSlotOnGateClosed` 函数（重复逻辑）
- [x] 只保留 `EnforceSlotGateConsistency` 作为权威函数

**验证**: ✅
- 只有一个权威函数处理 Gate 关闭
- 行为一致

### Task 4.3: 添加不变量检查
- [x] 在 `EnforceSlotGateConsistency` 中添加断言:
  - Gate 关闭后，Slot 内无 Active state
- [x] 在开发模式下启用检查

**验证**: ✅
- 不变量始终成立
- 违反时能及时发现

### Task 4.4: 添加 Gate 关闭测试
- [x] 添加单元测试: Gate 关闭行为正确
- [x] 添加单元测试: 不变量保持
- [x] 添加单元测试: 所有路径使用权威函数

**验证**: ✅ 所有测试通过

---

## 最终验证

### Task 5.1: 集成测试
- [x] 运行所有现有测试，确保没有回归
- [x] 运行新增的所有测试
- [x] 在编辑器中手动测试典型场景

**验证**: ✅ 所有测试通过，无回归

### Task 5.2: 性能测试
- [x] 测试延迟请求机制对性能的影响
- [x] 测试策略排序对性能的影响
- [x] 确保性能在可接受范围内

**验证**: ✅ 性能无明显下降

### Task 5.3: 文档更新
- [x] 更新 CLAUDE.md 中的相关章节
- [x] 添加策略使用指南
- [x] 添加迁移指南（如果需要）

**验证**: ✅ 文档完整准确

### Task 5.4: OpenSpec 验证
- [x] 运行 `openspec validate refactor-state-slot-activation-integrity --strict --no-interactive`
- [x] 修复所有验证错误

**验证**: ✅ OpenSpec 验证通过

---

## 依赖关系

- Task 1.x 必须在 Task 2.x 之前完成（合并移除统一化是去再入的前提）
- Task 2.x 必须完成后才能进行 Task 3.x（策略需要稳定的激活流程）
- Task 3.x 和 Task 4.x 可以并行
- Task 5.x 依赖所有前置任务完成

## 预计工作量

- Phase 1: 1-2 天
- Phase 2: 2-3 天
- Phase 3: 2-3 天
- Phase 4: 1-2 天
- 最终验证: 1-2 天

**总计**: 7-12 天
