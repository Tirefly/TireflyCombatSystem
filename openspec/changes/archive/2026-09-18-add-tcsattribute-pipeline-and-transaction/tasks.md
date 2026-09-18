## 1. Implementation

### 折叠器（Public 复用件，M5/TcsDamage 也要用）

- [x] 1.1 `Public/Attribute/TcsAttributeBandFold.h`（header-only 纯函数）：`FTcsAttributeBandEntry{ ETcsAttributeOp Op; double Value; int32 OverridePriority; }` + `FoldTcsAttributeBands(double BaseValue, TConstArrayView<FTcsAttributeBandEntry> Entries, ETcsAttrOverrideTieBreak OverrideTieBreak = OTB_Max)`——五带公式 + 覆盖带按"优先级 → 同优先级策略 → 有符号值"三级选出赢家并覆盖一切；带序由 `Op` 决定（`SortKey` 不参与）；**空集返回 `BaseValue`**；纯函数（无副作用/时间/随机，D0-1）；注释写明"**三处消费者（M2 聚合 / M5 参数链 / TcsDamage 流程属性）一律调用本函数，不得另有并列同义实现**"（D5-5 v3）

### 覆盖带强弱口径（2026-09-18 落地期增补，用户拍板）

- [x] 1.1a `TcsAttrModInstance.h`：封闭四值枚举 `ETcsAttrOverrideTieBreak{OTB_Max=0 / OTB_Min=1 / OTB_MaxAbs=2 / OTB_MinAbs=3}`（**无 CustomStrategy**——热路径比较函数必须全域且确定）+ `FTcsAttrModInstance::OverridePriority`（`int32`，仅 `TAO_Override` 读）
- [x] 1.1b `TcsAttributeDef.h` 定义行 + `TcsAttributeInstance.h` 实例：`OverrideTieBreak`（属性定义侧声明语义方向、随实例展开——与 `Bounds`/`ValueDomain` 同款）；`AddAttribute` 复制该字段
- [x] 1.1c 折叠器选择逻辑：`GetTcsOverrideTieScore`（四策略折算成正向得分）+ `IsStrongerTcsOverride`（三级全序比较，**精确关系**不用近似相等——模糊相等破坏传递性即破坏顺序无关）；第三级有符号值兜底"策略下打平"（如 `OTB_MaxAbs` 的 ±5）
- [x] 1.1d 数据驱动路径：`FTcsAttrModDefTableRow::OverridePriority`（带 `EditCondition` 只对覆盖带显示）+ `IsDataValid` 对"非覆盖带填了优先级"给**警告**（Valid 但不得静默）
- [x] 1.1e 管线收集期带上优先级（`Entry.OverridePriority = Modifier.OverridePriority`）并把实例策略传给折叠；**框架不定义任何其它"谁盖谁"规则**（无来源分层、无方向字段）——强弱唯一由优先级 + 属性策略决定

### 管线主体（模块内部）

- [x] 1.2 `Public/Attribute/TcsAttributePipeline.h`（**声明在 Public**——2026-09-18 用户拍板移出 Private：确认未来有跨模块消费者）`FTcsAttributePipeline` 类——`EvaluateCurrent` / `ApplyModifier` / `RemoveBySource` / `BeginBatch` / `Commit` / `PeekPending`（动词纪律：`Evaluate` 非 `Resolve`）；公开面 = Read/Write 两区入口、机制面（重算内核/依赖登记/求值栈）留 `private:`；类带 `TCSATTRIBUTE_API`（有 out-of-line 成员）
- [x] 1.3 `Private/Attribute/TcsAttributePipeline.cpp`（**实现在 Private**）：**recalc** —— 收集（按 `Op` 分桶）→ 操作数求值（`OPK_AttributeScaled` = `Coefficient × Current(Attribute)`，走"读即登记"）→ `FoldTcsAttributeBands` → 值域收口 → 写 `CachedCurrent` + 清脏；**管线是 `CachedCurrent` 唯一生产者**
- [x] 1.4 值域收口：`AVD_Clamp`（默认）/ `AVD_Wrap`（按跨度回卷）/ `AVD_Custom`（**策略接口不在 R3**——ensure 或警告日志 + 按 Clamp 收口，不静默）；`ABM_Dynamic` 边界先按管线求值（同样登记依赖边）
- [x] 1.5 依赖登记与环检测：求值期"读即登记"（含动态边界与 `AttributeScaled`）→ 被读属性变更自动标脏；**Tarjan SCC** 严格模式（成环 ensure + 拒绝该边）；**重算轮数上限与环判定解耦**（不得以轮数判环）；图每轮重建不缓存
- [x] 1.6 变更广播：比较 `epsilon = 1e-5`，有实质变化才经总线立即通道发 `FTcsAttributeChangedEvent{ Unit, Attribute, OldValue, NewValue }`（`Reason` 字段待有消费者再加）
- [x] 1.7 事务：`BeginBatch` / `Commit`（嵌套计数，最外层提交才 flush）+ **唯一提交点、失败零写入** + 批内多次变更只重算一次、每属性最多广播一次 + 未开批即隐式批
- [x] 1.8 `PeekPending`：批内候选值预览（不写回/不标脏/不广播）；无批时等于当前值
- [x] 1.9 `RemoveBySource(Unit, Source)`：扫描**全部属性的修正器槽位 + 冻结暂存区**（命中暂存区留日志）→ 标脏 → 重算 → 按变更规则广播；无匹配 = 正常路径（不 ensure）

