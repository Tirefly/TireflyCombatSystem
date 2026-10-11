## 1. 取证准备（无插件代码改动）

- [x] 1.1 冻结基线：记录 TCS / LAC 两仓的 `git rev-parse HEAD`，并按 `git status --porcelain` 记录工作区状态（**只记指针，不逐条抄文件与条数**——该字段寿命极短，本仓四次实证过期）
- [x] 1.2 记录改动前三命令读数基线：`Tcs.Test.SkillCast.Run` 26/0、`.Reject` 16/0、`Tcs.Test.SkillCast.Phase` 29/0 + 延时段 6 项 + 进入段 5 项；`Tcs.Test.SkillParamChain.Run` 26/0 / `.Reject` 9/0
- [x] 1.3 记录既有 **22 份 `.uasset` + 1 份 `.umap`** 的大小 / mtime 清单（`Content/TcsDev/` 递归：根 7 + `Checks/` 10 + `E2E/` 6 = 22 份 `.uasset`），作为"未误改既有资产"的对照
- [x] 1.4 记录当前屏显段占用（`…0000`~`…7000`），确认 `…8000` 空闲；建立 `Saved/TcsAcceptance/Task6/` 归档目录
- [x] 1.5 记录**基线**：`git -C Plugins/Tirefly/TireflyCombatSystem status --porcelain -- Source/` 的**逐行清单**存为 `Saved/TcsAcceptance/Task6/source-baseline.txt`，并把这 14 项的 **SHA-256 + mtime** 存为 `source-hashes.txt`。
  **判据口径（MUST，2026-10-11 二度实测订正）**：本变更的零插件改动判据**不是**"`git diff -- Source/` 为空"——**该命令在基线时本来就不为空**（它 diff 的是 `HEAD`，而这 14 项**相对 HEAD 就是改过的**：实测 `git diff --stat -- Source/` = `7 files changed, 878 insertions(+), 130 deletions(-)`）。
  ⇒ 正确判据 = **收尾时** ① `status --porcelain -- Source/` 与 `source-baseline.txt` **逐行相同**；② 14 项的 **SHA-256 与 `source-hashes.txt` 逐项相同**；③ 14 项的 **mtime 全部早于基线记录时刻**（基线记录于 `2026-10-11 03:02:47`，实测 14 项最大 mtime = `2026-10-10 11:38:39` ⇒ 早 15 小时以上）。
  **为什么不用 `git diff -- Source/`**：它是**相对 HEAD** 的，与本变更无关的既有改动会让它恒非空 ⇒ 写进判据必然产生**假失败**（这正是 `MEM-20261010-06` 记的那类病灶，本轮在**第二处**又踩了一次）。

## 2. Step 0 其一：跨帧判据的战斗时钟骨架（`TcsDev` 私有）

> **判据来源**：用户 2026-10-10 裁定（骨架先私有、MUST NOT 提升为 `UTcsClockSubsystem` 公共口）。
> **反面教材**：Task 5 的时段面探针两次在"等待"上踩坑——① 按固定墙钟秒数赌取样时刻（实测比完成早 28 毫秒）；
> ② 把判据执行体放进 `FTimerManager` 回调（用户编辑器内实测 `Passed=-1514964509`，**负计数 = 释放后使用**）。

- [x] 2.1 新增 `Source/TcsDev/Public/Dev/TcsDevCombatClockWait.h`：按**战斗时钟时轴**轮询"条件成立"的共用骨架；借既有范式**挂起 → 唤醒 → 持有者继续**（`TcsStepWaitDelay` + `FTcsChainRun::PendingExpiry` + `UTcsEffectSubsystem::ResumeRun`），**MUST NOT 另造一套**
- [x] 2.2 骨架注释写进三条判据：① 时轴 = `FTcsClock.Elapsed`（按 `ScaledDt` 累加），**MUST NOT** 用 `FTimerManager` 的未缩放真实时间；② 轮询**被测事件本身**，MUST NOT 轮询一个相邻但不同的量；③ **超时 MUST 报 FAIL**，MUST NOT 静默跳过
- [x] 2.3 跨帧捕获纪律：一切捕获为**值或弱引用**；计数器 MUST 有明确的跨帧持有者（`TSharedPtr` 按值捕获进回调，或门面持有的对象），MUST NOT 依赖任何栈帧存活
- [x] 2.4 骨架**MUST NOT** 提升为插件公共面（零消费者不预建）；若实施期发现需要，**停下报告**再决定
- [x] 2.5 骨架自证读数：一条"条件在 N 拍内成立 ⇒ PASS / 超时 ⇒ FAIL"的最小用例，两条路线各跑一次

