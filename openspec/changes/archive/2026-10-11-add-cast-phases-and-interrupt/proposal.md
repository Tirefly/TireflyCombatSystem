# Change: 施法时段、查询契约、打断与完成终结（含 `CHAIN-7` 闭合）

> 对应计划 `PLN-R6` Task 5（`Documents/combat-system-design/plans/plan-r6-skill-layer.md`，Step 1~7）。
> 依赖：Task 3（`FTcsCastRun` / 六道门禁 / 施法事件四枚，提案 `add-cast-run-and-gates` 已归档）/
> Task 4（参数快照消费面 + 参数链，提案 `add-skill-param-chain` 已归档）。
>
> **★ 本稿对计划做了一处形状扩充，经用户 2026-10-09 裁定（甲′）**：施法终结**不是三路而是四路**——
> 计划 Step 3 只写"全时段走完 / 打断 / 顶替"**三路**汇聚，其中"全时段走完"**对空时段表（瞬发）永不成立**
> ⇒ 本稿补上**第四路：瞬发 = 激活当刻完成**。判据与代价见 `## Why` 第 3 节。

## Why

### 1. 技能今天"放得出去"，但**没有过程、也不会正常结束**

Task 3 交付了六道具名门禁与施法运行态，Task 4 让参数真的能改数；但施法运行态一旦建立，
**它只会在"被顶替"时消失**。三处实测：

- `FTcsCastRun::PhaseIndex` **恒为 0**、`PhaseExpiryEntry` **零写入者**（`TcsCastRun.h:65/126`）；
- `Tag_TcsEvent_Cast_PhaseChanged` / `Tag_TcsEvent_Cast_Completed` 两枚**零产生者**（`TcsCastEvents.h:30-34`）；
- 池的全部进入点只有三条（实测 `TcsCastOps.cpp:224/432/457`）：`Allocate`（激活）、`Free`（**仅** `TerminateRun` 内）、
  `Reset`（世界拆解）。而 `TerminateRun` **按设计只发 `OnCastInterrupted`**
  （`TcsCastOps.cpp:398` 明文 *"MUST NOT 广播完成事件"*，因为它在 Task 3 的唯一调用者是顶替）。

⇒ **"技能跑完了"这件事今天无法表达**：唯一的关闭口只会说"被打断了"。
本变更让施法有过程、并且**有一条不为打断服务的终结例程**。

### 2. 参数快照的**时段时长**消费面是 Task 3 留下的欠账

`SPEC-04-skill` §3.3 与 D5-1 都写"时段 `Duration` 支持 `Param()` 引用"，而计划 Task 5 Step 1 明确
"`Duration` **从 `ParamSnapshot` 取**"。今天 `Phases` 字段已就位（Task 1 交付）却**全仓装置零配置**
（实测 `TcsDevSkillCastProbe.cpp` 的 `Phases` 命中 = **0**）⇒ 这条设计承诺从未被运行验证过。

### 3. ★ 空时段表（瞬发）的完成时点，计划未定义（本稿补）

`skill-def-asset` 规格把"瞬发"**钉死为 `Phases` 空数组**（MUST NOT 用"单一零时长段"等价表达）。
而空表意味着**没有任何时段到期** ⇒ 计划 Step 1 的"全时段走完 → 发 `OnCastCompleted`"
**对瞬发技能是一条永不成立的规则**。

**这不是"瞬发特有的问题"，而是"框架缺一条完成路径"的暴露点**：配了时段的长技能至少有
"时段还没走完"可以挡着，而瞬发技能把这层遮挡拿掉了——它当场就暴露"没有任何正常结束"这一事实。

**用户 2026-10-09 裁定 = 甲′：建一条「完成」终结例程，瞬发 = 激活当刻完成。**
判据：**没有它，`Completed` 这条事件词在 R6 内不可达**（只剩"配了时段且真的等到到期"一条窄路），
Task 5 Step 1 与验收①各自只有一半成立；而"框架有一条正常结束路径"本就是施法运行态的**必要形状**，
不是为瞬发特设的机制。

**实现形状上"瞬发"不需要分支**：空 `Phases` 自然产生"没有待推进时段"这一状态 ⇒ 激活流程末尾
直接走完成终结即可（**判据 = 待推进时段数，不是"是否空表"**）。**MUST NOT** 写成
`if (Phases.IsEmpty())` 这类空表判定——那会让"配了时段但全为 0 时长"的等价情形走另一条路
（同一件事两个入口）。

### 4. 台账 `CHAIN-7` 在本轮闭合

