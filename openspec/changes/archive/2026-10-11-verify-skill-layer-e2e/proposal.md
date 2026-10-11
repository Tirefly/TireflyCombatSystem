# Change: verify-skill-layer-e2e — R6 技能层端到端竖切验证（含 `CHAIN-7` 三路闭合读数）

> 对应计划 `PLN-R6` Task 6（`Documents/combat-system-design/plans/plan-r6-skill-layer.md`，Step 0~6）。
> 依赖：Task 0~5（提案 `add-trigger-host-slots` / `add-tcs-skill-module` / `add-skill-registry` /
> `add-cast-run-and-gates` / `add-skill-param-chain` / `add-cast-phases-and-interrupt`）。
>
> **★ 本变更只 ADDED 一个新能力，零 MODIFIED、零 REMOVED。** 判据见 `## Why` 第 4 节：
> `add-cast-phases-and-interrupt` **仍未归档**，它已持有 `skill-cast-phases` / `skill-cast-runtime`
> 两份 delta，而本变更验收的判据**正是那两份 delta 里写下的** ⇒ 再发 delta 会在归档时撞车。

## Why

### 1. 六个技能层能力各有证据，但没有一条证据穿过它们的耦合点

Task 1~5 各自都有一份归档证据（`EVID-2026-10-07-skill-def-asset` /
`EVID-2026-10-08-skill-registry` / `EVID-2026-10-08-skill-cast-runtime` /
`EVID-2026-10-09-skill-param-chain` / `EVID-2026-10-10-skill-cast-phases`）。但这五份证据里的**每一个读数
都是装置在运行期手搓定义、自持空链**跑出来的——`TcsDevSkillCastProbe.cpp` 的 `Phases` 命中数为 **0**，
链是 `EffectChain.Check.CastProbe`（**零步骤**）。⇒"**一条真内容技能资产走完全程**"这件事，
R6 至今**一次都没发生过**。

计划 `PLN-R6` 的 Q-7 剧本把它写死为 Task 6 的目标：
`GrantSkill` → `TryActivate` 逐道门禁 → 参数链折叠生效 → 起 `CastChainId` 链 → 真改属性 →
`OnCast*` 事件链可见 → `RevokeSkill` 级联摘净。

### 2. `CHAIN-7` 的三路闭合读数本轮唯一可取的窗口（8.7 项）

台账 `CHAIN-7`（"链挂的账本条目没有框架侧回收触发点"）已于 2026-10-07 标"✅ 已闭合"，
闭合机制落 Task 5 的统一终结例程；但 Task 5 §8.7 **未取到闭合读数**，理由如实：
闭合读数要求"主链**真的挂了 `ModifyAttribute` 并改了属性**"，而装置自持链是空链、
内容侧的链在 `-game` 无头下**发现 0 条**。用户 2026-10-10 裁定该项**顺延 Task 6**，
判据原样搬迁、**未放宽**（完成 / 打断 / 顶替三路各一读数，属性现值 + 条目数**成对**，
对照组 = R5 实测的"常驻停在 X−5"）。

⇒ **本变更是 `8.7` 的兑现轮**，也是 Task 5 那条"结构已落地、闭合读数归 Task 6"边界的关账轮。

### 3. 一条判据级纪律需要从注释升格为契约（用户 2026-10-10 裁定）

Task 5 的时段面探针**两次**在"等待"上踩坑，第二次是用户编辑器内 PIE 实测到的
`SUMMARY PhaseEnterDeferred: Passed=-1514964509 Failed=537`（**负的通过数** = 释放后使用）。
根因是同一个：**"等待"被写成了"在回调里干活"**。用户裁定该问题的骨架落 `TcsDev` 私有面，
并裁定该纪律**写进本能力的规格**（不当注释里的口头约定）。

### 4. 为什么零 MODIFIED（不是遗漏，是判据）

`add-cast-phases-and-interrupt` **目前仍是活跃提案**（`openspec list` = `70/71 tasks`），
它持有 `skill-cast-phases`（ADDED）与 `skill-cast-runtime`（MODIFIED ×2）两份 delta；
而本变更要取的三路闭合读数与跨帧时轴纪律，**判据文字早已写在 `skill-cast-runtime` 的
`Scenario: 瞬发完成时属性按来源回退（CHAIN-7 闭合读数）` 与
`Scenario: 打断路径与顶替路径同样回退` 两处**。⇒ 本变更对它们**只产出证据、不动规格**；
若此时再发一份 delta，归档时会与那份待归档的 delta 争同一需求体。

## What Changes

### 1. ADDED 新能力 `skill-layer-e2e-validation`（5 条需求）

详见 `specs/skill-layer-e2e-validation/spec.md`：

1. **技能层端到端竖切矩阵**——`Tcs.Test.SkillLayer.Run`，由**真内容资产**驱动，零非预期红字；
2. **技能层拒绝面独立取证**——`Tcs.Test.SkillLayer.Reject`，opt-in、集中承载全部预期红字；
3. **`CHAIN-7` 三路回收读数**——完成 / 打断 / 顶替各一读数，**属性现值 + 条目数成对**，
   并自带一条**阴性对照**（非施法起链的条目仍常驻）；
