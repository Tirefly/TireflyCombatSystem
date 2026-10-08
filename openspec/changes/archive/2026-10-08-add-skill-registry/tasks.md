# Tasks: add-skill-registry

> 对应计划 `PLN-R6` Task 2（`Documents/combat-system-design/plans/plan-r6-skill-layer.md`，Step 0~6）。
> **本清单是 Task 2 的验收面**：每一项 MUST 在实施后回读本文件逐条勾选
> （`MEM-20260916-01` 第 3 条的教训：**"计划写了"不等于"实施时照做"**）。
>
> **勾选状态口径**：`[x]` = 代码/文档已落地**并有本轮实测取证**；`[ ]` = 尚未取得证据。
> 6.x 依赖**人工在 PIE 内执行宿主装置**，与 Task 1 的 7.5 / 7.6 同款——未实测前 MUST 保持未勾选。

## 1. 规格与账本前置（Step 0）

- [x] 1.1 `UTcsSkillSubsystem` 增登记口 `RegisterSkillDef(FGameplayTag DefTag, const FTcsSkillDefData& Def) -> bool`——**签名与语义逐字照** `UTcsStateSubsystem::RegisterStateDef`（身份由调用方显式传入；登记表 `TUniquePtr` 自持副本使解析指针地址稳定；拒绝面 = `DefTag` 无效 / 同 id 重复 → Error + false，**不静默覆写**）
- [x] 1.2 配套 `UnregisterSkillDef` / `GetRegisteredSkillDef(FGameplayTag) -> const FTcsSkillDefData*` / `GetRegisteredSkillDefCount()`；未登记返回 `nullptr`（正常查询路径，**不 ensure**）；解析返回**裸 struct 指针**（UHT 不支持 struct 指针作反射返回）
- [x] 1.3 `UTcsDefinitionSubsystem::SeedWorld` 增"技能定义 → 技能门面"一段——照状态定义那一段（`TcsDefinitionSubsystem.cpp:220-230`）**逐字同构**；装配顺序排在链 / 触发行**之后**（与状态定义同档）；`DefTag` 无效或重复（正常路径不可达）留 **Warning** 并继续，**MUST NOT** 中断装配、**MUST NOT** 用 Error
- [x] 1.4 世界装配日志行增**技能定义已装配条数**（就绪行现有四计数不变）
- [x] 1.5 **依赖方向单向**：定义库写进门面、门面 **MUST NOT** 反查定义库；**MUST NOT** 给 `TcsSkill.Build.cs` 加 `TcsIntegration`（成环）
- [x] 1.6 门面 `AddReferencedObjects` MUST 覆盖技能定义登记表（内容含 `FInstancedStruct` ⇒ 非 `UPROPERTY` 容器里的内层对象引用会被静默回收；判据 = "容器是否 GC 可见"）

## 2. 账本数据形状（Step 1 / Step 2）

- [x] 2.1 新增 `Source/TcsSkill/Public/Skill/TcsSkillEntryHandle.h`——`USTRUCT(BlueprintType) FTcsSkillEntryHandle{int32 Index = -1, int32 Generation = 0}` + `IsValid()`（判 `Index >= 0 && Generation > 0`，**MUST NOT** 判"下标非 0"）+ `operator==` + `GetTypeHash`；照 `FTcsStateHandle` 体例；**展平两字段**（内嵌模板句柄会因模板类型不可作 `UPROPERTY` 而让脚本往返得到空壳）
- [x] 2.2 新增 `Source/TcsSkill/Public/Skill/TcsCastRunHandle.h`——`USTRUCT(BlueprintType) FTcsCastRunHandle{int32 Index = -1, int32 Generation = 0}`，照 `FTcsChainRunHandle`（`TcsChainRun.h:52`）的展平形态。**本轮只交纯句柄值类型**：作为 `FTcsLearnedSkillEntry.RunHandles` 的元素，**零写入者**（写入归 Task 3）；**MUST NOT** 在本轮建 `FTcsCastRun` 结构体、池或 cast-run 注册表
- [x] 2.3 新增 `Source/TcsSkill/Public/Skill/TcsSkillRegistry.h`——`FTcsLearnedSkillEntry{FGameplayTag DefTag, int32 Level, FTcsSourceHandle LearnSource, TArray<FTcsCastRunHandle> RunHandles}`；**非反射**（无 `USTRUCT`——桶内池化纯数据，本轮零过网/脚本消费面；同 `FTcsStateInstance` 的处置与理由），**类型注释 MUST 写明该理由**并指向台账 `SCRIPT-10`
- [x] 2.4 `FTcsLearnedSkillEntry` **MUST NOT** 声明冷却轨道状态字段（`R6.5-a`）或参数修正链字段（参数链轮）——两者元素类型此刻不存在，而 `TArray<T>` 成员要求 `T` 可见 ⇒ 提前声明即**未声明标识符**
- [x] 2.5 per-unit 桶 + 账本注册表——照 `FTcsStateRegistry` / `FStateBucket` 体例（**不复用** `TTcsInstancePool`：需要"每单位一个句柄空间"）；槽位分配取进程内不复用奇数、释放 +1 置偶数；`Find` 做代际校验（失配 / 越界返回 nullptr，**不 ensure**——时序竞态语义）
- [x] 2.6 桶的遍历口 MUST 为**稳定序**（按槽位下标升序）；文档写明"遍历期间 MUST NOT 增删（需增删先收集句柄、遍历后再动）"