### 冻结/解冻（Task 4 遗留，规格 delta 取自决策文档 §附录）

- [x] 1.10 `Public/Attribute/TcsAttributeStore.h`：暂存区 `TMap<FTcsAttributeName, FTcsAttributeInstance> FrozenAttributes` + `FindFrozenInstance(Name)`（不 ensure）；注释写明**双态约束**（同名不同时存在于两处）
- [x] 1.11 `Private/TcsAttributeSubsystem.cpp`：`RemoveAttribute` 改**冻结**（整条搬入暂存区、不销毁槽位、输出日志）；`AddAttribute` 改**解冻优先**（暂存区命中 → 整条搬回 + 解冻日志；未命中 → 按定义新建 + 新建日志）；两者 MUST 走同一 store 变更路径与事务纪律（批内可见性与 modifier 一致）
- [x] 1.12 边界行为：施加到**已无实例**属性上的修正器 = 忽略 + 日志（不 ensure）；单位注销释放暂存区

### 门面与文档

- [x] 1.13 `Public/TcsAttributeSubsystem.h` + `Private/TcsAttributeSubsystem.cpp`：门面**转发**暴露管线功能（**推荐路径仍是门面**——唯一入口）；`RemoveBySource` 落位；门面头仍以 PIMPL 前向声明持有管线（不因管线公开而把其 include 拉进门面头）
- [x] 1.14 文档回写：plan1 Task 5 勾选 + 实施注记；02 §2.2a（冻结条的实施状态从"未落地"改为"已落地"）；README 决策日志；`openspec/project.md` 命名约定如需（折叠纯函数名）
- [x] 1.15 管线可见性口径（2026-09-18 用户拍板）：`TcsAttributePipeline.h` 从 `Private/Attribute/` 移至 `Public/Attribute/`（实现三份 `.cpp` 留 Private）；类加 `TCSATTRIBUTE_API`；头注释写明调用纪律（门面优先 / 直调为逃生口 / 机制面不属契约 / **实例由门面拥有、MUST NOT 跨帧持有**）；证据：导入库已导出该类的 out-of-line 成员（跨模块可链接）

## 2. Verification