4. **跨帧判据的时轴与双路线取证纪律**——MUST 挂战斗时钟时轴、MUST 轮询被测事件本身、
   超时 MUST 报 FAIL；取证 MUST **两条路线都跑**；
5. **双面读数与边界登记纪律**——属性面 / 事件面各自成证，边界逐条标"实测 / 未覆盖"。

### 2. 宿主侧交付物（**插件仓 `Source/` 零改动**）

- **新装置**：`Source/TcsDev/Private/Dev/TcsDevSliceRig_Skill.cpp` + `Public/Dev/TcsDevSliceRig_Skill.h`
  （照 `TcsDevSliceRig_State.cpp` **803 行**的体例同构；**不扩既有 856 行的
  `TcsDevSkillCastProbe_Phase.cpp`**——见 `design.md` D2）；
- **Step 0 其一（跨帧时钟骨架）**：`TcsDev` 私有的"按战斗时钟时轴轮询条件成立"共用骨架
  （形如 `Source/TcsDev/Public/Dev/TcsDevCombatClockWait.h`）——
  **MUST NOT** 本轮提升为 `UTcsClockSubsystem` 的公共口（判据 = 零消费者不预建）；
- **新内容资产**（住 `Content/TcsDev/E2E/`，**不动既有 5 份**）：
  单段可打断技能定义 + 顶替档技能定义 + 施法面主链（真挂 `ModifyAttribute`）；
- **新检查词**：`Config/DefaultGameplayTags.ini` 追加以 `SkillDef.Check.*` /
  `EffectChain.Check.*` / `TcsStateParam.Check.*` 为根的行 ⇒ **不新开根**
  （`gameplay-tag-governance` 根表 **12 不动**——插件侧根表口径；宿主 `Config/DefaultGameplayTags.ini`
  现有 **11** 个根，本变更新增 5 词全落既有根下 ⇒ 宿主侧也仍是 11。两个数口径不同，MUST 分清）；
- **新证据档**：`Documents/combat-system-design/evidence/2026-10-XX-skill-layer-e2e.md`。

### 3. 零 MODIFIED 的连带代价（如实登记，不藏）

本变更**不修** `add-cast-phases-and-interrupt` 里那两处 `skill-cast-runtime` 场景的措辞，
也**不修**该提案 `tasks.md` 里 `8.7` 的 `[ ]`——那两项归 **Task 7 归档收束**时统一处置。
⇒ 本变更交付后，`openspec list` 仍会显示 `add-cast-phases-and-interrupt` 有 1 项未勾
（`8.7` 的读数已由本变更取得，其勾选动作仍是 Task 7 的事）。

## 明确不做（非目标）

- **不做冷却与 Cost**（`R6.5-a` / `R6.5-d`）——门禁第 ③⑤ 道保持占位判据不变；
- **不做 `CQM_Custom` 查询档与 `MCS_Custom` 起链档的求值**（`R6.5-g`）——保持具名未实现；
- **不做 `PrimaryAssetTypesToScan` 注册与 AssetManager 切换**（台账 `INTEG-3`，归 R7/M6）
  ⇒ 无头路线 `AssetRegistry` 发现 0 条这一限制**本轮不修**，只如实登记；
- **不做网络姿态、不做动画/表现耦合、不做 Mass 路径**；
- **不把跨帧时钟骨架提升进插件公共面**（零消费者不预建；若 R6.5 / R7 反复需要，届时以
  MODIFIED 增补 `TcsCore` 的时钟需求）；
- **不修子链残留边界**（`CHAIN-7` 已如实登记的"子链挂的条目归子链生命周期、不递归下探"）。

## Impact

### Affected specs

- **`skill-layer-e2e-validation`（ADDED）**——新能力，5 条需求。
- **零 MODIFIED / 零 REMOVED**（判据见 `## Why` 第 4 节）。
- **能力数 38 → 39**（`openspec list --specs` 实测对账：本变更新增 1 个能力；
  **注意** `openspec validate --all` 的条目数 = 能力数 + 活跃提案数，本变更落纸后为 **40 = 38 + 2**，
  归档两个提案后才收敛为 `40 = 40 + 0` ⇒ **两个数字含义不同，MUST NOT 互相搬用**）。

### Affected code

**插件仓（TCS）——`Source/` 零改动**。**可核对判据（2026-10-10 实测订正）**：
**不是**"`git diff` 为空"——基线本来就不空（Task 5 遗留 **14 项**：`Source/TcsSkill/` 下 7 改 + 7 新增，
仍在工作区、未提交）。正确判据 = 收尾时
`git -C Plugins/Tirefly/TireflyCombatSystem status --porcelain -- Source/` 与
`Saved/TcsAcceptance/Task6/` 下的基线清单**逐行相同**（零新增 / 零删除），
且 `git diff -- Source/` 对这 14 项为空（未二次改动）。
插件仓的唯一改动面 = 本提案自身（`openspec/changes/verify-skill-layer-e2e/`）。

