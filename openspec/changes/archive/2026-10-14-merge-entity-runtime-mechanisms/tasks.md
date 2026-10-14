## 1. Delta 编写

- [x] 1.1 `specs/entity-runtime-mechanism/spec.md`（**新能力** ADDED）——5 条需求：
      统一运行态句柄判据与转换（**否定式要求**：MUST NOT 合并类型）/ 统一 per-entity 桶与注册表 /
      统一 GC 引用收集口 / 统一门面清理口与重入广播守卫 / 统一重申请决策接口
- [x] 1.2 `specs/state-instance-lifecycle/spec.md`——2 条 MODIFIED（状态实例句柄 / per-entity 桶注册表与代际校验）：
      句柄**保持独立类型**但判据收敛；桶**载体改为共享模板**（`TTcsInstancePool`）
- [x] 1.3 `specs/skill-cast-runtime/spec.md`——2 条 MODIFIED（施法运行态与实例池 / 六道具名门禁与激活入口）：
      运行态句柄保持 `FTcsCastRunHandle`（判据统一 + 逻辑收敛）；第 ④ 道 Instancing 判定改经统一决策接口
- [x] 1.5 `specs/instance-handle-pool/spec.md`——1 条 MODIFIED（实例池的分配释放与解析）：
      **实施期发现本变更真的改了池**（提案初稿猜"沿用、可能无需改"，实测**需要**）——
      新增第三模板参（代际发号策略，默认档保持既有行为）、`IsValid` 补奇偶校验、
      `Resolve() const` / `ForEach() const` 两个重载、五个按槽位下标驱动的口；
      **零丢失预检**：场景集 5→5（+3 新增 = 8），逐条比对**零丢失**
- [x] 1.4 **MODIFIED 全量复制预检**（已执行，机械比对）：逐需求比对原规格与本 delta 的场景集 ——
      `状态实例句柄` 5→5、`per-entity 桶注册表与代际校验` 3→4（+1 新增）、
      `施法运行态与实例池` 8→8、`六道具名门禁与激活入口` 5→6（+1 新增）；
      `实例池的分配释放与解析` 5→8（+3 新增）；**零丢失**

## 2. 待补的 delta（本提案**有意不含**，MUST 在同轮实施时同批处理）

> 以下三处经**符号方向扫描**（`FTcsStateHandle` / `FTcsCastRunHandle` / `FTcsSkillEntryHandle` /
> `FStateBucket` / `FTcsStateRegistry` / `FTcsStateStackDecisionFragment` 全规格库 grep）发现
> 仍有**行内提及**，但**不是需求正文的定义或验收判据** ⇒ 归**实施期同批改述**，不单独出 delta：

- [x] 2.1 `effect-trigger`（「触发行实例与级联退订」需求内 2 行：`Subject` 内层类型说明）
- [x] 2.2 `state-behavior-fragments`（1 行：上下文结构体字段说明）
- [x] 2.3 `skill-cast-phases` / `skill-param-chain` / `skill-layer-e2e-validation`（各 1~2 行签名与脚注）
- [x] 2.4 `state-stacking-policies`（「Custom 决策 Fragment」需求——**已出 delta**）：
      3.2 的两个裁决点**都取默认档**（`STAT-7` 不改 / `STAT-6` 不补）⇒ 该 Fragment 类型**保留**、
      形状**可逐字保留**，只把三方法的**契约上提到 `FTcsEntityAdmission`** 并由本类型继承；
      **零丢失预检**：场景集 3→5（+2 新增），逐条比对**零丢失**

## 3. 校验

- [x] 3.1 `openspec validate merge-entity-runtime-mechanisms --strict --no-interactive` = **valid**
- [x] 3.2 **实施前 MUST 先落的两个裁决点**（`PLN-REFACTOR-1` Task 1 Step 4）：
      `STAT-7`（层模型是否重开）→ **不改**，判据 = **零消费者不预建**（**不是**"改起来贵"）；
      `STAT-6`（`PerStackValue` 是否顺手补）→ **不补**，同判据。**结论已写进 `PLN-REFACTOR-1` §0.3**
      （2026-10-13 已按"成本不进判据"口径重述）。⇒ 因两者都取默认档，
      `state-stacking-policies` 的 `Custom 决策 Fragment` 需求形状**可逐字保留、只换名字与落点**，
      该 MODIFIED delta 随本变更同批补出（见 2.4）
- [x] 3.3 双配置编译（Development Editor + Game Shipping）**0 error / 0 warning**
      —— 实测：`LegendAutoChessEditor Win64 Development` = `Result: Succeeded`；
      `LegendAutoChess Win64 Shipping` = `Result: Succeeded`（两条日志 error / warning 命中数均为 **0**）
