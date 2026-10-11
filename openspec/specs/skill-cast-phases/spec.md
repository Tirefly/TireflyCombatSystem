# skill-cast-phases Specification

## Purpose

M5 技能层的**施法时段面**：让"施法是一个有过程的事件"成为可裁决、可观测的运行事实——时段表按战斗时钟到期堆**逐段推进**并广播 `TcsEvent.Cast.PhaseChanged`；可打断性与可移动性按**查询契约三档**（定义开关 / 当前时段 / 宿主自定义）各自读自己的载体；打断**先过查询契约**再结算；以及**链重定向栈**（按单位与目标链把一次施法让渡给另一条链，带优先级与后挂者胜）。

**为什么需要它**：`skill-cast-runtime` 交付的是"技能放得出来且当刻就有运行态"，但**施法没有过程**——没有时段就没有"打到一半"，没有中断点就没有打断的语义落点，也就无法表达"前摇 / 后摇 / 引导"这类宿主内容词汇（本能力刻意**不内置**这些词，只提供宿主可声明的时段标识）。

**边界**：本能力管**时段的推进与终结**（到期驱动 / 逐段事件序 / 三档查询 / 打断与顶替的门禁 / 重定向挂摘），不管**恢复与冷却**（归 `R6.5`），也不管段内表演（表现层归 R8）。四条终结路径（全时段走完 / 瞬发当刻完成 / 打断 / 顶替）**汇聚同一个终结例程**，本能力规定其**只在"发哪一枚终止事件"处分岔**。

## Requirements

### Requirement: 时段推进与到期驱动

`TcsSkill` MUST 按定义的 `FTcsSkillDefData.Phases` 推进施法过程，且**推进 MUST 经到期堆驱动、MUST NOT 自建定时机制**：

- 时段时长的求值 MUST 取**本次施法运行态的激活期参数快照**：进入时段时求值该段 `Duration`（`FTcsParamValue`），求值上下文的 `ParamTable` MUST 绑定本 run 的 `FTcsCastRun.ParamSnapshot`（经 `UTcsStateParamTableReader` + `FTcsStateSnapshotScope`，与 `ParamChainRows` 物化**同一套绑定手法**）。⇒ 引用类时长（`Param()`）取的是**激活时刻冻结的值** ⇒ "被修正器改变时段时长 ⇒ 下一次激活生效"**语义天然成立**；
- 入堆 MUST 经 `UTcsClockSubsystem::PushExpiry(DueTime, OwnerId, OnDue)`，到期时刻 = `Clock->GetClock().Elapsed + Duration`（**Elapsed 时轴上的绝对时刻**）。**`FTcsExpiryHeap` 是泵私有，MUST NOT 直接触达**（`AdvanceTo` 保持泵私有——时间推进唯一驱动）；
- 到期条目句柄 MUST 存入 `FTcsCastRun::PhaseExpiryEntry`**作为取消锚点**——施法终结（四路任一）MUST 经 `UTcsClockSubsystem::CancelExpiry` 取消它，**MUST NOT** 让作废的到期回调在池槽位复用后命中下一个 run（句柄代际校验是第二道防线，取消是第一道）；
- 到期推进 MUST 依次：`++PhaseIndex` → 发 `TcsEvent.Cast.PhaseChanged` → 若仍有待推进时段则注册下一段，否则走**完成终结**（终结例程与四路汇聚语义住 `skill-cast-runtime` 能力的「施法运行态与实例池」需求——**本能力只定义"何时该完成"，不重复定义"完成后做什么"**）；
- **"还有没有待推进时段"的判据 MUST 取 `PhaseIndex < Phases.Num()`**，**MUST NOT** 写成 `Phases.IsEmpty()` 这类空表判定——那会让"配了时段但已走到末尾"与"空时段表"走两条不同的路（同一件事两个入口）。**瞬发（空时段表）因此不需要分支**：它天然产生"零个待推进时段"这一状态；
- **瞬发（空 `Phases`）= 激活当刻完成**：激活流程末尾直接走完成终结，**MUST NOT** 留下一个永不终结的在册运行态（那会让唯一的关闭口是打断 ⇒ 一次成功施法被报成"被打断"，且池槽位与账本 `RunHandles` 永久占用）；
- 时钟子系统不可得（世界拆解 / 单测无时钟）时 MUST **静默降级**（`Log` 级、零红字）并**不留待推进时段**——退化为瞬发语义，MUST NOT ensure。

