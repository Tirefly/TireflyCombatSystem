# 变更：为 TCS Buff 增量反应语义建模

## 背景

`refactor-tcs-state-buff-skill-split` 已经把一个关键边界明确下来：`Duration`、`Stack`、`Merge`、`Period` 等语义都属于 Buff 侧，而不是共享 `State Core`。

`optimize-buff-merge-runtime` 也已经把另一个关键边界明确下来：`UTcsBuffMerger` 只应该负责 Buff group 的合并与淘汰，不应该顺手承载“叠层后要不要刷新时长”“时长归零后是移除整 Buff 还是掉一层”“Period 是否需要补发一次”这类生命周期反应。

但当前 Buff 运行时仍然缺少一套正式化的增量反应语义模型，导致下面几个问题仍然悬空：

- `UTcsBuffDefinition` 还没有一个收紧后的 stack / duration 反应配置面
- `SetStackCount()` 与 duration expire 路径还缺少统一、明确的反应入口
- `Period` 到底应不应该成为通用 BuffDef 策略项，当前仍需要正式定型

本提案的目标就是把这套边界固定下来：

1. 通用增量反应只覆盖 `Stack / Duration`
2. `Period` 不进入通用 BuffDef 反应策略，而是回到 BuffStateTree 自己驱动
3. `UTcsStateComponent` 继续作为状态实例应用许可、移除与共享执行周期的权威宿主，而 `UTcsBuffComponent` 作为 Buff 专属协作组件，只处理通用 Buff 生命周期反应，不负责 Period 调度

## 变更内容

- 在 `UTcsBuffDefinition` 上新增一套收紧后的 Buff 增量反应配置，但仅覆盖叠层相关的生命周期语义：
  - `ETcsBuffDurationRefreshPolicy`
  - `ETcsBuffStackExpirationPolicy`
  - `FTcsBuffOnStackIncreasePolicy`
  - `FTcsBuffOnDurationExpiredPolicy`
- 明确这批新增配置当且仅当 `MaxStackCount > 1` 时有意义；当 `MaxStackCount <= 1` 时在编辑器中隐藏，运行时也忽略。
- 明确 `OnDurationExpired` 相关配置只对有限持续时间 Buff 有意义；无限时长 Buff 隐藏并忽略这批配置。
- 明确有限持续时间但 `MaxStackCount <= 1` 的 Buff 继续沿用默认的整 Buff 移除语义，不需要显式配置 `ExpirationPolicy`。
- 保留 `UTcsBuffDefinition::Period` 作为 BuffStateTree 可读取的默认周期间隔输入，但不新增任何通用 `PeriodPolicy` / `PeriodReactionPolicy`。
- 在 Buff 自有运行时路径中补齐统一执行入口，并保持 `UTcsStateComponent` 为共享状态宿主、`UTcsBuffComponent` 为 Buff 专属协作组件：
  - 叠层上涨后的 Duration 反应
  - Duration 归零后的整 Buff 移除 / 掉层 / 掉层并刷新 Duration 反应
- 明确持续时间耗尽导致的最终离场仍应以 `Expired` 等真实移除原因收敛，而不是错误折叠成 `StackDepleted`。
- 明确 `UTcsBuffMerger` 与 merge runtime 不承担这批反应执行；它们只决定 survivor / merged-out / final stack 收敛。
- 为 BuffStateTree 增加一个可复用的 `FTcsBuffPeriodDriverTask`，负责根据 `UTcsBuffDefinition::Period` 或 override 值产出 `Event.Buff.PeriodTick`。
- 明确“叠层时重置 Period”“叠层时立刻补一次 Period”这类 Period-special 语义不再进入通用 BuffDef 配置，而由具体 Buff 的 StateTree 自己实现。
- 当前不引入第二套 `UTcsBuffReactivePolicy` CDO 策略类，也不引入 `ExtendDuration` 之类额外枚举分支。

## 影响范围

- 受影响规范：
  - `buff-runtime`
- 受影响代码：
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/Buff/TcsBuffDefinition.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/Buff/TcsBuffInstance.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/Buff/TcsBuffComponent.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/Buff/TcsBuffComponent.cpp`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/StateTree/**`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/StateTree/**`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystemTests/**`
- 受影响文档：
  - `Plugins/TireflyCombatSystem/Documents/文档：Buff增量反应语义扩展方案（叠层、时长、Period）.md`
  - Buff / StateTree 相关实现说明与验证记录
- 与现有变更的关系：
  - 依赖 `refactor-tcs-state-buff-skill-split` 已经确认的 `buff-runtime` 边界，不重新讨论 Buff 是否应拥有 Duration / Stack / Period 语义
  - 与 `optimize-buff-merge-runtime` 协同，但不改动 merge contract，只补“merge 之后如何反应”的生命周期语义
- 明确不在本提案范围内：
  - 重新设计现有 Buff merger 的业务语义
  - 在 `UTcsBuffComponent` 上新增组件侧 `PeriodTracker` 或统一 `TickBuffPeriods()` 调度器
  - 新增通用 `PeriodPolicy` / `PeriodReactionPolicy`
  - 引入新的 Buff CDO reactive-strategy 抽象
  - 统一自动化测试体系的全面补齐；当前阶段先以编译和最小场景验证为主