## 3. Step 0 其二准备 + 内容资产（编辑器 PIE 面）

- [x] 3.1 `Config/DefaultGameplayTags.ini` 追加检查词行（**不新开根**）：技能定义身份 2 条、施法面链身份 1 条、参数键 1 条、时段标识 1 条；**`gameplay-tag-governance` 根表 12 不动**（收尾时核对）
  **两个数 MUST 分清（2026-10-11 实测订正，防重犯）**：① **插件侧根表 = 12 个根**（`gameplay-tag-governance` spec `:19` 原文"共 12 个"+ `:23-34` 的 12 行表）——这是 §8.2 要核的那个数；② **宿主 `Config/DefaultGameplayTags.ini` 现有 11 个根**（`Attribute`/`AttrModDef`/`CastPhase`/`DamageFlowTemplate`/`EffectChain`/`EffectTriggerDef`/`Probe`/`SkillDef`/`StateDef`/`TcsEvent`/`TcsStateParam`）——这是**另一个测量**，且本变更新增 5 词全落在既有根下 ⇒ 宿主侧也仍是 11。**MUST NOT** 把宿主 ini 的根数当成插件根表的数（同一句子里放两个测量口径 = 上一轮已实证过的病灶）。
  **附记（既有 spec 内部不一致，本变更**不修**）**：`gameplay-tag-governance` spec `:213` 与 `:218` 写"本插件 **11** 个根"，而 `:19` 写"共 **12** 个"——`:213` 那段标注"2026-10-05 订正"，成文于 `SkillDef` 根落地（2026-10-06 R6 Task 1）**之前** ⇒ 两处是**陈旧数**。`specs/` 是事实来源，改它需另开 delta，**不在本变更范围内**；此处只如实登记。
- [x] 3.1a **同一文件内的既有缺陷就地修复 + 防复发自查**（2026-10-11 实测；`Config/DefaultGameplayTags.ini:57`）：原第 57 行把两条 `+GameplayTagList=` 挤在**同一个物理行**上（`StateDef.Check.StateLayer` 与 `TcsStateParam.Check.Override`），而引擎 ini 解析是**严格逐行**的（`ConfigCacheIni.cpp:1950` → `FParse::LineExtended`；段头判定要求 `Start[0]=='[' && Start[Last]==']'`）⇒ **第 2 条被静默丢弃**。
  **实测证据**：引擎自身在 `Saved/Logs/LegendAutoChess.log:2355-2359` 打了三条 `LogGameplayTags: Warning: Invalid GameplayTag TcsStateParam.Check.Override found in property …`，分别落在 `DA_State_E2E` / `DA_SkillDef_E2E` / `DA_SkillDef_E2E_Second` **三份真资产**上。
  **为何当时 R5 的 S7 仍 PASS（本条最值得记）**：资产字节里仍存着该 tag 字符串（实测 ascii 命中 1 次），加载路径 `ImportSingleGameplayTag` 经 `ValidateTagCreation` 会**自动建节点**且不打警告 ⇒ 该 tag 靠**编辑器自动登记**侥幸可用、名字匹配成立，S7 的负对照键照常读出 10.000——**但它不在任何配置词表里**。
  **动作**：① 拆成两行（`+1 byte`，bare-LF 与无 BOM 保持）；② 落 `Saved/TcsAcceptance/Task6/check-tag-ini.ps1` 作**机械自查**（判据 = 每行至多一条 `+GameplayTagList=`；顺带核对 tag 总数 == 唯一数）；③ 证据记"修复前 / 后"两侧的 tag 条数与根数。
  **MUST NOT** 靠人眼复核本条——该缺陷的症状是**静默**的（错词若被资产引用过就连警告都不打）。
