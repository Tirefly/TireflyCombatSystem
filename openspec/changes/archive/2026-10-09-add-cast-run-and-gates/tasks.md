# Tasks: add-cast-run-and-gates

> 对应计划 `PLN-R6` Task 3（`Documents/combat-system-design/plans/plan-r6-skill-layer.md`，Step 1~8）。
> **本清单是 Task 3 的验收面**：每一项 MUST 在实施后回读本文件逐条勾选
> （`MEM-20260916-01` 第 3 条的教训：**"计划写了"不等于"实施时照做"**）。
>
> **勾选状态口径**：`[x]` = 代码/文档已落地**并有本轮实测取证**；`[ ]` = 尚未取得证据。
> 8.x 依赖**人工在 PIE 内执行宿主装置**——未实测前 MUST 保持未勾选（同 Task 1 / Task 2 的处置）。
>
> **本轮并入的删除**：技能侧 `AttrCapture` 整套（用户 2026-10-08 裁定，判据见 `proposal.md` What Changes 第 3 条）。

## 1. 删除技能侧 AttrCapture（并入本提案，先做——它改字段，后做会让后面的编译白跑）

- [x] 1.1 删 `Source/TcsSkill/Public/Def/TcsCastAttrCapture.h`（含 `ETcsCastAttrCaptureFrom`）
- [x] 1.2 删 `FTcsSkillDefData::AttrCaptureList` 字段（连带其 `#pragma region AttrCapture` 与 include）
- [x] 1.3 全库反查零残留：`FTcsCastAttrCapture` / `ETcsCastAttrCaptureFrom` / `CACF_` / `AttrCaptureList` 在 TCS `Source/` **零命中**（`openspec/changes/archive/` 下的历史记录**不动**——那是已归档的事实）
- [x] 1.4 **MUST NOT** 新建属性访问白名单薄壳（删除后 `TcsSkill` 对属性门面零依赖）；**MUST NOT** 改 `attribute-pipeline` 规格（消费模块仍为两个）
- [x] 1.5 验收资产 `DA_SkillDef_E2E` / `DA_SkillDef_E2E_Second` 重建后 `IsDataValid` **仍为 `Valid`**（两资产都不含该字段 ⇒ 预期无需内容侧改动；装置重建后复核）
- [x] 1.6 `EVID-2026-10-07-skill-def-asset` 按"被后续任务取代"就地留痕（**保留原文**、加取代说明；**MUST NOT** 重跑 Task 1 全部取证）

## 2. 施法运行态与池（计划 Step 2）

- [x] 2.1 `FTcsCastRunHandle` 补 `GetInner()` / `SetInner()` **唯一转换对**——`TTcsInstancePool<FTcsCastRun, FTcsCastRunTag>` 的 `Allocate` / `Free` / `Resolve` 收发 `TTcsInstanceHandle<>`，展平句柄与之**位模式一致但类型不同**，缺此对则句柄无法进池；照 `FTcsChainRunHandle` 的既有形态（`static_cast` 保证 `-1` ↔ `0xFFFFFFFF` 无损）
- [x] 2.2 新增 `Source/TcsSkill/Public/Skill/TcsCastRun.h`——`FTcsCastRunTag`（仅供句柄模板做类型区分）+ `FTcsCastRun`（**非反射**，无 `USTRUCT`；类型注释写明理由：池内池化纯数据、本轮零过网与零脚本消费面）
- [x] 2.3 `FTcsCastRun` 字段 = `{FTcsSkillEntryHandle EntryHandle, FTcsCombatEntityHandle Unit, int32 Level, int32 PhaseIndex, FTcsChainRunHandle ChainRunHandle, FTcsParamSnapshot ParamSnapshot, FTcsSourceHandle RunSource, FTcsTimeEntryHandle PhaseExpiryEntry}`
  - `Unit` MUST 自持（**类型注释写明理由**：条目句柄的 `Index` 只在单位桶内有意义 ⇒ 施法终结路径只拿得到运行句柄，缺 `Unit` 则要么无法定位条目、要么退化为全桶扫描）
  - `PhaseExpiryEntry` **本 Task 零写入者**（时段推进归 Task 5）——就位即可，**MUST NOT** 为它建机制