`CHAIN-7`（"链挂的账本条目没有框架侧回收触发点"）**已于 2026-10-07 由用户裁定在 R6 内闭合**，
落点即 Task 5 Step 3。闭合依据（R6 新事实）：施法运行态是链运行态的**第一个长生命周期持有者**
——`TcsStepModifyAttribute.cpp:80` 把修正器条目挂在 `Context.RunSource` 上，而
Task 3 已定 `Context.RunSource` = `FTcsCastRun.RunSource` ⇒ **触发条件成立**。

**★ 闭合的是"触发点缺失"，MUST NOT 读成"三条死路被修"**：① 即时链当场释放运行态
（`TcsEffectSubsystem_Run.cpp:104-105`）、② 按 `CausedBy` 级联**已被明文禁止**
（`TcsEffectContext.h:54-55`）、③ `FTcsStepModifyAttribute` **刻意不提供**回收器
——**三条今天依然成立**。本变更落地的正是台账认定的"今天唯一合法用法"
（宿主自存 `RunSource` 再自调 `RemoveBySource`）所缺的那个触发点。

### 5. 链重定向栈：计划说"落挂点"，实测**宿主注入接口根本不存在**

计划 Step 4 写"`CastChainId` 解析 = 全局定义 → 链重定向栈 → Custom Fragment"，
而 `SPEC-04-effects` §2.2 说重定向栈"经注入接口 `IChainRedirectResolver`"。
**实测该接口与 `FChainRedirect` 类型在全仓 `Source/` 零命中**（`Source/` 全树 `Redirect` 仅 1 处命中，
且是 `GameplayTagRedirects` 的注释）⇒ **本轮不是"接一个既有挂点"，而是"从零建栈 + 建注入契约"**。
本稿按"零消费者不预建"收窄：**只落栈结构、挂/摘 API 与解析顺序，MUST NOT 落 Fragment 求值**。

## What Changes

### 1. ADDED `skill-cast-phases`（新能力）

新能力的四条需求（详见 `specs/skill-cast-phases/spec.md`）：

1. **时段推进**：进入时段 → `Duration` **从参数快照求值** → 经 `UTcsClockSubsystem::PushExpiry` 入到期堆
   → 到期推进 `PhaseIndex` → 发 `OnCastPhaseChanged`；**`FTcsExpiryHeap` 是泵私有，MUST NOT 直接触达**。
2. **施法状态查询契约三级实现**（D5-1）：`ITcsCastStateQuery` 的 `IsInterruptibleNow()` / `CanMoveNow()`，
   三档 = `DefSwitches`（默认）/ `PhaseTable`（当前时段字段）/ `Custom`（Fragment，**本轮具名报未实现**）；
   **它是打断结算与宿主移动系统询问的唯一合法入口**。
3. **打断**：来源优先级 vs `IsInterruptibleNow()` → `CancelCast(Run, Reason)` → 发 `OnCastInterrupted`；
   **止于未来不追溯**（M4 约定）。
4. **`CHAIN-7` 闭合 + 施法终结统一回收点**：见下第 3 节。

### 2. ADDED 完成终结（第四路终结；用户 2026-10-09 裁定甲′）

- **`Completed` 事件获得第一个产生者**（`TcsEvent.Cast.Completed`，Task 3 只落了词与载荷）；
- **瞬发（空时段表）= 激活当刻完成**：待推进时段数 == 0 ⇒ 激活流程末尾走完成终结；
- **`MCS_OnPhaseEnter` / `MCS_OnCastCompleted` 两档随之落地**（Task 3 只实现 `MCS_OnCastStarted`，
  其余三档**具名 Warning**）：完成路径使 `MCS_OnCastCompleted` 可达；时段推进使 `MCS_OnPhaseEnter` 可达
  （按 `MainChainStartPhaseTag` 命中）。`MCS_Custom` **仍归宿主编排 ⇒ 保持具名未实现**。

### 3. MODIFIED `skill-cast-runtime`（终结语义由"顶替专用"升为"四路统一"）

- 「施法运行态与实例池」的需求体**整块替换**（标题与全部 scenario 逐字保留，仅追加 scenario），
  补**四路终结**与**统一回收例程**的语义；
- 「施法事件四枚」的需求体**整块替换**，把 `PhaseChanged` / `Completed` 由"零产生者"改为"产生者已落地"。

**★ MODIFIED 的写法纪律（本仓三次实证）**：MODIFIED **整块替换**原需求 ⇒ MUST 粘贴**完整**的
原需求文本（标题 + **全部** scenario）+ 增补内容。**MUST NOT** 只写增量——那会在归档时丢掉原 scenario。

### 4. 施法终结 = **四路**汇聚到同一个例程（含 `CHAIN-7` 闭合）