- [x] 3.2 建 `Content/TcsDev/E2E/DA_SkillDef_E2E_SinglePhase`（`UTcsSkillDef`）：**单段**、时长 **3600 秒**、`bInterruptibleDefault = true`、`CastChainId` = 施法面主链、`ParamChainRows` ≥2 行且**分属不同运算带**、`Instancing = CI_InstancePerExecution`
- [x] 3.3 建 `Content/TcsDev/E2E/DA_SkillDef_E2E_Replaceable`（`UTcsSkillDef`）：`CI_InstancePerEntity` + `bRetriggerOnActive = true` + 可打断（顶替路径读数用）
- [x] 3.4 建 `Content/TcsDev/E2E/DA_Chain_E2E_SkillCast`（`UTcsEffectChainDef`）：真挂 `FTcsStepModifyAttribute`（写 `Attribute.Attack`，减量取**与其它检查都不同**的数，避免掩盖）+ 前置 `FTcsStepSelectTargets`（`TcsSelSelf`）——**内容步骤 MUST NOT 写死运行时实体句柄**
- [x] 3.5 **内容资产的类型稳定性核对**：三份资产引用的类型 MUST 全部住插件（`TcsEffect` / `TcsTargeting` / `TcsIntegration`），MUST NOT 引用 `TcsDev`（切片侧）的任何类型——否则切片退役后该步骤会**静默变空**
- [x] 3.6 逐个跑 `IsDataValid` 确认 `Valid` 且零错误（走 `unreal.EditorValidatorSubsystem.validate_assets_with_settings`，**一次只验一个**才能归因；需 `load_assets_for_validation=True`），并记录三份资产的字节大小
- [x] 3.7 **既有 5 份技能面资产一字不动**（`DA_SkillDef_E2E` / `DA_SkillDef_E2E_Second` / `DA_Chain_E2E_Apply` / `DA_Chain_E2E_Behavior` / `DA_ModDef_E2E`）——按 §1.3 的清单核对大小 / mtime 未变

## 4. 新装置实现（LAC `Source/TcsDev/`）

- [x] 4.1 新增 `Public/Dev/TcsDevSliceRig_Skill.h`：只声明 `void RegisterCommands();`（照 `TcsDevSliceRig_State.h` 体例）
- [x] 4.2 新增 `Private/Dev/TcsDevSliceRig_Skill.cpp`：复用 `TcsDevSliceRig_Internal.h` 的共用件（屏显段 / 找世界 / 造单位 / 拆夹具 / 来源发号器），**MUST NOT 复制一份**（两份 `GTcsDevRigActors` = 两条命令各自以为清理干净）
- [x] 4.3 屏显段取 `0x7C5D8000`（段内游标基准 `0x7000`），注释写进既有段分配表
- [x] 4.4 实现 `Tcs.Test.SkillLayer.Run` 骨架：fail-fast 前置门（世界 / 技能门面 / 效果门面 / 属性门面 / 时钟 / 定义库），**缺任一项即出具体 FAIL 行**，不静默降级
- [x] 4.5 实现 Q-7 剧本的**内容资产路径**：定义库就绪行计数 → `GetRegisteredSkillDef` 按 `DefTag` 命中且与缓存同一份 → `GrantSkill` → `TryActivate` 逐道门禁 → 参数链折叠读数 → 起主链 → 链改属性 → `TcsEvent.Cast.*` 事件计数 → `RevokeSkill` 级联摘净
- [x] 4.6 实现**参数链折叠的可判读数**：让"折叠生效"与"落兜底值"两个可能读数**互不相同**（覆盖 / 定义 / 兜底三值两两不同 + 配一条负对照键）——R5 的教训是 `ParamRef` 未命中时**静默取 `Fallback`**，两个读数相同则该检查**不可判**
- [x] 4.7 实现 `CHAIN-7` **三路读数**（完成 / 打断 / 顶替），每路取**属性现值 + 该属性上的账本条目数成对**，MUST NOT 取"回收函数被调用几次"
- [x] 4.8 实现**同一次施法内的阴性对照**：主链挂的条目随终结摘净，而**非本次施法来源**挂的条目仍常驻——两个方向都可判（"摘干净"与"摘过头"）
- [x] 4.9 三路读数**串行**取值，每路前 `ResetCounts()`——"恰 N 次"类计数判据，**同一观察者名下 MUST 只有一个在飞 run**（Task 5 缺陷 8 的形态）
- [x] 4.10 实现 `Tcs.Test.SkillLayer.Reject`：未登记定义身份 / 未授予即激活 / 已撤销条目上的激活 / 悬空运行句柄的查询与终结 / 不可打断段上的打断与顶替 / 世界拆解后的门面访问；**全部预期红字集中于此**，头部逐条枚举
- [x] 4.11 拒绝面**MUST NOT** 依赖"磁盘上放一个坏资产"（定义库是全库按类扫描，坏资产会污染所有命令的失败清单）⇒ 一律**运行期造瞬态定义**
- [x] 4.12 实现复现性摘要行：只留**不变量**（如"恰一次 / 互异 / 归零"），MUST NOT 打印进程内单调计数器（来源句柄 id）或帧驱动量（轮询拍数）
- [x] 4.13 在 `TcsDevModule.cpp` 加一行命令注册
- [x] 4.14 **`TcsDevSkillCastProbe_Phase.cpp` MUST NOT 因体例原因被拆分**（其 856 行超限债的触发条件 = "下一次需要改动本文件任何判据时"；本变更不取该文件读数）；若因其它原因必须改它，则先拆再改，并把该次拆分计入 §8 回归面

