## MODIFIED Requirements
### Requirement: 挂起-恢复协议（步内挂起）

步骤执行器 MUST 以 `ETcsStepResult{ TSR_Completed, TSR_Running }` 表达**步内挂起**（D4-17，与 PC 挂起、事件订阅并列的第三种挂起形态）：

- `TSR_Completed` → PC 前进；`TSR_Running` → **PC 停在原步**、运行态保持活动、解释器立即让出控制权（MUST NOT 原地自旋）；
- 挂起中的运行态 MUST NOT 产生每帧成本——门面 MUST NOT 是 Tickable 子系统，唤醒一律由唤醒源驱动。**本轮落齐三类唤醒源**（`04 §2.4` 四种之中除"外部显式调用"外）：①**到期堆**（`WaitDelay` 已实证；`WaitEvent` 的超时复用）；②**事件匹配**（`WaitEvent`）；③**子链完成**（`RunSubChain` / `Branch`）；
- **每个唤醒源 MUST 在运行态上留可配平的锚**，且**运行态释放即解锚**：`ReleaseRun` MUST 释放该运行态挂在任何唤醒源上的锚（到期条目 + 事件订阅）——否则留下"回调打到已回收运行态"的幽灵锚（到期锚已由代际校验兜底，**事件订阅锚不会**：总线按 Tag 派发、不看运行态代际 ⇒ 订阅泄漏是静默的）；
- **多锚竞态**：同一挂起步同时挂两类锚时（如 `WaitEvent` 的"事件命中"与"超时"），先到者 MUST 取消另一者——两条路径都兑现会留下幽灵锚并使步骤被二次唤醒；
- 唤醒回入口 `ResumeRun(FTcsChainRunHandle Handle)`：MUST 做**句柄代际校验**，悬空/已释放/世界将拆的句柄静默返回（挂起条目与运行态的竞态是正常路径，不是契约违规）；
- **"运行态被释放"也是唤醒源**：`ReleaseRun` 是链结束的唯一汇聚点（正常走完、单帧步数熔断、链定义不可解析、未知步骤类型四条路径全部经过它），它 MUST 通知"正在等待本链结束"的等待方（子链步骤）——**异常结束同样通知**（否则父链永久挂起 = 死链，比"错误地继续"更难排查）；
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

#### Scenario: 释放时解锚

- **WHEN** 一条挂起在事件等待上的运行态被释放（走完 / 熔断 / 断链任一路径）
- **THEN** 它在该事件上的订阅被同步退订，随后发布同一事件不再触发任何回调（订阅数回落）

#### Scenario: 双锚竞态只兑现先到者

- **WHEN** 一个同时挂了"事件命中"与"超时"两锚的等待步，事件先到
- **THEN** 步骤按事件命中完成，且超时条目被取消——到期时刻过后不再产生第二次唤醒

### Requirement: 单帧步数熔断

**单次进入执行**的步数 MUST 受链定义 `MaxStepsPerFrame`（默认 64）限制——超限即视为运行中链失控（自激/循环）：

- 超限 MUST 熔断：ensure 提示 + Error 日志（含链 id 与上限）+ 释放运行态（断链）；
- 熔断判定 MUST 与"链正常走完"区分——正常走完是零诊断噪音的正常路径。
- **预算按"最外层进入"计**（本轮补，2026-10-03）：`MaxStepsPerFrame` MUST 是**一次最外层进入**（`ExecuteChain` / `ResumeRun` 触发的 `RunFrom`）的**总**预算，**嵌套起链共用父链预算**——子链步数 MUST 计入父链的本次进入，**MUST NOT** 各自获得新预算。适用于 `RunSubChain` / `Branch` 的同步嵌套（`bWait=true` 时步内直接起链）。
- **取哪一条链的上限**：嵌套时以**最外层**链定义的 `MaxStepsPerFrame` 为准（内层链定义的上限在本轮不参与裁定——它是"该链单独被执行时的上限"，见上一条）。
- **嵌套深度 MUST 另有硬上限**（本轮补）：预算由链定义给出、**作者可配到很大**，故它 MUST NOT 作为调用栈深度的唯一护栏。嵌套深度（同时活跃的 `RunFrom` 层数）MUST 受一个**固定常量**上限约束，超限即熔断断链 + Error 日志（含链 id 与深度）。该上限是**调用栈护栏而非行为语义**，MUST NOT 做成链定义字段。**实测值 MUST 写进计划注记**（本轮 Task 3.5 Step 4）。

