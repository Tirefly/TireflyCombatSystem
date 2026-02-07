# Tasks: 简化状态持续时间计算策略

本文档列出实施本提案所需的所有任务，按执行顺序排列。

## Phase 1: 删除枚举和配置字段

### Task 1.1: 删除 ETcsDurationTickPolicy 枚举定义
- [ ] 在 `TcsStateSlot.h` 中找到 `ETcsDurationTickPolicy` 枚举定义（约第 27-36 行）
- [ ] 删除整个枚举定义及其注释
- [ ] 确保删除后文件格式正确（空行符合项目规范）

**验证**:
- 编译通过（会有未使用字段的警告，后续任务会解决）
- 枚举定义完全移除

### Task 1.2: 删除配置字段
- [ ] 在 `FTcsStateSlotDefinition` 结构体中找到以下字段：
  - `DurationTickPolicy`（约第 93-96 行）
  - `bFreezeDurationWhenPaused`（约第 98-101 行）
- [ ] 删除这两个字段及其注释
- [ ] 确保删除后结构体格式正确

**验证**:
- 编译通过
- 配置字段完全移除

---

## Phase 2: 简化持续时间计算逻辑

### Task 2.1: 重写 TickDurationTracker 函数
- [ ] 在 `TcsStateComponent.cpp` 中找到 `TickDurationTracker` 函数
- [ ] 找到持续时间计算逻辑部分（约第 280-370 行）
- [ ] 删除以下内容：
  - `bFreezeDurationWhenPaused` 的判断（约第 309-313 行）
  - 整个 `switch (SlotDefinition.DurationTickPolicy)` 分支（约第 324-347 行）
  - 所有与 `DurationTickPolicy` 相关的变量和逻辑
- [ ] 实现新的简化逻辑：
  ```cpp
  // 只有 Active 和 HangUp 阶段才计算持续时间
  // Pause 阶段冻结，Inactive 阶段不计时
  if (CurrentStage != ETcsStateStage::SS_Active
      && CurrentStage != ETcsStateStage::SS_HangUp)
  {
      continue;
  }
  ```
- [ ] 保持其他逻辑不变（过期检查、持续时间递减等）

**验证**:
- 编译通过
- 代码行数减少到 ~20 行
- 逻辑清晰易懂

### Task 2.2: 更新调试输出
- [ ] 在 `TcsStateComponent.cpp` 中找到 `GetStateSlotDebugString` 函数
- [ ] 找到输出 `DurationTickPolicy` 的代码（约第 603-607 行）
- [ ] 删除以下输出：
  - `DurationTickPolicy` 的枚举值输出
  - `bFreezeDurationWhenPaused` 的布尔值输出
- [ ] 调整格式字符串，移除相关占位符

**验证**:
- 编译通过
- 调试输出不再包含已删除的字段
- 输出格式正确

---

## Phase 3: 更新注释和文档

### Task 3.1: 更新 ETcsStateSlotGateClosePolicy 注释
- [ ] 在 `TcsStateSlot.h` 中找到 `ETcsStateSlotGateClosePolicy` 枚举（约第 40-47 行）
- [ ] 更新 `SSGCP_HangUp` 的 ToolTip：
  ```cpp
  ToolTip = "Gate关闭时挂起槽位中的状态：\n"
            "- 持续时间：继续计时\n"
            "- 叠层合并：正常参与（保持叠层数准确）\n"
            "- 逻辑执行：暂停StateTree执行"
  ```
- [ ] 更新 `SSGCP_Pause` 的 ToolTip：
  ```cpp
  ToolTip = "Gate关闭时暂停槽位中的状态：\n"
            "- 持续时间：完全冻结\n"
            "- 叠层合并：正常参与（保持叠层数准确）\n"
            "- 逻辑执行：暂停StateTree执行"
  ```
- [ ] 保持 `SSGCP_Cancel` 的 ToolTip 不变

**验证**:
- 编译通过
- 注释清晰准确，明确说明叠层合并行为
- 符合项目注释规范