#### Scenario: 多时段逐段推进且事件序正确

- **WHEN** 一个配三段的技能激活并逐帧推进到全部时段到期
- **THEN** `PhaseIndex` 依次为 `0 → 1 → 2`，`TcsEvent.Cast.PhaseChanged` **恰好广播 2 次**（进入第 2、3 段各一次；进入第 1 段**不计**——激活时的 `PhaseIndex` 已是 0，它是起点不是变化），随后 `TcsEvent.Cast.Completed` **恰好 1 次**，且 `Completed` 在所有 `PhaseChanged` **之后**

#### Scenario: 瞬发技能激活当刻完成

- **WHEN** 一个 `Phases` 为空数组的技能激活成功
- **THEN** 返回 `SAR_Success`；该 run **不是**在飞状态（随后的在飞读数 = 0）；`TcsEvent.Cast.Completed` **恰好 1 次**，`TcsEvent.Cast.PhaseChanged` **零次**，`TcsEvent.Cast.Interrupted` **零次**——**判据 MUST 含"打断零次"**（只验"完成一次"会让"用打断冒充完成"的实现全绿）

#### Scenario: 时段时长从激活期快照求值

- **WHEN** 某段 `Duration` 配 `Param()` 引用技能参数键 `CastTime`，先激活一次（`CastTime` = `2.0`）并让它跑完，再改 `CastTime` = `1.0` 后**第二次**激活
- **THEN** 第一次实际等待约 `2.0` 秒、第二次约 `1.0` 秒（两次读数互异 ⇒ 证明时长**确实取自快照**而非写死常量）；且**同一 run 存续期内**改 `CastTime` 不影响该 run 的剩余时长

#### Scenario: 终结时取消未到期的时段锚点

- **WHEN** 一个三段技能在第 1 段期间被打断（或顶替）
- **THEN** 后续时段的到期回调**不触发**（`PhaseIndex` 不再前进、`PhaseChanged` 不再增加）；即使池槽位被下一次激活复用，新 run 的时段也**不被旧 run 的残留到期条目**推进（症状对照 = 旧句柄代际失配被跳过）

### Requirement: 施法状态查询契约三档实现

`TcsSkill` MUST 提供 `IsInterruptibleNow()` / `CanMoveNow()` 两个查询，且它们是**打断结算与宿主移动系统询问"这个施法现在能不能打断 / 能不能移动"的唯一合法入口**——宿主 MUST NOT 自行读 `FTcsSkillDefData` 的开关字段来代判（那会在三档之间制造第二真相）：

- **接口形态** MUST 为 `UINTERFACE(MinimalAPI, Blueprintable)` + `UFUNCTION(BlueprintNativeEvent)`（照 `TcsEntityLevelProvider.h:17-34` 的既有体例），形参 MUST 为**已反射的句柄** `FTcsCastRunHandle`（`FTcsCastRun` 自身**非反射**，MUST NOT 作 `BlueprintNativeEvent` 形参——那是 UHT 硬错误）；接口住 `Public/Skill/TcsCastStateQuery.h`，类名 `UTcsCastStateQuery` / `ITcsCastStateQuery`；
- **宿主可达性**（为什么必须是 `UINTERFACE` 而非 USTRUCT 策略基类）：宿主**既有类**可直接挂接口（一个类可挂多个插槽），不必为每个插槽专造对象；且反射分发使蓝图 / C# / Lua / TS 任一语言都可实现；
- **三档 MUST 各有字段载体、MUST 按 `ECastQueryMode` 分派**（Task 1 已交付三档字段，本需求是它的**运行期消费者**）：
  - `CQM_DefSwitches`（**默认档**）⇒ 读 `bInterruptibleDefault` / `bCanMoveDefault`；
  - `CQM_PhaseTable` ⇒ 读**当前时段**（`Phases[PhaseIndex]`）的 `bInterruptible` / `bCanMove`；
  - `CQM_Custom` ⇒ 求值 `CastQueryFragment` 为 `ITcsCastStateQuery`；
