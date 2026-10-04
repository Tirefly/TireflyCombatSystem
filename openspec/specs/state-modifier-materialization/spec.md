# state-modifier-materialization Specification

## Purpose
定义 M3 状态层**改数值**的那条腿（D3-19 落地）：施加状态时把定义里的修正器模板引用行**物化**成 M2 属性账本条目——模板解析与操作数求值（引用类操作数经**该实例自己的参数快照**取值、值约定在物化边界转规范值一次）、施加与刷新两个挂点（同批内完成、排在生命周期广播之前）、移除与到期的**按来源级联摘除**，以及"取属性门面"这一个可替换的解析点。它让"施加态"第一次真正改得动属性数值，并为行为面（触发行、行为 Fragment）钉死"同一个状态实例 = 同一个来源句柄"这条级联锚点。

## Requirements

### Requirement: 修正器物化入口与逐条流水

TcsState MUST 提供修正器物化入口 `FTcsStateModifierMaterializer::Materialize(UTcsStateSubsystem& Subsystem, const FTcsBuffDef& Def, const FTcsStateInstance& Instance, TArray<FTcsAttrModInstance>& Out)`，把 `Def.ModifierRows` 逐条物化为属性账本条目（D3-19：模板 + 引用行 + 覆写走参数传值）。

输入取**实例**而不是散字段：`Unit` / `Instigator` / `Level` / `Source` / `ParamSnapshot` 全是实例已有的信息（实例自持 `Unit` 正是为了让物化不必遍历桶找单位）——多传几个形参只会让"某处少填一个"成为可能，而少填在源侧表现为**静默落兜底**而不是报错。

逐条流水 MUST 为：

1. **解析模板**：`TSoftObjectPtr<UTcsAttrModDef>` 先取**已加载对象**（`Get()`），仅在为空时才同步加载（`LoadSynchronous()`）——已持有对象的路径 MUST NOT 触发加载；空引用 MUST 跳过该条并留 Warning。
2. **求值操作数**：`Kind == OPK_Literal` ⇒ 以物化上下文求值 `Operand.Literal`（`FTcsParamValue`，可为 `Literal` / `ParamRef` / 等级类源 / 宿主源）；`Kind == OPK_AttributeScaled` ⇒ **不物化**（该种类按设计保持 live 求值），`Attribute` 与 `Coefficient` 原样进账本。
3. **值约定**：模板行 `ValueConvention` 非 `VCF_None` 且数值来源的能力位允许时 MUST 在**物化边界**经 `FTcsValueConvention::ConvertToCanonical` 转规范值（**只转一次**）；能力位为假时 MUST NOT 转换（运行期静默降级，错配由作者期 `IsDataValid` 暴露）。
4. **装配条目**：经 `FTcsAttrModInstance::MakeFromDef(const FTcsAttrModDefTableRow& Row, const FTcsAttrModOperand& Operand, FTcsSourceHandle Source)` 装配——`Target` / `Op` / `OverridePriority` / `SortKey` 取自模板行，`Source` MUST 为传入的状态来源句柄。

`Materialize` MUST 先清空 `Out`（调用方无需自行清空），且 MUST 是**只读操作**：不写账本、不广播、不产生任何世界副作用。

#### Scenario: 字面量模板物化为账本条目

- **WHEN** 以一条 `OPK_Literal` 模板（`Target` = 某属性、`Op` = `TAO_Add`、`Literal` 字面量源、`OverridePriority` / `SortKey` 有值）调用物化入口
- **THEN** `Out` 增加一条条目，其 `Target` / `Op` / `OverridePriority` / `SortKey` 与模板行一致，`Operand.Literal` 为已解析的规范值，`Source` 等于传入的来源句柄

#### Scenario: ParamRef 操作数从状态快照取值

- **WHEN** 模板操作数的数值来源是引用源（引用某参数键），而该状态实例的快照含该键
- **THEN** 物化出的账本条目取值等于快照内的冻结值（**不是**引用源的兜底值）

#### Scenario: 值约定在物化边界转一次

- **WHEN** 模板配字面量源且行约定列为 `VCF_Percent`、书写值为 85
- **THEN** 账本条目取值为 0.85（转换发生一次，账本不做二次猜测）

#### Scenario: 属性换算操作数不被物化

- **WHEN** 模板操作数种类为 `OPK_AttributeScaled`（配属性与系数）
- **THEN** 账本条目保留该种类、属性与系数（`Literal` 不参与求值，保持 live 求值路径）

### Requirement: 物化求值的参数表即状态快照