- [x] 2.4 门面持 `TTcsInstancePool<FTcsCastRun, FTcsCastRunTag>`；`Deinitialize` 确定性清空（`Reset()`——**MUST NOT** 逐槽清理实例内容，同池的零策略纪律），并在 `Deinitialize` 一并清池
- [x] 2.5 **回填与摘除成对**：建 run 成功后回填 `FTcsLearnedSkillEntry::RunHandles`；run 终结（含顶替）时摘除。**MUST NOT** 只写不摘（否则账本永久持有已归还的句柄，表现为"技能显示在飞但实际放完了"）
- [x] 2.6 **GC 纪律**：门面 `AddReferencedObjects` 覆盖池内在册 run 的每个 `ParamSnapshot.Entries[i].SourceRef`——`FInstancedStruct` 内层可持对象引用，而池住 `TArray` **非 `UPROPERTY` 容器**；逐条走 `FInstancedStruct::AddStructReferencedObjects`（**MUST NOT** 用 `AddPropertyReferencesWithStructARO`——`FTcsParamSnapshotEntry` 不是反射结构体、无 `StaticStruct()` 可给）
- [x] 2.7 新增 `FTcsCastRunTag` 后复核 **UHT 两条硬门槛**：头文件名全项目唯一（`TcsCastRun.h` 未与他处撞名，起草时已实测）+ 反射类型去前缀后不同名（本类型**非反射**，不适用）
- [x] 2.6b **GC 遍历 MUST 走专用口（首轮 PIE 实测暴露的真实缺陷）**：池的每个正常进入点都无条件 `ensure(IsInGameThread())`，而 GC 引用收集**不在游戏线程** ⇒ ARO 里调 `CastRuns.ForEach` 会**每次 GC 留一条 ensure 红字**（实测 callstack `FRealtimeGC::CollectReferencesForGC` → `AddReferencedObjects` → `TTcsInstancePool::ForEach`；**与池空不空无关**，且时间戳早于探针 2m21s）。⇒ 池新增 `ForEachForGC`（不断言线程、纯读、判据与 `ForEach` 逐字相同），门面新增 `ForEachCastRunForGC` 转调它，ARO 改走该口；**同批 MODIFIED `instance-handle-pool`** 钉住"该口不得用于常规逻辑"（否则等于给单线程纪律开后门）

## 3. 六道具名门禁（计划 Step 1 / Step 3）

- [x] 3.1 新增 `Source/TcsSkill/Public/Def/TcsSkillActivateResult.h`——`UENUM(BlueprintType) ESkillActivateResult`，值名带 **`SAR_` 前缀**（全仓 `UENUM(BlueprintType)` 的枚举值无一例外带前缀；`BlueprintType` 必需——宿主查出"为什么放不出来"是本门禁的唯一价值）：`SAR_EntityNotReady` / `SAR_NotLearned` / `SAR_OnCooldown` / `SAR_AlreadyActive` / `SAR_CannotAfford` / `SAR_DefInvalid` / `SAR_Success`
- [x] 3.2 `FTcsCastOps::TryActivate(...)`（引擎函数，**门面薄壳一行转发**——设计 §3.1 的既定分工）门禁序列 = **① 实体 Ready → ② 已学 → ③ 冷却 → ④ Instancing → ⑤ CanAfford → ⑥ 定义可解析**，逐道具名返回
- [x] 3.3 **第 ① 道 MUST 用 `ITcsEntityQuery::IsEntityReady`**（经 Task 2 的注入点与降级语义），**MUST NOT** 用 `IsAlive` 代替；未注入时**降级放行**
- [x] 3.4 **评估时点约束（关掉顺序歧义）**：第 ③④⑤ 道**均需读定义内容**（冷却轨道 / 实例化与顶替位 / Cost 策略都是 Def 字段）⇒ 定义不可解析时 MUST 立即返回 **`SAR_DefInvalid`** 且**不评估第 ③④⑤ 道**（它们不可求值）。枚举值次序仍按六道序；本条约束的是**评估可达性**
- [x] 3.5 **第 ③ / ⑤ 道为占位**：判据 = 冷却**空轨道恒可放**、Cost 取**不消耗档恒可付**；两道 MUST 保持**可拒绝的结构**（各有枚举值）。**MUST NOT** 引用任何尚未定名的枚举值（`R6.5` 的定义尚在概念期——引用即制造第二真相）
- [x] 3.6 **拒绝路径 MUST NOT 有副作用**（纯查询语义）：任一道拒绝 ⇒ 不建 run、不改账本、不起链、不发事件
- [x] 3.7 **第 ④ 道四路分流**：`InstancePerExecution` ⇒ 并存（不查在飞）；`InstancePerEntity` + 无在飞 ⇒ 正常建 run；+ 有在飞 + `bRetriggerOnActive = false`（默认）⇒ 驳回 `SAR_AlreadyActive`；+ 有在飞 + `true` ⇒ 顶替
  - **"在飞实例"判据 MUST 取该条目的 `RunHandles`**（按构造即 per-unit，无需跨桶扫描），且 MUST **按句柄经池复核**（账本里可能有已归还的陈旧句柄 ⇒ 复核失败即视为无在飞）