- **`PhaseTable` 档的前置条件 MUST 具名降级**：**无当前时段**（`Phases` 为空，或 `PhaseIndex` 已越界）⇒ MUST 记具名 `Log` 并**回退 `DefSwitches` 档**。**MUST NOT** 静默读一个不存在的时段（越界访问）；**MUST NOT** 因"没有时段"就返回"永不可打断"（那会让瞬发技能变得不可打断，是一处无声的行为变更）；
- **`Custom` 档本轮 MUST 具名报出"未实现"并回退 `DefSwitches` 档**（契约求值归 `R6.5-g`）：MUST 记具名 `Warning`（含档名），**MUST NOT** 静默当作默认档、也 **MUST NOT** 让该档的技能变成永不可打断（回退是本轮**明确选定**的行为，不是漏做）；
- **Task 3 的零回归接缝 MUST 保住**：Task 3 的顶替路径取 `bInterruptibleDefault`（即 `DefSwitches` 档）。本需求落地后顶替**改调 `IsInterruptibleNow()`**，而该档读的就是同一字段 ⇒ **行为逐字不变、零回归**；
- 查询 MUST NOT 有副作用（纯查询语义）：拒绝判定 MUST NOT 改运行态、MUST NOT 发事件、MUST NOT 动账本。

#### Scenario: 三档各一读数且互不混同

- **WHEN** 同一技能分别配 `DefSwitches`（开关为 `false`）/ `PhaseTable`（第 1 段 `bInterruptible = true`）/ `Custom` 三档并各查询一次
- **THEN** 依次得 `false`（默认档开关）／`true`（当前时段字段）／`false` + **一条具名"该档未实现"的 `Warning`**（回退默认档）——三档读数由各自载体决定，**MUST NOT** 三档都得同一个数

#### Scenario: 时段表档按当前时段变化

- **WHEN** 一个三段技能配 `PhaseTable` 档，第 1 段 `bInterruptible = false`、第 2 段 `bInterruptible = true`，在激活后与推进到第 2 段后各查询一次
- **THEN** 第一次为 `false`、第二次为 `true`（⇒ 证明读的是**当前**时段而不是首段或某个固定段）

#### Scenario: 无当前时段时回退默认档而非越界读

- **WHEN** 一个 `Phases` 为空的技能配 `PhaseTable` 档（无当前时段）
- **THEN** 查询返回 `bInterruptibleDefault` 的值（**不是**越界访问、**不是**无条件的 `false`），并留下一条具名 `Log`

### Requirement: 打断

`TcsSkill` MUST 提供打断入口 `CancelCast(FTcsCastRunHandle Run, <打断原因>)`，其结算 MUST 先过查询契约、MUST NOT 无条件打断：

- 结算序：① 经 `IsInterruptibleNow()` 判可打断性 → ② 不可打断 ⇒ **拒绝**（返回 false + 具名 `Log`，**运行态原样继续**、MUST NOT 发任何事件、MUST NOT 动账本）→ ③ 可打断 ⇒ 走**四路共用的终结例程**并记原因为"打断"（终结例程与回收面住 `skill-cast-runtime` 能力的「施法运行态与实例池」需求——**本能力只定义"什么时候允许打断 + 记什么原因"，不重复定义"终结时摘什么"**）；
- 打断 MUST 发 `TcsEvent.Cast.Interrupted`、**MUST NOT** 发 `TcsEvent.Cast.Completed`——"被打断"与"跑完了"是两件事，**MUST NOT** 用完成事件冒充打断（那会让配了 `OnCastCompleted` 起链的技能在被打断时真的打出主链）；
- **止于未来不追溯**（M4 约定）：打断 MUST NOT 回退**已发生**的副作用（已挂的账本条目按来源回收是**生命周期终结**，不是"回滚历史"）；
- **来源优先级仲裁本轮如实登记为未落面**：设计原词是"来源优先级 vs `IsInterruptibleNow()`"，而**全仓无任何类型声明"打断来源的优先级"**（技能关系字段 `Priority` 的语义属 `R5.5-e` 关系字段检查器，尚未定稿）⇒ 本轮**只落可打断性裁决**，**MUST NOT** 为它发明一个零消费者的优先级形参（"零消费者不预建"）。

#### Scenario: 可打断段被打断成功

