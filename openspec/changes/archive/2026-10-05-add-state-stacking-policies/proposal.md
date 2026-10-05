# Change: 五轴堆叠与刷新政策（D3-4 / D3-17 落地）

## Why

R5 状态层至今的"重复施加"是一条**暂用判据**：同单位 + 同 `DefTag` 即刷新（`TcsStateOps.cpp` 的替换点已就地标注释）。`FStateStackPolicy` 的五个轴（Task 1 落的形状）**零行为**，`EApplyResult::EAR_Stacked` 不可达，`TcsEvent.State.StackChanged` 只有声明、零广播——"同一 buff 重复施加"这件玩法上的头等大事今天没有确定语义。

本变更把五轴落成确定性决策：分组、层数上限、溢出处置、数值叠加、时长刷新各自可选，且**四个 `EApplyResult` 档全部可达**。

## What Changes

- **五轴形状补全**：`FStateStackPolicy` 补 `FInstancedStruct CustomDecision`（Custom 决策载荷）；`EOverflowPolicy` 由三档**裁为两档**（`RejectNew` / `ReplaceExisting`）；`EGroupByPolicy` 的"按分组词"档与随之而来的 `GroupTag` 字段**裁撤**（理由见裁定 9）。
- **组键解析**：组键 = `(单位, DefTag)` 再按 `GroupBy` 附一项（来源句柄 / 发起者实体 / 无）；`EGB_Custom` 交给决策 Fragment 判定。**本模型下"组内至多一条实例"**（替换路径先移除旧条目再建新），且**不同 `DefTag` 永不共组**。
- **共存决策树 + 四档回执**：无同组实例 ⇒ `Applied`；**同来源**未满仓 ⇒ `Refreshed`（层数不变）；**异来源**未满仓 ⇒ `Stacked`（层数 +1）；满仓 + `RejectNew` ⇒ `Rejected`（无广播）；满仓 + `ReplaceExisting` ⇒ 移除旧 + 建新 ⇒ `Applied`。**调用方未声明来源时视为同来源**（否则"不关心来源"的调用方会无限叠层）。
- **叠层与刷新流水**：层数 → 数值（`KeepMax` 不随层 / `AddValues` 值随层成倍）→ 快照重建（D3-12）→ 修正器按来源重挂（复用 D3-19 刷新挂点、条数不累加）→ 时长（`ESD_None` 保留 / `ESD_RefreshRemainingToTotal` 回满额；周期走既有 `EPR_*`）→ 广播。
- **Custom 决策 Fragment**：新增 `FTcsStateStackDecisionFragment`（`USTRUCT(meta = (Hidden))`，三个决策方法全是中性默认实现）；载体 = **裸 `FInstancedStruct`** + 手写 `BaseStruct` 元数据；框架**零具体策略**（宿主样本住 LAC `TcsDev`）。
- **事件面首次有产生者**：`StackChanged` 在叠层路径上广播（**先 `StackChanged` 后 `Refreshed`**），且**只在层数真的变化时**广播。

## 待评审的裁定（本提案的核心，逐条给判据）

