# ARCH-08-skill · TcsSkill（技能与施法编排层）

- **文档 ID**：`ARCH-08-skill`
- **类型**：模块档案（审核记录 + 现状权威）
- **状态**：生效中（2026-10-17 审核完成）
- **审核对象**：`Source/TcsSkill/` —— **63 文件 / 11 052 行**（`.h` + `.cpp`；含 `TcsSkill.Build.cs` 25 行 ⇒ 64 文件 / 11 077 行）。**全仓最大的模块**，比第二大的 `TcsEffect`（7 115）多 55%，是 `TcsTargeting`（1 144）的近十倍。
- **权威层**：`openspec/specs/` 下 **8 个 `skill-*` 能力 / 1 769 行 / 39 Requirement / 154 Scenario**
- **旧文档**：`Documents/combat-system-design/spec/05-module-skill.md`（`SPEC-04-skill`，**498 行**，ACTIVE）

---

## §1 模块定位与边界

### 1.1 一句话职责【实测】

> **`TcsSkill` 是"技能放得出来"这件事的全部记账与裁决：它持"谁学了什么"的账本、裁决"这次能不能放"的七道门、管一次施法的生与死、并维护冷却与消耗这两本横向账。**

它不是执行者——链步骤、伤害、状态施加全在别的模块；它只负责**把该做的按顺序编排起来，并在每一步之后重新确认手里的指针还活着**。

### 1.2 文件组与职责【实测】

| 文件组 | 文件数 | 行数量级 | 职责 |
|---|---|---|---|
| `Public/Def/` + `Private/Def/` | 10 | ~1 500 | Def 数据形状（`FTcsSkillDefData` / `FTcsPhaseSpan` / `FTcsBoolSwitchRow` / 冷却与消耗类型 / 三个施法枚举）、资产类 `UTcsSkillDef` + 作者期校验 |
| `Public/Skill/` + `Private/Skill/` | 41 | ~9 000 | 引擎函数族：账本（`FTcsSkillOps`）、施法（`FTcsCastOps`）、参数链（`FTcsParamChainOps`）、冷却（`FTcsCooldownOps`）、消耗（`FTcsCostOps`）+ 五个共享/工具头 |
| `Public/TcsSkillSubsystem.h` + `Private/TcsSkillSubsystem*.cpp` | 8 | ~1 300 | 世界子系统门面（**全部薄壳一行转发**）+ 七个分片实现 |
| `Public/Param/` + `Private/Param/` | 2 | ~70 | 两个框架契约键（`CooldownPct` / `CostAmount`） |
| `TcsSkillModule.h` / `.cpp` | 2 | 20 | 模块启动样板 |

**分工判据（本模块反复出现的一句话）**：**"桶只存数据与索引，操作全在引擎函数"**。门面 `UTcsSkillSubsystem` 的每个业务方法都是**一行转发**（`TcsSkillSubsystem_Registry.cpp:12-86` 共 8 个方法，全部 `return FTcsXxxOps::Yyy(*this, ...);`）。这不是形式主义——它让"改流程"只碰引擎函数，而 UE 生命周期样板（`Initialize` / `Deinitialize` / `DoesSupportWorldType` / `AddReferencedObjects`）永不受牵连。

### 1.3 本模块最反常的一点：**它是全仓唯一"规格与代码各错一半、而且错的方向也相反"的模块**【实测】

前六篇各自有一个主方向：

| 篇 | 主方向 |
|---|---|
| `ARCH-04` `TcsAttribute` | 规格更严、代码更松（G-10） |
| `ARCH-05` `TcsState` | 代码搬走、引用滞留（G-12） |
| `ARCH-06` `TcsEffect` | 规格与代码各错一半（纯代码 5 / 纯规格 4） |
| `ARCH-07` `TcsBuff` | 纯规格 5 / 纯代码 1（最偏斜） |
| **`ARCH-08` `TcsSkill`** | **同一件事上，规格比代码旧，而另一件事上，代码比规格松** |

具体到本模块，**两条相反的偏差同时存在于同一个函数里**：

- **规格比代码旧**：`skill-cast-runtime:162` 说 `MainChainStart` 的 `OnPhaseEnter` / `OnCastCompleted` / `Custom` 三档"**本轮未实现、命中时必须报 Warning**"，而代码 `TcsCastOps_Chain.cpp:140-191` **四档全部实现**（只有 `MCS_Custom` 报 Warning）。规格停在 Task 3 时态，代码在 Task 5 就补上了，**此后没有任何一轮回头改规格**。
- **代码比规格松**：`skill-cast-runtime:100` 的关系门注记要求"三种原因**各自返回具名枚举值**"，而代码 `TcsCastOps.cpp:206-209` **只返回一个** `SAR_RelationBlocked`，三种原因只体现在三条不同的 `Log` 文本里。

⇒ 读者无法用一句话概括"技能层文档准不准"。**必须逐条查**。这也是本档案第一次遇到"主方向本身是分裂的"。

### 1.4 依赖面与"唯一一条代码级状态侧依赖"【实测】

`TcsSkill.Build.cs:12-23` 的依赖 = `Core` / `CoreUObject` / `Engine` / `GameplayTags` / `TcsCore` / `TcsNotation` / `TcsAttribute` / `TcsEffect` / `TcsState`。

**`TcsSkill` MUST NOT 依赖 `TcsBuff`**，而这条禁令的落地形态是一处**具体的代码事实**，值得单独记：

> 技能侧要判关系门，就必须读**状态域**的在跑实例（它们的类别词 + 它们的 `TagsToBlock`）。而 `TcsBuff` 是技能侧的兄弟模块、不是上游。⇒ 这个读口 MUST 经**共同下层** `TcsState` 中转。

落地 = `ITcsStateLiveQuery`（`TcsState/Public/State/TcsStateLiveQuery.h`，2026-10-16 `R5.5-e1` 立）。**它的类注释自己写明了为什么由域侧注册而不是宿主注入**："若要求宿主接线，则每个宿主都要接一次，而**漏接的表现是关系门静默失效**——本仓最忌的失效形态"。

**依赖图实测的两条边**：

| 边 | 方向 | 证据 |
|---|---|---|
| `TcsSkill` → `TcsState`（参数表读口） | 技能 → 共享层 | `TcsCastOps.cpp:547-552` 与 `TcsCastOps_Phase.cpp:44-49` 各一处 `GetParamTableReader()`，**仅此两处** |
| `TcsSkill` → `TcsState`（关系门查询契约） | 技能 → 共享层 | `TcsCastOps.cpp:200-204` 取 `GetLiveQuery()` |
| `TcsSkill` → `TcsBuff` | **禁止** | `Build.cs` 无该模块；`Source/TcsSkill/` 内 `grep TcsBuff` 命中 **0** |

**但有一处值得注意的"反向"事实**：`TcsSkill` 的**头文件**在插件内被另一个模块 include 的只有 **1 处**（`TcsIntegration/Private/TcsDefinitionSubsystem.cpp`，为登记技能定义）。⇒ 本模块的**编译期耦合面极窄**，而它的**运行期影响面极宽**（见 §4）。这个不对称本身就是本模块的一个特征。

---

## §2 已落地

### 2.1 七道门禁：**评估序与枚举值序刻意解耦**，且第 ⑥ 道被"提前"了【实测】

`TryActivate` 的代码实际评估顺序与规格声明的编号**不是同一个序**：

| 代码评估序 | 门禁 | 枚举值 | 代码锚点 |
|---|---|---|---|
| 1 | ① 实体 Ready | `SAR_EntityNotReady = 0` | `TcsCastOps.cpp:141-146` |
| 2 | ② 已学 | `SAR_NotLearned = 1` | `:149-160` |
| **3** | **⑥ 定义可解析** | `SAR_DefInvalid = 5` | `:166-173` |
| **4** | **⑦ 关系门** | `SAR_RelationBlocked = 7` | `:188-210` |
| 5 | ③ 冷却 | `SAR_OnCooldown = 2` | `:223-233` |
| 6 | ④ Instancing | `SAR_AlreadyActive = 3` | `:235-351` |
| 7 | ⑤ CanAfford | `SAR_CannotAfford = 4` | `:353-372` |

**第 ⑥ 道为什么被提前到第 3 位**（`TcsCastOps.cpp:162-165` 的注释）："第 ③④⑤ 道**均需读取定义内容**（冷却轨道 / 实例化与顶替位 / Cost 策略都是 Def 字段）⇒ 定义取不到时 MUST 立即返回 `SAR_DefInvalid` 并**不评估它们**；否则它们会拿到'取不到定义'的空数据并返回**看似合理但错误**的枚举值（假读数）。"

**这条判据是对的，且是本模块最值得学的一处设计**：它区分了"**门禁的编号**"与"**门禁的评估可达性**"。规格 `skill-cast-runtime:88` 也写明了这一点（"本条约束的是**评估可达性**，不是重排门禁"），**规格与代码在这一处完全一致**。

**枚举值序与评估序解耦的第二处**（`TcsSkillActivateResult.h:31-33`）：门禁⑦ 的枚举值取 `7` 而非 `6`，因为 `SAR_Success = 6` 是宿主 UI 分支的既有读数，**MUST NOT 改动**。⇒ 枚举值 `6` 与 `7` 在声明顺序上是倒置的，这是**有意为之**。

### 2.2 四路终结：**一个例程、六段顺序、三个"重新解析"点**【实测】

`FTcsCastOps::FinishSkillInstance`（`TcsCastOps.cpp:678-923`，**246 行**）是四路终结（完成 / 瞬发当刻 / 打断 / 顶替）的唯一汇聚点。它的结构是**六段顺序 + 一个守卫**：

| 段 | 动作 | 锚点 | 为什么必须在这个位置 |
|---|---|---|---|
| 守卫 | 重入窗口检测（`FinishBroadcastGuard`） | `:706-712` | 广播同步派发 ⇒ 订阅者可重入同一句柄 |
| ⓪ | **完成档起主链** | `:735-738` | 必须**排在回收之前**——否则新起的链挂的条目会被自己的终结例程当场摘掉 |
| ① | 取消时段到期锚点 | `:743-746` | 取消是第一道防线，代际校验是第二道 |
| ② | 按 `CascadeAnchor` 回收 **M2 属性账本** | `:774-795` | 两本账、同一个来源句柄 |
| ③ | 按 `CascadeAnchor` 回收 **M5 参数链账本** | `:804` | 同上；只摘其一 ⇒ `CHAIN-7` 的"常驻停在 X−5" |
| ④ | 摘账本 `RunHandles` | `:820-823` | 与激活期回填成对 |
| ③′ | **冷却 / Cost 的 Timing 分派** | `:845-866` | 必须**排在回收之后**（理由见下） |
| ⑤ | 广播终止事件 | `:880-893` | **MUST 排在归还槽位之前**（载荷里的 run 句柄在回调内仍须有效） |
| ⑥ | 归还池槽位 | `:901-911` | 归还前复核句柄仍有效（防双次 `Free` 的 `ensureMsgf`） |

**⓪ 与 ③′ 的次序相反，判据相同**——这是本模块最精彩的一段注释（`:831-837`）：

