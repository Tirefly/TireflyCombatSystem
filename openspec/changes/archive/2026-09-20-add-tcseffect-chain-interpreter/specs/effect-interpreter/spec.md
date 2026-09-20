## ADDED Requirements

### Requirement: 链执行与池化运行态

`UTcsEffectSubsystem` MUST 提供 `ExecuteChain(FName ChainId, FTcsEffectContext Context) -> FTcsChainRunHandle`（起链：立即执行至首个挂起点或走完）：

- 运行态 `FTcsChainRun` **池化**（`TTcsInstancePool`，D0-2 代际句柄防悬空）——控制流状态住数据、**不活在调用栈里**（D4-3 异步语义）；
- `FTcsChainRun` MUST 自持：`ChainId`（**不是链定义指针**——每步入器按 id 重解析；登记表变更不得使运行中链悬空）、`PC`、`Context`（黑板）、宿主门面弱引用与自身句柄（唤醒回入口）、挂起锚；
- 返回句柄**仅在链未走完时有效**：链同步走完即释放运行态（句柄随之失效）；`IsRunActive(Handle)` 是活性查询入口；
- 未登记的链 id MUST 拒绝：Error 日志 + 无效句柄（不 ensure、不崩溃——执行期配置错误，不用 ensure 刷屏）；
- 运行态指针 MUST NOT 跨步骤执行持有：**每次进入步骤执行前按句柄重解析**（引擎事实 2026-09-17：池元素住连续缓冲，新增其他运行态即搬移；步骤执行期间做任何池新增都会让缓存指针失效）。

#### Scenario: 全即时步骤的链同步走完

- **WHEN** 执行一条不含挂起步骤的链
- **THEN** `ExecuteChain` 返回时全部步骤已执行、运行态已释放（`IsRunActive` 为 false）

#### Scenario: 未登记链被拒

- **WHEN** 执行一个未登记的 ChainId
- **THEN** 返回无效句柄 + Error 日志（不崩溃、不 ensure）

#### Scenario: 挂起期间登记新链不影响续走

- **WHEN** 一条链挂起期间又登记了若干其他链定义，随后该链被唤醒
- **THEN** 唤醒重入按 id 解析到自己的定义并正常续走（运行态未持链指针，登记表增长不使其悬空）

### Requirement: 挂起-恢复协议（步内挂起）

步骤执行器 MUST 以 `ETcsStepResult{ TSR_Completed, TSR_Running }` 表达**步内挂起**（D4-17，与 PC 挂起、事件订阅并列的第三种挂起形态）：

- `TSR_Completed` → PC 前进；`TSR_Running` → **PC 停在原步**、运行态保持活动、解释器立即让出控制权（MUST NOT 原地自旋）；
- 挂起中的运行态 MUST NOT 产生每帧成本——门面 MUST NOT 是 Tickable 子系统，唤醒一律由唤醒源驱动（R3 落到期堆一路；事件/子链完成随各自轮次）；
- 唤醒回入口 `ResumeRun(FTcsChainRunHandle Handle)`：MUST 做**句柄代际校验**，悬空/已释放/世界将拆的句柄静默返回（挂起条目与运行态的竞态是正常路径，不是契约违规）；
- 恢复 MUST 从运行态 PC **重入同一执行器**（由步骤依据运行态上的挂起锚自辨"首入 / 被唤醒"）；
- 走完最后一步即释放运行态；释放后旧句柄 MUST 失效（不得复活已结束的链）。

#### Scenario: 挂起步不前进 PC

- **WHEN** 链首步返回 `TSR_Running`
- **THEN** 该步 PC 不变、运行态保持活动、后续步骤不执行、解释器立即返回

#### Scenario: 唤醒后从原步续走

- **WHEN** 唤醒源按句柄调用 `ResumeRun`
- **THEN** 从挂起步重入（该步据挂起锚判定为完成）→ PC 前进 → 后续步骤继续执行

#### Scenario: 悬空句柄唤醒静默

- **WHEN** 用一个已释放（代际失配）的句柄调用 `ResumeRun`
- **THEN** 静默返回（无 ensure、无 Error 日志）

### Requirement: 单帧步数熔断

**单次进入执行**的步数 MUST 受链定义 `MaxStepsPerFrame`（默认 64）限制——超限即视为运行中链失控（自激/循环）：

- 超限 MUST 熔断：ensure 提示 + Error 日志（含链 id 与上限）+ 释放运行态（断链）；
- 熔断判定 MUST 与"链正常走完"区分——正常走完是零诊断噪音的正常路径。

#### Scenario: 超限熔断

- **WHEN** 一条链的单次进入执行步数超过其 `MaxStepsPerFrame`
- **THEN** 停止执行、留 ensure + Error 日志（含链 id 与上限）、运行态释放

#### Scenario: 上限内零噪音

- **WHEN** 链的单次进入执行步数不超过上限
- **THEN** 正常走完或挂起，无任何熔断诊断输出

### Requirement: 等待步骤（WaitDelay）

`TcsEffect` MUST 提供控制流步骤 `FTcsStepWaitDelay{ double Seconds; }`（D4-16 本模块 9 个原语之一；挂起-恢复协议的首个实证载体）：

- **首入**：按**战斗时钟**（`UTcsClockSubsystem`，唯一取时入口 D0-5）入到期堆，到期时刻 = 当前 `Elapsed` + `Seconds`，随后返回 `TSR_Running`；
- **等待期零每帧重入成本**（不轮询、不自 tick——到期由时钟泵回调驱动，见 `clock-expiry-heap`）；
- **到期唤醒**：回调按运行态句柄调 `ResumeRun` → 本步判为完成 → 续走；
- **时间基准 = 战斗时钟（ScaledDt 时轴）**：`slomo` 减速时等待同比例拉长（R3 剧本检查点 7 的成立依据）；`pause` 冻结期间时钟不推进 → 等待不兑现；
- **缺时钟设施**（世界或时钟子系统不可得）时 MUST 降级为"本步按完成处理" + Error 日志（不挂死、不崩溃）。

#### Scenario: 到点后才续走

- **WHEN** 起一条 `[WaitDelay 0.5, 后续步]` 的链
- **THEN** 后续步在到期时刻之后才执行；等待窗口内该链零步骤执行

#### Scenario: 减速时等待同比例拉长

- **WHEN** 以 `slomo 0.1` 运行同一条 `[WaitDelay 0.5]` 链
- **THEN** 等待按战斗时间 0.5s（实时约 5s）兑现——到期基准是战斗时钟 `Elapsed` 而非实时秒

#### Scenario: 无时钟设施时降级

- **WHEN** 运行态所属世界取不到时钟子系统
- **THEN** 本步按完成处理 + Error 日志（不挂起、不崩溃）