## 3. 门面与授予 / 撤销（Step 3）

- [x] 3.1 `UTcsSkillSubsystem : UWorldSubsystem` 骨架——`DoesSupportWorldType`（仅游戏世界）/ `Initialize` / `Deinitialize` / `static AddReferencedObjects`
- [x] 3.2 `GrantSkill(FTcsCombatEntityHandle Unit, FGameplayTag DefTag, FTcsSourceHandle Source)`——**授予前置门禁**：`DefTag` 未在本世界登记 ⇒ **拒绝** + `Warning`（**不 ensure**）；**MUST NOT** 建出取不到定义内容的条目（否则读取面全落空、失败面表现为"读数为 0"，属静默错误）
- [x] 3.3 `RevokeSkill(...)` + **按来源级联撤销**（摘该来源在该单位的全部条目）；**只持 `LearnSource` 一个句柄**——MUST NOT 照抄状态侧的 `Source` + `CascadeAnchor` 双句柄（账本条目不挂修正器/触发行，无独立级联锚点需求）
- [x] 3.4 `Deinitialize` 清空**三者**：技能定义登记表 / 账本注册表 / 注入的 `TScriptInterface`（跨世界确定性清理，旧句柄凭代际失配失效）——**Task 1 的 `UTcsDefinitionSubsystem::Deinitialize` 曾漏掉 SkillDefs 清理**，同款不重犯
- [x] 3.5 观测口：某单位账本条目数 / 跨单位合计条目数（供装置断言用；**验收读数 MUST 取"账本条目数"**，MUST NOT 取"`GrantSkill` 被调用了几次"——后者在"授予被拒但计数器照加"时仍全绿）

## 4. 账本参数读取面（Step 4）

- [x] 4.1 `GetLevel(FTcsSkillEntryHandle) -> int32`——`EffectiveLevel = clamp(0, LevelBase + Σ参数账本 Level 键修正)`；`LevelBase` 取自该条目 `DefTag` 对应的**定义内容**（经登记表解析）；钳下界到 0
- [x] 4.2 `GetNumericParam(Entry, Key, double&) -> bool` / `IsSwitchSet(Entry, Key, bool&) -> bool`——**未命中落 miss**（返回 false）；**MUST NOT** 静默返回 0 / false 当命中；两表**互不兜底**（数值参数表 ↔ 布尔开关表）
- [x] 4.3 `Level` 修正键的**写入面**归参数链轮 ⇒ 本 Step 先落**求值函数**（形参接收"带 `Level` 键的修正器列表"），接入点在参数链轮；**MUST NOT** 依赖条目记录自持修正链字段
- [x] 4.4 参数键归属 **`TcsStateParam` 根**（该根 `State` 取广义、含技能激活运行态）——**MUST NOT** 因"技能不住 `TcsState` 模块"另立根

## 5. 门禁第一道：实体可操作（Step 5）

- [x] 5.1 `Source/TcsEffect/Public/Host/TcsEntityQuery.h` 给 `ITcsEntityQuery` 加第四支能力 `virtual bool IsEntityReady(FTcsCombatEntityHandle Entity) = 0;`（**纯追加**）；注释写明与 `IsAlive` 的**正交轴**分工 + **R7 转发纪律**（世界注册表落地后 MUST 改为 `GetEntityState(handle) == Ready` 的转发，MUST NOT 成为第二真相）
- [x] 5.2 `UTcsPieEntityQuery` 实现该方法；**注释 MUST 注明"PIE 简化，真项目 MUST 分开覆写"**（其默认实现与 `IsAlive` 重合）
- [x] 5.3 `IsAlive` **原封不动**（声明 / 定义 / override 三处均不改签名与语义）——**实测本轮改动面 = 2 个文件**，`Source/TcsDev/` **零改动**（`TcsDev` 两处用的都是 `UTcsPieEntityQuery`，无自己的实现）
- [x] 5.4 `UTcsSkillSubsystem::SetEntityQuery(TScriptInterface<ITcsEntityQuery>)` 注入点 + `UPROPERTY` 持有；**未注入 = 配置状态不是错误** ⇒ 门禁**降级为"只判句柄有效"**且**通过**（MUST NOT 因未注入把一切施法拦死、MUST NOT ensure）
- [x] 5.5 门禁判据 MUST 为 `IsEntityReady`，**MUST NOT** 用 `IsAlive` 代替；**MUST NOT** 反查 `TcsIntegration`