#### Scenario: 超限熔断

- **WHEN** 一条链的单次进入执行步数超过其 `MaxStepsPerFrame`
- **THEN** 停止执行、留 ensure + Error 日志（含链 id 与上限）、运行态释放

#### Scenario: 上限内零噪音

- **WHEN** 链的单次进入执行步数不超过上限
- **THEN** 正常走完或挂起，无任何熔断诊断输出

#### Scenario: 子链步数计入父链预算

- **WHEN** 一条自激链通过子链步骤递归起链（A 起 B、B 再起 A）
- **THEN** 子链的步数累加进同一次最外层进入的预算；累计超限即熔断断链（**MUST NOT** 每层各自重置为 64 步而永续跑下去）

#### Scenario: 嵌套过深被护栏拦下

- **WHEN** 一条把 `MaxStepsPerFrame` 配到远超深度上限的自激链，通过子链步骤递归起链
- **THEN** 嵌套深度触到固定上限即熔断断链 + Error 日志（含深度值），不因预算宽松而耗尽调用栈

## ADDED Requirements
### Requirement: 链内变量步骤（SetVar）

`TcsEffect` MUST 提供链原语 `FTcsStepSetVar`（D4-3 元原语；`Public/Chain/TcsStepSetVar.h`）——**它是 `FTcsEffectContext::Variables` 的首个写入方**（该字段此前只有门面按句柄访问器可写，"R3 无写入方"的留位注记随之作废）：

- 形状 `FTcsStepSetVar{ FGameplayTag VarKey; FTcsParamValue Value; }`（`USTRUCT()`）；
- **`Value` MUST 用 `FTcsParamValue`**（全插件统一数值配置载体）——**MUST NOT** 退化为裸 `double` 字段：那会让"变量只能写字面量"成为隐式限制，与链上其它数值字段的书写能力不一致；
- 求值 MUST 经 `FTcsParamValue::Evaluate(const FTcsParamEvaluateContext&)`；求值上下文由**本步骤所在运行态**装配（求值器/参数账本的取用口径以 `param-value` 能力为准，本需求不重复）；
- **`VarKey` MUST 落在 `EffectChainRunVar` 根下**——与门面 `SetRunVariable` / `TryGetRunVariable` **同一个键空间**（两者写读的是同一张 `Variables` 表；分根会让"步骤写的变量脚本读不到"）；
- **即时步骤**：恒返回 `TSR_Completed`（不挂起）；
- `VarKey` 无效（`!VarKey.IsValid()`）时 MUST 跳过写入 + Warning（**不静默丢弃**：写不进去与写进去值不对必须可区分）；
- 已存在的键 MUST 覆写（同键重复 SetVar = 后写生效——顺序链的天然语义）。

#### Scenario: 写入后同链后续可读

- **WHEN** 链中 `SetVar` 写入 `EffectChainRunVar.Foo = 3.0`，后续步骤经门面 `TryGetRunVariable` 读同一键
- **THEN** 读到 `3.0`

#### Scenario: 脚本层可读步骤写入的变量

- **WHEN** 一条含 `SetVar` 的链挂起时，宿主脚本层按运行态句柄调 `TryGetRunVariable(Handle, EffectChainRunVar.Foo, Out)`
- **THEN** 读到该步骤写入的值（两者共用一个键空间、同一张表）

#### Scenario: 无效键不写入但留痕

- **WHEN** `SetVar` 的 `VarKey` 为无效 tag
- **THEN** 不改动 `Variables`，留 Warning 日志，链继续前进

#### Scenario: 数值可来自参数源而非仅字面量

- **WHEN** `SetVar.Value` 的源配成非 Literal 的源（如 ParamRef）
- **THEN** 写入的是该源在本次运行态下的求值结果，而非任何字面量默认值

### Requirement: 控制流步骤（RunSubChain / Branch）

`TcsEffect` MUST 提供两个控制流原语（`Public/Chain/TcsStepRunSubChain.h`、`Public/Chain/TcsStepBranch.h`）：