- [x] 3.4 **硬判据：宿主既有装置读数逐字不变**（本变更只搬机制、不改行为）——
      2026-10-14 无头实测 **10 条命令**全部与冻存基线**逐字相同**：
      `SkillCast.Run` **26/0**（在册 run=8）、`SkillCast.Reject` **16/0**（在册 run=0 条目=6）、
      `SkillParamChain.Run` **26/0**、`.Reject` **9/0**、`SkillLayer.Run` **24/0**（`[复现性摘要]` 与基线**逐字相同**）、
      `.Reject` **7/0**、`CooldownCost.Run` **33/0**（含跨帧尾段）、`.Reject` **7/0**、
      `State.Reject` **6/1**、`Slice.Run` **3/3**。
      **★ 无头基线口径（MUST 读，防止把既有边界读成回归）**：`State.*` / `Slice.*` 三条**本来就是 PIE-only**——
      内容资产在无头下定义库 **0 条** ⇒ `State.Run` = **4/11**、`State.Reject` = **6/1**、`Slice.Run` = **3/3**
      是**既有边界**（先例 `EVID-2026-10-11-skill-layer-e2e` §5.2 逐字记录同值），**非本变更引入**。
      本轮**未**在编辑器 PIE 下重跑这三条（如实登记的边界）
- [x] 3.5 **ARO 阴性对照 MUST 复现**——**已复现，读数与先例逐字吻合**：
      把 `TTcsInstancePool::ForEachForGC` 临时改为 `if (false)` 并重编 ⇒
      `SkillCast.Run` 由 **26/0 变 24/2**、`C15 ARO 读数` 由 `存活=是` 变 **`存活=否`**；
      **还原后复跑回到 26/0、`存活=是`**。（先例 `EVID-2026-10-08-skill-cast-runtime`：26/0 → 24/2、`存活=否`）
      ⇒ **该检查确实能检出 ARO 失效，不是恒过的假阳性**；且它同时证明**共享层的 `ForEachForGC`
      真的被两域门面的 ARO 走到**（状态侧原为手写桶、无线程断言，本轮换成池后由本对照组覆盖）
- [x] 3.6 **旧句柄跨世界失配**读数（建实例 → 世界反初始化 → 旧句柄应全部失配）——
      由 `State.Run` 的 **S1（跨 PIE 无残留）** 与 `SkillCast.Reject` 的跨世界读数覆盖；
      **如实边界**：状态侧 S1 在无头下基于 0 实例，**未**在 PIE 下复取
- [x] 3.7 **广播内递归移除被静默拒绝**读数（先例：状态侧 `Removed` 递归移除静默拒绝）——
      **★ 已取到行为级读数（2026-10-14 补跑，先前的"归 PIE 复核面"结论已撤回）**：
      `Tcs.Test.SkillCast.Phase` 的 **`P6 重入终结`** 三项全 PASS ——
      「首次返回真、**二次返回假且静默**（守卫命中、零 ensure）」/「`Interrupted` 首次后=1、**二次后仍=1**」
      /读数行「首次=真 二次=假 `Interrupted` 1→1（期望 真/假/1/1）」；该命令 **`FAIL` 0 条、`ensure` 0 条**。
      ⇒ 该检查**正是设计来证伪守卫缺失的**（其源码注释原话："若本轮的重入守卫没落地，本项必失败（`Interrupted=2`）"），
      **故它是本次"守卫合为一份实现"的直接行为证据**。
      **同批复核口径（MUST 读）**：该命令的被动段计数 = `Phase` **29/0** + `PhaseDeferred` **34/0**，
      与**权威基线**（LAC 提交 `810826e` 的验收记录：`SkillCast.Phase 29/0(+PhaseDeferred 34/0)`）**逐字相同**。
      **★ 一处取证订正（防误读）**：更早的 `EVID-2026-10-10-skill-cast-phases` 记的是 `PhaseDeferred 35/0`，
      那是**该提交之前**的形态（彼时那一项走 `Counter->Check`；`810826e` 落地后改为 `ReportSkillCastProbe`、
      **不再计入 `Passed=`**）⇒ **基线的权威来源是 `810826e` 的记录，不是那份更早的证据档**
- [x] 3.8 `openspec validate --all --strict --no-interactive` 全绿（`changes/` 仍为本提案**活跃**——
      **归档属 Phase 3，须在交付验证之后另行执行**，见 §4）

## 4. 归档