## 5. 编译与回归

- [x] 5.1 Editor 编译：`LegendAutoChessEditor Win64 Development` → `Result: Succeeded`，零 warning / error（**先关编辑器**；实测 Live Coding 激活时 UBT 直接拒绝构建）
- [x] 5.2 Shipping 编译：`LegendAutoChess Win64 Shipping` → `Result: Succeeded`，零 warning / error
- [x] 5.3 **产物时间戳 / 大小对账**：`UnrealEditor-TcsDev.dll`（**住宿主 `Binaries\Win64\`**）/ `LegendAutoChess-Win64-Shipping.exe` 的时间戳 MUST **早于**全部快照；`UnrealEditor-TcsSkill.dll` 住**插件仓** `Binaries\Win64\`（宿主路径查会得 MISSING，那是查错位置）
- [x] 5.4 回归六命令按结论回归：`SkillCast.Run` 26/0、`.Reject` 16/0、`.Phase` 29+6+5 PASS/0 FAIL、`SkillParamChain.Run` 26/0 / `.Reject` 9/0、`SkillDef.Run` / `SkillRegistry.Run` 基线不变
- [x] 5.5 回归既有装置：`Tcs.Test.State.Run` 15/0 / `.Reject` 7/0；`Tcs.Test.Slice.Run` 即时 65/0 + 延迟 72/0 / `.Reject` 11/0
- [x] 5.6 定义库就绪行**分母移动预声明**：新增 2 条技能定义 ⇒ 编辑器 PIE 下应从 `技能定义 2 条` 变为 `技能定义 4 条`；无头下恒为 `0 条`（既有边界，见 §7）。**MUST NOT 只说"回归通过"而不给逐项读数**，MUST NOT 声称"零回归"

## 6. 两条执行路线取证

- [x] 6.1 **无头路线**（`UnrealEditor-Cmd <uproject> /Game/TcsDev/L_TcsDev_Slice -game -nullrhi -nosound -noPause -unattended -ABSLOG=<日志> -ExecCmds=<单条命令>`，每条 ~110 秒后 Kill）：跑回归面 + 不依赖内容发现的判据；**读数 MUST 在同一会话内取齐**（日志随编辑器重启轮转）
- [x] 6.2 **编辑器 PIE 路线**：跑内容资产驱动面 + `CHAIN-7` 三路读数 + 拒绝面；装置侧**不假装自动取得**。
  **实测订正（2026-10-11）**：本项原写"**由用户手动执行**；MCP `ue-editor` 当前 `disconnected`"——**该状态已变**：编辑器开启后 MCP **已连上**，本路线**由 agent 自主完成**（经 `SlateInspectorToolset` 驱动编辑器控制台的 `py` 通道；注：`ProgrammaticToolset.execute_tool_script` **沙箱禁 `import unreal`**，故走控制台而非脚本工具）。
  **★ 本路线抓出了第 8 处缺陷**（拒绝面与 `Run` 共用定义身份 + 幂等登记口 ⇒ 后跑者被静默跳过登记；**无头给的是假绿灯**）⇒ 详见 `EVID-2026-10-11-skill-layer-e2e` §4 缺陷 8 与 §5.4 的顺序对照。
- [x] 6.3 第一轮：运行 `Tcs.Test.SkillLayer.Run`，断言**零非预期红字**；`CHAIN-7` 三路读数齐备且阴性对照成立
- [x] 6.4 第一轮：运行 `Tcs.Test.SkillLayer.Reject`，断言全部拒绝按预期留痕、计数与断言一致、头部枚举与实测逐条吻合
- [x] 6.5 第二轮：重跑 `Tcs.Test.SkillLayer.Run`，断言**复现性摘要行逐字相同** + 各检查结论相同（**MUST NOT** 比字节 / 行数 / 哈希）
- [x] 6.6 跨 PIE 残留检查：停止 PIE 后确认无残留世界 / 实例 / 触发行；重进 PIE 后起点读数归零，结论记入边界清单
- [x] 6.7 冻结日志快照到 `Saved/TcsAcceptance/Task6/`，记录**字节数 + 行数 + SHA-256**（provenance：mtime 与大小**只作 provenance**，MUST NOT 当复现性判据）
- [x] 6.8 **事件面基线 MUST 在起因之前取**（R5 的教训：施加/激活路径全程同步，`ApplyState` 内即广播 ⇒ 基线晚取会让增量恒 0）
- [x] 6.9 `IsDataValid` 复核三份新资产，并把"三份新资产 + 既有 5 份未动"的字节清单写进证据

## 7. 证据与文档

- [x] 7.1 写 `Documents/combat-system-design/evidence/2026-10-XX-skill-layer-e2e.md`（`EVID-2026-10-XX-skill-layer-e2e`）：摘要行（只留不变量）、检查编号 ↔ 标题对照表（**逐字取自日志**，MUST NOT 按计划措辞或既往习惯推断）、预期红字清单、双面读数（属性面 + 事件面）、`CHAIN-7` 三路表、如实边界（逐条）
- [x] 7.2 边界清单 MUST 覆盖：单机 / 单世界 / 单 PIE 进程；**无头路线 `AssetRegistry` 发现 0 条资产**（承接轮 R7 `INTEG-3`）；编辑器 PIE 读数经 **MCP** 取得（**已连上**——见 §6.2 的订正）；`CQM_Custom` / `MCS_Custom` 两处具名未实现；**子链挂的条目按设计常驻**（`CHAIN-7` 已登记的残留边界，本变更不递归下探）；Shipping 只验编译；跨帧骨架仍住 `TcsDev` 私有（未提升为框架契约）；两轮读数只在**不变量**上逐字一致
- [x] 7.3 更新 `INDEX.md`：§1 现状、§4.4 R6 行、§4.5 台账计数、§4.6 证据行、§2 目录树计数、`下一步`
- [x] 7.4 更新 `ledger/deferred-inputs-ledger.md`：`CHAIN-7` 行**行内补记**"三路闭合读数已取"（**不改状态**——该行已是"✅ 已闭合"；同 Task 5 那一笔的处置口径）；**零新增零删除，总数不变**
- [x] 7.5 更新 `log/implementation-log.md`：Task 6 条目（含两条路线的取证形态、`CHAIN-7` 三路读数、实施期缺陷逐条留痕）
- [x] 7.6 更新 `log/decisions-log.md`：跨帧时轴纪律落成规格这一条口径、三路读数取"不可分属性"的裁定
- [x] 7.7 计划回写：`plan-r6-skill-layer.md` Task 6 按 `docs-convention` §7「冻结档只追加」补**实施注记块**（**正文不改**）；Task 5 §8.7 的 `[ ]` **MUST NOT 在本变更内勾**（归 Task 7 收束时统一处置）
- [x] 7.8 校验：`openspec validate verify-skill-layer-e2e --strict --no-interactive` 通过；`openspec validate --all --strict --no-interactive` 全绿。**口径 MUST 分清**（实测：本变更落纸后 `--all` = **40 passed / 0 failed (40 items)** = 能力 **38** + 活跃提案 **2**；能力数本变更后 **38 → 39**）——**MUST NOT** 把 `--all` 的条目数当能力数，也 MUST NOT 用"全绿"代替逐项数字
- [x] 7.9 行尾 / 编码复核：改动文件零 CRLF、零 BOM

## 8. 收尾对账（本变更不自行归档）

- [x] 8.1 复核 §1.5：`git -C Plugins/Tirefly/TireflyCombatSystem status --porcelain -- Source/` 与 `source-baseline.txt` **逐行相同**（Task 5 遗留的 **14 项**不变、零新增零删除）；14 项 **SHA-256 与 `source-hashes.txt` 逐项相同**；且 14 项 **mtime 全部仍早于基线记录时刻**（`2026-10-11 03:02:47`）⇒ 本变更未二次改动插件源码。
  **MUST NOT** 用 `git diff -- Source/` 作判据（它相对 `HEAD`，基线时已非空——见 §1.5 的口径订正）。
- [x] 8.2 复核 `gameplay-tag-governance` 根表**仍为 12 个根**（**插件侧根表口径**，见 §3.1；本变更不新开根，只加 `Check` 词）；并**重跑** `check-tag-ini.ps1` 确认零违规（§3.1a 的防复发项）
- [x] 8.3 复核 `TcsDevSkillCastProbe_Phase.cpp` 行数**仍为 856**（未被本变更拆分）；若被拆，把该次拆分计入 §5 回归面
- [x] 8.4 台账机械复核：表行数 == 唯一 id 数；本变更零新增零删除
- [x] 8.5 归档归 **Task 7 收束**（`openspec archive verify-skill-layer-e2e --yes`）；**本变更不自行归档**（判据 = 归档会把 delta 合入 `specs/` 真值面，而那时 `add-cast-phases-and-interrupt` 应已一并归档）