- `FTcsStepRunSubChain{ FGameplayTag ChainId; bool bWait = true; }`——起一条已登记的链；`bWait=false` = 放支线（父立即前进，不跟踪子链结局）；
- `FTcsStepBranch{ TArray<FInstancedStruct> Conditions; FGameplayTag ThenChainId; FGameplayTag ElseChainId; bool bWait = true; }`——条件**全过**走 `ThenChainId`、否则走 `ElseChainId`（空 id = 该支不执行，仍算本步完成）；
- **`Branch` 与 `RunSubChain` MUST 共用同一套"子链完成唤醒"机制**——**MUST NOT** 各写一套唤醒逻辑（`Branch` 的 `bWait` 直接复用 `RunSubChain` 的语义）。

**步内起链纪律（既有运行态纪律的直接推论，本轮首次被真正消费）**：`FTcsChainRun.h` 已明文禁止"执行器持运行态/上下文引用期间起新链"（池元素住连续数组，扩容即搬移）。本批两个步骤 MUST 遵守：

- 起链**前**取齐所需数据（链 id、需要传给子链的黑板字段），起链**后 MUST NOT 再触碰** `Context` 与 `Run` 两个引用；
- 解释器侧 MUST 在步骤返回后**按句柄重解析**运行态再推进 PC（该纪律已由 `RunFrom` 落实，本需求重申它是本批两个步骤成立的前提）。

**`bWait=true` 的挂起语义**：

- 子链**同步走完**（全部即时步骤）时，父 MUST NOT 挂起——本步按完成处理，链继续；
- 子链**挂起**时，父 MUST 挂起（`TSR_Running`）并被"子链完成"唤醒；唤醒后本步按完成处理；
- 子链**异常结束**（单帧步数熔断 / 链定义不可解析 / 未知步骤类型）**同样唤醒父**并按完成处理 + Error 日志（裁定依据见 `design.md`：不唤醒 = 父永久挂起 = 死链，比"错误地继续"更难排查）；
- **`ChainId` 未登记**时 MUST NOT 挂起：Error 日志 + 本步按完成处理（照 `ExecuteChain` 未登记即拒绝的既有口径，不 ensure）；
- **`ChainId` 为空**时 MUST 按完成处理 + Warning（`Branch` 的空支同理，但属正常配置、不记 Warning）。

**`Branch` 的条件求值**：

- MUST 复用 `effect-trigger` 的**条件注册表**（一套条件类型两处可用，零重复实现）；
- 链侧求值上下文由 `FTcsEffectContext` **最小映射**构造——本批**只有 `Caster` 有来源**；`FTcsTriggerContext` 的 `EventTag` 与 `ClassificationTags` 在链侧**本轮无来源**，MUST 以无效 tag / 空数组构造 ⇒ **依赖这两者的条件在链侧恒不通过**。这是本批**明示接受的限制**，MUST 写进 `deferred-inputs-ledger.md`（自然补法 = 起链时把触发事件 tag 记进运行态）；
- 求值取随机值的口径 MUST 与触发行一致（取自门面的种子流，可复现）。

#### Scenario: 子链同步走完时父不挂起

- **WHEN** `RunSubChain bWait=true` 指向一条全即时步骤的链
- **THEN** 子链在本步内走完并释放，父本步返回 `TSR_Completed`、PC 前进（父不产生挂起-恢复往返）

#### Scenario: 子链挂起时父挂起并在子链结束时恢复

- **WHEN** `RunSubChain bWait=true` 指向一条含 `WaitDelay` 的链
- **THEN** 父停驻本步（运行态保持活动、零每帧成本）；子链走完被释放时父被唤醒、本步按完成处理、后续步骤继续

#### Scenario: 子链异常结束时父不被永久挂起

- **WHEN** `RunSubChain bWait=true` 指向一条会在单帧步数上熔断的链
- **THEN** 子链熔断断链，父被同样唤醒、本步按完成处理，并留 Error 日志（父 MUST NOT 停在挂起态）

#### Scenario: 未登记子链不挂起

- **WHEN** `RunSubChain` 的 `ChainId` 是有效 tag 但未登记
- **THEN** 本步按完成处理 + Error 日志，链继续（不挂起、不崩溃、不 ensure）

#### Scenario: 条件走 Then 与 Else 两支

- **WHEN** `Branch` 的条件数组分别全过 / 有不过，且 `ThenChainId` 与 `ElseChainId` 都配了链
- **THEN** 分别起对应那一支（另一支 MUST NOT 被执行）

#### Scenario: 空分支不执行但仍算完成

- **WHEN** 条件的走向那一支的 id 为空 tag
- **THEN** 不起任何链，本步按完成处理（`bWait=true` 时也不挂起）

