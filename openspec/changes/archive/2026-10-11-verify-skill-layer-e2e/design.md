## Context

R6 的六个技能层能力各自都有一份归档证据，但**没有一条证据穿过它们的耦合点**。
本变更要在**宿主装置**里把 Q-7 剧本串成一条竖切，并产出可复核证据。约束来自本轮侦察的实测事实：

1. **既有技能的验收资产不能动。** `DA_SkillDef_E2E` 被 Task 1 的探针钉了 `D9`（`Params.Num()==1`）/
   `D10`（`BoolSwitches.Num()==1`）/ **`D11`（瞬发 = 空时段表）** / `D14`（`Descriptions.Num()==1`）
   四条读数（`TcsDevSkillDefProbe.cpp`）。Task 6 要验三路终结就**必须**有非空 `Phases`，
   要验参数链折叠就**必须**有 `ParamChainRows` ⇒ **改它 = 打掉一条已归档能力的验收面**。
2. **内容资产驱动面与无头路线互斥。** 冻结快照实测（`Saved/Task5/final/snapshot-R6Task5-phase.log`）：
   无头下 `定义库就绪——链定义 0 条，触发定义 0 条，状态定义 0 条，修正器模板 0 条，
   技能定义 0 条，失败 0 条`；编辑器 PIE 下是 `链 11 / 触发 2 / 状态 2 / 修正器模板 1`。
   ⇒ 本变更的主交付面**只能在编辑器 PIE 取**，而 MCP `ue-editor` 当前 `disconnected`。
3. **活跃提案会打架。** `add-cast-phases-and-interrupt` 仍未归档（`openspec list` = `70/71`），
   它**持有** `skill-cast-phases`（ADDED）与 `skill-cast-runtime`（MODIFIED ×2）两份 delta，
   而 `CHAIN-7` 三路回退的判据文字**就在那两份 delta 里**。
4. **一处体例债的触发条件悬在旁边。** `TcsDevSkillCastProbe_Phase.cpp` = **856 行**
   （`TcsDevSliceRig.cpp` 已 3094 行），其超限债的触发条件 = "下一次需要改动本文件**任何判据**时先拆再改"；
   拆分被尝试过**两次**，两次都改坏了已验收代码。
5. **屏显段已占到 `…7000`**：观测者 `…0000` / Slice `…1000` / GC `…2000` / 状态 `…3000` /
   技能账本 `…4000` / 技能施法 `…5000` / 参数链 `…6000` / 施法时段 `…7000`
   ⇒ 本变更取 **`…8000`**（段内游标基准 `0x7000`）。同 key = 静默覆写、无报错。
6. **`ITcsEntityQuery` 的门禁第一道已落地**（Task 2 Q-11 的 `IsEntityReady`），
   `TcsDevBootstrap::ApplySliceAttributes` 可给装置单位装配属性（含 `Attribute.Attack` 与
   `Attribute.Armor`），故三路读数取"属性现值 + 条目数"的两个读数面**都已就绪**。

## Goals / Non-Goals

- **Goals**：一条真内容技能资产驱动的竖切，穿过"发现 → 授予 → 门禁 → 参数链折叠 → 起链 →
  真改属性 → 事件链 → 三路终结 → 撤销级联摘净"；`CHAIN-7` 三路闭合读数 + 阴性对照；
  双面证据（属性面 + 事件面）；跨帧判据的时轴纪律**落成规格**；拒绝面独立成命令；
  边界清单**每条标注"已实测"或"未覆盖"**；两条执行路线各自成证。
- **Non-Goals**：不做冷却 / Cost（R6.5）；不做 `CQM_Custom` / `MCS_Custom` 求值（R6.5-g）；
  不做 `PrimaryAssetTypesToScan` 注册（R7 `INTEG-3`）；不做网络姿态 / 表现耦合 / Mass 路径；
  **不改插件 `Source/` 任何一行**；**不拆分既有 856 行文件**。

## Decisions

### D1：竖切装置落新文件 `TcsDevSliceRig_Skill.cpp`，而非扩既有探针

`TcsDevSkillCastProbe_Phase.cpp` = **856 行**，其超限债的触发条件是"下一次需要改动本文件任何判据时"。
本变更若把新竖切塞进它，等于**主动踩响自己定的触发条件**，并按纪律必须先做一次全文件拆分——
而拆分**被尝试过两次、两次都改坏已验收的判据执行体**。

- 取 `TcsDevSliceRig_Skill.cpp`（照 `TcsDevSliceRig_State.cpp` **803 行**同构，复用
  `TcsDevSliceRig_Internal.h` 的共用件：屏显段 / 找世界 / 造单位 / 拆夹具 / 来源发号器）。