### Task 3.2: 更新 ETcsStatePreemptionPolicy 注释
- [ ] 在 `TcsStateSlot.h` 中找到 `ETcsStatePreemptionPolicy` 枚举（约第 51-58 行）
- [ ] 更新 `SPP_HangUpLowerPriority` 的 ToolTip：
  ```cpp
  ToolTip = "高优先级状态抢占时，低优先级状态进入挂起：\n"
            "- 持续时间：继续计时\n"
            "- 叠层合并：正常参与（保持叠层数准确）\n"
            "- 逻辑执行：暂停StateTree执行"
  ```
- [ ] 更新 `SPP_PauseLowerPriority` 的 ToolTip：
  ```cpp
  ToolTip = "高优先级状态抢占时，低优先级状态进入暂停：\n"
            "- 持续时间：完全冻结\n"
            "- 叠层合并：正常参与（保持叠层数准确）\n"
            "- 逻辑执行：暂停StateTree执行"
  ```
- [ ] 保持 `SPP_CancelLowerPriority` 的 ToolTip 不变

**验证**:
- 编译通过
- 注释清晰准确，明确说明叠层合并行为
- 符合项目注释规范

### Task 3.3: 更新 CLAUDE.md 文档
- [ ] 在 `Plugins/TireflyCombatSystem/CLAUDE.md` 中查找与 `DurationTickPolicy` 相关的内容
- [ ] 如果存在相关说明，更新为新的简化逻辑
- [ ] 添加 HangUp 和 Pause 的语义说明

**验证**:
- 文档内容准确
- 与实际实现一致

---

## Phase 4: 验证和测试

### Task 4.1: 编译验证
- [ ] 使用 Development 配置编译 Editor Target
  ```bash
  "E:\UnrealEngine\UE_5.6\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" TireflyGameplayUtilsEditor Win64 Development -Project="e:\Projects_Unreal\TireflyGameplayUtils\TireflyGameplayUtils.uproject" -rocket -progress
  ```
- [ ] 确保无编译错误
- [ ] 确保无编译警告

**验证**:
- 编译成功
- 无错误和警告

### Task 4.2: 功能测试
- [ ] 在编辑器中创建测试场景
- [ ] 测试 Active 状态的持续时间计算（应该正常计时）
- [ ] 测试 HangUp 状态的持续时间计算（应该继续计时）
- [ ] 测试 Pause 状态的持续时间计算（应该冻结）
- [ ] 测试 Gate 关闭时的行为：
  - `GateCloseBehavior = HangUp`：状态进入 HangUp，持续时间继续
  - `GateCloseBehavior = Pause`：状态进入 Pause，持续时间冻结
  - `GateCloseBehavior = Cancel`：状态被取消

**验证**:
- 所有测试场景行为正确
- HangUp 和 Pause 的语义符合预期

### Task 4.3: 回归测试
- [ ] 运行所有现有的状态系统测试
- [ ] 确保没有功能回归
- [ ] 检查是否有测试需要更新

**验证**:
- 所有测试通过
- 无功能回归

---

## Phase 5: 清理和文档

### Task 5.1: 代码审查
- [ ] 检查所有修改的文件
- [ ] 确保代码格式符合项目规范：
  - Tab 缩进
  - 空行规范
  - 注释规范
- [ ] 确保没有遗留的 `DurationTickPolicy` 引用

**验证**:
- 代码格式正确
- 无遗留引用

### Task 5.2: 提交变更
- [ ] 使用 Git 提交所有变更
- [ ] 提交信息格式：`【DEL】删除ETcsDurationTickPolicy枚举，简化状态持续时间计算逻辑`
- [ ] 确保提交包含所有修改的文件

**验证**:
- Git 提交成功
- 提交信息符合规范

---

## 依赖关系

- 所有任务按顺序执行
- Phase 1 必须在 Phase 2 之前完成（先删除定义，再删除使用）
- Phase 2 必须在 Phase 3 之前完成（先修改逻辑，再更新注释）
- Phase 3 必须在 Phase 4 之前完成（先更新文档，再测试）
- Phase 4 必须在 Phase 5 之前完成（先验证，再提交）

## 预计工作量

- Phase 1: 15 分钟
- Phase 2: 30 分钟
- Phase 3: 20 分钟
- Phase 4: 30 分钟
- Phase 5: 15 分钟

**总计**: 约 2 小时