1. **【用户已拍板 2026-10-05】层 = 单实例内的计数**（甲案）。代价如实登记（实施后共**三条**）：legacy `NoMerge`（规格 §3.2 的 `None + ∞`）只能**近似**——得到的是"一条实例、层数累加、默认轴下数值与时长都不变"，而 legacy 是"每次施加各自独立成实例、各自计时"；替换轴两档在"一组一条实例"下**完全同义**；"按分组词分组"档**无法表达**（见裁定 9）。
2. **决策树的四档判据**：`Refreshed` = 同组**同来源**（续杯，层数不变）、`Stacked` = 同组**异来源**加入（层数 +1）。判据 = **只有这一读法让计划 Step 2 的"未满仓则 `Stacks++`"与 Step 3 的"同组**同来源**的重新施加 = `Refresh`"同时成立**，且四档回执全部可达（否则 `Refreshed` 或 `Stacked` 必有一档成为死档）。
3. **未声明来源 = 通配（视为同来源）**：来源句柄在门面处按无效值发号（每次一枚新号），若"未声明"被判成"换了施加方"，任何不关心来源的调用方在默认策略（层数无限）下**每次施加都叠一层**——一个只在长跑里才显形的错误。⇒ 副作用**好消息**：既有装置检查 19c / 21e（"重复施加 ⇒ `Refreshed`"）**无需改动**即可继续成立（它们本就不传来源）。
4. **`EOverflowPolicy` 裁为两档**：`ReplaceOldest` / `ReplaceNewest` 在"一组一条实例"下是同义动作 ⇒ 保留即死档。裁撤在**零消费者**时进行（无资产配置过它 ⇒ 无迁移面）。**BREAKING**（枚举值变更）。
5. **`StackChanged` 只在层数变化时广播**：计划 Step 2 的括号注把替换路径也写成广播 `StackChanged`，与事件词的既定语义（"层数变化"）冲突 ⇒ 以事件词语义为准；替换的可观测面 = `Removed` + `Applied` 两笔。
6. **`EValueStackPolicy::EVS_PerStackValue` 本轮只落形状**（枚举值 + 注释 + 台账 `STAT-6`，零行为）。理由：它的合规实现要么引入"层数读口 + 按层参数源"这套新机制（内容侧无消费者），要么会退化成规格 §3.5 明文撤回的 `FollowStacks`（"增长策略零内置"）；且计划 Step 5 的验收清单不覆盖它。`KeepMax` / `AddValues` 两档落真实行为。
7. **Custom Fragment 的载体与形状**：计划 Step 4 写的 `TInstancedStruct<FTcsStateStackDecisionFragment>` **订正为裸 `FInstancedStruct`**——全仓 2026-09-24 已换型（`switch-strategy-carrier-to-plain-instanced-struct`：`TInstancedStruct<T>` 字段在宿主脚本层导出为空壳，脚本配不了值），先例 = `FTcsParamValue.Source` / `FTcsSelector` / `TcsTargetSortItem.Scorer`；类型收窄改由**手写 `meta = (BaseStruct = ...)`** 提供（先例 = `FTcsTargetSelectorStrategy`）。决策方法三个：`IsSameGroup` / `ShouldAccept` / `ResolveStacks`。载荷缺失/类型不符 ⇒ 退化为内置"不分组"语义 + `Warning`（配置错误语义；该面住拒绝命令）。
8. **满仓拒绝是业务结果不是错误**：用 `Log` 记录（常规验收命令保持零非预期红字）。
9. **【用户已拍板 2026-10-05】`EGB_PerTag` 与 `GroupTag` 裁撤**（实施期新发现，第三条甲案代价）：组键基座**必须含 `DefTag`**（一条实例 = 一个定义的数据：快照、修正器、事件载荷全按该定义来），而分组词取自定义自身 ⇒ 同一 `DefTag` 内恒定 ⇒ 该档与 `None` **完全同义**；它唯一有意义的读法是"不同定义共享一个组（层数上限跨定义累计）"，那要求**组内允许多条实例**（"层 = 独立实例"的另一套模型）。⇒ 裁撤该档与字段，把"跨定义共享层数组"登记台账 `STAT-7`。
10. **四处文档订正**（随本变更落纸）：① 规格 §3.2 "每轴枚举值 0=默认/None、**值 1=Custom**" ⇒ "值 0 = 默认；**Custom 走高位置位 `1 << 4`**"（代码与计划为准；决策日志 D3-4 同句一并订正）；② 规格 §3.2 等价表 `NoMerge = None + ∞` ⇒ 加注"**近似**"；③ 规格 §3.2 `Overflow` 三档 ⇒ 两档；④ 规格 §3.2 `GroupBy` 四态 ⇒ **三态 + Custom**（`PerTag` 裁撤及其理由）。

## Impact