四个终结点：① **完成**（全时段走完 / **瞬发当刻**）、② **打断**（`CancelCast`）、
③ **顶替**（Task 3 既有）、④ 世界拆解（`ResetAll`，**不逐路清理、不动账本**——见边界）。

**MUST 汇聚到同一个终结例程**（形如 `FinishCastRun(Subsystem, RunHandle, Reason)`），
**MUST NOT 各写一份清理**——三份清理必然漂移，而漂移的后果是**静默泄漏**（条目留在账本里、
属性不回退，且无人报错）。**四路只在一个地方分岔**：发哪一枚终止事件（完成 → `Completed`；
打断与顶替 → `Interrupted`）。

**回收动作**（`CHAIN-7` 闭合）：对 `Run.Unit` 调
`UTcsAttributeSubsystem::RemoveBySource(Unit, RunSource)`（该口**公开且已有真实使用者**
`TcsDamageSubsystem.cpp:226`），**且**既有 `FTcsParamChainOps::RemoveParamModifiersBySource`（Task 4 已落）。
两者**账本不同**（M2 属性账本 / M5 技能账本）、**来源句柄同源**（都取 `RunSource`）⇒ 一次摘净。

**重入语义 MUST 一并裁（Task 3 留下的如实边界）**：`TerminateRun` 里的广播是**当场同步派发**
⇒ 订阅者可在回调内**重入终结同一句柄**。Task 3 只挡住了"双次归还槽位"这一半，
**"打断事件发两次"仍在**（`TcsCastOps.cpp:420-428` 自认，归属即本 Step）。本轮按状态侧**已验证的解法**收口：
`InRemovalBroadcastHandles`（`TcsStateSubsystem.h:414`，进出广播窗口成对标记/解除，见
`TcsStateOps.cpp:188/244/247`）⇒ 递归终结同句柄**返回 false 且静默**，既不重复广播也不重复归还。

### 5. `MCS_OnPhaseEnter` / `MCS_OnCastCompleted` 两档落地（计划未明写，本稿补齐）

Task 3 的三档 Warning 是"**暂时**未实现"的具名报出，其归属本就是 Task 5（`TcsCastOps_Chain.cpp:25-28` 自认）。
本变更把 `OnPhaseEnter` 与 `OnCastCompleted` 两档实现，`Custom` 保持具名未实现。
**MUST NOT 静默跳过**——那会让配了该档的技能"不报错也不生效"（本仓反复拦的静默错误）。

### 6. 链重定向栈（D5-7 链级让渡；**只落挂点，不落 Fragment**）

`CastChainId` 解析顺序 = **全局定义 → 链重定向栈 → Custom Fragment**。
本轮落：栈结构（后挂/高优先）、挂/摘 API、解析顺序接线。
**不落**：`Custom Fragment` 求值（`CastQueryFragment` 同族，归 `R6.5-g`）；
**不落** `FFlowRedirect` 流程级实现（同族，归 `R5.5-d` / 台账 `STAT-3`，**不重复造**）。

**让渡模式 = 两粒度**（参数级 + 链级）——**技能级 = 换成新 Skill**（`EffectiveDefId` 已于 2026-09-11 整体移除）。

## 明确不做（非目标）

- **不做冷却与 Cost**（`R6.5-a` / `R6.5-d`）——门禁第 ③⑤ 道保持 Task 3 的占位判据不变。
- **不做 `Custom` 档查询契约与 `CastQueryFragment` 求值**（`R6.5-g`）——本轮**具名报出未实现且零行为**。
- **不做 `FFlowRedirect` 流程级重定向**（`STAT-3` / `R5.5-d`）。
- **不做网络姿态、不做动画/表现耦合**（设计 §7）。
- **不做"子链挂的条目随施法回收"**——**如实登记的残留边界**（见下）。
- **不做形态组内建机制**（`R6.5-g`）。

## Impact

### Affected specs

- **`skill-cast-phases`（ADDED）**——新能力（4 需求）。
- **`skill-cast-runtime`（MODIFIED ×2）**：「施法运行态与实例池」（补四路终结 + 统一回收例程）、
  「施法事件四枚」（`PhaseChanged` / `Completed` 产生者由"零"改为"已落地"）。

### Affected code（插件仓 TCS）

