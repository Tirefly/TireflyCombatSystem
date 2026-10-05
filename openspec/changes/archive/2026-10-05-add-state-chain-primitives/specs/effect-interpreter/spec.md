## ADDED Requirements

### Requirement: 链运行态来源锚点与因果边

`TcsEffect` MUST 让每一次链运行持有**一个进程内唯一的来源锚点**（`FTcsSourceHandle`，经统一发号器 `FTcsSourceHandleRegistry::Allocate()` 发放）与**一条因果边**，分别装在黑板 `FTcsEffectContext::RunSource` 与 `FTcsEffectContext::CausedBy` 上。二者职责 MUST 分开（2026-10-05 拍板；起因见下"为什么不是一个字段"）：

- **`RunSource` = 身份（发号规则）**：`ExecuteChain` MUST 在入参黑板未自带有效 `RunSource` 时发放一枚（`ExecuteChainForCaster` 一类门面包装随之覆盖）；**子链 MUST 另发新号、MUST NOT 沿用父链的**（每一次运行 = 一个独立施加方）；**MUST NOT** 复用别的运行的锚点。语义 = "由本次运行产生的东西同源"：同一运行内重复施加同一状态 ⇒ 续杯；两次运行 ⇒ 异源 ⇒ 可叠层；
- **`CausedBy` = 因果边（写入规则）**：触发行起链时 MUST 写该行的 `Source`（`FTcsEffectTriggerInstance::Source`——定义期登记的行是该行的来源句柄、状态施加期登记的行是该状态实例的级联锚点）；子链起链时 MUST 写**父链的 `RunSource`**（逐跳成边）；宿主 / 流程直接起链时可留无效（无成因可记）；
- **因果边 MUST NOT 参与撤销或正确性判定**：MUST NOT 按 `CausedBy` 做级联撤销、MUST NOT 用 `CausedBy` 判共存（续杯 / 叠层）——原始版既有结论（父来源生命周期可先于子级结束；域之间不应按来源互相影响生命周期）。撤销一律按各实例自己的锚点；
- **边只落"直接成因"一跳**：完整血缘（成树、遍历、Explain、回放）归 R8（台账「溯源树 / 血缘」行）；链路 MUST NOT 为此预建祖先数组或反查注册表（原始版 `CausalityChain` 的教训：只写不读的字段不构成能力）；
- **MUST NOT 作配置数据**（同 `effect-chain` 黑板纪律：不加 `EditAnywhere`、不进 Def 资产）。

**为什么不是一个字段（判据）**：身份要"每次运行唯一"才能让"异源叠层"可达；成因要"指回上游"才能回答"这条链是谁触发的"。合一即二者互斥——定义库登记的系统级触发行**全部共用一枚**来源句柄，把该值当身份用会让所有系统级规则起出的链互为同源，链路径下**只能续杯、无法叠层**（"受击叠一层"这类内容不可达）。

#### Scenario: 起链自动获得锚点且两次运行互异

- **WHEN** 调用方构造一份不带 `RunSource` 的黑板调 `ExecuteChain`，再做一次同样的调用
- **THEN** 两次运行的黑板 `RunSource` 均有效（非 0）且互不相等

#### Scenario: 子链另发身份、成因指向父链

- **WHEN** 一条链里跑 `RunSubChain` 起子链，父子两条运行各自读黑板 `RunSource` 与 `CausedBy`
- **THEN** 子链的 `RunSource` 与父链的不同（另发新号），且子链的 `CausedBy` = 父链的 `RunSource`

#### Scenario: 触发行起链时成因是该行的来源

- **WHEN** 一条已登记的触发行命中并起链
- **THEN** 该运行的黑板 `CausedBy` = 该行实例的 `Source`（溯源读数：可据此判定"这条链由哪个来源触发的"）

#### Scenario: 因果边不参与撤销

- **WHEN** 检查链运行态与状态撤销路径的依赖关系
- **THEN** 撤销只按状态实例的级联锚点（`FTcsStateInstance::CascadeAnchor`）与触发行的 `Source` 值执行，无任何按 `CausedBy` 级联的路径

### Requirement: 属性修改步骤（ModifyAttribute）

`TcsEffect` MUST 提供 `FTcsStepModifyAttribute`（D4-16 十五原语之一，属本模块）：

- 字段：`Target: FTcsCombatEntityHandle`（无效则取黑板 `Targets[0]`，仍无效 ⇒ 软失败）/ `Attribute: FGameplayTag` / `Op: ETcsAttributeOp`（**复用 M2 运算带枚举，不新造词表**）/ `Operand: FTcsParamValue`（复用统一数值载体）；
- 执行体 MUST 构造一条一次性 `FTcsAttrModInstance` 并经**属性访问解析点**写入账本（解析点口径见 `attribute-pipeline` 的「属性门面的解析点」）；条目的 `Source` MUST = 黑板 `RunSource`（⇒ 同一次运行挂上的条目可按该锚点回收）；
- **依赖边界（本步骤是首次真实使用）**：`TcsEffect` 允许依赖**下层**领域模块 `TcsAttribute`，**MUST NOT** 依赖上层领域模块（`TcsDamage` / `TcsTargeting` / `TcsState` / `TcsSkill`）——头注释 MUST 写明该边界；
- 步骤为**即时步骤**：恒 `TSR_Completed`；失败面（目标无效 / 单位无属性账本 / 属性键无效）MUST 记 `Warning` 后按完成处理，**MUST NOT 断链**（`ETcsStepResult` 无中断档，软失败接管归 `OnError`）；
- 自注册 MUST 走 `effect-step-dispatch` 的静态自注册宏（键 = 步骤 struct 的 `const UScriptStruct*`）。

#### Scenario: 链里改得动属性账本

- **WHEN** 一条链执行 `ModifyAttribute(属性 A, 加, 10)` 后读该属性的当前值
- **THEN** 读数比执行前大 10，且账本上多出一条 `Source` = 本次运行 `RunSource` 的条目

#### Scenario: 单位无属性账本时不断链

- **WHEN** 对没有属性账本的单位执行本步骤
- **THEN** 记一条 `Warning`、本步返回 `TSR_Completed`，后续步骤照常执行（链不断）