> - ⓪ 段（完成档起链）**必须排在回收之前**——否则新起的链挂的条目会**当场被自己的终结例程摘掉**（同一个 `CascadeAnchor`），表现为"配了完成档的技能永远打不出效果"；
> - 本段（冷却 / Cost 触发）**必须排在回收之后**——因为回收会**广播属性变更事件**，排在回收之前的话，回收尚未完成就可能提交消耗。
> - ⇒ **两处次序相反、判据相同：别让自己的动作被自己的清理吃掉。**

**"重新解析"出现了三次**（`:497-508` 起冷广播后、`:856-858` 冷却/Cost 广播后、`:818` 回收广播后），每次注释都写明"任何可能广播的调用之后，手里的指针都可能失效"。⇒ **这不是防御性编程，是本模块的一条硬纪律**，且在 §5 D-1 里会看到它**执行得不完整**。

### 2.3 时段推进：**判据取"下标落在表内"，不取"表是否为空"**【实测】

`EnterPhase`（`TcsCastOps_Phase.cpp:145-237`）的待推进判据是 `Def->Phases.IsValidIndex(Run->PhaseIndex)`（`:172`），**不是** `Phases.IsEmpty()`。注释写明了理由（`:169-171`）：

> 那会让"配了时段但已走到末尾"与"空时段表"走两条不同的路（同一件事两个入口）。瞬发（空表）因此**不需要分支**：它天然产生"下标 0 不落在空表内"这一状态，直接返回 false。

**这条判据的收益是结构性的**：瞬发不需要单独的分支代码。激活流程末尾（`TcsCastOps.cpp:632-640`）只是 `if (!EnterPhase(...)) { FinishSkillInstance(..., ECFR_Completed); }`——**一个 `if` 同时覆盖"瞬发"与"时钟不可得"两条路径**。

**`AdvancePhase` 的第一条语句是复位自己的锚点**（`TcsCastOps_Phase.cpp:251`）：`Run->ExpiryEntry = FTcsTimeEntryHandle();`。这是防 `ensure` 的关键——`CancelExpiry` 对悬空句柄是 `ensureMsgf`，而"到期回调正在跑"那一刻它自己的条目**仍然有效**（`FireDueItem` 在回调返回之后才释放）。复位后 `IsValid()` 为假，后续清理路径天然跳过。

**最后一段到期时 `PhaseIndex` 停在最后一个有效下标**，**不前进到 `Num`**（`:271-281` 的注释）："那会让它落在'已越界'的状态上，而本函数的越界分支就成了**正常路径**——'正常路径报日志'会让日志失去信号意义，也让 `PhaseTable` 档的'无当前时段'降级在本该有时段时被误触发。"

### 2.4 查询契约三档：**一个函数、两处具名降级、零副作用**【实测】

`DispatchCastQueryColumn`（`TcsCastOps_Query.cpp:35-78`）用**成员指针**把"读哪一列"参数化，使 `CanBeCanceled` 与 `CanMoveNow` 共用同一份分派：

```cpp
bool DispatchCastQueryColumn(
    const FTcsSkillDefData& Def, const FTcsSkillInstance& Run,
    const TCHAR* ColumnName,
    bool FTcsSkillDefData::* DefSwitch,
    bool FTcsPhaseSpan::* PhaseSwitch)
```

**为什么必须共用一份**（`:17-20` 注释）："两列的差别**只是读哪个字段**——分派规则、两处具名降级、回退处置**逐字相同**。若各写一遍，改动一处必然漏另一处（例如将来给 `Custom` 档补上求值时只改了一条）。"

**两处具名降级**：
- `CQM_PhaseTable` 档**无当前时段** ⇒ `Log` 级 + 回退 `DefSwitches`（`:49-57`）。**MUST NOT** 越界读；**MUST NOT** 返回无条件 `false`（"那会让瞬发技能变得不可打断"）。
- `CQM_Custom` 档 ⇒ `Warning` 级（含档名）+ 回退 `DefSwitches`（`:62-71`）。**回退是本轮明确选定的行为，不是漏做**。

**降级级别不同的判据**（`:64-65`）："这是**宿主配了但框架没做**，与'瞬发无时段'那种正常路径不同级"。

### 2.5 参数链：**双形状 + 单一折叠调用点 + 竞争组读侧选优**【实测】

**双形状**（`TcsNumericParamModifier.h`）：
- 定义侧 `FTcsNumericParamModifier`（`USTRUCT(BlueprintType)`，7 字段，含 `FTcsParamValue Operand` ⇒ 可持对象引用）
- 账本侧 `FTcsNumericParamModInstance`（**纯 C++ struct，无 `USTRUCT`**，7 字段，`double ResolvedValue` ⇒ **物理上不可能**持对象引用）

**为什么必须双形状**（`TcsNumericParamModifier.h:43-49`）：账本条目住 `TArray<FTcsLearnedSkillEntry>`——非 `UPROPERTY` 容器、元素还是非反射 struct ⇒ GC 看不见它。若账本侧持定义侧形状，那些对象会被**静默回收**（症状 = 参数链里的源变成空引用，**不崩溃**）。

**`Source` 只住账本侧**（`TcsNumericParamModifier.h:51-64`，2026-10-09 用户质疑后订正）：三条判据 —— ① `FTcsSourceHandle` 是非反射纯 C++ struct ⇒ 不能作 `UPROPERTY` ⇒ 定义侧的 `Source` 是**假字段**；② 定义侧物化路径根本不读它；③ **"源"从来不是"这一行"的属性**——同一个 `ParamChainRows[0]`，甲玩家靠装备给、乙玩家靠天赋给。

**单一折叠调用点**（`TcsParamChain_Evaluate.cpp:67-99`）：`FoldParamChainInstances` 是全模块**唯一**的 `FoldTcsAttributeBands` 调用点（实测：`Source/TcsSkill/` 内该符号只在 `:98` 被**调用**一次，其余 8 处命中全是注释）。普通参数键与特殊键 `Level` **共用它**。

**竞争组是读侧选优，不是休眠池**（`:25-56`）：`IsWinningParamChainCompetitor` 在求值时遍历同组找最大者 ⇒ 最大者撤销后次大者**自动递补**，不需要任何唤醒调用。

### 2.6 冷却：**三处"必须先撤锚点再丢弃"的落点 + 组槽住实体**【实测】

`FTcsCooldownOps`（`TcsCooldownOps.cpp`，**1 021 行**）里同一条纪律出现在**三个**位置：

| 位置 | 场景 | 锚点 |
|---|---|---|
| `:162-171` | `StartCooldownOnEntry` 重建轨道数组前 | 撤旧锚点再 `Reset()` |
| `:746-753` | `ClearTrack` 组轨分支 | 判有效 → 撤锚点 → 整体复位 |
| `:788-793` | `ClearTrack` 自身轨分支 | 同上 |

**不撤的后果**（`:155-159` 注释）："`Reset()` 会把旧句柄**直接丢掉**，而每个自身轨都持一枚到期堆条目 ⇒ 不撤就重建 = 那些条目**失去唯一的取消锚点**、只能等各自到期后空跑一次回调（回调里 `EntryHandle`/`TrackIndex` 仍能命中，于是它会**清掉新起的冷**——症状极具误导性：**'刚起冷就立刻被清'**）。"

**组槽住实体而非条目**（`TcsCooldownState.h:52-61`）：`CooldownGroups` 是 `TMap<FTcsCombatEntityHandle, TMap<FGameplayTag, FTcsCooldownGroupState>>`。**为什么两层而不是平表**（`TcsSkillSubsystem.h:738-746`）："公共冷却的作用域是**实体**……平表会让 A 实体的冷却拦住 B 实体的技能（跨实体串味）。"

**组槽刻意不随条目撤销而摘**（`TcsCooldownOps.cpp:826-829`）："替它解除会**替同组的其它技能**解除公共冷却（症状：撤销 A 之后同组的 B 立刻能放——而 GCD 本该还在）。"

### 2.7 消耗：**策略模式 + 三层分离 + "未钳基数"出参**【实测】

**三层结构**（`TcsCostTypes.h` + `TcsCostOps.cpp`）：

| 层 | 载体 | 判据 |
|---|---|---|
| 时机 | `ETcsSkillCostTiming`（四档，`ECT_None` 默认） | "不消耗"的唯一入口；框架只认识"施法事务自身的边界" |
| 策略 | `FTcsSkillCostPolicy`（`USTRUCT` 虚分派，中性默认 + `meta=(Hidden)`） | 内建与宿主自定义**同一套分发** |
| 脚本面 | `ITcsSkillCostPolicyHost` + `FTcsSkillCostPolicy_HostDelegate` | `USTRUCT` 策略对脚本物理不可达（无 vtable） |

**`BlueprintType` 是必需项、不是装饰**（`TcsCostTypes.h:223-241`，2026-10-13 实测抓出的缺陷）：内建策略原先只有裸 `USTRUCT()`，结果 Python 侧 `hasattr(unreal, "TcsSkillCostPolicy_ResourceAttr")` 为 **False**，而姊妹类 `_HostDelegate`（带 `BlueprintType`）**在**。判据 = `IsScriptExposedStruct`（`PyGenUtil.cpp:1560-1581`）**沿父链**逐个查该元数据。**"编辑器里选得到"推不出"脚本里也配得了"**——picker 的 `FInstancedStructFilter::IsStructAllowed` **不读** `BlueprintType`。

**"未钳基数"出参是本模块最好的一处设计**（`TcsCostOps.cpp:76-91`）：初版把"钳 0"当成唯一收口 ⇒ 把**负基数**（配置错误、漏洞形态）与**减免过度**（合法的"免费"）压成同一个值 ⇒ 前者被静默抹成 0、其守卫永不触发。修法 = `ResolveEffectiveAmount` 增 `OutBaseAmount` 出参。**该缺陷编译与代码阅读都过，只有行为级取证才抓得到**。

**扣减走 `SetBaseValue` 直写、不走 `ApplyModifier`**（`TcsCostOps.cpp:370-386`，判据三条）：① 语义（`SetBaseValue` = 改写基值，正是"花掉了"）；② 来源锚点矛盾（`ApplyModifier` 必须给 `Source`，而施法一终结 ⇒ 级联摘除 ⇒ **扣掉的资源回来了**）；③ `CHAIN-8` 已立"修正器层效力不等同于直写基值"（会被任一 `TAO_Override` 整体抹掉）。

### 2.8 反射面：**门面零 `UFUNCTION`，但模块内有两张 `UINTERFACE`**【实测】

| 位置 | `UFUNCTION(` 实数 | 说明 |
|---|---|---|
| `TcsSkillSubsystem.h` | **0** | 34 个公开方法全部无反射；头注释逐处写明"本轮不加 `UFUNCTION`——脚本读写面归台账 `SCRIPT-10`" |
| `TcsCastStateQuery.h` | 2 | `ITcsCastStateQuery::CanBeCanceled` / `CanMoveNow`（`BlueprintNativeEvent`） |
| `TcsCostPolicyHost.h` | 2 | `ITcsSkillCostPolicyHost::CanAfford` / `CommitCost`（`BlueprintNativeEvent`） |