- **Affected specs**：
  - `state-stacking-policies`（**ADDED**，新能力：五轴形状与默认 / 组键解析 / 共存决策与四档回执 / 叠层与刷新的数值时长语义 / Custom 决策 Fragment / 层数与刷新的事件契约）
  - `state-instance-lifecycle`（**MODIFIED** 2 条：「状态门面」的返回值语义（`Stacked` 由"归 Task 5"改为四档可达）+「广播点与阶段迁移」（暂用位替换为策略驱动；"重复施加 ⇒ `Refreshed`"的场景补"同一来源"限定））
- **Affected code**（插件仓 TCS）：
  - `Source/TcsState/Public/State/TcsStateStackPolicy.h`（改：`CustomDecision` + 裁两处档 + 注释口径）
  - `Source/TcsState/Public/State/TcsStateStackFragment.h` + `Private/State/TcsStateStackFragment.cpp`（新：决策词汇（请求 / 决策）与 Fragment 基类的中性默认）
  - `Source/TcsState/Private/State/TcsStateOps_Stack.cpp`（新：组键解析 + 决策树 + 刷新/叠层流水）
  - `Source/TcsState/Public/State/TcsStateOps.h`（改：新声明区）/ `Private/State/TcsStateOps.cpp`（改：替换暂用判据）
  - `Source/TcsState/Private/State/TcsStateOps_Lifetime.cpp`（改：`ScheduleTime` 按 `StackDurationPolicy` 决定刷新时保留还是回满额——原实现**无条件回满额**）
  - `Source/TcsState/Private/State/TcsStateModifierMaterializer.cpp`（改：`AddValues` 在物化边界按层数成倍）
  - `Source/TcsState/Public/State/TcsStateEnums.h`（改：`EAR_*` 四档 ToolTip 与可达性口径）
- **Affected code**（宿主仓 LAC `Source/TcsDev/`，**同一验收装置**）：
  - `TcsDevSliceRig.cpp`：新增检查 **22a–22m**（**六组**：不分组 / 满仓替换 / 满仓拒绝 / 按发起者+累加档 / 按来源 / 时长两档 + Custom 宿主样本）；拒绝面新增检查 **K**（自定义载荷缺失退化——它必然产生 Warning，故只能在拒绝命令）
  - `TcsDevStackDecisionSample.h/.cpp`（新）：宿主侧决策样本（恒不同组 ⇒ 每次新建）——"框架零具体策略"的现场证明
  - `TcsDevScreenObserver.h/.cpp` + `TcsDevBootstrap.cpp`：状态事件槽位 5 → 6（纳入 `TcsEvent.State.StackChanged`）并加两条定序读数（`LastRefreshedSeq` / `LastStackChangedSeq`）
  - `Config/DefaultGameplayTags.ini`：新增两枚定义身份探针词（`StateDef.Probe.Stack` / `StateDef.Probe.Stack.Alt`，沿用既有 `StateDef.Probe.*` 功能根）
- **资产迁移**：无。裁档与补字段都不影响既有定义资产（`DA_Check_BuffDef` 的 `StackPolicy` 是全默认值，语义仍有效）。

## 非目标

- `EVS_PerStackValue` 的行为面（台账 `STAT-6`）。
- **"跨定义共享层数组"**（`PerTag` 的合规语义，台账 `STAT-7`）——需"组内多条实例"的层模型。
- **逐层来源归属**（层数来自哪个来源、按来源只摘某一层——台账 `STAT-8`）：叠层路径只保留实例的**首个来源**为级联锚点（Task 4 的"`Source` 刻意不动"纪律），新来源只参与"续杯 / 叠层"判定。
- 状态级重定向（R5.5-d）与"溢出升级替换"的三步组合用法（规格 §3.2 明文不内建）。
- 关系表检查器（R5.5-e）；技能参数行分派（R6）。
- legacy `NoMerge` 的"每次施加独立成实例"语义（甲案下不可表达；**宿主要它就用 Custom 决策 Fragment 表达**——本轮装置样本正是这个用法）。