- **WHEN** 一个当前时段 `bInterruptible = true` 的技能在飞时调用 `CancelCast`
- **THEN** 返回成功；`TcsEvent.Cast.Interrupted` **恰好 1 次**、`TcsEvent.Cast.Completed` **零次**；该 run 不再是飞状态

#### Scenario: 不可打断段被打断被拒（阴性对照）

- **WHEN** 同上但当前时段 `bInterruptible = false`（或默认档开关为 `false`）
- **THEN** 返回**失败**；`TcsEvent.Cast.Interrupted` **零次**、`TcsEvent.Cast.Completed` **零次**；**且旧 run 原样继续**——判据 MUST 含"运行态仍可解析 + 在飞读数不变"（只验"没有事件"会让"静默把运行态弄坏了"的实现全绿）

### Requirement: 链级重定向栈

`TcsSkill` MUST 提供**链级让渡**的重定向栈，使 `CastChainId` 的解析为三级顺序：**全局定义（`FTcsSkillDefData.CastChainId`，默认）→ 链重定向栈 → Custom 契约**：

- **栈条目 MUST 为纯反射数据** `FTcsChainRedirectEntry`（至少含 `Unit` / `TargetChainId` / `ReplacementChainId` / `Source` / `Priority`），使"策划零代码"表达成立（D5-7 的"数据重定向行，标准件保留"）——**MUST NOT** 做成"只能由 C++ 注册"的形状；
- **挂 / 摘 API MUST 成对**：按来源 `Source` 级联摘除（判据同参数账本的成对性——不存在"摘不掉的条目"）；**来源无效 ⇒ 拒绝挂载**；
- **解析 MUST 按 `(Unit, TargetChainId)` 命中**，多条命中时**取 `Priority` 大者、打平取后挂者**（"后挂/高优先"的设计原词）；
- **`CastChainId` 解析结果 MUST 用于起链**（Task 3 的 `StartMainChain` 改走本解析口），且 **MUST NOT 修改共享 Def**（设计原文明文："绝不修改共享 Def"）——重定向是**解析期让渡**，不是**数据改写**；
- **Custom 契约本轮 MUST 具名报出"未实现"**（`ITcsChainRedirectResolver` 注入点可留在形状上，但其求值归 `R6.5-g`）；未命中任何条目 ⇒ 落**全局定义**（默认档），**MUST NOT** 报"未实现"（那是正常路径）；
- **MUST NOT 落 `FFlowRedirect` 的流程级实现**（同族，归台账 `STAT-3` / `R5.5-d`——不重复造）；
- **技能级让渡不存在**：`EffectiveDefId` 已于 2026-09-11 整体移除 ⇒ 让渡模式是**两粒度**（参数级 + 链级）；**技能级 = 换成新 Skill**。**MUST NOT** 在本能力里重建技能级重定向。

#### Scenario: 挂上重定向后解析改变目标链

- **WHEN** 某技能的 `CastChainId` 为 `ChainA`，挂一条 `(Unit, ChainA) → ChainB` 的重定向后激活该技能
- **THEN** 实际起来的链是 `ChainB`（**不是** `ChainA`）——读数取**链运行态实际用的链身份**，**MUST NOT** 取"重定向表里有几条"

#### Scenario: 摘除后解析回到全局定义

- **WHEN** 承接上一条，按 `Source` 摘除该重定向后再激活
- **THEN** 实际起来的链回到 `ChainA`——**挂与摘各取一次读数**（只验"挂上生效"会让"摘除没实现"或"摘错了"全绿）

#### Scenario: 高优先级与后挂者胜出

- **WHEN** 同一 `(Unit, ChainA)` 上先挂 `→ ChainB`（`Priority` 小）、再挂 `→ ChainC`（`Priority` 大）
- **THEN** 解析结果为 `ChainC`——判据 MUST 覆盖**优先级**与**后挂**两个判据（只验其一无法区分两种实现）

#### Scenario: 未命中任何重定向时走全局定义且不报未实现

- **WHEN** 技能自身 `CastChainId` 有效但栈上没有任何 `(Unit, 该 id)` 条目时激活
- **THEN** 起来的链就是该技能自己声明的链，且**不产生**"重定向未实现"的 `Warning`（未命中是**正常路径**，不是未实现）