**"门面零反射"这个决定有据可查**，且**有一处明确的反驳记录**（`TcsSkillSubsystem.h:521-528`）：

> `PLN-R6.5-1` §2.4 的签名表在这些口上写了 `UFUNCTION()`，**本轮刻意不加**——判据：本类**今天零 `UFUNCTION`**……且账本与施法面的脚本读写面整体归台账 `SCRIPT-10`。单独给冷却加宏会造出"技能面里只有冷却能脚本调"的**孤岛**。

⇒ 这是**"部分反射比零反射更坏"**的一个实例，且它与 `ARCH-07` 的 G-20（"决定不做"无处可写）**形成对照**：这里至少写在头注释里了，只是**规格里没有**（见 §5 D-6）。

### 2.9 链重定向：**三级解析 + 优先级与后挂者双判据**【实测】

`ResolveCastChainId`（`TcsCastOps_Chain.cpp:19-47`）= 全局定义 → 数据栈 → Custom 契约。

**数据栈的裁决**（`TcsChainRedirect.cpp:94-106`）：`Priority` 大者胜、打平取后挂者。实现用 `>=` 而非 `>`，因为 `Records` 是挂载序数组、`Best` 总是更早挂的那条 ⇒ `>=` 即"取后挂者"。**注释把两个判据的作用都写清了**（`:96-102`）。

**起链结果的读数取 `ActiveChainId` 而非 `ChainRunHandle`**（`TcsCastOps_Chain.cpp:107-117`）："链运行态不持链身份、`FindChain` 也只答'某条链是否登记' ⇒ 从 `ChainRunHandle` **反查不到**这次起的是哪条链。若装置改取'解析函数返回了什么'，那是在测**解析**而不是测**起链**——一个'解析对了但起链仍用旧 id'的实现会全绿。"

---

## §3 对外契约

### 3.1 稳定契约（其它模块可以放心依赖）【实测】

| 契约 | 载体 | 消费者（实测） |
|---|---|---|
| 定义登记口 `RegisterSkillDef` / `UnregisterSkillDef` / `GetRegisteredSkillDef` | `UTcsSkillSubsystem` | `TcsDefinitionSubsystem.cpp`（唯一插件内消费者） |
| 两个框架契约键 | `Tag_TcsSkillParam_CooldownPct` / `Tag_TcsSkillParam_CostAmount`（`TCSSKILL_API`） | 宿主 ini / 装置 |
| 七枚事件词 | `TcsEvent.Cast.{Started,PhaseChanged,Completed,Interrupted,CooldownStarted,CooldownUpdated,CooldownEnded}` + `CostApplied`（共 **8** 枚） | 宿主订阅方 |
| 两张宿主插槽 | `ITcsCastStateQuery` / `ITcsSkillCostPolicyHost` | 宿主 C++ / 蓝图 / 脚本 |
| 关系门所需的下游契约 | `ITcsStateLiveQuery`（**住 `TcsState`**，本模块是消费者） | 本模块 `TcsCastOps.cpp:200-204` |

**注意：本模块"对外提供"的面很窄**——只有定义登记口 + tag 词汇 + 两个宿主接口。其余全部公开方法（`GrantSkill` / `TryActivate` / `GetNumericParam` / `StartCooldown` / `PayCost` …）**在插件内零消费者**，只有宿主验收装置（LAC `Source/TcsDev/`）在调。

### 3.2 MUST NOT 依赖（内部实现细节）【实测】

| 类型 | 理由 |
|---|---|
| `FTcsSkillInstance` | 非反射、池化纯数据；池元素**扩容即搬移** ⇒ 调用方 MUST NOT 跨"可能触发池扩容的调用"缓存指针 |
| `FTcsLearnedSkillEntry` | 非反射、桶内纯数据；同上 + 遍历期间 MUST NOT 增删 |
| `FTcsNumericParamModInstance` | 纯 C++、无对象引用；账本条目**不持任何 `UObject` 引用** |
| `FSkillBucket` / `FTcsSkillRegistry` | 共享层模板的**绑定**，不是新实现 |
| `FTcsCastOps` / `FTcsSkillOps` / `FTcsParamChainOps` / `FTcsCooldownOps` / `FTcsCostOps` | 引擎函数族；虽带 `TCSSKILL_API`，但它是**测试面不是契约面** |

### 3.3 一条反直觉的边界：**引擎函数带导出宏，但不是给业务用的**【实测】

五个引擎函数类全部带 `TCSSKILL_API`（如 `TcsCastOps.h:54`）。`TcsCastOps.h:51-52` 写明了理由：

> **导出宏是必需项**（判据 = "该符号是否被跨模块引用"）：宿主侧装置（`TcsDev`）若直接调本族静态成员，不带 `TCSSKILL_API` 必 `LNK2019`。

⇒ **导出宏在这里服务的对象是"测试"，不是"契约"**。这与 `ARCH-07` §3.3 对 `FTcsBuffOps` 的判定**完全同型**（两处都是"带导出宏但是测试面"）。**这是一条跨模块的规律**：本仓的引擎函数族普遍被装置直接调用，于是导出宏成了"装置可达性"的副产品，而不是"模块契约"的声明。

### 3.4 一条被刻意保留的"重复入口"：`TerminateSkillInstance`【实测】

`FTcsCastOps::TerminateInstance`（`TcsCastOps.cpp:925-930`）只有一行：`return FinishSkillInstance(Subsystem, RunHandle, ECFR_Replaced);`。

**为什么保留它**（`TcsCastOps.h:126-129`）："它是 Task 3 已交付的公开面（宿主装置 `TcsDevSkillCastProbe.cpp` 的夹具清理动作在调它）⇒ 改签名会波及既有装置；且'顶替'这个语义在调用点比'终结'更可读。"

⇒ 这是本仓对"同一件事两个入口"禁令的一次**显式豁免**，且豁免理由写清了（调用点可读性 + 既有装置）。

---

## §4 竖切影响（对其它模块的实际影响面）

### 4.1 调用方向（谁在什么时候碰它）【实测】

| 时机 | 谁 → 谁 | 影响 |
|---|---|---|
| 世界初始化 | `TcsIntegration` → `TcsSkill` | 定义库把技能定义写进登记口 |
| 宿主授予 | 宿主 → `TcsSkill` | 账本写入 |
| 宿主激活 | 宿主 → `TcsSkill` → **`TcsEffect`** | `ExecuteChain` 同步执行整条链 |
| 施法终结 | `TcsSkill` → **`TcsAttribute`** | `RemoveBySource` 摘属性账本条目 |
| 关系门 | `TcsSkill` → **`TcsState`** | `ITcsStateLiveQuery::ForEachLiveState` 读在跑状态 |
| 时段/冷却 | `TcsSkill` → **`TcsClockSubsystem`** | `PushExpiry` / `CancelExpiry` |
| 事件 | `TcsSkill` → **`TcsEventBusSubsystem`** | 8 枚事件的 `PublishImmediate` |

### 4.2 时序位置（硬约束，逐条有代码锚点）【实测】

**激活流程的完整时序**（`TcsCastOps.cpp:131-643`）：

```
门禁①②⑥⑦③④⑤
  → [ECT_OnCastStarted 档] CommitCost + BroadcastCostApplied + 重新解析
  → 建 run（整体重置后逐字段赋值）
  → 分配 CascadeAnchor
  → [ECT_OnCastStarted 档] StartCooldownOnEntry + 重新解析
  → 回填 RunHandles
  → BuildSkillSnapshot
  → MaterializeParamChainRows（绑快照作用域）
  → EvaluateEffectiveLevel（重算并写回 run）
  → 拷贝快照读数（SnapshotLevel / SnapshotCascadeAnchor / EntryCopy）
  → StartMainChain（同步执行链）
  → 按句柄回写 ChainRunHandle
  → 广播 OnCastStarted
  → EnterPhase（排定首段 或 立即完成终结）
```

**其中三处顺序判据值得单列**：

1. **快照必须晚于门禁**（`:515-516`）："**MUST NOT 在门禁之前构建**（门禁失败即白算一份快照）。"
2. **参数链物化必须晚于快照构建、早于生效等级求值**（`:558-567`）："上一步刚把那些行落进账本，所以**必须排在它之后**算，否则会漏掉它们。"
3. **起链前必须把读数拷成值**（`:573-578`）："起链会同步执行链步骤，而链步骤可改状态/属性并广播 ⇒ 之后 MUST NOT 再假定 `Run` 与 `Entry` 指针有效。"

### 4.3 一条竖切级的教训：**"同步广播"是本模块所有复杂度的来源**【实测】

全模块 `PublishImmediate` 的**调用点只有 3 处**（`TcsCastOps.cpp:120` / `TcsCooldownOps.cpp:82` / `TcsCostOps.cpp:656`），但它们**覆盖全部 8 枚事件**（施法 4 枚 + 冷却 3 枚 + 消耗 1 枚）——因为每族各有一个「统一广播出口」。⇒ **8 枚事件全部走立即通道**，而**每一次立即广播都是一次重入窗口**，于是代码里出现了：

- **3 处"广播后重新解析"**（§2.2）
- **1 个重入守卫**（`TTcsEntityBroadcastGuard`）
- **1 处"两段式遍历"**（`ExpireGroup` 先收集再广播，`TcsCooldownOps.cpp:959-1010`）
- **1 处"广播前把日志读数拷成值"**（`TcsCastOps.cpp:99-112`）

`TcsCooldownOps.cpp:959-965` 对两段式写了判据：

> **判据 = 本文件与全仓既有纪律**："遍历期间 MUST NOT 增删条目"（`FSkillBucket::ForEach` 的注释原文）、两段式先例 = `FTcsSkillOps::RevokeBySource`（先收集命中句柄，遍历结束后再动）。

⇒ **本模块的行数（11 052）里，有相当一部分是在付"同步广播"的账**。这不是设计缺陷，而是"事件必须同步可见"这条语义的必然代价。但它解释了一个现象：**为什么技能层是最大的模块**——它不是功能最多，而是**重入面最多**。

### 4.4 一条跨模块的隐性契约：**`CascadeAnchor` 与 `Context.RunSource` 的分工**【实测】

| 概念 | 载体 | 语义 |
|---|---|---|
| `FTcsSkillInstance::CascadeAnchor` | 基底 `FTcsStateInstance` | "按它一次摘净"的**级联撤销锚点** |
| `FTcsEffectContext::RunSource` | `TcsEffect` | 链运行态的来源句柄 |

起链时 `Context.RunSource = Run->CascadeAnchor`（`TcsCastOps_Chain.cpp:90`）⇒ **两个模块的两个不同名字指向同一个句柄**。而 `TcsEffectContext.h:53` 明文规定"**子链 = 父链的 `RunSource`**"——等等，这与技能侧的"**子链另发新号、不沿用父链的**"（`TcsEffectContext.h:49`）矛盾吗？

**不矛盾，但两句话说的不是同一件事**：`TcsEffectContext.h:49` 说的是"子链另发新号"（子链有自己的运行态与自己的来源），而 `:53` 说的是"子链登记的行 = 父链的 `RunSource`"（施加来源继承）。**前者是运行身份、后者是施加来源**。