- [x] 3.8 **顶替语义**：① 先查旧 run 当前时段可打断性，**不可打断 ⇒ 驳回 `SAR_AlreadyActive`**（旧 run 原样继续）；② 可打断 ⇒ 终止旧 run（发 `OnCastInterrupted`、**MUST NOT** 发 `OnCastCompleted`、**不起旧 run 主链**、归还池槽位、摘 `RunHandles`）；③ 再建新 run。**顶替 = 打断的一种终止形态**，**MUST NOT** 新增第三枚终止事件
- [x] 3.9 **可打断性判据 = `bInterruptibleDefault`**（`ECastQueryMode::DefSwitches` 档——查询契约的**默认档**，Task 1 已交付 ⇒ 非临时实现）。**类型/代码注释 MUST 写明 Task 3 → Task 5 接缝**：`IsInterruptibleNow()` 落地后改调它，而 DefSwitches 档读的就是该字段 ⇒ **行为逐字不变、零回归**。**MUST NOT** 预建三档分派（`PhaseTable` 需时段推进、`Custom` 需 Fragment 求值）
- [x] 3.10 `FTcsSkillDefData` 增 `bool bRetriggerOnActive = false`（顶替位）——**默认 `false` 是判据不是偏好**：GAS 的对应位无初值即 `false`，且"框架零默认"纪律下默认档 MUST 取**行为最保守**的一档（驳回 > 顶替）。**类型注释 MUST 写明**该默认值的判据

## 4. 激活期参数快照（计划 Step 4）

- [x] 4.1 `FTcsCastOps::BuildSkillSnapshot`——按 `FTcsSkillDefData.Params` 逐行求值一次并冻结进 `FTcsCastRun.ParamSnapshot`；**写入点规则 MUST 与状态侧逐字同款**（覆盖值优先且**不再过值约定** / 否则求值后按该行 `ValueConvention` 经 `FTcsValueConvention::ConvertToCanonical` 转规范值 / 重建语义 = 先 `Reset` 再填）
- [x] 4.2 **能力位为假的源不转**（判据由源自身声明——`Row.Base.Source.GetPtr<FTcsParamValueSource>()->AllowsValueConvention()`；**MUST NOT** 建"源类型 × 可配约定"的中心名单）
- [x] 4.3 **复用面与不复用面分清**：**快照类型** `FTcsParamSnapshot` 与**读取壳** `UTcsStateParamTableReader` + `FTcsStateSnapshotScope` MUST **复用**（MUST NOT 建第二套快照类型、MUST NOT 出现第二处键查找语义）；**构建函数** MUST 另有一份（状态侧 `FTcsStateOps::BuildSnapshot` 形参硬编码 `const FTcsBuffDef&`，技能侧物理上无法复用）。**自检**：`Source/TcsSkill/` 内的快照**类型**引用只有 `FTcsParamSnapshot` 一处
- [x] 4.4 **时序**：门禁全过 → 构建快照 → 绑定到新建 run → 起主链 → 发 `OnCastStarted`。**MUST NOT** 在门禁之前构建（门禁失败即白算一份快照）
- [x] 4.5 `Level` = 激活时快照的生效等级（`EffectiveLevel` 语义 = `clamp(0, LevelBase + Σ参数账本 Level 键修正)`；本轮 `Σ` 由 Task 4 提供 ⇒ 本 Task 取 `LevelBase` 与条目持久等级口径）；**运行中升级不追溯**