**宿主仓（LAC）**：

- 新 `Source/TcsDev/Public/Dev/TcsDevSliceRig_Skill.h`、`Private/Dev/TcsDevSliceRig_Skill.cpp`；
- 新 `Source/TcsDev/Public/Dev/TcsDevCombatClockWait.h`（Step 0 其一的骨架）；
- 改 `Source/TcsDev/TcsDevModule.cpp`（多一行命令注册）；
- 新 `Content/TcsDev/E2E/` 三份资产（技能定义 ×2 + 施法面主链）；
- 改 `Config/DefaultGameplayTags.ini`（追加检查词，**不新开根**）；
- `Source/TcsDev/Private/Dev/TcsDevSkillCastProbe_Phase.cpp` **MUST NOT 在本轮因体例原因被拆分**
  （其 856 行超限债的触发条件 = "下一次需要改动本文件**任何判据**时"；
  本变更不取该文件的读数，故触发条件**不该响**——若因其它原因必须改它，则先拆再改，
  并把该次拆分计入本变更的回归面）。

**文档（TCS 仓 `Documents/`）**：新证据档 + `INDEX.md` / `ledger/deferred-inputs-ledger.md` /
`log/implementation-log.md` / `log/decisions-log.md` 回写。

### 台账预期

- **`CHAIN-7` 行补记"三路闭合读数已取"**（该行状态已是"✅ 已闭合"，本变更**不改状态**，
  只补落地读数的可核指针）——同 Task 5 那一笔的处置口径；
- 本变更**零新增台账条目**（Step 0 两项均有 Task 归属 ⇒ 按《入册判据》第 3 条不另立条目）；
- 若实施期暴露新的未落面，当轮按入册判据登记并在证据档逐条留痕。

### 回归面（MUST NOT 声称"零回归"）

| 面 | 预期 | 处置 |
|---|---|---|
| `Tcs.Test.SkillCast.Run` / `.Reject` / `.Phase` | Task 5 基线 **26/0**、**16/0**、**29+6+5 PASS / 0 FAIL** | 逐字复核（插件 `Source/` 零改动 ⇒ 理论不变，仍需实测确认） |
| `Tcs.Test.SkillParamChain.Run` / `.Reject` | Task 4 基线 **26/0** / **9/0** | 同上 |
| `Tcs.Test.SkillDef.*` / `Tcs.Test.SkillRegistry.*` | Task 1/2 基线 | 同上 |
| `Tcs.Test.State.Run` / `.Reject` | **15/0** / **7/0** | 同上 |
| `Tcs.Test.Slice.Run` / `.Reject` | 即时 **65/0** + 延迟 **72/0** / **11/0** | 同上 |
| 定义库就绪行 | 编辑器 PIE 下 `技能定义 2 条` ⇒ 新增资产后**分母移动** | **预声明**：编辑器 PIE 应读到 `技能定义 4 条`；无头下恒为 `0 条`（既有边界） |

**判据 = 各命令读数按结论回归、分母移动按增量核对表逐项对账**，
**MUST NOT** 只说"回归通过"而不给逐项读数。

### 验收信号

- 双配置（`LegendAutoChessEditor Win64 Development` / `LegendAutoChess Win64 Shipping`）
  **0 error / 0 warning**；
- **两条路线都跑**：无头 `UnrealEditor-Cmd` + 编辑器内 PIE（后者由**用户手动**执行，
  见 §边界）——内容资产驱动面**只能**在编辑器路线取值；
- `Tcs.Test.SkillLayer.Run` 零非预期红字；`.Reject` 全部红字逐条有意构造且逐条列出；
- `CHAIN-7` 三路读数齐备（**属性现值 + 条目数成对**）且阴性对照成立；
- 两轮**摘要行逐字相同 + 各检查结论相同**（MUST NOT 比字节 / 行数 / 哈希）；
- `openspec validate verify-skill-layer-e2e --strict --no-interactive` = valid；
  `openspec validate --all --strict --no-interactive` 全绿。

### 已知边界（提案期即声明，MUST NOT 在证据里写成已覆盖）

1. **无头路线 `AssetRegistry` 发现 0 条资产**（Task 3 / 4 / 5 三次实证）⇒
   内容资产驱动面**只能在编辑器 PIE 取**；无头路线承担的是**回归面**与**不依赖内容发现的判据**。
   **本项不修**（`PrimaryAssetTypesToScan` 归 R7 `INTEG-3`）。
2. **编辑器 PIE 取证由用户手动执行**（MCP `ue-editor` 当前 `disconnected`）⇒
   该路线的读数以用户回报的日志为准，装置侧**不假装自动取得**。
3. **子链残留边界**：只摘本 run **直接**挂的条目；子链另发新号 ⇒ 子链挂的条目仍会常驻
   （`CHAIN-7` 已登记，本变更**不递归下探**）。
4. **Shipping 只验编译**，不跑运行期。
5. **单机 / 单进程 / 单 PIE 会话**；无网络、无多世界并行、无 Mass 路径。