⇒ 这处容易误读的边界直接决定了 §6 U-1 的结论：技能终结时**只摘自己起的链**，子链挂的条目**常驻**，这是**如实登记的残留边界**。

### 4.5 跨模块：**本模块是"编译期耦合最窄、运行期影响最宽"的一类**【实测】

- **编译期**：插件内只有 1 个文件 include 本模块头（`TcsDefinitionSubsystem.cpp`）。
- **运行期**：一次施法会触碰 `TcsEffect`（起链）、`TcsAttribute`（回收）、`TcsState`（关系门 + 参数表）、`TcsClock`（时段 + 冷却）、`TcsEventBus`（8 枚事件）。

⇒ **它的编译期影响面小到可以忽略，运行期影响面大到任何一处顺序改动都会波及三四个模块**。这正是 §5 D-1（use-after-free）之所以危险的原因：**它的触发条件是另一个模块的订阅者做了一件完全合法的事**。

---

## §5 技术债（期望 vs 实际，逐条带证据）

> 判据见 `INDEX` §4：**技术债 = 需求说了 / 设计定了，但代码做了却做偏了**。每条必带"期望 / 实际 / 证据 / 影响面 / 是否已登记"。

### D-1 ★★★ `FinishSkillInstance` 在**两次广播之后**仍读 `Run->DefTag`，而同函数的注释刚立下"此后不得再用 `Run`"的纪律【实测】

**期望**：`TcsCastOps.cpp:733-734` 的注释原文 ——

> 本段会同步执行链步骤（可再起激活、可广播）⇒ 之后 MUST 重新解析运行态，且**不得**再假定 `Run` 指针有效（池可扩容搬移）——故本条之后的全部动作一律用 `RunCopy`。

**实际**：`:818` 与 `:858` 两处仍是 `Run->DefTag`：

```cpp
// :818 —— 此时已经过 ⓪ 起链（同步执行整条链）与 ②③ 回收（广播属性变更）
const FTcsSkillDefData* Def = Entry ? Subsystem.GetRegisteredSkillDef(Run->DefTag) : nullptr;

// :858 —— 此时又经过了 ③′ 的冷却/Cost 分派（三枚事件同步广播）
Def = Entry ? Subsystem.GetRegisteredSkillDef(Run->DefTag) : nullptr;
```

**偏差证据**：`:683` 的 `Run = ResolveInstance(...)` 是**唯一**一次解析；此后 `Run` 从未被刷新（`:743` 的 `LiveRun` 是**另一个**局部变量，且只用于 `CancelCastFinishExpiry`）。而 `:843-844` 的注释**逐字**写着"全部只读 `RunCopy` 的拷贝字段……`Run` 指针在本函数 ⓪ 段起链之后即不可信（既有纪律）"——**同一段注释的下一行就是违规代码**。

**触发条件（全部是合法操作）**：
- 链步骤里再激活另一个技能（`CI_InstancePerExecution` 下可直接并存）⇒ 施法池 `TArray` **扩容搬移**；
- 属性变更事件的订阅者激活一个技能 ⇒ 同上；
- 冷却事件的订阅者激活一个技能 ⇒ 同上。

**影响面**：`Run->DefTag` 是 `FGameplayTag`（`FName` + 4 字节），读一个已搬移的槽位得到的是**上一任占用者的 `DefTag`** ⇒ `GetRegisteredSkillDef` 可能命中**另一个技能的 Def**，于是 `DispatchCooldownTiming` / `DispatchCostTiming` 按**别人的冷却与消耗配置**分派。**症状是"配了冷却的技能有时不进冷却"，而不是崩溃**——这是比 `ARCH-07` D-1（use-after-free 崩溃）**更隐蔽**的一类：它只在池扩容 + 槽位复用时才发生。

**修法 3 行**：`RunCopy.DefTag`（`FTcsStateInstance` 的基类字段，`RunCopy` 是**按值拷贝**，已含该字段）。与 `ARCH-07` D-1 的修法**同型**（都是"用拷贝不用指针"），且**同一个作者在同一天写下了正确的纪律与违规的代码**。

**是否已登记**：否。

### D-2 ★★★ `MainChainStart` 的三档：**规格说"未实现、必须报 Warning"，代码四档全实现**——且规格里有一个 Scenario 照此会**假失败**【实测】

**期望**（`skill-cast-runtime:162`，原文）：

> 本 Task 只实现 **`MCS_OnCastStarted`**；`MCS_OnPhaseEnter` / `MCS_OnCastCompleted` / `MCS_Custom` 三档**命中时 MUST 记 Warning 并明确报出"该档未实现、主链未起"**。

配套 Scenario（`:170-174`）：

> #### Scenario: 未实现的启动档位具名报出
> **WHEN** 配 `MCS_OnCastCompleted`（本轮未实现）的技能激活成功
> **THEN** 激活**仍然成功**，但留下一条具名 `Warning` 指明该档未实现且主链未起

**实际**（`TcsCastOps_Chain.cpp:140-191`）：

| 档 | 代码 | 行为 |
|---|---|---|
| `MCS_OnCastStarted` | `:142-147` | 触发点匹配即起 |
| `MCS_OnPhaseEnter` | `:149-171` | **已实现**：匹配当前时段的 `Tag` |
| `MCS_OnCastCompleted` | `:173-178` | **已实现**：触发点匹配即起 |
| `MCS_Custom` | `:180-191` | 具名 `Warning` + 返回 |

⇒ **规格写"三档未实现"，实际只有一档未实现**。而那个 Scenario 在今天的代码上**必然失败**（`MCS_OnCastCompleted` 会真的起链、不会报 Warning）。

**为什么这条特别值得记**：它是**"Task 编号成了规格的过期锚点"**的一个干净标本。规格里的"本 Task"指 Task 3，而代码在 Task 5 就把三档补齐了；规格**后来还加了两次"取代了原文"的留痕**（`:25-29` 与 `:192-197`，针对 `PhaseExpiryEntry` 与事件产生者），**但这两处留痕都没有覆盖 `MainChainStart` 那一段**。⇒ 留痕是**逐句手工补的**，没有"按 Task 编号扫一遍"的机制。

**影响面**：宿主按规格实现会以为 `OnCastCompleted` 档不可用，于是绕道自己做"完成后起链"——**重复实现一套框架已有的机制**。

**是否已登记**：否。

### D-3 ★★ 门禁⑦ `SAR_RelationBlocked` 在**整个 `openspec` 里零收录**，且规格要求的"三种原因各自返回具名枚举值"代码只返回一个【实测】

**期望**（`skill-cast-runtime:100`，2026-10-16 `R5.5-e1` 追加的门禁注记）：

> 门禁在**第 ⑥ 道之后新增一道关系门**：**`ForbiddenTags` 命中 / `RequiredTags` 未满足 / 被存活实例的 `TagsToBlock` 覆盖** ⇒ **各自返回具名枚举值**，MUST NOT 与第 ①~⑥ 道混同。

**实际**（`TcsCastOps.cpp:199-210`）：

```cpp
if (!FTcsSkillOps_Relation::CheckRelationGate(LiveQuery, Entity, DefTag, *Def))
{
    return ESkillActivateResult::SAR_RelationBlocked;   // ← 三种原因共用一个值
}
```

三种原因只体现在 `TcsSkillOps_Relation.cpp` 的**三条不同 `Log` 文本**里（`:72-75` / `:81-84` / `:92-95`），而枚举层面**完全不可分**。

**更严重的第二层**：`SAR_RelationBlocked` 这个符号在 `openspec/specs/` 下的**命中数 = 0**（实测 `grep -rn "RelationBlocked" openspec/specs/` 为空）。也就是说：
- 枚举值 `7` **存在**（`TcsSkillActivateResult.h:53`）；
- 门禁⑦ **存在且可拒**；
- 宿主 UI 分支**会用到它**；
- 但**没有任何一份规格知道它存在**——连那唯一一句提到"关系门"的注记也只描述了三类原因，**没写枚举值名**。

**影响面**：宿主从规格出发实现"为什么放不出来"的 UI，**拿不到这个值**（规格里没有）。而 `SAR_RelationBlocked` 是唯一"技能被状态域阻断"的读数——它是 `TcsSkill` 与 `TcsBuff` 两个域**唯一的运行期交界面**，恰恰零记录。

**是否已登记**：否。

### D-4 ★★ 一批旧符号名在 `Source/` 命中 0，且**注记加了、正文没改**【实测】

`ARCH-05` 立的 G-12 说"凡经历拆分的模块，审核 MUST 逐项核对符号名"。本模块**没有经历拆分**，但它经历了一次**语义改名**（`Interrupt` → `Cancel`，2026-10-16 `R5.5-e1`），结果与 G-12 完全同型：

| 规格写的 | 代码真名 | 规格命中数 | 代码命中数 |
|---|---|---|---|
| `IsInterruptibleNow()` | `CanBeCanceled()` | **5 处 / 3 份规格** | **0** |
| `bInterruptible` / `bInterruptibleDefault` | `bCancellable` / `bCancellableDefault` | **13 处 / 3 份规格** | **0** |
| `ECT_OnCastInterrupted` | `ECT_OnCastCanceled` | **4 处 / `skill-cooldown`** | **0** |
| `ECFR_Interrupted` | `ECFR_Canceled` | **2 处 / 2 份规格** | **0** |
| `FinishCastRun` | `FinishSkillInstance` | **2 处 / `skill-cooldown`** | **0** |
| `FTcsEntrySelector` | `FTcsSkillEntrySelector` | **2 处 / `skill-def-asset`** | **0** |

**这一条与 G-18（`ARCH-07`）的区别在于"改名的执行方式"**：`ARCH-07` 是**没有单一规则**（每个类型各自落在三种组合之一），而本模块是**规则正确、执行只做了一半**——三份规格都加了 2026-10-16 的改名注记（`skill-cast-phases:84` / `skill-cast-runtime:100` / `skill-def-asset:66`），**但注记是追加在段落末尾的，上面的正文一字未动**。

**最刺眼的一处**：`skill-cooldown` **完全没有改名注记**，它的分派表（`:229-232`）两侧全错：

```
| ECFR_Interrupted | ECT_OnCastInterrupted |    ← 两个名字在代码里都不存在
| ECFR_Replaced    | ECT_OnCastInterrupted |
| ECFR_Completed   | ECT_OnCastCompleted   |
```

而代码（`TcsCastOps.cpp:945-947`）写的是 `ECFR_Canceled` / `ECFR_Replaced` / `ECFR_Completed` × `ECT_OnCastCanceled` / `ECT_OnCastCompleted`。⇒ **照规格实现这张表，编译不过**。

**是否已登记**：否（`ARCH-07` 的 G-18 记的是"没有单一规则"，本条记的是"注记与正文分离"——**判据不同，不重复**）。

### D-5 ★★ `FTcsSkillInstance.RunSource` 与 `PhaseExpiryEntry` 在 4 份规格里共 **24 处**，代码里 **0 处**；且**同一份规格两个方向并存**【实测】

**期望**：`PLN-REFACTOR-1` Task 3 把两个字段上提到基底并改名 —— `RunSource` → `CascadeAnchor`、`PhaseExpiryEntry` → `ExpiryEntry`。