- **新**（落 `Public/Skill/` 与 `Private/Skill/`，**沿用既有领域子目录惯例**；
  计划 Step 交付物写的 `Public/Cast/` 是**新目录**，与 `TcsSkill` 现有 `Def/` `Skill/` 布局不一致
  ⇒ **本稿取既有 `Skill/`**，并在计划变更记录里如实订正）：
  - `Source/TcsSkill/Public/Skill/TcsCastPhase.h`——`FTcsCastPhaseState`（或等价物）+ 终原因枚举；
  - `Source/TcsSkill/Public/Skill/TcsCastStateQuery.h`——`UTcsCastStateQuery` / `ITcsCastStateQuery`
    （照 `TcsEntityLevelProvider.h:17-34` 的 `UINTERFACE(MinimalAPI, Blueprintable)` +
    `UFUNCTION(BlueprintNativeEvent)` 形态）+ 三级分派入口；
  - `Source/TcsSkill/Public/Skill/TcsChainRedirect.h`——重定向栈结构 + 挂/摘 API + `ITcsChainRedirectResolver` 注入契约；
  - `Private/Skill/TcsSkillOps_Phase.cpp`——时段推进与到期回调。
- **改**：
  - `Private/Skill/TcsCastOps.cpp`——`TerminateRun` 升级为四路统一终结例程 + 重入收口；
  - `Private/Skill/TcsCastOps_Chain.cpp`——`OnPhaseEnter` / `OnCastCompleted` 两档落地；
  - `Private/Skill/TcsCastOps_Snapshot.cpp` 或新增分片——时段 `Duration` 经快照求值；
  - `Public/TcsSkillSubsystem.h` + 分片——**新增 `GetClockSubsystem()` 读口**（门面现状**没有**它，
    实测零命中；照 `UTcsStateSubsystem::GetClockSubsystem()` 的既有形态）、`CancelCast` 入口、查询契约注入点；
  - `Public/Skill/TcsCastRun.h`——`PhaseExpiryEntry` 由"零写入者"改为"时段推进的取消锚点"（注释同步）。
- **跨仓**（宿主 LAC，`Source/TcsDev/`）：**扩既有 `TcsDevSkillCastProbe*`**（复用既有装置，不新建第四个）；
  **装置 def 夹具须补非空时段**才能继续测顶替四态（见下方"回归面"）；`Config/DefaultGameplayTags.ini`
  补检查词。**新屏显段 `0x7C5D7000`**（cursor base `0x6000`，实测为下一个空段）。

### 台账

- **`CHAIN-7` 标已闭合**（本轮为闭合落地轮）。
- **新增残留边界**（如实登记、**不另立条目**，随行标注）：**只摘本 run 直接挂的条目**——
  子链**另发新号**（`TcsEffectContext.h:49`）⇒ 子链挂的条目归子链自己的生命周期，本轮**MUST NOT 递归下探**
  （递归会摘掉不属于自己的东西，且 `CausedBy` 级联已被禁止）。
  ⇒ "技能起链 → 链起子链 → 子链挂属性且子链比施法活得久"这一组合下条目**仍会常驻**，
  **本轮如实登记、MUST NOT 假装已解**。

### 回归面（**逐条实测，MUST NOT 扩大或缩小**）

用户追加裁定甲′带来**一处真实的跨轮回归管理**，如实列出全部影响：

| 装置读数 | 影响 | 处置 |
|---|---|---|
| `R7`（`MCS_OnCastCompleted` 档未实现 Warning） | **本质上被本变更改掉**——该档正是本稿要实现的 | 与甲′**无关**；改为断言"该档**现在会起链**" |
| `C7` / `C8`（顶替四态：需"有在飞旧 run"） | 装置 6 个 def **从不设 `Phases`**（实测）⇒ 瞬发当刻完成 ⇒ **顶替找不到旧 run** | **夹具补齐**：顶替四态用的 def 配非空时段 |
| `C3` / `C5`（激活后"在册 run 数恰增 1"、`Started` 恰一次） | 瞬发完成发生在激活**返回之后**（到期堆语义）⇒ 读数时 run 仍在册 | **预计不变**；实施后以实测为准并回写 |
| 其余读数（门禁六道 / `C13` 快照 / `C15` GC / Task 4 参数链 26+9） | 不涉时段与终结 | **预期逐字不变** |

**判据 = Task 3 的 `26/0` + `10/0` 基线在夹具补齐后重新取一次，并把"被 Task 5 取代"的部分就地留痕**
（**MUST NOT** 只改代码不记账，也 **MUST NOT** 声称"零回归"）。

### 一处设计文本同批划改（MUST NOT 漏）

`spec/05-module-skill.md:145` 的"时段驱动"只写"进入时段 → 到期堆注册 → `OnCastPhaseChanged`"，
**未写完成路径**；`:146` 的"打断"未写**归一例程**。⇒ 按"保留原文 + 就地注明"处置
（本仓 `MEM-20261009-01` 的纪律：设计文本的字段表是意图速写，MUST NOT 当形状真相）。