物化求值 MUST 复用与快照构建**同一处**的上下文装配（`Subject` / `Instigator` / `EffectiveLevel` / 等级读口与快照构建同值），且上下文中的 **`ParamTable` 位 MUST 由该状态实例的快照承担**——"本次求值的参数表"在物化点就是该实例的快照（D3-19 原文："物化点从施加方 `ParamSnapshot` 解析"）。

MUST NOT 为同一次求值再开一条并行的数值表通道（同一个问题只能有一个答案）。引用源的 miss 语义 MUST 仍由该源自身的兜底规则承担——物化器 MUST NOT 复用、复制或重写键查找与兜底逻辑（快照读取入口唯一）。

#### Scenario: 上下文参数表就是快照

- **WHEN** 以某状态实例的快照作为参数表调用物化入口，模板操作数为引用源且该键在快照内
- **THEN** 求值命中快照（不经任何旁路通道），且快照缺键时落该源自身的兜底值

### Requirement: 施加时挂载修正器

`ApplyState` MUST 在快照构建完成后、`TcsEvent.State.Applied` 广播**之前**完成修正器挂载，且 MUST 在**同一个属性变更批**内完成（`BeginBatch` → 逐条 `ApplyModifier` → `Commit`）——多个修正器只触发一次重算与一次广播，且订阅者在 `Applied` 回调里读到的已是挂载后的数值。

- 属性门面一律经**属性访问解析点**取得；
- `Def.ModifierRows` 为空时 MUST NOT 产生账本条目、MUST NOT 产生额外广播；**施加路径**在此情形 MUST NOT 触碰属性账本（连批都不开）——零条不是错误。**刷新路径不受此限**：它 MUST 先按来源摘掉上次施加留下的条目，再按新快照挂新；
- 单条模板解析失败 MUST 跳过该条并留 Warning，**不中断施加**（状态已生效，缺一条修正器不等于"施加失败"）；
- 属性门面不可解析（世界类型不支持）或**目标单位没有属性账本**（状态与属性是两套登记，这是合法配置）时 MUST 无操作 + `Log` 级记录，MUST NOT ensure、MUST NOT 留 Warning；
- **挂载提交之后 MUST 重新定位实例与桶**：提交会重算 + 广播（属性变更事件），订阅者可在其中重入状态操作（移除本实例 / 注销单位），那会让实例指针与桶引用双双失效；重新定位后实例已不在册时 MUST 不再补播施加事件（对已不在册的实例广播是错语义），并留 Warning。

#### Scenario: 施加后账本条目出现且数值可观测

- **WHEN** 对一个已持有目标属性的单位施加一个带修正器行的状态
- **THEN** 该属性的账本出现条目，且求值当前值读数与施加前可区分（差值等于物化出的操作数）

#### Scenario: 施加广播前已完成挂载

- **WHEN** 订阅者订阅 `TcsEvent.State.Applied` 并在回调里读该属性当前值
- **THEN** 读到的已是挂载后的数值（挂载先于广播）

#### Scenario: 无修正器行的定义不产生账本条目

- **WHEN** 施加一个 `ModifierRows` 为空的状态
- **THEN** 不产生任何账本条目、不产生属性变更广播，且不留下未配对的批（批深度回到 0）

#### Scenario: 单位无属性账本时静默无操作

- **WHEN** 对一个没有属性账本的单位施加一个带修正器行的状态
- **THEN** 状态照常施加且不产生任何账本操作、不留红字、不 ensure

### Requirement: 刷新时按来源重物化

刷新路径（共存判据判定命中既有实例）MUST 在**重建快照之后**先按来源摘除该实例的既有修正器条目、再按新快照重新挂载，`Source` MUST 保持该实例的来源句柄不变——使账本条目的**条数与数值恒与最近一次施加的快照一致**（"修改 = 重新施加"在账本面上的体现）。

#### Scenario: 刷新后数值随快照变化且条数不累加

- **WHEN** 对同一单位以同一 `DefTag` 再次施加并传入不同的覆盖值，使物化操作数变化
- **THEN** 账本条目数不增加（旧条目已按来源摘除），且取值等于按新快照物化的结果

#### Scenario: 刷新不更换来源句柄

- **WHEN** 对比刷新前后账本条目与实例的来源句柄
- **THEN** 两者相等（级联撤销仍能一次摘净），且 `FTcsStateInstance::Source` 未变

### Requirement: 移除与到期时按来源级联摘除