**实际**：`skill-cast-runtime:17` **正确记录了这次改名**（"由原 `RunSource` 改名——基底只放撤销锚点"），而**同一份文件的 `:22` / `:31` / `:33` / `:35` / `:37` / `:48` / `:68` / `:160` / `:168` 共 9 处仍写 `RunSource`**。

全量分布：

| 规格 | `RunSource` | `PhaseExpiryEntry` |
|---|---|---|
| `skill-cast-runtime` | 10 | 4 |
| `skill-param-chain` | 5 | 0 |
| `skill-layer-e2e-validation` | 2 | 0 |
| `skill-cooldown` | 1 | 1 |
| `skill-cast-phases` | 0 | 1 |
| **合计** | **18** | **6** |

**一处必须说清的歧义**：`RunSource` **不是完全作废的词**——它在 `FTcsEffectContext::RunSource`（`TcsEffect/Public/Chain/TcsEffectContext.h:50`）里**仍然有效**，且技能起链时 `Context.RunSource = Run->CascadeAnchor`（`TcsCastOps_Chain.cpp:90`）。⇒ 规格里的 `FTcsSkillInstance.RunSource` 是**错的**，而 `Context.RunSource` 是**对的**，两者在同一句话里出现（如 `skill-cast-runtime:160`）。**这才是它比普通改名滞后更难修的原因**：不能全局替换。

**是否已登记**：否（`ARCH-07` D-3/D-4 记的是 `TcsBuff` 那批；本条是 `TcsSkill` 自己的两个字段）。

### D-6 ★★ 门面零 `UFUNCTION` 的决定**零规格记录**（`ARCH-07` G-20 的第二次命中）【实测】

**期望**：`UTcsSkillSubsystem` 的 34 个公开方法**全部无反射**，而这是一个**明确的、有理由的、有反驳记录的**决定。

**实际**：`TcsSkillSubsystem.h` 有 **4 处**逐字注明"本轮不加 `UFUNCTION`——脚本读写面归台账 `SCRIPT-10`"（`:173` / `:268` / `:338` / `:521-528`），其中 `:521-528` 还**明确反驳了 `PLN-R6.5-1` §2.4 的签名表**（那张表写了 `UFUNCTION()`）。

而 8 份 `skill-*` 规格里：`UFUNCTION` 只出现 **2 次**，且**都不是在讲门面**——`skill-cast-phases:49` 讲 `ITcsCastStateQuery` 的接口形态、`skill-cost:31` 讲 `ITcsSkillCostPolicyHost`。

⇒ **"技能门面整体不可脚本调用"这件事，规格里一个字都没有。** 这与 `ARCH-07` U-6（`TcsBuff` 脚本面整体为零、规格零记录）**完全同型**，是 G-20 的**第二次命中**——说明 G-20 不是 `TcsBuff` 的偶发问题，而是**规格结构性的**：它只给"实现了的"留格子。

**影响面**：下一轮读者从规格出发，会以为 `TryActivate` / `GrantSkill` / `PayCost` 这些口脚本可调（因为它们都在规格里被反复提到，且都带完整签名），**实际一次都调不到**。

**是否已登记**：部分（`ARCH-07` G-20 记的是 `TcsBuff`；本条是它的第二个实例，建议在 G-20 行追加）。

### D-7 ★★ `skill-cast-runtime:19-22` 有一段**逐字重复的段落**，且重复的那一份用旧名【实测】

**期望**：一份规格的同一处不出现重复条款。

**实际**：`:19-20` 与 `:21-22` 是**四行的近乎逐字重复**：

| 行 | 内容 |
|---|---|
| `:19` | **`Entity` 为何必须自持**……（与 `:21` 逐字相同） |
| `:20` | **`ExpiryEntry` 的写入者自本变更起存在**……（**新名**） |
| `:21` | **`Entity` 为何必须自持**……（与 `:19` 逐字相同） |
| `:22` | **`PhaseExpiryEntry` 的写入者自本变更起存在**……（**旧名**） |

⇒ **这不是简单的复制粘贴残留，而是"改名前后两个版本都留在了文件里"**。判据：`:19-20` 那一对用新名，`:21-22` 那一对用旧名——**顺序说明先写的是旧的、后加的是新的，但旧的那份没删**。

**为什么它比 D-5 更值得单列**：D-5 是"漏改"，本 D 是**"改了但旧的还在，于是同一件事在相邻四行里有两种说法"**。读者读到 `:20` 会认为字段叫 `ExpiryEntry`，读到 `:22` 会认为叫 `PhaseExpiryEntry`——**两个名字只差两行**。

**是否已登记**：否。

### D-8 ★★ 冷却与消耗的 `ParamRef` **恒落兜底**，而两份规格都把它列为该次换型的正当理由之一【实测】

**期望**（`skill-cooldown:73-76`，讲 `FTcsCooldownTrack::Duration` 为什么从 `double` 换成 `FTcsParamValue`）：

> ① 按等级配时长（"1 级 8s / 2 级 6s"）靠等级表参数源；
> ② **按来源配时长（`HostDelegate` / `ParamRef` 等既有源）**；
> ③ 缩减靠**契约键** `Tag_TcsSkillParam_CooldownPct` 的系数。

`skill-cost:267-271` 对 `FTcsSkillCostPolicy_ResourceAttr::Amount` 写了**同样的三条**，其中 ② 同样列了 `ParamRef`。

**实际**：两条路径的求值上下文**参数表恒空**：

```cpp
// TcsCooldownOps.cpp:150
DurationCtx.ParamTable = TScriptInterface<ITcsParamTableReader>();   // 恒空

// TcsCostOps.cpp:42
Ctx.ParamTable = TScriptInterface<ITcsParamTableReader>();           // 恒空
```

而 `FTcsParamSource_ParamRef` 的取值**只从 `Ctx.ParamTable` 来** ⇒ 配了 `ParamRef` 的冷却时长 / 消耗量**恒落各自 `Fallback`**，不报错、不崩溃。

**三处同族字段的实际可用性对比**（本模块内）：

| 字段 | 求值点 | `ParamTable` | `ParamRef` 可用？ |
|---|---|---|---|
| `FTcsPhaseSpan::Duration` | `TcsCastOps_Phase.cpp:47-55` | **绑本次 run 的快照** | ✅ 可用 |
| `FTcsCooldownTrack::Duration` | `TcsCooldownOps.cpp:146-150` | 恒空 | ❌ 落兜底 |
| `FTcsSkillCostPolicy_ResourceAttr::Amount` | `TcsCostOps.cpp:38-42` | 恒空 | ❌ 落兜底 |

**代码是诚实的**：两处注释都写明"**参数表恒空**……引用类源（`ParamRef`）在这个时点没有更外层参数表可查 ⇒ 落各自兜底"。**不诚实的是规格**——它把 `ParamRef` 与 `HostDelegate` 并列写成"既有源"可用，而这两者在 `ParamTable` 这件事上**完全不是一回事**（`HostDelegate` 直接调宿主委托，不需要 `ParamTable`；`ParamRef` 必须要有）。

**影响面**：策划照规格配一条 `ParamRef` 冷却时长 ⇒ 冷却恒为 0（落兜底默认值）⇒ **技能永远可放**。这是本仓反复在拦的那类"配置看着生效、实际被无视"的静默缺陷。

**是否已登记**：否。

### D-9 ★★ `skill-registry:65` 的 `GetLevel` 签名与代码不符（**少两个形参**）【实测】

**期望**（`skill-registry:65`，原文）：

> MUST 提供 `GetLevel(FTcsSkillEntryHandle) -> int32`：返回**生效等级** `EffectiveLevel`……

**实际**（`TcsSkillSubsystem.h:466-470`）：

```cpp
int32 GetLevel(
    FTcsCombatEntityHandle Entity,
    FTcsSkillEntryHandle Handle,
    const TArray<double>& LevelModifiers) const;
```

⇒ 规格里**少了 `Entity`**（条目句柄的 `Index` 只在实体桶内有意义，没有实体根本定位不到条目）和 **`LevelModifiers`**（这条路径是"不改账本试算等级"的保留口）。

**同一份规格的 `:74` Scenario** 又说"以'带 `Level` 键的修正器列表'（合计 +2）调用求值"——**Scenario 里承认有这个形参，需求正文里没有**。⇒ 需求正文与它自己的 Scenario **互相矛盾**。

**影响面**：宿主按规格写调用会**编译不过**；而更麻烦的是"少 `Entity`"这个错——它恰好掩盖了 §2.1 那条"条目句柄的 `Index` 只在实体桶内有意义"的关键事实。

**是否已登记**：否。

### D-10 ★ "重复授予 ⇒ 刷新既有条目"是实现超文档【实测】

**期望**：`skill-registry` 关于授予只写了三条 —— 授予后可查询、按来源级联、未登记拒绝。

**实际**（`TcsSkillOps.cpp:55-82`）：重复授予同一 `DefTag` 时**不新建第二条**，而是**刷新**既有条目的 `Level` 与 `LearnSource`：

```cpp
if (bFound) {
    Handle = Existing;
    Entry->Level = Def->LevelBase;      // 刷新
    Entry->LearnSource = Source;        // 刷新
}
```

**这条行为是重要的**：它决定了"同一实体会同一个技能只有一条事实"，也决定了 `RevokeSkillBySource` 的语义（换来源后旧来源摘不掉这条）。**规格里一个字都没有**。

**是否已登记**：否。

### D-11 ★ 一批观测口零规格收录，而它们是**装置断言的唯一读数来源**【实测】

零收录（`grep -rn "<符号>" openspec/specs/` 全部为空）：

| 符号 | 代码锚点 | 装置消费者（实测） |
|---|---|---|
| `GetCastPhaseIndex` | `TcsSkillSubsystem.h:392` | `TcsDevSkillCastProbe_Phase.cpp` |
| `GetCastActiveChainId` | `:401` | `TcsDevSliceRig_Skill.cpp:578` / `TcsDevSkillCastProbe_Phase.cpp`（**6 处**） |
| `GetSkillInstanceCount` | `:441` | 装置与验收多处 |
| `GetEntryCount` / `GetTotalEntryCount` | `:225` / `:228` | 验收读数 |
| `GetRegisteredSkillDefCount` | `:160` | 装置断言 |
| `GetChainRedirectCount` | `:420` | 装置断言 |

**为什么这条值得记**：`GetCastActiveChainId` 的头注释自己写明了它的存在理由——"**链运行态不持链身份 ⇒ 从 `ChainRunHandle` 反查不到'这次起的是哪条链'**。若装置改取'解析函数返回了什么'或'重定向表里有几条'，那是在测解析而不是测起链。" ⇒ 它是**重定向能力的唯一合法验收读数**，却是规格零收录。

⇒ 这与 `ARCH-06` 的 G-17（`IsChainRegistered` 有真实消费者却零收录）**同型**，也与 `ARCH-07` 的 D-12 同型。**三次命中说明这不是偶发**：规格倾向于记录"设计上重要的"，而漏掉"为绕开语言限制 / 为可观测性而造出来的替身"。

**是否已登记**：部分（G-17 记的是 `TcsEffect` 那一个；本模块 6 个是第二批）。