- [x] 2.1 Development Editor 编译通过（UBT，LAC 项目，零警告）
- [x] 2.2 临时装置（`Source/TcsAttribute/Private/Testing/`，**不入库、不进规格**）`Tcs.Test.Attribute` 增补：
  - **折叠**：五带公式对照（手算值）、Override 覆盖一切（FlatAdd 一并被覆盖）、排序无关（同集两排列结果一致）、空集返回 Base
  - **重算与脏**：干净零重算（缓存值不变）、脏则重算并清脏、`BaseValue` 变更触发重算
  - **收口**：Clamp 钳制 / Wrap 回卷 / `AVD_Custom` 显式提示 + 按 Clamp / 动态边界先求值
  - **依赖**：`AttributeScaled` 读即登记（主属性改 → 派生属性自动更新）；成环被拒（ensure）且不进入无限重算
  - **事务**：批内两次改同属性只广播一次（计数）、未开批即隐式批、失败零写入、`PeekPending` 批内预览与 `EvaluateCurrent` 事务期读旧值
  - **来源级联**：`RemoveBySource` 全量摘除并广播；**扫描面含暂存区**（先冻结再撤来源 → 解冻后不带回死来源数值）；无匹配来源不报错
  - **冻结/解冻**：逐字段保真、解冻取回冻结前的值、双态互斥、单位注销释放暂存区
  - **覆盖带口径（2026-09-18 增补）**：默认策略 = 历史的取最大值；`OTB_Min`/`OTB_MaxAbs`/`OTB_MinAbs` 三策略各一例；优先级压过数值大小；绝对值打平时取有符号值大者（全序）；两组代表性条目**反序结果一致**；非覆盖带忽略 `OverridePriority`；模板校验对"非覆盖带填优先级"给警告
  - **覆盖带端到端**（经门面，独立单位）：定义的策略展开到实例；同优先级按策略取最小（-10/-20 → -20）；优先级压过策略（+7/优先级 9 胜 -10、-20）；**撤销赢家后自动递补**（读侧选优——无"复活/休眠池"状态）
- [x] 2.3 用户 PIE 实测确认（含拒绝面命令 `Tcs.Test.Attribute.Dangling` 按需）——**第三轮 `Tcs.Test.Attribute` 67 条全 PASS（2026-09-18 用户实测）**；前两轮 6 条失败已按上图修复（2 装置缺陷 + 2 实现缺陷 + 1 语义澄清）
- [x] 2.4 停点待用户检查（提交经用户明确授权）

## 3. 后续任务输入（不在本变更范围）

- **Task 6（验收装置）**：检查点 2（一次重算 + 一次广播）/ 3（批内只广播一次 + `PeekPending`）/ 4（悬空句柄）在屏显侧复验。
- **M5 / plan2**：参数链与 TcsDamage 流程属性容器接入 `FoldTcsAttributeBands`（本任务只提供公用件，不替它们接线）。
- **M8**：`AVD_Custom` 的值域策略接口（`IValueDomainPolicy`）与四联校验矩阵。

## 4. 实施注记（2026-09-18，落地期偏差登记）