- [x] 4.1 `openspec archive merge-entity-runtime-mechanisms --yes`
      —— **已执行（2026-10-14）**：归档为 `changes/archive/**2026-10-14**-merge-entity-runtime-mechanisms`。
      归档器应用 `+ 5 added / ~ 6 modified / - 0 / → 0`，与提案逐条吻合；
      活跃提案 **1 → 0**；能力数 **42 → 43**（新增 `entity-runtime-mechanism`）。
      **★ 一处手工补正（归档器的已知语义边界：只替换「需求」、不碰 `## Purpose`）**：
      新能力的 `## Purpose` 原为占位 `TBD - created by archiving …`（`MEM-20261002-01` 记录的形态，
      **不补则 `validate` 永久红**）⇒ 已手写正式 Purpose（含四条边界说明）。
- [x] 4.2 **★ 本条的判据已订正（2026-10-14 实施后）**：初稿写"复核 `openspec/specs/` 内
      **旧类型名零命中**"——**该判据不成立**，因为实施时**刻意保留了这些名字**：
      `FStateBucket` / `FSkillBucket` 改为共享模板的**别名**（域侧大量调用点无需改动），
      `FTcsStateStackDecisionFragment` **保留为统一接口的状态侧薄型**（它是 Def 资产作者面类型）。
      ⇒ 正确判据 = **"这些名字不再是各自独立的实现"**：
      ① `TcsStateRegistry.h` / `TcsSkillRegistry.h` 内**零实现**（只剩 `using` 别名与文档）；
      ② 宿主的 `FTcsDevStackDecisionAlwaysNew` 已随接口化改签名（不再是旧形参）；
      ③ 全库 grep 确认**不存在第二份**槽位 / 代际 / 空闲栈逻辑
      （判据 = `SlotGenerations` / `FreeSlots` 只在共享层模板里出现）。
      ——**MUST NOT** 按"名字零命中"去删这些别名（那会平白改动域侧全部调用点）。
      **注意**：`changes/archive/` 内的历史提案 **MUST NOT** 改写（现场记录）
      —— **复核结果（2026-10-14）**：`specs/` 内**精确词边界**扫描，`FSkillBucket` / `FTcsSkillRegistry`
      = **0**；`FStateBucket` / `FTcsStateRegistry` 各 **1**，**均在"它取代了此前…"的历史对照句里**
      （**合法**，不是陈旧引用）；`FTcsStateStackDecisionFragment` 4 处**均属刻意保留**
      （接口化后它是状态侧薄型）。`Unit` / `单位` 在 `specs/` 内**零残留**。
      **★ 同批抓到并订正一处规格过期（真实的实质错误）**：`skill-registry` 的账本需求原写
      「**MUST NOT 复用 `TTcsInstancePool`**（该池是『一池 = 一个句柄空间』，与 per-entity 桶的形状不符）」——
      该禁令**依据不足**（池是**每实体各持一个实例**，不是全局一池）；真约束是"代际 MUST 进程唯一"
      ⇒ 已改述为"用池 + MUST 注入进程级发号器"。
- [x] 4.3 **能力数回写**：`INDEX` §3 的「**42 条能力规格**」→ **43**（实测 `openspec/specs` 目录数 = 43）。
      **`openspec/project.md` 无需改**（该文件不声称能力数，实测其"能力"字样均指 tag 规则迁出，
      与计数无关）。**权威口径**：`validate --all` 条目数 **43** = 能力 **43** + 活跃提案 **0**
      （**MUST NOT 用 `--all` 条目数当能力数**——口径见 `MEM-20261010-07`）。

> **★ 该缺口已于 2026-10-14 闭合（MUST 读，本节保留以便追溯）**：`state-stacking-policies`
> 的「Custom 决策 Fragment」需求**明文定义了** `FTcsStateStackDecisionFragment` 的形状
> （三方法 + 中性默认 + 载体纪律），而本变更把它**上提为共享层的统一决策接口**。
> 初稿**有意不含**该 delta，因为"接口化后该 Fragment 是保留、改名、还是被取代"
> 取决于 3.2 的两个裁决点。
> ⇒ **3.2 两个裁决点都取默认档**（`STAT-7` 层模型不改 / `STAT-6` 不补）⇒ 该 Fragment **保留**、
> 形状**逐字保留**、只把三方法契约**上提 + 继承**。
> **该 MODIFIED delta 已补出**（`specs/state-stacking-policies/spec.md`，见 2.4），
> **落地也已同批完成**（`FTcsStateStackDecisionFragment : public FTcsEntityAdmission`，
> 宿主样本同步改签名）⇒ **缺口已闭合，不再是缺口**。
> ⇒ **实施到该处时 MUST 先裁 3.2，再同批补 `state-stacking-policies` 的 MODIFIED delta。**