### D-12 ★ `FTcsBoolSwitchRow::Mode` 零读取者，规格只说了"零内置消费者"没说"该列无效"【实测】

**期望**（`skill-def-asset:119`）：形状 = `{Key, Base, Mode}`，`ETcsParamMode` 从 `TcsState` 复用。

**实际**（`TcsSkillOps_ParamRead.cpp:128-135`）：`IsSwitchSet` **只读 `Row.Base`**，从不看 `Row.Mode`：

```cpp
for (const FTcsBoolSwitchRow& Row : Def->BoolSwitches)
{
    if (Row.Key == Key) { OutValue = Row.Base; return true; }
}
```

**代码是诚实的**（`TcsBoolSwitchRow.h:30-33`）：

> ① 本表的 `Mode` 列**零读取者**——`IsSwitchSet` 只读 `Base`，配 `实时` 与配 `快照` 行为相同；
> ② 布尔**修正器**整体归 `R6.5-e`，今天没有"改开关"的机制，本表只提供初值。

**规格的半覆盖**：`skill-def-asset` 有一个 Scenario 叫「布尔开关本轮零内置消费者」，说的是"消费者只有账本读取面与作者期校验"——**它承认了零消费者，但没承认"这一列被无视"**。⇒ 读者从规格出发会以为 `Mode` 列生效。

**影响面**：策划把开关配成"实时" ⇒ 期望"随状态变化"，实际**永远返回初值**。而"开关永远不变"这个读数**看起来完全正常**。

**是否已登记**：否（`R6.5-e` 登记的是"布尔修正器未落"，不是"Mode 列零读取者"）。

---

### 债务小结

| 编号 | 严重度 | 性质 | 归属 |
|---|---|---|---|
| D-1 | ★★★ | `Run->DefTag` 两处 use-after-free | **纯代码** |
| D-2 | ★★★ | `MainChainStart` 三档：规格说未实现、代码全实现 | **纯规格**（+1 个会假失败的 Scenario） |
| D-3 | ★★ | 门禁⑦ 零收录 + 三原因共用一个枚举值 | **双方** |
| D-4 | ★★ | 6 组旧符号名，注记加了正文没改 | **纯规格** |
| D-5 | ★★ | `RunSource` / `PhaseExpiryEntry` 共 24 处 | **纯规格** |
| D-6 | ★★ | 门面零 `UFUNCTION` 零记录 | **纯规格** |
| D-7 | ★★ | 四行重复、两份用两种名字 | **纯规格** |
| D-8 | ★★ | 冷却/消耗的 `ParamRef` 恒落兜底 | **双方** |
| D-9 | ★★ | `GetLevel` 签名少两个形参 | **纯规格**（且与自己 Scenario 矛盾） |
| D-10 | ★ | 重复授予刷新行为超文档 | **纯规格**（漏写） |
| D-11 | ★ | 6 个观测口零收录 | **纯规格**（漏写） |
| D-12 | ★ | `Mode` 列零读取者，规格半覆盖 | **双方** |

**分布**：**纯代码 1 条**（D-1）/ **纯规格 8 条**（D-2、D-4、D-5、D-6、D-7、D-9、D-10、D-11）/ **双方各错一半 3 条**（D-3、D-8、D-12）。

⇒ **本模块的债极度偏向"规格侧"**：12 条里 8 条是纯规格问题，1 条纯代码，3 条双方。这与 `ARCH-07`（纯规格 5 / 纯代码 1）**同向但更极端**——而 `ARCH-08` 的代码量是 `ARCH-07` 的近两倍。

**一个反直觉的结论**：**技能层的代码质量是本档案已审八个模块里最好的**（11 052 行只有 1 条纯代码缺陷，且这条是"用指针不用拷贝"这种低复杂度错误），**而它的规格质量是最差的**（8 条纯规格问题，含一个会假失败的 Scenario、一段四行重复、一张照抄就编译不过的分派表）。

---

## §6 未实现（设计是否还成立 + 前置条件）

> 判据见 `INDEX` §4：**未实现 = 代码根本不存在**。每条必写：**设计是否还成立** / **当初的依据今天是否仍有效** / **前置条件**。

### U-1 ★★ 子链挂的账本条目**永久常驻**——规格明文登记的残留边界，代码也如实写了，但**两处都没给承接轮次**【实测】

**现状**：`FinishSkillInstance` 只按本 run 自己的 `CascadeAnchor` 回收（`TcsCastOps.cpp:769-773`）：

> **★ 只摘自己起的链（MUST NOT 越界）**：`CascadeAnchor` 取的是**本 run 自己的**。**子链另发新号、不沿用父链的**（`TcsEffectContext.h:49` 原文）⇒ 只摘本 run **直接**挂的条目；**MUST NOT 递归下探**（递归会摘掉不属于自己的东西，且 `CausedBy` 级联已被禁止）。**残留边界（如实登记，MUST NOT 假装已解）**："技能起链 → 链起子链 → 子链挂属性且子链比施法活得久"这一组合下条目**仍会常驻**。

**设计是否还成立**：**成立，且是有意的**。三条死路今天依然成立（`:762-767`）：① 随链运行结束回收——即时链的运行态在 `ExecuteChain` 返回前已释放；② 按 `CausedBy` 级联——**已被明文禁止**；③ 框架自动回收——`FTcsStepModifyAttribute` 刻意不提供回收器。

**当初的依据今天是否仍有效**：有效。**递归下探会摘掉不属于自己的东西**——这个判据没有过期。

**但缺一件东西**：**没有任何轮次承接它**。`ARCH-06` 的 U-1（跨模块载荷生命周期契约缺位）与它**同源**——都是"链运行态与持有者的生命周期不对齐"。建议与 `ARCH-06` U-1 合并成一条跨模块待办。

**前置条件**：需要一个"子链必须随父生命周期回退"的真实内容需求。台账 `CHAIN-7` 的触发条件正是这一型，而它已在施法运行态这一层闭合（**只闭合了父链那一半**）。

### U-2 ★★ `CQM_Custom` 与 `MCS_Custom` 两档仍未落，且**两处都只报"未实现"、不给宿主任何替代路径**【实测】

**现状**：

| 档 | 代码 | 行为 |
|---|---|---|
| `CQM_Custom` | `TcsCastOps_Query.cpp:62-71` | `Warning` + **回退 `DefSwitches`** |
| `MCS_Custom` | `TcsCastOps_Chain.cpp:180-191` | `Warning` + **不起链** |

**两处降级语义不同，且这个不同是有理由的**：
- 查询档**有默认档可回退**（`DefSwitches` 就是它的"无契约时"语义）；
- 起链档**没有可回退的档**——"宿主自己决定何时起链"无法由框架代劳。

⇒ 这是本模块**降级语义最讲究的一处**：同一个"未实现"，一个有回退、一个没有，且判据写在两处注释里。

**设计是否还成立**：**两档的设计都还成立，但阻塞面已经变了**。旧文档 `SPEC-04-skill` §11.2 写"依赖 `R5.5-e` 仍未满足"，而 2026-10-13 语义拍板后**形状门已开**；真正未满足的是 `PLN-R5.5` Task 2（`e2`）的实现。

**一处代码与规格的细节分歧**：`ITcsCastStateQuery` 与 `ITcsChainRedirectResolver` 两个接口**都已落地且可被宿主实现**，但：
- `ITcsChainRedirectResolver` 有**注入点**（`SetChainRedirectResolver`，`TcsSkillSubsystem.h:427`）；
- `ITcsCastStateQuery` **没有任何注入点**——全仓 `grep SetCastStateQuery` 命中 **0**。

⇒ **`CQM_Custom` 档的接口连"注入"这一步都还没做**，而规格 `skill-cast-phases:49` 只说"接口形态 MUST 为 `UINTERFACE`……"，**没说怎么把它接进来**。这是 §6 里最实的一条缺口。

**前置条件**：`e2` 落地 + 一处注入点形状裁定（"由宿主把实现对象注入门面"还是"让片段持一个指向实现对象的引用"——`TcsCastStateQuery.h:42-45` 自己把这个问题列为待裁）。

### U-3 ★★ `ESS_ByCategoryTags` 选择器档：**载体已落地、规则仍未定**【实测】

**现状**（`TcsParamChain.cpp:94-102`）：命中该档 ⇒ 具名 `Warning` + **零施加** + 返回 `false`。

**载体已经到位**（这点与旧文档不同）：`FTcsStateDefBase::CategoryTags` 已随变更 `merge-status-tag-into-category-tags`（2026-10-15）落地。代码注释也**同步订正了**（`:90-92`）：

> 载体已于 2026-10-15 落地（`FTcsStateDefBase::CategoryTags`）……但 `HasAll` / `HasAny` 一类规则与真实现时机仍未定。

**设计是否还成立**：成立。判据三条里 **① 载体不存在已消失**，只剩 ② 规则未拍板、③ 可能另立模块（"甚至可能单独开一个 `TcsGameplayTag` 模块"）。

**一处值得记的对照**：`TcsSkillEntrySelector.h:129-132` 的字段注释也**同步订正了**（"阻塞已减少一条（2026-10-15）"）。⇒ **这个模块的代码注释对"阻塞面变化"的跟进度是全仓最好的**——与 D-4/D-5 那种"规格不跟"形成鲜明对比。

**前置条件**：`PLN-R5.5` Task 2（`e2`）——类目降维层 + 筛选规则。

### U-4 ★★ `AttributeScaled` 参数源全仓零实现者，**且这个缺口在本模块出现了三次**【实测】

**现状**：`FTcsParamSource_AttributeScaled` 需要一个 `ITcsAttributeProvider` 实现者，而全仓为零。本模块**三处**注释如实登记：

| 位置 | 说明 |
|---|---|
| `TcsSkillDefData.h:107-109` | "本轮已实测可用的是 `字面量` / `参数引用` / `等级表` 三条" |
| `TcsNumericParamModifier.h:132-134` | "配了它只会落兜底值（不会报错）" |
| `TcsParamChain.h`（Purpose） | 同上 |

**设计是否还成立**：**成立，且这个缺口是跨模块的**。`ARCH-04`（`TcsAttribute`）已审，该模块**没有** `ITcsAttributeProvider` 的实现；`ARCH-07` 也登记了同一件事。

⇒ **这是全档案第一次出现"同一个缺口在三个模块的档案里各记一次"**。建议在 `INDEX` §6 立一条跨模块行（见 §8.4 的建议 ②）。

**前置条件**：宿主提供 `ITcsAttributeProvider` 实现（这本身是"框架不提供宿主词汇"的必然结果——属性名是宿主本体论）。

### U-5 ★★ 内联触发行 `Triggers` **零框架登记**——字段可读、无生命周期，且**规格与旧文档都把它记成"字段已交付"**【实测】

**现状**：`FTcsSkillDefData::Triggers` 存在、可配置、作者期校验（`TcsSkillDef.cpp:82-98` 校验 `EventTag` 与 `EffectChainId` 有效性），但**框架侧不建登记表、不建生命周期**。

**为什么技能侧与状态侧不同**（`TcsSkillDefData.h:214` 与旧文档 §2 的说明）：状态侧有明确登记时点（施加时逐行登记、`Source` = 实例级联锚点、移除/到期级联退订）；技能侧**无对应时点**（激活由宿主主动调用）。