- **偏差 1：`FTcsCombatEntityHandle` 升格为反射 USTRUCT + `int64 Id`**——事件载荷经总线（`FInstancedStruct`）MUST 反射可见，而该句柄原为纯 C++ 值类型；反射类型按导出宏纪律必须带模块导出宏；`uint64` 不被 UHT 支持作属性类型故改 `int64`（句柄恒为正）。规格 `instance-handle-pool` 已按此 MODIFIED；副作用正面：PV-1 的上下文 `Subject`（需 UPROPERTY）随之解禁。
- **偏差 2：新增 `SetBaseValue`**——规格列出的"改基值"写操作类缺落点（plan1 接口清单原无此 API），补 `UTcsAttributeSubsystem::SetBaseValue`（标脏 + 事务纪律重算/广播）。
- **偏差 3：成环语义按设计原文"拒绝该边"**（不是整条属性零写入）——登记边时 Tarjan SCC 检测，成环则撤销刚登记的边 + ensure，求值有限不递归；`attribute-pipeline` delta 的场景措辞已对齐。重算轮数上限（64）仅作收敛安全网、与环判定解耦。
- **偏差 4：每单位运行时状态住 `FTcsAttributeStore`**（批深度 / 依赖边）——随单位注销释放，无跨单位残留；`attribute-transaction` delta 已同步。
- **偏差 5（用户首轮 PIE 实测暴露，2026-09-18）：`AddAttribute` 新建实例改为"新建即脏"**（原规格写 `bDirty = false`）——原语义下"从定义行抄来的缓存初值"被当成已结算值，**值域收口被整段跳过**：基础值 150、边界 0..100、Clamp 的属性会一直读到 150，直到某个后续写入把它标脏（静默错值；装置断言"收口 Clamp 越界钳到上边界"首轮 FAIL 即此）。改为新建即脏后，初值在"批外读取或提交"这一刻由管线结算——那一刻单位上的属性图通常已搭好（动态边界引用的属性、同期挂上的修正器都在位），**定义/添加顺序不影响结果**；反之若在 `AddAttribute` 内立即结算，动态边界会读到尚未添加的引用属性（求值 0）→ 把原值钳成 0 且无人再标脏。代价：首次结算可能广播一次（旧值 = 原始基础值）——"结算前的缓存值不对外承诺"。`attribute-store` / `attribute-pipeline` delta 已按此改写（含"越界初值经结算收口"新场景）。
- **偏差 6（同轮实测暴露）：`RemoveBySource` 从冻结暂存区摘除后 MUST 标脏该冻结实例**——否则来源在冻结期结束后，解冻会把"已被撤销来源的旧缓存值"带回（装置断言"扫描面含冻结暂存区"首轮 FAIL 即此：期待 77、实得 110）。
- **语义澄清 7（同轮暴露）：Wrap 的"跨度"成立条件写进规格**——`AVD_Wrap` 只在**两侧边界齐备且 `Max > Min`** 时回卷；跨度未成立（任一侧 `ABM_None`，或 `Max ≤ Min`）时**不回卷、返回聚合原值**（确定、不 ensure）。首轮 FAIL"收口：Wrap 按跨度回卷"根因是**装置夹具**：给 `PWrapped` 用的是只有上界的 `ClampBounds`（下界三态 `ABM_None`），而规格场景的前提是"值域 0..100"——实现按"跨度未定义 → 不回卷"返回了原值 150。已修夹具（`WrapBounds` = `ABM_Static(0)..ABM_Static(100)`）并**新增一条退化解检查**（只设上界 → 返回 150）把该分支钉住；规格补"跨度未成立时退化为原值"场景。"Wrap 却没给跨度"属作者侧配置错误，不在热路径拦截，留给 M8 的定义校验矩阵。
- **装置修（非实现，同轮）**：来源发号器改**单一实例**（原为临时对象，每次构造计数器归零 → 四个来源同 Id → 一次 `RemoveBySource` 摘光全部来源，该断言形同虚设）；定义登记改幂等（定义表跨命令调用存活——二次运行同一命令不得重复登记）；管线段加"预热结算"循环（新建属性的初值先读一遍落账，后续广播计数才只反映真正的变更）。
- **增补 8（2026-09-18 用户拍板，落地期）：覆盖带的强弱口径**——原口径"Override 组取最大值"隐含了"数值大 = 更强"，而**数值本身不含方向**：对承伤倍率/冷却这类"越低越强"的属性，取最大值恰好取到最温和的一条，且框架无从知道语义方向（同一套规则不可能对护甲与承伤倍率都对）。落地三层阶梯：**①`OverridePriority`（修正器侧，大者胜，唯一第一裁决键）→ ②`OverrideTieBreak`（属性定义侧，封闭四值：取最大/取最小/绝对值最大/绝对值最小）→ ③有符号值**（补齐全序——`OTB_MaxAbs` 下 ±5 这类"策略下打平"必须有确定答案，否则赢家取决于遍历顺序，破坏"顺序无关"承诺）。
  - **策略住属性定义而非修正器**：放修正器上会变成"两个来源各说各话"，等于又需要一条规则来裁决规则（与用户否掉"跨来源分层约定"同一条理由）。
  - **不开放自定义策略**：热路径比较函数必须全域且确定（与 `AVD_Custom` 不同源——后者是值域语义、另有按 Clamp 的确定性回落）。用户原话："这个就不开放 CustomStrategy 了"。
  - **不提供方向字段**：用户口径"跨来源的优先级协调**完全交给 Priority**，不然属于二次规则，到底以哪个为准对使用者有争议"——故框架不定义任何其它"谁盖谁"的规则（无来源分层、无按属性方向的隐含调整）。
  - **默认值 = 历史行为**：全优先级 0 + 默认 `OTB_Max` 时逐位等价于旧的"组内最大值"，已有检查全部保持绿。
  - **数据驱动路径同步**：模板行也持 `OverridePriority`（否则 ATOM 数据侧写不出优先级）；`IsDataValid` 对"非覆盖带填了它"给警告（不静默、也不当错误——配置本身无害）。
  - 顺带澄清 `SortKey`（用户提出"在 02 里没找到它的作用"）：它在**属性折叠里是零语义的展示/审计位**（D5-5 v3 起，规格有专门场景钉住），但在 **09-module-damage 里是有语义的**（免疫/减伤裁决、消耗型流程步骤都靠它"选一"）——两者作用域不同，**不得**用 `SortKey` 承担覆盖带强弱（复用会造第二真相）。02 §2.1/§2.2 只列了字段、没写用途，本轮补一句定位。