### Requirement: 事件等待步骤（WaitEvent）

`TcsEffect` MUST 提供等待原语 `FTcsStepWaitEvent`（`04 §2.4` 唤醒源之"事件匹配"的首个载体；`Public/Chain/TcsStepWaitEvent.h`）：

- 形状 `FTcsStepWaitEvent{ FGameplayTag EventTag; double TimeoutSeconds = 0.0; }`（`USTRUCT()`）；
- **首入**：订阅 `EventTag`（立即通道），`TimeoutSeconds > 0` 时同时入**到期堆**（基准 = 战斗时钟 `Elapsed`，同 `WaitDelay`），随后返回 `TSR_Running`；
- **命中**：事件载荷 MUST 被写入该运行态的 `FTcsEffectContext::EventPayload`（后续步骤据此读本次事件）→ 唤醒 → 本步退订、配平锚、返回 `TSR_Completed`；
- **超时**：唤醒 → 本步退订、配平锚、返回 `TSR_Completed`（`EventPayload` 保持原样——超时不是事件，MUST NOT 伪造成载荷）；
- 两条路径 MUST 互相取消对方（见 `effect-interpreter` 的挂起-恢复协议"多锚竞态"条）；
- `EventTag` 无效时 MUST NOT 挂起：立即完成 + Warning（照 `WaitDelay` 的缺设施降级口径）；
- 世界/总线设施不可得时 MUST 降级为"本步按完成处理" + Error 日志（不挂死、不崩溃——同 `WaitDelay`）。

**订阅形态纪律（本批的核心设计决定，依据 `design.md`）**：

- **同一 `EventTag` 的多个等待运行态 MUST 共用一条总线订阅**（计数配对：首个等待者订阅、最后一个离开时退订）——**MUST NOT** 每个等待各订一次。依据 = 既有明文纪律（`FTcsTriggerRegistry`："那会让总线订阅表随行数膨胀且退订易漏"）；另一条硬事实是**总线派发不把订阅句柄交给 Handler**（只传 `(EventTag, Payload)`）⇒ 每等待一条订阅既无收益、又让回调无法自辨身份；
- 运行态 MUST NOT 持有订阅句柄；它持**"在等哪个 tag"**这一可配平锚（句柄归门面侧的等待表）；
- 唤醒路由 MUST 由门面侧完成：**按 tag 取等待者快照 → 逐个代际校验 → 写载荷 → `ResumeRun`**——MUST NOT 在遍历中持运行态指针（`ResumeRun` 会执行步骤、可能扩容池并搬移元素）；
- 门面 MUST 持一个**可被 GC 看见的**共享事件处理器对象承接该订阅（总线订阅表对 Handler 是**弱引用**，不 root 会被 GC 掉、订阅静默失效——与触发求值器同款纪律）；
- 退订 MUST 覆盖三条路径：①事件命中；②超时；③运行态被释放（见挂起-恢复协议的"运行态释放即解锚"）。

#### Scenario: 事件命中唤醒并带回载荷

- **WHEN** 一条链停在 `WaitEvent`（`EventTag = TcsEvent.Damage.AfterDamage`、无超时），随后该事件被发布
- **THEN** 链被唤醒、后续步骤读到 `Context.EventPayload` 为本次事件的载荷、后续步骤继续执行

#### Scenario: 超时唤醒且不伪造载荷

- **WHEN** 一条链停在 `WaitEvent`（`TimeoutSeconds = 0.5`）且期内无该事件
- **THEN** 到战斗时间 0.5s 时链被唤醒、本步按完成处理，且 `Context.EventPayload` 未被改写

#### Scenario: 同 tag 多等待者共用一条订阅

- **WHEN** N 条链同时停在等待同一个 `EventTag` 的 `WaitEvent` 上
- **THEN** 该 tag 在总线上只有一条订阅（订阅数不随 N 增长）；事件发布时 N 条链各自被唤醒一次、各拿到同一份载荷

#### Scenario: 释放后不泄漏订阅

- **WHEN** 一条停在 `WaitEvent` 的链被释放（或世界反初始化），随后发布该事件
- **THEN** 无任何回调触发该已释放运行态（订阅计数回落；无 ensure、无 Error）

#### Scenario: 无效事件 tag 不挂起

- **WHEN** `WaitEvent` 的 `EventTag` 为无效 tag
- **THEN** 本步立即按完成处理 + Warning（链不挂起）