**设计是否还成立**：**成立，但"零登记"这个结论的适用范围需要澄清**。技能侧**有**一个潜在的登记时点——**激活成功**。若将来要落"技能在跑期间订阅事件"，那个时点就是现成的。⇒ 目前的"零登记"更像是"**还没决定要不要**"，而不是"**结构上不可能**"。

**前置条件**：出现"技能在跑期间订阅事件"的真实内容需求（旧文档写的是"后续动作游戏的强输入引导"）。

### U-6 ★★ `FTcsSkillDefTableRow`（编辑期表行）**全仓零消费者**【实测】

**现状**：`FTcsSkillDefTableRow` 定义存在（`Public/Def/TcsSkillDefTableRow.h`，21 行），但全仓 `grep` 只命中它自己的声明处。

**规格怎么写的**（`skill-def-asset:160-176`）：给了两条 Scenario —— "表行字段往返保真"与"运行期不读 DataTable"。

**"运行期不读"这条是成立的**（实测 `Source/TcsSkill/` 内无任何 DataTable 读取）。**但"往返保真"这条 Scenario 是空转的**：没有任何代码执行过"写进 DataTable 再读回来"这件事。

**这是 `ARCH-07` U-7 的同型第二例**（`FTcsBuffDefTableRow` 也全仓零消费者）。⇒ **"双轨制"（资产轨 + 表轨）在本仓是两个模块各落了一个字段、但表轨从未接通**。建议合并成一条跨模块待办。

**设计是否还成立**：**存疑**。判据：表轨的价值是"批量编辑"，而本仓的技能定义资产走 `UTcsSkillDef`（`PrimaryDataAsset`）。两条轨并存需要一个"从表行生成资产"的管线——**那个管线不存在**。

**前置条件**：出现"批量编辑技能定义"的真实工作流需求。

### U-7 ★ 关系门的**技能↔技能阻断判定**未落，而代码注释如实写了【实测】

**现状**（`TcsSkillOps_Relation.cpp:87-89`）：

> **技能↔技能的阻断判定本批未落**（如实登记）：那需要遍历同实体全部在跑 run 的类别词，而技能在跑实例的词面今天没有读口 ⇒ MUST NOT 读成本条已覆盖两个方向。

**判据是准的**：`FTcsSkillInstance` 确实没有"类别词"这一面——类别词住 Def 的 `CategoryTags`，而运行态只持 `DefTag`。要判"另一个在跑的技能是否用它的 `TagsToBlock` 覆盖我"，就得**遍历同实体全部在跑 run → 各自查 Def → 取它的可寻址词集**。

**★ 一处随 2026-10-24 并集裁定变化的细节（MUST 读，否则会照旧口径实现）**：要取的那个词集**不是 `CategoryTags`，而是 `CategoryTags` ∪ `{DefTag}`**——与状态域同口径（`state-relation-check` 的「主体面与客体面同口径」需求）。判断依据 = 一个资产**拥有的全部词**，与它被拿去匹配的哪一侧无关。⇒ 本条的**结论不变**（仍缺读口、仍卡在枚举面），但**实现时的取词口径已定**，不必再问。

**设计是否还成立**：**成立，且代价很低**——遍历的原材料全都现成（`Registry` 的桶 + `RunHandles` + 定义登记表）。缺的只是一个"技能域自己的 `ITcsStateLiveQuery` 等价物"。

**一处结构性观察**：`TcsSkill` 有 7 道门禁，而**唯一"只覆盖一半"的就是关系门**——它读得到状态域的在跑实例（经 `ITcsStateLiveQuery`），读不到技能域自己的。⇒ **互通口是单向的**：状态域向下注册、技能域向上消费，但技能域没有给自己留读口。

**前置条件**：出现"技能之间互斥"的真实内容需求（如"姿态类技能互斥"——旧文档 §2 的 `Cancels` 原词正是这个用例）。

---

### 未实现小结

| 编号 | 严重度 | 性质 | 归属轮次 | 前置条件是否已满足 |
|---|---|---|---|---|
| U-1 | ★★ | 子链条目常驻 | **无承接** | 需要真实内容需求 |
| U-2 | ★★ | 两个 `Custom` 档 | `R6.5-g` / `PLN-R5.5` `e2` | **部分**（形状门已开，实现未做） |
| U-3 | ★★ | `ByCategoryTags` 档 | `PLN-R5.5` `e2` | **部分**（载体已落，规则未定） |
| U-4 | ★★ | `AttributeScaled` 源 | **跨模块** | 需要宿主实现 `ITcsAttributeProvider` |
| U-5 | ★★ | 内联触发行零登记 | **无承接** | 需要真实内容需求 |
| U-6 | ★★ | 表行零消费者 | **无承接** | 需要批量编辑工作流 |
| U-7 | ★ | 技能↔技能阻断 | **无承接** | 需要技能互斥需求 |

**七条里只有两条有承接轮次**（U-2 / U-3，都是 `e2`），**其余五条都是"需要真实内容需求"**。

⇒ 这与 `INDEX` §5.1 第 3 条判据（"'零消费者'不等于'未实现'"）**同向但更进一步**：本模块的五条未实现**不是有意留白**，而是**真的没人认领**。它们的共同特征是"**框架能做、但今天没有内容逼它做**"——这正是本仓"零消费者不预建"纪律的**代价**：纪律保证了不建无用设施，代价是**没人记录这些设施将来由谁触发**。

---

## §7 文档资产对照

### 7.1 三层真相源在本模块的分布【实测】

| 层 | 载体 | 规模 | 在本模块的可靠度 |
|---|---|---|---|
| **可执行需求** | 8 份 `skill-*` | 1 769 行 / 39 Req / 154 Scen | **差**——8 条纯规格债（§5） |
| **实际实现** | `Source/TcsSkill/` | 11 052 行 / 63 文件 | **好**——1 条纯代码债 |
| **历史理由** | `SPEC-04-skill`（498 行） | 单篇 | **好**——见 §7.2 |

**这个分布与前七篇全部相反**：前七篇的"可执行需求"层大多是**最可靠**的一层（`INDEX` §2 的判据是"这一层赢"）。而本模块**实现层最可靠、需求层最不可靠**。

### 7.2 `SPEC-04-skill` 与代码的差异清单（**只登记，不修补**——旧文档已冻结）

`SPEC-04-skill` 是本档案审到的**质量最高的旧文档**。它有一个**六节收束结构**（§11.1 落地 / §11.2 不落带归属 / §11.3 实现名对照 / §11.4 错前提更正 / §11.5 口径裁决 / §11.6 正文订正 / §11.7 边界 / §11.8 重构收束回写），且**每一节的"时态到期"都自己标了**。

**逐项差异清单（14 项）**：

| # | `SPEC-04-skill` 的声称 | 代码实况 | 判定 |
|---|---|---|---|
| 1 | §11.3 写 `FCastRun` → `FTcsSkillInstance`（2026-10-15 改述） | 一致 | ✅ **自纠准确** |
| 2 | §11.8 写 `RunSource` → `CascadeAnchor`、`PhaseExpiryEntry` → `ExpiryEntry` | 一致 | ✅ **自纠准确** |
| 3 | §11.8 写"依赖收窄为共享层 `TcsState`、MUST NOT 依赖 `TcsBuff`" | 一致（`Build.cs` 实测） | ✅ **自纠准确** |
| 4 | §11.8 写"唯一代码级状态侧依赖 = `GetParamTableReader()`（实测两处）" | 一致（`TcsCastOps.cpp:547` / `TcsCastOps_Phase.cpp:44`） | ✅ **自纠准确，且计数准** |
| 5 | §11.2 写 `R6.5-a/b/c/d` 四批"✅ 已落" | 一致 | ✅ **自纠准确** |
| 6 | §11.2 写 `CQM_Custom` / `MCS_Custom` 归 `R6.5-g`、"❌ 未落（不变）" | 一致（§6 U-2） | ✅ **准确** |
| 7 | §11.2 写"关系字段检查器 ❌ 未落（不变）" | **已落**（2026-10-16 `R5.5-e1`，`TcsSkillOps_Relation.cpp` 99 行） | ❌ **声称未落地**（时态：该行写于 2026-10-12，`e1` 在其后） |
| 8 | §11.7 写"门禁只 4 道可拒（③⑤ 为占位）" | **六道全可拒**（§2.1） | ❌ **过期**（但 §11.1 已加"时态已到期"注记） |
| 9 | §3.4 写 `FTcsNumericParamModifier` 字段表**含 `Source`** | **不含**（§2.5） | ❌ **正文过期**（但 §3.4 就地订正 ⑥ 已完整说明） |
| 10 | §3.4 写 `SortKey` "退化为带权、不再承担排序语义" | 两字段并存（`SortKey` + `OverridePriority`） | ❌ **正文过期**（就地订正 ① 已说明） |
| 11 | §3.4 写"Override 组取最大值" | 三级比较（优先级 → 策略 → 数值） | ❌ **正文过期**（就地订正 ② 已说明） |
| 12 | §3.4 写 `Level` 键"由 `TcsSkill` 原生声明" | **由 `TcsState` 声明** | ❌ **正文过期**（就地订正 ③ 已说明） |
| 13 | §2 写 `FStateDefBase` / `FSkillDef` / 基类含 `FragmentSet` | `FTcsStateDefBase` / `FTcsSkillDefData` / **无 `FragmentSet`** | ❌ **正文过期**（§11.4① 已更正） |
| 14 | §3.3 写 `ICastStateQuery` 的 `IsInterruptibleNow()` | `ITcsCastStateQuery` 的 `CanBeCanceled()` | ❌ **过期**（§11.3 有实现名对照，但**未跟 2026-10-16 的改名**） |

**统计**：**6 项自纠准确**（#1~#6）/ **8 项过期**（#7~#14）。

**关键观察：8 项过期里有 7 项是"正文过期但注记已给正确答案"**（#9~#14 全部有就地订正，#8 有"时态已到期"注记）。⇒ **它的过期是"分层"的：正文停在设计轮，收束节是最新的**。

**因此本档案对该文档的判定是**：

> **`SPEC-04-skill` = 「正文停在设计轮、§11 收束质量极高」。读法：先读 §11，再读正文；冲突时 §11 赢。**

这与 `ARCH-06` 对 `SPEC-03-effects` 的判定（"局部自纠 + 局部从不回写"）、`ARCH-07` 对 `SPEC-02-states` 的判定（"§12 收束回写质量高、正文停在设计轮"）**同型**——**三篇旧文档都是这个模式**。

⇒ 这已经足够立一条**跨模块规律**（见 §8.4 建议 ③）：**旧文档族的正文一律不可当现状读，但它们的"收束节"质量普遍高于 `openspec` 的对应能力**。

### 7.3 差异的**类型分布**

| 类型 | 条目 | 占比 |
|---|---|---|
| **改名滞后** | #14（`IsInterruptibleNow` → `CanBeCanceled`） | 1 |
| **声称未落地** | #7（关系字段检查器） | 1 |
| **实现超文档** | 无 | 0 |
| **已推翻仍留存（但已就地订正）** | #9、#10、#11、#12、#13 | 5 |
| **时态过期（有注记）** | #8 | 1 |
| **自纠准确** | #1~#6 | 6 |