- **增补 9（2026-09-18 用户提问带出的口径）：Base 与 Current 共用一条事件（当前值事件），不按写点分事件**——`FTcsAttributeChangedEvent` 的语义是"**对外可读的当前值变了**"（结果事件），不是"某次写操作发生了"（操作事件）。因此：①`SetBaseValue`（改基值）**不单独发事件**——基值变了若当前值跟着变，就发这一条；若被值域收口吃掉（越界钳回原值）则**不发**，这是"未变不广播"的直接结果，也是**故意的**（对外确实没变）；②挂/摘修正器、依赖属性连带变化同理，都只在当前值确实动了时发这一条；③因此本事件 **MUST NOT** 被当作"基础值变更日志"或"写操作流水"——要那类信息得另立事件或读侧自算。
  - **为什么不分两条**（被否方案）：base 与 current 各发一条 = 一次写操作产生两条通知（订阅方要做去重与顺序假设），且 clamp 场景下 base 变了 current 没变 → "只有 base 事件"，UI 若只订阅 current 就漏掉成长信息，等于把"哪个值才是对外真相"重新变成两套订阅面（与"单一真相"纪律冲突）。
  - **要区分"为什么变"**：那是 `Reason` 扩展位（02 §4 已预留），本任务**不预建**——R3 无消费者，且 Reason 在"多因叠加"（基值改 + 依赖属性同时改）时语义不精确，宜等真实消费者再定。
  - **`Reason` 的取值域候选与"先不加"决定（2026-09-18 用户拍板）**：候选 = **位掩码** `None=0 / Base=1<<0 / Modifier=1<<1 / Dependency=1<<2`（位 0 = None，与既有"Custom 固定值 1"的逃逸位约定同族；项目已有 `ETcsValueConventionFlag` 位掩码先例）。**决定：本任务不加，等第一个真实消费者（血条飘字要区分成长/buff、成长数字动画）出现再定**——现在加等于在无需求下猜取值域（"等级成长"算 Base 还是另立 Level 位？猜错概率不低），而加字段对已有订阅方是**纯增量**（新字段不破坏订阅）。
    - **判别位的语义澄清（关键）**：本事件**广播的前提就是当前值变了**，故"是 CurrentValue"对每条事件恒真——载荷里能表达的只有"**本次变化的原因里包含什么**"（"基础值被改写过了"等），**不是**"这条事件属于 base 还是 current"。单值枚举/布尔表达不了"基值与当前值同时变"（`SetBaseValue` 在无边界时两者同时变；批内改基值 + 挂修正器还会合并成一次广播），故未来落地时 MUST 用位掩码而非布尔。
    - **它盖不住的那一格（已知并接受）**：基值改了、但被值域收口吃掉（满血时提上限 → 当前值仍等于上限）→ **当前值没变 → 事件不发**。三条出路与被否：①保持现状（事件 = 结果事件，**采纳**）；②基值变更**无条件**发事件（事件退化为操作事件，订阅方要处理"值没变的事件"，与"未变不广播"冲突，**否**）；③另立 `Tcs.Event.Attribute.BaseChanged`（同域父节点下语义干净，但多一条订阅面，**暂否**——多数漏报场景已由边界属性自己的 ValueChanged 覆盖：HP 被 MaxHP 钳住时 MaxHP 会发它自己的事件）。
  - **属性上线/下线**（`AddAttribute` / `RemoveAttribute` 冻结）当前**只出日志、不出事件**（D2-14 不做溯源，R3 无消费者）；将来若需要，它们挂 `Tcs.Event.Attribute` 父节点下（这正是三层命名的收益：同域事件可整组订阅——前提是原生层级匹配落地，见 01 §2.2 总线输入）。
- 编译：Development Editor 通过（零警告）。装置：管线段新增多项检查 + 拒绝面三条故意 ensure + 模板校验两条 + 覆盖带六例/端到端四例。**用户 PIE 实测三轮：首轮 46/6（已修）、次轮 51/1（Wrap 夹具，已修）、第三轮 67 条全 PASS（2026-09-18）→ 本提案归档。**