`RemoveState` / 到期路径 MUST 在进入 `Expiring` 迁移**之前**按该实例的来源句柄做级联摘除（`RemoveBySource`），并 MUST 在同一个属性变更批内提交——`TcsEvent.State.Expired` / `TcsEvent.State.Removed` 的订阅者在回调里读该属性能读到**已归位**的数值。`UnregisterUnit` MUST 复用同一条移除链。

撤销顺序 MUST 为：**按来源摘除修正器（批内提交）→ 撤时间条目 → `Expiring` → 广播 → 归还槽位**。

**摘除 MUST 排在"取实例指针"之前**：摘除的批提交会重算 + 广播（属性变更事件），订阅者可在其中重入状态操作（移除本实例 / 注销单位）⇒ 摘除之后 MUST **重新定位桶与实例**，重入造成的失效由"桶不存在 / 实例查不到"两道守卫接住（MUST NOT 在摘除前后跨用同一个实例指针）。

摘除条数 MUST 由属性账本返回；返回 0（无匹配）MUST 是正常路径（来源可能只挂过已摘除的条目），不 ensure。

#### Scenario: 驱散后条目消失且属性归位

- **WHEN** 移除一个已挂载修正器的状态实例
- **THEN** 账本条目消失、属性当前值回到施加前，且 `TcsEvent.State.Removed` 的订阅者在回调里读到的已是归位值

#### Scenario: 到期路径同款

- **WHEN** 让时钟推进跨过到期时刻使实例到期
- **THEN** 账本条目同样被摘除、属性归位（`Expired` 与显式移除走同一条摘除链）

#### Scenario: 单位注销清理该单位全部条目

- **WHEN** 注销一个持有多个带修正器状态实例的单位
- **THEN** 该单位属性账本上这些实例来源的条目全部消失（逐条走同一条移除链）

### Requirement: 属性访问解析点

TcsState MUST 只经**单一解析点** `FTcsStateAttributeAccess::Resolve(const UWorld*)` 取属性门面（住 `Private/State/`）。该解析点的头文件名 MUST NOT 与将来 TcsEffect 侧的同类解析点重名（UHT 要求全项目头文件名唯一）。

本轮允许触碰的属性 API **上限**为：语义面三个（`UTcsAttributeSubsystem::ApplyModifier` / `RemoveBySource` / `EvaluateCurrent`）+ **事务对**（`BeginBatch` / `Commit`）+ **一个存在性判据**（`ResolveStore`，只作"账本认不认识这个单位"，MUST NOT 用于取值）。上限之外 MUST NOT 暴露任何属性 API；解析点 MUST 以注释写明"将来补属性访问注入契约时**只换这一处**"。

**上限靠结构而不是靠记性**：解析点 MUST 是**薄包装**（暴露面 = 白名单方法，而不是把属性门面指针交出去）——越界因此在编译期就不可能。

- **事务对为什么在允许面内**：逐条挂载若不开批，N 条修正器 = N 次重算 + N 次广播（重入窗口放大 N 倍），且"摘旧与挂新在同一批内"是"中间态不被订阅者看到"的前提；
- **存在性判据为什么必要**：状态与属性是**两套登记**——状态可以施加到没有属性账本的单位上（合法），而 `BeginBatch` 对未注册单位会 `ensure`。挂点须先问一句，把"没有可改的属性"当配置状态静默处理（`Log` 级、不留红字）。

MUST NOT 在 TcsState 内另建第二处属性门面解析或第二套属性读写路径。

#### Scenario: 解析点为唯一入口

- **WHEN** 检视 TcsState 内对属性门面的取用
- **THEN** 只有该解析点取门面（物化挂载与级联摘除都经它），且暴露的转发面只有上述三个方法

#### Scenario: 解析失败不 ensure

- **WHEN** 传入非游戏世界或该世界无属性子系统时调用解析点
- **THEN** 返回空，调用方按无操作 + Warning 处理，不 ensure、不崩溃

### Requirement: 修正器来源句柄的身份一致性

物化的每一条账本条目 MUST 以该状态**实例的来源句柄**（`FTcsStateInstance::Source`）作为 `FTcsAttrModInstance::Source`；刷新 MUST NOT 更换它。由此 MUST 成立：**同一个状态实例 = 同一个来源** ⇒ 一次按来源摘除清掉该实例的全部修正器条目（R5 Task 6 的内联触发行退订将复用同一句柄，一次摘净两族条目）。

#### Scenario: 同实例多条修正器共用来源

- **WHEN** 一个状态的 `ModifierRows` 物化出两条作用于不同属性的条目
- **THEN** 两条的 `Source` 相等，且按该来源一次摘除即两条全清