**最有价值的观察**：**"实现超文档"是 0**。⇒ 本模块的**旧文档从未声称过代码没做的事**（除了 #7 那个时态问题）。这与 `ARCH-04`（`TcsAttribute`，规格更严、代码更松）**完全相反**。

⇒ **`SPEC-04-skill` 的问题是"旧"，不是"错"**。它的每一句在写下时都是对的，只是没跟上。

---

## §8 审核记录

### 8.1 本次审核做了什么【实测】

1. **读 8 份 `skill-*` 规格**（1 769 行），提取 39 个 Requirement / 154 个 Scenario。
2. **读全部 63 个代码文件**（11 052 行）——本档案迄今**唯一一次读完全部文件**的模块（前七篇都按核心路径抽样）。
3. **符号级比对**：把规格里的类型名 / 方法名 / 枚举值 / 字段名逐个拿到 `Source/` 里 `grep`，命中 0 的即为旧名（产出 D-4 / D-5）。
4. **读旧文档 `SPEC-04-skill` 全部 498 行**，与代码逐项对照（产出 §7.2 的 14 项清单）。
5. **查竖切**：追 8 枚事件的全部广播点（8 处 `PublishImmediate`）、3 处"重新解析"、1 处两段式遍历。
6. **查依赖方向**：`Build.cs` + 插件内 include 面（1 处）+ 运行期调用面（5 个模块）。

### 8.2 本次审核用到的**五类判据**（`ARCH-05` G-12 的扩展应用）

`ARCH-05` 立的四类（符号名 / 模块名 / 类型名 / 字段位置）是针对**拆分**的。本模块没有拆分，但有**语义改名**与**时态迁移**，于是扩展出第五类：

| # | 判据 | 本模块命中数 |
|---|---|---|
| 1 | **符号名**是否还指向它 | **6 组**（D-4） |
| 2 | **模块名**是否还指向它 | 0（本模块无拆分） |
| 3 | **类型名**是否还指向它 | 含在 #1 |
| 4 | **字段位置**是否还指向它 | **2 个字段 / 24 处**（D-5） |
| **5** | **时态词**（"本 Task" / "本轮" / "尚未定稿"）是否还成立 | **3 处**（D-2 的 `MainChainStart`、旧文档 §11.2 的 #7、旧文档 §5 的"未定稿"） |

⇒ **第 5 类是本模块新增的**。它的判据很简单：**凡规格里出现"本 Task" / "本轮" / "尚未" / "未定稿"这类相对时态词，MUST 核对它相对的那一轮是否已经过去**。

**本模块的命中率极高**：8 份规格里 `本 Task` 出现 **12 次**，其中 **2 次已被"取代"注记覆盖、1 次（D-2）没有**。

### 8.3 置信度分布

| 置信度 | 条数 | 说明 |
|---|---|---|
| **【实测】** | 12（全部债务）+ 7（全部未实现）+ 全部 §2 条目 | 读过代码或跑过 `grep` 直接确认 |
| **【推断】** | 0 | 本模块无一条结论依赖推断 |
| **【待核】** | 0 | — |

⇒ **本模块是八个模块里唯一"零推断、零待核"的**。原因是它**代码量最大但结构最清晰**——五个引擎函数族各管一件事，门面全是薄壳，没有跨模块的类型别名或转发层。

### 8.4 一次**方法论层面的收获**（记录在此，因为它是跨模块的）

本次审核产出三条**建议提到 `INDEX` §6** 的事项：

**① G-20 需要升级为"规格结构缺位"**（`ARCH-07` 立的 G-20 记的是"决定不做无处可写"）。本模块是它的**第二次命中**（D-6），且这次命中的是**整个门面的反射面**——比 `TcsBuff` 那次更严重。⇒ 建议在 G-20 行追加"已二次命中，且第二次是整个门面"。

**② `AttributeScaled` 缺口需要在 `INDEX` 立跨模块行**（U-4）。它已在 `ARCH-04` / `ARCH-07` / `ARCH-08` 三个档案各记一次，但**没有任何一条跨模块待办**。⇒ 建议立一条"宿主契约缺口"行。

**③ 旧文档族的"收束节质量高于 openspec"是可复用判据**（§7.2）。三篇旧文档（`SPEC-02-states` / `SPEC-03-effects` / `SPEC-04-skill`）全部呈现同一模式：**正文停在设计轮、收束节质量高**。⇒ 建议在 `INDEX` §5 的审核方法里加一条："**读旧文档 MUST 先读它的收束节（§11 / §12 一类），再读正文**"。

**一条自我约束的记录**：本次审核**没有**试图修任何一条债。`ARCH-07` 的 D-1 曾被提议"3 行就修"，本模块的 D-1 同样只需 3 行——**但本档案的性质是"登记"而非"修补"**，修补需要用户裁决（见 §9.4）。

---

## §9 结论

### 9.1 三句话

1. **技能层的代码是全档案已审八个模块里最好的**：11 052 行、五个引擎函数族、七道门禁、四路终结、8 枚事件，只有 **1 条纯代码缺陷**（D-1，且是"用指针不用拷贝"这种低复杂度错误）。它的**纪律密度极高**——"任何可能广播的调用之后，手里的指针都可能失效"这一句话在代码里出现了 **6 次**（含 3 处执行、3 处遗漏）。
2. **技能层的规格是全档案最差的**：8 条纯规格债，含**一个会假失败的 Scenario**（D-2）、**一段四行重复且新旧名并存**（D-7）、**一张照抄就编译不过的分派表**（D-4 的 `skill-cooldown:229-232`）、**一个签名少两个形参的需求**（D-9）。
3. **这两件事同源**：规格的每一次滞后都发生在"代码补完了、规格没跟"的那个窗口里。而代码之所以补得那么快，恰恰是因为**它的纪律是写在注释里的**（而不是写在规格里）——**注释跟得上代码，规格跟不上**。

### 9.2 一句值得记住的话

> **`TcsSkill` 的代码把纪律写在了注释里，于是注释成了唯一跟得上代码的文档；而规格停在 Task 编号上，于是它记录的是一个已经不存在的中间状态。**

### 9.3 与前五篇的对照

| 篇 | 模块 | 行数 | 债数 | 纯代码 / 纯规格 / 双方 | 主方向 |
|---|---|---|---|---|---|
| `ARCH-04` | `TcsAttribute` | 3 012 | 11 | — | 规格更严、代码更松（G-10） |
| `ARCH-05` | `TcsState` | 2 753 | 12 | — | 代码搬走、引用滞留（G-12） |
| `ARCH-06` | `TcsEffect` | 7 115 | 12 | 5 / 4 / 3 | 规格与代码各错一半 |
| `ARCH-07` | `TcsBuff` | 5 839 | 12 | **1 / 5 / 3**（+3 承接） | 纯规格为主（G-18/G-19/G-20） |
| **`ARCH-08`** | **`TcsSkill`** | **11 052** | **12** | **1 / 8 / 3** | **纯规格为主，且更极端** |

**五篇债数全部落在 11~12 条**——**债务密度与模块规模无关**，这是一个稳定的观察。但**债的构成随模块性格剧烈变化**：

- `TcsAttribute` / `TcsState` / `TcsEffect`（前三个）：规格与代码**互相拉锯**；
- `TcsBuff` / `TcsSkill`（后两个）：**规格单方面掉队**，代码是干净的。

⇒ **一个可检验的假设**：**越靠下游的模块，规格越容易掉队**。判据：下游模块的规格写作时点更早（因为它们的设计在 R6 就定了），而代码被后续批次（`R6.5` / `R5.5` / `PLN-REFACTOR-1`）反复改过。`TcsSkill` 经历了**至少四次**改代码不改规格的批次：`R6.5-1`（冷却/消耗）、`PLN-R6.5-CC`（策略化）、`PLN-REFACTOR-1`（基底化）、`R5.5-e1`（改名 + 门禁⑦）。

### 9.4 给下一轮的四条建议（按代价从低到高）

**① 修 D-1（3 行）** —— 把两处 `Run->DefTag` 改成 `RunCopy.DefTag`。这是本模块唯一的纯代码缺陷，且修法与 `ARCH-07` D-1 完全同型。**代价最低、收益最高**。

**② 修 D-4 / D-5 / D-7（纯替换，约 30 处）** —— 三批旧符号名 + 四行重复。全部是机械替换，**不需要任何设计判断**。唯一需要小心的是 `RunSource`（它在 `Context.RunSource` 里仍然有效，见 D-5）。

**③ 修 D-2 / D-9（需要判断，约 2 处）** —— 这两条是"规格写了、代码不符"，且**方向相反**：
- D-2：**规格该改**（代码已实现三档，规格还停在 Task 3）；
- D-9：**规格该改**（代码的签名是对的——少了 `Entity` 根本定位不到条目）。

⇒ 两条都是"改规格"，且都能**顺手把那个会假失败的 Scenario 一起改掉**。

**④ 补 D-3 / D-6 / D-11（需要新增内容）** —— 这三条是"规格漏写"，需要**新增条款**而不是改字：
- D-3：把 `SAR_RelationBlocked` 写进规格（并裁定"三种原因是否要各自具名"——这是**设计裁决**，不是文档修补）；
- D-6：把"门面零反射"这个决定写进规格（或写进 `SCRIPT-10` 台账）；
- D-11：把 6 个观测口写进规格（或统一在 `INDEX` 立一条"观测口的登记规则"——这正是 `ARCH-07` G-19 未裁清的那条）。

**一条不建议现在做的**：D-8（`ParamRef` 恒落兜底）。它的正确修法是"给冷却与消耗的求值上下文绑参数表"，而**绑什么表**是一个设计问题——冷却在起冷时刻没有"某次施法的快照"可绑（`ECT_Manual` 档可以在任何时候起冷）。⇒ 这一条应**先裁设计再修文档**。

---

## §10 修订记录

| 日期 | 变更 |
|---|---|
| 2026-10-17 | 建立。`ARCH-08-skill` 完成（**959 行**，本档案迄今最长）。审出 **D-1..D-12 / U-1..U-7**。三条结构性发现：① **代码质量与规格质量完全反向**——11 052 行只有 1 条纯代码债，却有 8 条纯规格债；② **两条相反的偏差同时存在于同一个函数里**（`FinishSkillInstance` 的注释立了"此后不得用 `Run`"的纪律，紧接着两行就是 `Run->DefTag`）；③ 新增第五类引用判据 **时态词**（"本 Task" / "本轮" / "尚未定稿"是否还成立），8 份规格里 `本 Task` 出现 12 次、命中 3 处。另首次给出 `SPEC-04-skill` 的 **14 项差异清单**（6 项自纠准确 / 8 项过期，且 8 项里有 7 项"正文过期但注记已给正确答案"）——判定与 `ARCH-06` / `ARCH-07` **同型**，足以立一条跨模块规律。**同时建议 `INDEX` §6 追加三条**（G-20 升级为结构缺位 / `AttributeScaled` 立跨模块行 / 旧文档先读收束节）。 |