## 6. 编译与验收取证

- [x] 6.1 `LegendAutoChessEditor Win64 Development` 编译 **0 error / 0 warning**
- [x] 6.2 `LegendAutoChess Win64 Shipping` 编译 **0 error / 0 warning**（改动含新 `UPROPERTY` 与 `USTRUCT` ⇒ **双配置必跑**）
- [x] 6.3 `openspec validate add-skill-registry --strict --no-interactive` = **valid**
- [x] 6.4 **Step 0 读数**（缺此则 Step 0 无验收面）：① 世界装配行技能定义计数**随内容目录实际条数变化**（`0 → 1` 的唯一变量是磁盘上多了一个 `.uasset`——同 Task 1 的 7.6 手法，证明发现+装配路径真在读内容目录）；② 装配后 `GetRegisteredSkillDef(DefTag)` **命中**且内容与定义库缓存**一致**；③ **未登记就授予** ⇒ 拒绝面（`Warning`，不 ensure）且**账本条目数不变**（阴性对照）
  - **三读数齐备（第三轮，2026-10-08）**：① **达标**——三次 PIE 会话的计数为 `1`（`:2335`）→ `1`（`:2508`）→ **`2`（`:2673`）**，`1 → 2` 的**唯一变量是磁盘上多了一个 `.uasset`**（`DA_SkillDef_E2E_Second`，mtime `16:14:38`，落在第 2/3 次会话之间）；装配行同步 `技能定义 2/2 条`（`:2703`）。这是 Task 1 那条 `0 → 1` 的**同构复现**。② **达标**（`L1` + `L2`，`L2` 用 `CompareScriptStruct` 逐字段比对两处登记）。③ **达标**——`R2` 打印**有效 tag 名**（`定义 SkillDef.Check.Unregistered 未在本世界登记`）而**不是** `None`，与 `R3`（`DefTag 无效（空身份）`）**路径分离**。
  - **★ 6.4-③ 的一处真实缺陷（第一轮抓到、第二轮复验、第三轮持续有效）**：装置 `R2` 初版用的 `SkillDef.Check.NotRegisteredAtAll` **未在 `ini` 声明** ⇒ `RequestGameplayTag(..., false)` 返回**空 tag** ⇒ 与 `R3`（直接传 `FGameplayTag()`）**输入完全相同**（第一轮日志侧证：两条都打印 `定义 None`）⇒ 只验了"**空身份**被拒"，而"**合法 tag 但未登记**"（= 资产发现失败的真实情形）**未测到**。**已修**：`ini` 补声明 `SkillDef.Check.Unregistered`（有声明、无资产）+ `R2` 改用该词 + 把 `bTagIsValid` 纳入判据（否则本检查会**静默退化成 `R3` 的同义重复**）。
  - **同批修掉两处"内容脆性"（加第二个资产才暴露的自有缺陷）**：`D3` 与 `L1` 原判据都是 `== 1`——那是"当时只有一个资产"的**巧合**，而规格明确允许**多个**技能定义资产 ⇒ 加资产即**假失败**。已改为 **`>= 1`**（`L1` 另加"按 `DefTag` 可解析"，那才是硬判据）。**判据 = 内容条数不是契约，MUST NOT 绑死。**
- [x] 6.5 **账本读数**：授予 1 → 撤销 0；按来源级联**恰摘 1 条**且另一来源的条目原样保留（读数 = 2 → 1，剩余条目 `DefTag` = 另一个）
  - **按字面达标（第三轮，2026-10-08）**：补第二个验收资产（`DA_SkillDef_E2E_Second`，`DefTag = SkillDef.Check.SecondSkill`）后，**同一单位可挂两条不同 `DefTag`** ⇒ 本项**按字面**实测：`L11` 挂两条（计数 = 2、两来源互异）PASS ⇒ `L12` 按其中一个来源级联，**读数 2 → 1** 且剩余条目 = **另一个** `DefTag`（屏显 `:2807`：`摘除 1 条（期望 1），剩余 1 条且 DefTag=SkillDef.Check.SecondSkill`）PASS ⇒ `L13` 阴性对照 PASS ⇒ `L13b` 重复授予走刷新（计数仍 1、句柄不变）PASS。
  - **一处时序判据（本轮实测确认）**：`L13b`（刷新）**MUST 排在级联之后**——刷新会**换掉 `LearnSource`**（日志 `技能授予（刷新）` 即实证），排在前面会让 `L12` 用**陈旧来源** ⇒ 摘 0 条而**假失败**。⇒ "检查次序本身也是判据的一部分"。
  - **一处有价值的阴性对照**：第三轮快照里**同时存在**两次 `Registry.Run`——第一次在 `Prepare` **之前**（第二个资产尚不存在）得 **14 PASS / 3 FAIL**（`:2450`/`:2452`/`:2456`，成因是 `:2449` `定义 SkillDef.Check.SecondSkill 未在本世界登记`），末会话得 **17/0**。**这 3 条 FAIL 恰恰证明 `L11`/`L12`/`L13b` 不是"空转即过"。**