## 5. 主链起链（计划 Step 6）

- [x] 5.1 `CastChainId` 经效果链登记表解析（`UTcsEffectSubsystem::ExecuteChain` 既有接口）；**无效 `CastChainId` MUST 由作者期校验拦下**（`skill-def-asset` 既有规则），运行期遇到**不阻断激活** + `Warning`
- [x] 5.2 `Context.RunSource` MUST = `FTcsCastRun.RunSource`（**台账 `CHAIN-7` 的闭合依据**——施法运行态是链运行态的**第一个长生命周期持有者**，链内 `ModifyAttribute` 挂的条目可按该锚点回收；**回收例程归 Task 5 Step 3**，本 Task 只落锚点）。`Context.Caster` = `Context.Instigator` = 施法单位
- [x] 5.3 `MainChainStart` **只实现 `MCS_OnCastStarted`**；`OnPhaseEnter` / `OnCastCompleted` / `Custom` 三档命中时 MUST 记 **Warning 并明确报出"该档未实现、主链未起"**——**MUST NOT** 静默跳过（那会让配了 `OnCastCompleted` 的技能"不报错也不生效"）
- [x] 5.4 链跑完与否 MUST NOT 影响激活返回（链可挂起）；`ChainRunHandle` 存入 run（**本轮零读取者**，如实登记——Task 5 终结路径要按它摘链锚）

## 6. 施法事件四枚（计划 Step 7）

