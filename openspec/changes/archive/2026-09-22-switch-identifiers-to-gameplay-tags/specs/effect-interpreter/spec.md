## MODIFIED Requirements

### Requirement: 链执行与池化运行态

`UTcsEffectSubsystem` MUST 提供 `ExecuteChain(FGameplayTag ChainId, FTcsEffectContext Context) -> FTcsChainRunHandle`（起链：立即执行至首个挂起点或走完；**2026-09-22 改造：`ChainId` 类型 `FName` → `FGameplayTag`**）：

- 运行态 `FTcsChainRun` **池化**（`TTcsInstancePool`，D0-2 代际句柄防悬空）——控制流状态住数据、**不活在调用栈里**（D4-3 异步语义）；
- `FTcsChainRun` MUST 自持：`ChainId`（**不是链定义指针**——每步入器按 id 重解析；登记表变更不得使运行中链悬空）、`PC`、`Context`（黑板）、宿主门面弱引用与自身句柄（唤醒回入口）、挂起锚；
- 返回句柄**仅在链未走完时有效**：链同步走完即释放运行态（句柄随之失效）；`IsRunActive(Handle)` 是活性查询入口；
- 未登记的链 id MUST 拒绝：Error 日志 + 无效句柄（不 ensure、不崩溃——执行期配置错误，不用 ensure 刷屏）；
- 运行态指针 MUST NOT 跨步骤执行持有：**每次进入步骤执行前按句柄重解析**（引擎事实 2026-09-17：池元素住连续缓冲，新增其他运行态即搬移；步骤执行期间做任何池新增都会让缓存指针失效）。

#### Scenario: 全即时步骤的链同步走完

- **WHEN** 执行一条不含挂起步骤的链
- **THEN** `ExecuteChain` 返回时全部步骤已执行、运行态已释放（`IsRunActive` 为 false）

#### Scenario: 未登记链被拒

- **WHEN** 执行一个未登记的 ChainId（有效 tag 但未登记）
- **THEN** 返回无效句柄 + Error 日志（不崩溃、不 ensure）

#### Scenario: 挂起期间登记新链不影响续走

- **WHEN** 一条链挂起期间又登记了若干其他链定义，随后该链被唤醒
- **THEN** 唤醒重入按 id 解析到自己的定义并正常续走（运行态未持链指针，登记表增长不使其悬空）