- [x] 6.6 **代际读数**：撤销后重新授予（槽位复用），用撤销前的旧句柄查询 ⇒ **被拒绝**（`Index` 可同、`Generation` 必不同）——**2026-10-08 实测 PASS**（`L14`：下标复用、代际推进、旧句柄被拒）
- [x] 6.7 **读取面读数**：`GetLevel` 按 `Level` 键修正求和且钳下界（含负值钳到 0 的一路）；未命中键落 **miss**（与"命中且为 0"可区分）；布尔开关与数值参数**分属两表**（互不兜底）——**2026-10-08 实测 PASS**（`L7` 命中 17 / `L8` 未配键落 miss / `L9` 两表互不兜底 / `L10`：基础 1、+2→3、−9999→**0 钳下界**）
- [x] 6.8 **门禁第一道正反两路**：未注入 ⇒ **通过**（降级，无红字）；注入后宿主判 `IsEntityReady == false` ⇒ **拒绝**（具名原因，且不再走后续门禁）；同句柄 `IsAlive == true` 也 **MUST NOT** 放行——**2026-10-08 实测 PASS**（`G1` 未注入降级放行 / `G2` `{Alive=真, Ready=假}` ⇒ 拒绝 / `G3` `{Alive=假, Ready=真}` ⇒ 放行 / `G4` 无效句柄被拒）。**★ 判据可证伪性**：`UTcsPieEntityQuery` 下 `IsAlive` 与 `IsEntityReady` **必然重合**（都是"句柄在映射里且 Actor 有效"）⇒ 拿它测门禁**分不出**"用了 Ready"与"误用了 Alive"；本轮用**发散探针** `UTcsDevSkillGateQuery` 让两问**答案相反**，`G2`/`G3` 因此互为反向对照（详见证据 §2）
- [x] 6.9 验收报告 MUST 附**预期红字清单**（"零红字" = 零**意外**红字）；否定路径 MUST 带**阴性对照**（证明红字来自拒绝、而非任何异常）；故意的拒绝输出独立成 `.Reject` 命令（常规命令 MUST 零红字）——**2026-10-08 两轮均 PASS**：`Run` 段与 `Gate` 段**零红字**；红字**全部落在 `Reject` 段**且逐条对应具名拒绝（第二轮 6 条：`:2498` 内容缺口 / `:2500` 空身份 / `:2502`+`:2506` 未注册单位 / `:2504` 无效句柄 / `:2509` 重复登记）；**两轮均零 `ensure`、零断言失败、零 Fatal**。证据 = `EVID-2026-10-08-skill-registry`
- [x] 6.10 `Source/TcsState/**` **零改动**全库反查（本轮不动状态层）

## 7. 人工验证点（人工在编辑器内执行；装置随本轮交付）

宿主装置（LAC 仓 `Source/TcsDev/`，**开发用、非正式内容**）提供：

1. **先关编辑器 → 完整双配置 UBT → 重开编辑器 → 跑 PIE**（**MUST NOT 用 Live Coding**——"现象只在重建二进制后消失"本身就是陈旧 DLL 的判据）；
2. 跑账本主命令（授予 → 查询 → 撤销 → 按来源级联 → 代际 → 读取面）——**常规命令 MUST 零红字**；
3. 跑独立的 `.Reject` 命令（未登记授予 / 未注入门禁降级对照 / 注入后拒绝）——预期红字**逐条列出**；
4. 读数以 **UTF-8 日志文件**为准（控制台回显为 ANSI，中文显示为 `?`）：`Saved/Logs/LegendAutoChess.log`。

> **6.4~6.10 的取证口径**：本清单交付的是**代码与装置**，上述读数要求**人工在编辑器内执行后**的结果 ——
> 未实测前 MUST 保持未勾选（同 Task 1 的 7.5 / 7.6 处置：交付当晚如实留白，2026-10-07 用户执行后按实测读数补勾）。