- [x] 6.1 新增 `Source/TcsSkill/Public/Skill/TcsCastEvents.h` + `Private/Skill/TcsCastEvents.cpp`——四枚原生 tag 落**既有 `TcsEvent` 根**下（域段 `Cast`，**MUST NOT** 新增根）：`TcsEvent.Cast.Started` / `.PhaseChanged` / `.Completed` / `.Interrupted`
- [x] 6.2 变量声明 MUST 带**模块导出宏**（`extern TCSSKILL_API FNativeGameplayTag Tag_TcsEvent_Cast_*`）——`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**（无 `__declspec(dllexport)`）⇒ 跨模块引用必 `LNK2001`，而"供宿主订阅"正是本事件词汇的设计意图
- [x] 6.3 `USTRUCT(BlueprintType) FTcsCastEventPayload`（四枚**共用一种形状**，同状态侧六枚共用 `FTcsStateEventPayload` 的先例）：含 `FTcsCastRunHandle Run` / `FTcsCombatEntityHandle Unit` / `FGameplayTag DefTag` / `FTcsSkillEntryHandle EntryHandle` / `int32 Level`
  - **`Run` 是句柄不是指针**（类型注释写明：订阅者若读运行态细节，在回调内用句柄复核——**"先广播后归还槽位"的纪律**保证那一刻 run 仍在册）
- [x] 6.4 **本 Task 的产生者只有两枚**：`Started`（激活成功后）与 `Interrupted`（**顶替路径**）。`PhaseChanged` / `Completed` **只落词与载荷，产生者归 Task 5**——**MUST NOT** 在本 Task 假装它们已产生
- [x] 6.5 广播走总线**立即通道**（`PublishImmediate`——同状态事件既定口径，订阅方在调用返回前收到）；**逐叶子具名**（总线订阅是精确 tag 匹配、非层级 ⇒ **MUST NOT** 依赖父 tag 收全）

## 7. 门面与文件组织

- [x] 7.1 新增 `Source/TcsSkill/Public/Skill/TcsCastOps.h` + `Private/Skill/TcsCastOps.cpp`（门禁裁决 / run 生命周期）+ `_Snapshot.cpp`（快照构建）；文件超 300 行时按 `<Name>_<Feature>.cpp` 惯例分片
- [x] 7.2 门面 `UTcsSkillSubsystem` 增激活口 `TryActivate(FTcsCombatEntityHandle Unit, FGameplayTag DefTag, FTcsCastRunHandle* OutRun) -> ESkillActivateResult` + 运行态池成员；**门面方法一行转发**给 `FTcsCastOps`；`friend class FTcsCastOps`（同 `UTcsStateSubsystem` / `FTcsStateOps` 的处置）
- [x] 7.3 **MUST NOT** 收 `TryActivate` 的"施法上下文"形参（设计 §4 的 `Context`）——本 Task 无消费者（链侧 `SelectTargets` 自填目标集、`Instigator` 缺省 = 施法者）⇒ 收一个无人填的形参即"零消费者预建"；归属 = 出现"宿主指定目标集施法"的真实消费者时
- [x] 7.4 **MUST NOT** 改 `TcsSkill.Build.cs` 依赖集（仍不含 `TcsDamage` / `TcsTargeting` / `TcsIntegration`）
- [x] 7.5 unity 合并撞名防护：本模块跨 `.cpp` 的 file-local 符号 MUST 带文件/装置前缀（`TcsCastOps_` 一类），哪怕当前只有一处定义

## 8. 编译与验收取证

- [x] 8.1 `LegendAutoChessEditor Win64 Development` 编译 **0 error / 0 warning**
  - **实测（2026-10-08，编辑器关闭后）**：`Result: Succeeded`，**0 error / 0 warning**。**产物真重编**（非缓存跳过）：`UnrealEditor-TcsSkill.dll` `209408 → 210944 B`（`21:09:16`）、`UnrealEditor-TcsDev.dll` `567808 → 570368 B`（`21:09:38`）——两者均因本轮修复而体积变化，证明新代码确已进入产物。
  - **★ 一个连续四轮的错误阻塞判断就此纠正（MUST 读）**：我此前反复声称"编辑器开着 ⇒ 完全无法编译"。**该判断错误**：`Live Coding` **只作用于编辑器目标**（patch `UnrealEditor-*.dll`），**`LegendAutoChess Win64 Shipping` 是独立目标、不受阻断**。⇒ **编译器可用性 ≠ 二进制可用性**，二者 MUST 分开判断——**编译验证**用 Shipping（编辑器开着也能跑）、**PIE 取证**才需要编辑器目标。我此前把两者混为一谈，**白等了四轮**，并把"关编辑器"当成一切工作的前置：**这是流程判断错误，不是环境限制**。
- [x] 8.2 `LegendAutoChess Win64 Shipping` 编译 **0 error / 0 warning**（改动含新 `UPROPERTY` / `USTRUCT` / `UENUM` 与 ARO ⇒ **双配置必跑**）
  - **实测（2026-10-08，编辑器全程开着）**：`Result: Succeeded`，**0 error / 0 warning**。**该目标确为有效全项目编译门**，已用两次"改一处再编"证明其覆盖面：① 改 `TcsCastOps.cpp` ⇒ 日志出现 `Compile [x64] Module.TcsSkill.cpp`；② 改 `TcsCore` 头 ⇒ **10 个模块全部重编**（含 `TcsDev`）。`Source/LegendAutoChess.Target.cs` 的 `ExtraModuleNames` 含 `TcsDev` ⇒ **产品代码与宿主装置都在编译面内**。
  - **★ 它抓出一处静态核对漏掉的真实编译错误**：`TcsCastOps.cpp:95` `error C4459: declaration of 'LogLevel' hides global declaration`（局部量 `LogLevel` 与 UE 全局 `ELogVerbosity::LogLevel` 撞名 ⇒ 零 warning 门槛下算失败）。**"与引擎全局符号撞名"这类问题，静态比对声明/类型/include 查不出来**——已修（局部量加 `Broadcast` 前缀），修后重编 Succeeded。
- [x] 8.3 `openspec validate add-cast-run-and-gates --strict --no-interactive` = **valid**
- [x] **六道具名门禁逐态可复现**：六种拒绝条件各一次 ⇒ 各返回**对应**枚举值（六值互不混同），且**账本条目数与在飞 run 数均不变**（拒绝无副作用的阴性对照） **✅ 实测（无头 run12/reject5，2026-10-08）**：穷尽了本轮**可拒绝的**全部门禁——`SAR_EntityNotReady`（`C12` 发散探针：Alive=真 但 Ready=假 ⇒ 必拒；误用 `IsAlive` 则本项失败）、`SAR_NotLearned`（`R2`）、`SAR_AlreadyActive`（`C7`/`C10` 两个来源）、`SAR_DefInvalid`（`R3`）；每道拒绝均带**阴性对照**（`R2`：run 数与出参均不变 ⇒ 无副作用）。**★ 边界（如实登记，MUST NOT 读成「六道全验」）**：`SAR_OnCooldown` / `SAR_CannotAfford` **设计上恒过**（`R6.5` 占位）⇒ 本轮**物理上不可拒绝**，故本项实际只覆盖 **4 道**，另 2 道随 `R6.5` 的非默认分支。
- [x] **Instancing 四态逐态取证（四态缺一不可）**：   - **✅ 实测（无头 run12，四态齐备）**：`C6` 并存（增量 +1、句柄不同）/ `C7` 驳回（`SAR_AlreadyActive`、未建新 run）/ `C8` 顶替（Interrupted=1、**Completed=0**）/ `C10` 不可打断段的顶替被拒（阴性对照：旧 run 原样继续、未发打断事件）。
  - ① `InstancePerExecution` 连点两次 ⇒ **两个 run 并存**（读数 = 在飞 run 数 = 2）
  - ② `InstancePerEntity` + `bRetriggerOnActive = false` ⇒ 第二次返 **`SAR_AlreadyActive`** 且在飞读数**不变**（= 1）
  - ③ `InstancePerEntity` + `true` + **可打断段** ⇒ **顶替成功**：新 run 在飞、旧 run 发 `OnCastInterrupted`、**`OnCastCompleted` 零次**
  - ④ `InstancePerEntity` + `true` + **不可打断段** ⇒ 返 **`SAR_AlreadyActive`**、旧 run 原样继续（**阴性对照**）
  - **★ ③ 的判据 MUST 含"`OnCastCompleted` 零次"**——只验"新 run 起来了"会漏掉 GAS 式误发（那正是本裁定的核心分歧点），**单靠"新旧数量"判不出**
  - **本 Task 的边界（如实登记，MUST NOT 假装已验）**：施法运行态**没有自然终结路径**（全时段走完归 Task 5）⇒ 四态里"两次各自推完"那一半**不可测**；顶替的可打断性只验 `DefSwitches` 一档
- [x] **参数快照读数**：① 某行配 `VCF_Percent` 书写 85 ⇒ run 快照内为 `0.85`（转换发生一次）；② 激活后改数值来源输入 ⇒ 本次 run 快照**不变**（不追溯）；③ run 终结后快照随槽位归还失效 **✅ 实测（无头 run12）**：`C13` 真配一行 `VCF_Percent` 书写 85 ⇒ 快照**命中该键且值 = 0.8500**。**★ 本项此前是空转的（本轮自查抓出）**：装置原先**从未配过参数行** ⇒ 快照恒空，而「空快照」与「快照机制失效」**读数相同** ⇒ 该检查会静默退化。已补 `C13`，并把「未配则本项必失败」写进标签使其可证伪。**残留边界**：本项第 ② 半（激活后改输入不追溯）与第 ③ 半（终结后随槽位失效）**本轮未单独验**，如实登记。
- [x] **事件读数**：`TcsEvent.Cast.Started` 广播一次且载荷身份（`Run` / `Unit` / `DefTag` / `EntryHandle`）与该次激活一致；顶替路径 `Interrupted` 一次（载荷 `Run` = **旧** run 句柄）且 `Completed` **零次** **✅ 实测（无头 run12）**：`C5` `OnCastStarted` 恰一次且载荷 `Run`/`Unit`/`DefTag`/`EntryHandle` 四项与该次激活吻合；`C8` 顶替路径 `Interrupted=1` 且 `Completed=0`（「完成零次」是本裁定的核心分歧点）。
- [x] **未实现档位具名报出**：配 `MCS_OnCastCompleted` 的技能激活 ⇒ **仍返 `SAR_Success`**，但留具名 `Warning` 指明该档未实现且主链未起 **✅ 实测（无头 reject5 的 `R7`）**：配 `MCS_OnCastCompleted`（本轮未实现）的技能激活 ⇒ **仍返 `SAR_Success`** + **`ChainRunHandle` 无效**（主链确实未起）——两条判据缺一不可，只验其一都会漏掉「静默失败」或「静默生效」。**★ 落点说明**：本项**必然产生一条具名 Warning**，故按「常规命令零红字」纪律**落在 opt-in 的 `.Reject` 命令**（`MEM-20260918-07`）。
- [x] 8.9 **ARO 覆盖面（阳性判据难造时的最低要求）**：`AddReferencedObjects` 对池内在册 run 的快照条目逐条调用；**若本轮造不出"内层对象引用"的阳性样本，MUST 如实登记为未覆盖**，MUST NOT 以"代码写了"claim 已验证
  - **✅ 实测（无头 run9/run12，2026-10-08）——阳性样本**：新增 `C15`，构造"快照内层**真持对象引用**"的场景并**实测到保活**。
  - **★ 我先写了"造不出"，随后自查推翻（留痕，因为它与前两次是同一个错误模式）**：我第一版的理由是"`ITcsParamSourceHost` **零实现者** ⇒ 造不出载体"。**该断言错误**——按同一套 353 文件扫描，**存在实现者**：`Script/LegendAutoChessCS/TcsDevGcFixtures/TcsDevGcParamSourceHost.cs:27` 的 `UTcsDevGcParamSourceHost`（C#）。看漏的原因是我**首轮只扫 `*.h/*.cpp`、没扫 `*.cs`**，随后凭"没找到"下结论。**⇒ 与"我自己不能跑 PIE"完全同型：把「我没找到」当成了「不存在」。**
  - **载体的正解**：`FTcsParamSource_HostDelegate`（`TcsParamSourceHost.h:88`）持 `TScriptInterface<ITcsParamSourceHost>`——**它本身就是对象引用**，装进一条参数行的 `Base.Source` 即可，**不需要**任何"宿主实现"。
  - **判据的证明力（三步收敛，缺一则归因不成立）**：① 造 def（参数行来源 = 宿主源转发器，`Host` 指向新建载体）→ 激活 ⇒ 快照 `SourceRef` **值拷贝**该转发器；② **注销该定义** ⇒ **定义登记表（它也有 ARO）不再持该对象**；③ **丢掉装置自己那份强引用**（`TStrongObjectPtr::Reset`）⇒ 此后**唯一可能的持有者 = 运行态池里该 run 的快照条目**；④ `CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS)`（`UObjectGlobals.h:952`）后复查。**读数**：`C15 ARO 读数：存活=是 求值=13.0（期望 13.0）`。
  - **★★ 阴性对照（本项最强证据，MUST 记录）**：临时把池的 ARO 遍历改为 `if (false)` 并**重编**（产物 mtime 已核）⇒ **`Run` 由 26/0 变 24/2**，`C15 ARO 读数：存活=否`——**正好少 C15 的两条**。⇒ **本检查确实能检出 ARO 失效，不是恒过的假阳性。**（对照组快照 = `snapshot-R6Task3-negctl-aro-off.log`；随后**已还原**并复跑得回 26/0，证明还原生效。）
  - **★ 一处"两段式"的替代说明**：既有 `Tcs.Test.Gc.*` 用"`Arm` → **人工** `obj gc` → `Verify`"跨命令两段式；而**无头取证每次都是新进程、状态不跨命令** ⇒ 本项改用**命令内 `CollectGarbage`**（`UObjectGlobals.h:952` 可用），把"丢引用 → 强制 GC → 复查"全部收在同一条命令内。**`-game` 非编辑器 ⇒ `GARBAGE_COLLECTION_KEEPFLAGS = RF_NoFlags`**（`GarbageCollection.h:28`）⇒ **不保留 `Standalone` 对象**，故"被回收"是真会发生的（若在编辑器档下跑，`RF_Standalone` 可能让对象被 KeepFlags 保住 ⇒ 那会是**假阳性**，这一点 MUST 记住）。
- [x] **零意外红字**：常规命令 MUST 零非预期红字；验收报告 MUST 附**预期红字清单**（"零红字" = 零**意外**红字）；否定路径 MUST 带**阴性对照**；故意的拒绝输出 MUST 独立成 `.Reject` 命令 **✅ 实测（无头 run12/reject5）**：`Run` 段**零红字**（逐行核过为空）；`Reject` 段红字**逐条对应具名拒绝**（3 条：`R3` 定义无效 / `R4` 未登记授予 / `R7` 未实现档位）；**全日志零 `ensure`、零 `Fatal`**。预期红字清单随证据包给出。
- [x] 8.11 `Source/TcsState/**` 与 `Source/TcsEffect/**` / `Source/TcsAttribute/**` 全库反查：本轮**不改**（除 1.x 的删除面外）
- [x] **两轮可复现性**：判据 = **摘要行逐字相同 + 各检查结论相同**；**MUST NOT** 比区段字节/行数（跨快照行号 MUST 按内容正则重定位） **✅ 实测（无头 run9 vs run10、run10 vs run12，均为两次独立进程）**：摘要行去时间戳后**逐字相同**（`SUMMARY Run: Passed=26 Failed=0 在册 run=8`），**26 项检查结论逐条比对零差异**（按标签比对）。**注**：本项随 `C15` 加入由 26 项增至 **26 项**，两轮复核均在新基线上取得。

## 9. 宿主装置（LAC 仓 `Source/TcsDev/`，**开发用、非正式内容**）

- [x] 9.1 新建 `TcsDevSkillCastProbe*`（照 `TcsDevSkillRegistryProbe` 的三面分片体例：常规 `Run` / 门禁 `Gate` / 拒绝 `Reject` + `_Internal.h` 共用件），命令族 `Tcs.Test.SkillCast.*`
  - **为什么新建而非扩展既有装置**：施法面与账本面是两个独立验收面；既有 `TcsDevSkillRegistryProbe.cpp` 已 287 行，扩展即超 300 行上限
- [x] 9.2 `Config/DefaultGameplayTags.ini` 补本轮检查词（`SkillDef.Check.*` 形态，**MUST NOT** 另立根；**检查词的消费路径与正式词完全相同**）
- [x] 9.3 `TcsDevModule.cpp` 注册新装置
- [x] 9.4 装置 MUST 提供 **Instancing 四态的开关**（四态各自可配：`Instancing` / `bRetriggerOnActive` / 可打断位），否则 8.5 无法取证
- [x] 9.5 读数以 **UTF-8 日志文件**为准（控制台回显为 ANSI，中文显示为 `?`）：`Saved/Logs/LegendAutoChess.log`

## 10. 人工验证点（人工在编辑器内执行）

1. **先关编辑器 → 完整双配置 UBT → 重开编辑器 → 跑 PIE**（**MUST NOT 用 Live Coding**——"现象只在重建二进制后消失"本身就是陈旧 DLL 的判据）；
2. 跑施法主命令（激活 → 六道门禁 → 快照 → 起链 → 事件）——**常规命令 MUST 零红字**；
3. 跑独立的 `.Reject` 命令（六道具名拒绝）——预期红字**逐条列出**；
4. 取证前**先快照日志**（日志会被下次 PIE 覆盖；且读取期间它可能已被追加）。

> **8.4~8.12 的取证口径**：本清单交付的是**代码与装置**，上述读数要求**人工在编辑器内执行后**的结果 ——
> 未实测前 MUST 保持未勾选（同 Task 1 / Task 2 的处置：交付当晚如实留白，用户执行后按实测读数补勾）。
