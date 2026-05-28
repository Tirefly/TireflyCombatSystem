## 背景

当前 TCS 已经把 Buff 的 `Duration`、`Stack`、`Merge`、`Period` 从共享状态核心中拆出到 Buff 侧，但 Buff 仍缺少一套正式化的“增量变化后如何反应”的统一语义。

如果不继续收紧，系统会自然滑向两种坏结果之一：

1. 把这些反应继续塞进 `UTcsBuffMerger`，让 merge 算法和生命周期反应纠缠在一起。
2. 把 `Period`、`Refresh`、`Expire` 全部做成一套更大的通用枚举面，导致 `UTcsBuffDefinition` 继续膨胀。

本提案选择第三条路：

- merge 继续只做 merge
- 通用增量反应只覆盖 `Stack / Duration`
- `Period` 回到 BuffStateTree 自己驱动

## 目标 / 非目标

- 目标：
  - 为可叠层 Buff 提供最小且稳定的通用增量反应配置面
  - 让叠层上涨和持续时间耗尽都走统一、清晰的 Buff 自有运行时入口
  - 明确 `Period` 属于 BuffStateTree 的节拍与执行能力，而不是通用 BuffDef 策略
  - 保持与现有 `buff-runtime` 边界和 `buff-merge-runtime` 契约一致
- 非目标：
  - 重做 `UseNewest`、`UseOldest`、`StackByInstigator` 等 merger 的业务语义
  - 在 `UTcsBuffComponent` 上新增组件侧 `PeriodTracker`、`RemainingTime` 或统一 `TickBuffPeriods()`
  - 引入新的 `UTcsBuffReactivePolicy` CDO 抽象
  - 把所有特殊 Period 行为都抽成通用配置
  - 在本提案中新增或执行专门测试代码；相关行为验证统一等待开发者手动执行编辑器测试

## 决策

- 决策：通用定义层只建模 `Stack / Duration` 两条稳定语义轴
  - 原因：目前真正稳定且可复用的通用语义只有两类：叠层上涨是否刷新 Duration，和 Duration 归零后如何掉层/移除。继续把 `Period` 也做成通用枚举，只会把个别 Buff 的节拍偏好错误提升为全系统配置。
  - 备选方案：
    - 原始五类枚举方案：拒绝，因为表面灵活，实际会把配置面做大且边界不清。
    - 直接照搬 GAS 的 `StackPeriodResetPolicy`：拒绝，因为 GAS 的 Period 本身就是 GE runtime 内建能力，而 TCS 当前明确要把 Period 留在 BuffStateTree。

- 决策：增量反应配置只对 `MaxStackCount > 1` 的 Buff 生效
  - 原因：这些语义本质上都是叠层相关语义。单层 Buff 没有“叠层上涨”和“掉一层后继续存在”这类问题，不应该被迫面对这套配置。
  - 备选方案：
    - 始终显示这些配置但运行时忽略：拒绝，因为会污染创作界面，设计师很难判断哪些项真正生效。

- 决策：`UTcsStateComponent` 保持共享宿主，而 `UTcsBuffComponent` 作为 Buff 专属协作组件执行 `Stack / Duration` 通用反应
  - 原因：Buff 本质上仍然是 State，状态实例的应用许可、移除与共享执行周期仍应由 `UTcsStateComponent` 统一承接；但 Buff 又确实有持续时间跟踪、叠层变化、合并编排、Buff-only 事件等专属运行时语义，因此需要 `UTcsBuffComponent` 这类 Buff 专属协作组件承接这部分工作。
  - 备选方案：
    - 把反应继续放回 merger：拒绝，因为这会让合并算法与生命周期反应紧耦合。
    - 把所有 Buff 专属反应继续并回 `UTcsStateComponent`：拒绝，因为这会再次把 Buff 专属职责塞回共享状态核心。
    - 让 `UTcsBuffComponent` 取代共享 State 宿主：拒绝，因为 Buff 仍是 State，不应该复制一套平行的 apply/remove/lifecycle 宿主框架。
    - 为每种增量反应引入独立 CDO 策略对象：当前拒绝，因为问题空间还没有复杂到需要第二层策略抽象。

- 决策：`Period` 由 BuffStateTree 通过 `FTcsBuffPeriodDriverTask` 自己驱动
  - 原因：当前 `UTcsStateInstance` 已经具备 `TickStateTree()` 与 `SendStateTreeEvent()` 能力，而 Buff 专用 schema 收敛正在由独立的 runtime-access change 推进。Period 缺的不是底层设施，而是一个可复用的 StateTree 节拍驱动节点。
  - 备选方案：
    - 在 `UTcsBuffComponent` 上维护 `PeriodTracker`：拒绝，因为会在树外复制一份 Period 相位状态，形成双状态源。
    - 在 `UTcsBuffInstance` 上新增公开 `PeriodRemaining` 字段：拒绝，因为这是调度现场，不是 Buff 持久定义语义。

- 决策：特殊 Period 行为留给具体 BuffStateTree 自己实现
  - 原因：“叠层时重置周期”“叠层时立即补一次 tick”这类需求未必适用于所有周期 Buff，把它们做成通用配置只会增加系统复杂度。
  - 备选方案：
    - 新增 `PeriodPolicy` / `PeriodReactionPolicy`：拒绝，因为这会重新把 Period 拉回定义层通用策略，破坏本提案最核心的边界。

## 风险 / 取舍

- 风险：`PeriodDriverTask` 引入后，PeriodTick 与 Duration 到期的同帧顺序需要明确，否则行为容易被误读。
  - 缓解：在实现阶段明确“先 Tick BuffStateTree，再处理 Duration 生命周期”的推荐顺序，并在开发者手动执行的最小范围编辑器测试中覆盖该场景。

- 风险：当前阶段的行为验证需要依赖开发者后续手动执行编辑器测试，验证节奏更依赖人工安排。
  - 缓解：本提案先要求编译验证，并把最小范围场景清单显式列出，等待开发者手动执行编辑器测试；更完整的覆盖体系作为 Buff / State 模块稳定后的后续工作。

- 风险：现有 periodic Buff 作者可能会本能地期待一个通用 `PeriodPolicy`。
  - 缓解：在文档和 proposal 中明确说明：通用配置只覆盖 `Stack / Duration`，特殊 Period 语义需要通过 BuffStateTree 专用节点表达。

## 迁移计划

1. 在 `UTcsBuffDefinition` 上增加新的增量反应策略字段，但用编辑器可见性和运行时忽略规则确保旧的单层 Buff 不受影响。
2. 把现有叠层上涨和持续时间耗尽路径收口到统一处理入口，避免 merger / Buff 专属组件 / 业务调用路径各自实现一套反应，同时保持 `UTcsStateComponent` 继续作为共享宿主。
3. 为周期 Buff 提供 `FTcsBuffPeriodDriverTask`，并把 Period 的自然节拍驱动收回 BuffStateTree。
4. 如果某些现有 Buff 需要“叠层即补一次 tick”之类特殊 Period 行为，则在对应 BuffStateTree 内新增专用节点，而不是扩定义层配置。