- 备选"扩既有探针"——拒绝：触发条件会响，且风险落在**已验收判据的执行体**上。
- 备选"复制一份共用件"——拒绝：`GTcsDevRigActors` 是共享表，两份会让两条命令各自以为清理干净
  （`TcsDevSliceRig_Internal.h` 的注释已把它列为真实缺陷而非风格问题）。

### D2：新建专用内容资产，`DA_SkillDef_E2E` 一族一字不动

见 Context 第 1 条。新资产（住 `Content/TcsDev/E2E/`）：

| 资产 | 类型 | 关键取值 | 用途 |
|---|---|---|---|
| 单段可打断技能定义 | `UTcsSkillDef` | 单段（时段时长取 **3600 秒**）、`bInterruptibleDefault = true`、`CastChainId` = 施法面主链、含 `ParamChainRows` ≥2 而行（**不同运算带**） | 完成 / 打断两路读数 |
| 顶替档技能定义 | `UTcsSkillDef` | `CI_InstancePerEntity` + `bRetriggerOnActive = true` + 可打断 | 顶替路径读数 |
| 施法面主链 | `UTcsEffectChainDef` | 真挂 `FTcsStepModifyAttribute`（写 `Attribute.Attack`），并含 `SelectTargets`（内容步骤不写死运行时句柄） | 三路回退的**被测物** |

**时段时长取 3600 秒是判据不是偏好**：Task 5 实测"单条命令的 PIE 会话可跑 110 秒以上"，
而战斗时钟首帧一次性补上加载期时间 ⇒ 60 秒**实测真的会到期**。
凡夹具里"时长 / 容量 / 上限"取值，MUST 以**整段会话的最坏情形**为界。

### D3：跨帧等待骨架落 `TcsDev` 私有，MUST NOT 提升进插件公共面

用户 2026-10-10 裁定。骨架 = "按**战斗时钟时轴**轮询条件成立"，
借 TCS 既有范式 **挂起 → 唤醒 → 持有者继续**（`TcsStepWaitDelay` + `FTcsChainRun::PendingExpiry` +
`UTcsEffectSubsystem::ResumeRun`）——该范式的状态住在池化运行态里，**不依赖任何栈帧**。

- **落点**：`Source/TcsDev/Public/Dev/TcsDevCombatClockWait.h`。
- **MUST NOT** 本轮提升为 `UTcsClockSubsystem` 的公共口——判据 = **零消费者不预建**
  （只有 3 个探针需要它，未到成为框架契约的时点）。若 R6.5 / R7 反复需要，再以 MODIFIED
  增补 `TcsCore` 的时钟需求。
- **两条判据写进骨架注释**：① 时轴 = 战斗时钟（`FTcsClock.Elapsed`，按 `ScaledDt` 累加），
  **MUST NOT** 用 `FTimerManager` 的未缩放真实时间；② 轮询**被测事件本身**，
  超时 MUST 报 FAIL。
- 备选"提升为 `UTcsClockSubsystem` 公共口"——拒绝：零消费者预建，且会把装置需求固化成框架契约。

### D4：`CHAIN-7` 三路读数取"属性现值 + 条目数"成对，并配**同一次施法内**的阴性对照

规格（`skill-cast-runtime` 的两处 scenario）明文：读数 MUST 取属性现值 + 条目数（成对），
MUST NOT 取"回收函数被调用了几次"——后者在"调了但摘错 `Source`"时仍会全绿。

**本变更追加一条自己的判据（不是规格要求，是本轮取证需要）**：
**同一次施法内**要有第二类条目作对照——主链挂的条目随终结摘净，而**非本次施法来源**挂的条目
仍常驻。这样"摘干净"与"摘过头"两个方向都可判。

- 原型 = R5 实测的"护甲精确复原 vs 攻击停在 X−5"（`EVID-2026-10-05-state-layer-pie` §1.2
  S14/S15），但 R5 那次的两条流水**分落两个属性**才互不掩盖；
  本变更的三路读数**用同一个属性**（`Attribute.Attack`）——因为被测物就是"链改的属性是否回退"，
  而同类对照由"**另一条非施法来源的条目**"承担。
- 备选"分落两个属性"——拒绝：那会让三路读数**偏离 `CHAIN-7` 的字面判据**（它问的是
  链挂的那条属性回不回退），且会把验收变成对实现的追认。

### D5：拒绝面独立成命令，全部预期红字集中

照本仓**第三次重申**的装置纪律：常规命令输出**零红字**，任何"故意触发 Warning / Error / ensure"
的检查 MUST 独立成 opt-in 命令。命名 = `Tcs.Test.SkillLayer.Run`（零红字）/
`Tcs.Test.SkillLayer.Reject`（自带屏显声明"红字为预期"）。

**本变更的特殊约束**：定义库的发现是**全库按类扫描**——一个故意配错的资产会污染**所有**命令的失败清单。
⇒ 拒绝面 MUST NOT 依赖"磁盘上放一个坏资产"，一律在**装置内运行期造瞬态定义**走拒绝路径。

### D6：屏显段 `0x7C5D8000`（段内游标基准 `0x7000`）

见 Context 第 5 条。同 key 静默覆写且**只能看出来、测不出来**（Task 5 由用户目视确认过一次）。

### D7：规格里写进"跨帧时轴 + 双路线"这条纪律（用户裁定）

判据级纪律住注释里会在下一轮失传（`MEM-20261010-01` 与 `-02` 正是这么产生的）。
故本能力**第 4 条需求**把它落成契约：时轴归属 / 轮询被测事件本身 / 超时报 FAIL /
跨帧捕获只许值或弱引用 / **两条路线都跑**。

- 备选"只住 `TcsDev` 注释与计划"——拒绝：规格是复核者能查到的唯一契约载体。
- 备选"另立通用验证纪律能力"——**否决但如实登记**：覆盖面确实更广（状态面装置同受约束），
  但会新开一个横切能力，与"每个能力单一用途、10 分钟可理解"相权不划算；
  本能力先按"技能层验收面"承载，若 R7/R8 再出现同款纪律，**届时**考虑提为通用能力。

### D8：零 MODIFIED（`add-cast-phases-and-interrupt` 仍活跃）

见 Context 第 3 条。本变更对 `skill-cast-phases` / `skill-cast-runtime` **只产出证据、不动规格**。
代价如实登记在 proposal：交付后 `openspec list` 仍显示 `add-cast-phases-and-interrupt` 有 1 项未勾
（`8.7` 的读数已取得，勾选动作仍归 Task 7）。

## Risks / Trade-offs

| 风险 | 缓解 |
|---|---|
| 编辑器 PIE 取证依赖人工，可能阻塞整轮 | proposal 已把它写成规格边界；先落装置 + 无头可达面 + 拒绝面，把内容面读数留作最后一拍 |
| 无头下 `AssetRegistry` 发现 0 条 ⇒ 主交付面在无头路线**恒不可达** | **本项不修**（`INTEG-3` 归 R7）；装置 MUST 把"无头下发现 0 条"识别为**环境限制**而非失败，并在证据里如实标注 |
| 三路读数可能因并发观测互相污染（Task 5 缺陷 8 的形态） | "恰 N 次"类计数判据，**同一观察者名下 MUST 只有一个在飞 run**；三路**串行**取值，每路先 `ResetCounts()` |
| 新装置落文件可能又超 300 行体例约定 | 照 `TcsDevSliceRig_State.cpp`（803 行）先例：装置侧大文件是既有体例例外，**如实登记**而非假装达标；但 MUST NOT 借机把既有 856 行文件拆分（触发条件不响） |

> **为什么创建本 `design.md`**（`openspec/AGENTS.md` 说它是可选文档）：存在需要在编码前先定的技术含糊之处，
> 且 D1 / D3 / D4 三条**各有被否备选**——不落纸，下一位执行者会重新走一遍同样的取舍。

## Migration Plan

1. 落装置骨架 + 共用件复用（D1 / D3），双配置编译通过；
2. 造内容资产（D2），在编辑器里逐资产跑 `IsDataValid` 并记录字节数；
3. 跑无头路线：回归面 + 不依赖内容发现的判据；
4. 由**用户手动**跑编辑器 PIE：内容资产驱动面 + 三路读数；
5. 写证据档（双面读数 + 边界清单 + provenance）；回写文档面；
6. **回滚**：本变更零插件 `Source/` 改动 ⇒ 回滚 = 删 `Source/TcsDev/` 两个新文件、
   删三份新资产、还原 `ini` 与 `TcsDevModule.cpp` 的那几行；既有能力面不受影响。

## Open Questions

1. **三路读数的"顶替"路能否在单次 PIE 里与"完成"路串行完成**——顶替需要"旧 run 在飞"，
   而单段 3600 秒的 run 恰好满足；但顶替会终止旧 run 并起新 run，
   新 run 也会在后续帧里……**不会**（3600 秒内不到期）⇒ 预计可在同一命令内串行排定。
   实施期若发现相互干扰，改由**多轮命令**分别取值，并在证据里如实标注轮次。
2. **参数链折叠的"可判读数"怎么造**：需要让"折叠生效"与"落兜底值"两个读数**互不相同**
   （R5 的教训：`ParamRef` 未命中时**静默取 `Fallback`**）。本变更取"覆盖 / 定义 / 兜底三值
   两两不同 + 配负对照键"的手法。
3. **内容资产里的 `ModifyAttribute` 步骤能否只引用插件类型**：`FTcsStepModifyAttribute` 住
   `TcsEffect`（随插件长期存在），`FTcsStepSelectTargets` 住 `TcsTargeting`——
   两者都长于内容 ⇒ 满足"内容引用的类型必须长于内容"这条纪律（R5 D2）。
   实施期 MUST 核对资产内**零引用切片侧类型**。
