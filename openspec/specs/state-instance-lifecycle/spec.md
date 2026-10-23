# state-instance-lifecycle Specification

## Purpose

定义 M3 状态层的**运行态骨架**：状态实例句柄与池化实例记录、per-entity 桶注册表与代际校验、世界级状态门面（施加 / 查询 / 移除 / 生命周期操作 + 定义登记口）、六枚生命周期事件 tag 与反射载荷，以及阶段迁移与广播点的纪律——让"状态定义"能变成可查、可移除、可被订阅的运行实例。

**边界**：本能力只覆盖"实例在册与生命周期广播"。参数快照与等级源（`PLN-R5` Task 3）、持续修正器物化（Task 4）、五轴堆叠与刷新政策（Task 5）、链原语 `ApplyState` 与行为 Fragment（Task 6）各自另有能力，本能力只保留它们的**接入位**（定义登记口、`ExtendDuration` / `SetRemaining` 签名、`EBuffApplyResult` 的 `EBAR_Stacked` 档）。

## Requirements

### Requirement: 状态实例句柄

`TcsBuff` MUST 提供 `FTcsBuffInstanceHandle`（`USTRUCT(BlueprintType)`）：两字段 `int32 Index` 与 `int32 Generation`（**展平形态**，与 `FTcsChainRunHandle` / `FTcsEffectTriggerInstance` 同款——反射与宿主脚本往返需要可反射的标量字段，对象身份不外传）。

**★ 本类型 MUST 保持为独立类型**（MUST NOT 与 `FTcsSkillInstanceHandle` / `FTcsSkillEntryHandle` 合并——合并会移除"跨桶误传编译期失败"这道现存护栏，判据见 `entity-runtime-mechanism` 能力的「统一运行态句柄判据与转换」需求）；但**判据 / 相等比较 / 哈希 / 池位模式转换的语义 MUST 收敛到一处公共件**，本类型委托它。

- `IsValid()`：`Index >= 0` 且 `Generation > 0`（0 恒为“从未分配”）；
- 相等比较按 `(Index, Generation)` 全等；MUST 提供 `GetTypeHash`（供 `TMap` / `TSet` 键控）；
- `Index` MUST 是实体桶内槽位下标；**分配代际 MUST 由共享层出线的进程级发号器（`FTcsStateGenerationIssuer`，住 `TcsState`）统一发放不复用的正奇数号**，世界拆解 MUST NOT 复位发号器；不同桶、被删除后重建的桶、不同世界即使使用同一下标，也 MUST NOT 发放相同实例身份；
- 释放 MUST 将该槽代际置为偶数；再次分配 MUST 取新的进程级奇数号，MUST NOT 仅将桶内旧代际 +1 当作分配身份；
- 句柄仍相对其世界门面有义，MUST NOT 将旧句柄跨世界重用；旧身份在新世界 MUST 无法命中。反射 `int32` 的正值代际空间耗尽时 MUST 明确失败，MUST NOT 回绕复用。

#### Scenario: 未分配句柄无效

- **WHEN** 取一个默认构造的 `FTcsBuffInstanceHandle` 并调 `IsValid`
- **THEN** 返回 false（`Index` 为负或 `Generation` 为 0）

#### Scenario: 分配出的句柄可直接判有效

- **WHEN** 施加一个状态并取回其句柄（`Index` 可能为 0）
- **THEN** `IsValid` 返回 true（合法性判据是“`Generation` 非 0”，不是“下标非 0”——下标 0 是合法槽位）

#### Scenario: 用过的句柄在槽位回收后失配

- **WHEN** 移除一个状态使槽位回收，随后用旧句柄查询
- **THEN** 查询被拒绝（代际失配），且同一槽位新分配的实例不受影响

#### Scenario: 不同实体的相同槽位身份互异

- **WHEN** 两个实体各自分配桶内第 0 个状态实例
- **THEN** 两个句柄的 `Index` 可以相同，`Generation` MUST 互异；各自调用 `GetState(Handle)` 命中各自实体，按句柄移除其中一个 MUST NOT 影响另一个

#### Scenario: 新世界不重用旧状态身份

- **WHEN** 世界 A 建出状态实例后被拆解，世界 B 再分配同一下标
- **THEN** 新句柄的代际与 A 互异，A 的旧句柄在 B 查询返回 nullptr

### Requirement: 池化状态实例记录

`TcsBuff` MUST 提供 `FTcsBuffInstance`——**纯数据**记录；它自 `PLN-REFACTOR-1` Task 2 起是**基底 `FTcsStateInstance` 的派生结构**（`struct FTcsBuffInstance : public FTcsStateInstance`），字段分**基底 6 + 域内 4 + 专属 4**：

- **基底 `FTcsStateInstance`（住 `TcsState` 共享层，6 字段）**：`DefTag` / `Entity` / `CascadeAnchor` / `ParamSnapshot` / `Level` / `ExpiryEntry`。**两域公共字段 MUST 住基底、MUST NOT 在派生结构重复声明**；
- **Buff 域内（4 字段）**：`Handle`（`FTcsBuffInstanceHandle`）/ `Source`（`FTcsSourceHandle`——**施加方来源句柄**，见下）/ `Instigator`（`FTcsCombatEntityHandle`）/ `Phase`（`EBuffPhase`）；
- **Buff 专属（4 字段）**：`Stacks`（`int32`，从 1 起）/ `DurationRemaining` / `PeriodRemaining`（`double`）/ `PeriodEntry`（周期到期条目锚点）。

**★ 桶载荷 MUST 绑派生结构 `FTcsBuffInstance`、MUST NOT 绑基底**——池按值存 `TArray<TPayload>`，绑基底会把层数 / 时值 / 周期**静默截断**（值截断，多数读数仍会过）。

**★ 本节原文字段表（改名前的形态，MUST 留痕）**：

- **身份与归属**：`DefTag`（`FGameplayTag`）/ `Handle` / `Source`（`FTcsSourceHandle`——**施加方来源句柄**，见下）/ `CascadeAnchor`（`FTcsSourceHandle`——**级联撤销锚点**，见下）/ `Instigator`（`FTcsCombatEntityHandle`）/ `Entity`（宿主实体——句柄里没有实体段，自持后 `GetState` 才是 O(1) 定位）；
> - **数值与阶段**：`Stacks`（`int32`，从 1 起）/ `Level`（`int32`）/ `Phase`（`EBuffPhase`）/ `ParamSnapshot`（`FTcsParamSnapshot`——施加瞬间冻结的生效数值，形状与构建规则见 `state-param-snapshot` 能力）；
> - **时间**：`DurationRemaining` / `PeriodRemaining`（`double`）/ `ExpiryEntry`（时值到期条目锚点）/ `PeriodEntry`（周期到期条目锚点）。
>
> **该表是 Task 2 之前的形态**（全部 14 字段平铺在一个类型里）；Task 2/3 之后 **6 个上提基底、8 个留 `TcsBuff`**（其中 `Handle`/`Source`/`Instigator`/`Phase` 归 Buff 域内、`Stacks`/`DurationRemaining`/`PeriodRemaining`/`PeriodEntry` 归 Buff 专属）。

**两个句柄的分工 MUST 守死（2026-10-05 解耦，判据见下）**：

- `CascadeAnchor` MUST 在**建出实例**时经 `FTcsSourceHandleRegistry::Allocate()` 取新值（进程内唯一、永不复用）；**刷新 / 叠层 MUST NOT 更换它**，替换路径是新实例 ⇒ 新锚点。它就是"撤销只摘本实例"的精确锚点——属性修正器条目与状态施加期登记的触发行都按它挂、按它一次摘净（`state-modifier-materialization` 与 `effect-trigger`）；
- `Source` MUST 是**施加方来源句柄**：调用方声明时沿用（同一施加方多次施加 ⇒ 同值），未声明时由门面发号（每次一枚新号）。它承担两件事——共存决策的"续杯 / 叠层"判据（`state-stacking-policies`）与生命周期事件载荷的归属读数；**MUST NOT 被当作撤销锚点**（一个来源可施加多个不同定义，按它撤销会互相误摘——这正是解耦的判据）。

**时值到期与周期到期 MUST 各持一个条目锚点**（`ExpiryEntry` / `PeriodEntry`）：`Finite` 与 `Period > 0` 是可同时成立的两个语义，挤在一个锚点里会让"撤销旧条目"分不清撤的是哪一条。

**MUST NOT** 持有：策略（`FInstancedStruct` / `TInstancedStruct` 作配置载体）、事件载荷、订阅句柄、任何 `UObject` / `TWeakObjectPtr<UObject>`（D3-7 v2 与 D2-9 纪律：值语义记录是池化与后续操作复制的地基）。**内联触发行同样不例外**：实例 MUST NOT 存触发行句柄，退订一律按 `CascadeAnchor` 级联。

**`ParamSnapshot` 内的 `SourceRef` 不构成本条的例外**：它是**已求值来源的副本**（值语义，非策略引用），作用是 Debug 可见取值来源与 Live 化零结构迁移；其内层若含对象引用，由门面的 `AddReferencedObjects` 统一补引用（GC 纪律，见 `state-param-snapshot` 能力）。

#### Scenario: 实例不持有策略与对象

- **WHEN** 检查 `FTcsStateInstance` 的字段集合
- **THEN** 无策略载体、无载荷、无订阅句柄、无 `UObject` 引用（策略归 Def 资产、载荷归事件、订阅句柄归注册表/总线）

#### Scenario: 两次施加得到互异的来源句柄

- **WHEN** 对同一实体连续施加同一 `DefTag` 两次，且共存策略使两次各自建出实例（`EGB_Custom` 判不同组 / `EGB_PerSource` 且两次来源不同）
- **THEN** 两个实例的 `Source` 互异且均非 0，且 `CascadeAnchor` 亦互异

#### Scenario: 同一来源的两个实例撤销互不牵连

- **WHEN** 以**同一个声明来源**先后施加两个**不同 `DefTag`** 的状态（各自建出实例、各自挂修正器），随后移除其中一个
- **THEN** 被移除者的修正器与触发行被摘净，另一实例的修正器与触发行**不受影响**（两实例的 `CascadeAnchor` 互异；按 `Source` 撤销会互相误摘，故撤销一律按锚点）

#### Scenario: 时值与周期各持条目锚点

- **WHEN** 检查一个 `Finite + Period > 0` 的在册实例
- **THEN** 它的 `ExpiryEntry` 与 `PeriodEntry` 是两个独立字段，各自可被独立撤销（互不挤占）

#### Scenario: 实例携带施加瞬间的数值快照

- **WHEN** 施加一个带参数行的状态后检查该实例
- **THEN** 它的 `ParamSnapshot` 含施加时刻求值得到的条目（不是空表，也不随外部输入变化）

### Requirement: per-entity 桶注册表与代际校验

`TcsBuff` MUST 提供 `FTcsBuffRegistry`（= `FTcsEntityRuntimeRegistry<FTcsBuffInstance, FTcsBuffInstanceHandle>` 的域侧别名）：**per-entity 桶**——`TMap<FTcsCombatEntityHandle, TUniquePtr<Bucket>>`（`TUniquePtr` 间接层，`TMap` 扩容**不搬移桶地址**）。**该注册表是共享层的统一模板实例**（详见 `entity-runtime-mechanism` 能力的「统一 per-entity 桶与注册表」需求）——它**取代**了此前状态层专属的手写桶（`FStateBucket`；其注册表在 `PLN-REFACTOR-1` Task 4 之前名为 `FTcsStateRegistry`），且其桶**载体改为 `TTcsInstancePool`**（不再手写槽位 + 代际 + 空闲栈）。

桶 = 槽位数组 + 每槽代际数组 + 空闲槽栈（**由 `TTcsInstancePool` 提供**）。分配 MUST 优先复用空闲槽；每次分配的身份 MUST 遵守「状态实例句柄」需求的进程级唯一代际规则；释放将代际 +1 置为偶数，使旧句柄失效。MUST NOT 令各桶各自从代际 1 重新发放相同实例身份。

**脏句柄一律拒绝 + `Warning`**（不 `ensure`——这是**时序竞态**语义，与“配置错误”分开）：代际失配 / 下标越界 / 实体未注册。拒绝 MUST NOT 影响同桶其它实例。

`UnregisterEntity` 删桶时 MUST 回收该实体全部槽位（否则池占用统计残留）。

#### Scenario: 脏句柄被拒绝且不影响他人

- **WHEN** 对同一实体建两个实例后，用代际失配的旧句柄调移除
- **THEN** 拒绝该调用并留 Warning，两个实例仍在册（无实例被误移除），不出现 ensure

#### Scenario: 桶地址不随登记表增长失效

- **WHEN** 持续为不同实体建桶与实例（`TMap` 发生多次扩容）后，用早先实体的句柄查询
- **THEN** 查询照常命中（桶经 `TUniquePtr` 间接持有，登记表扩容不搬移桶地址）

#### Scenario: 实体销毁后桶被回收

- **WHEN** 对某实体建若干实例后调 `UnregisterEntity`
- **THEN** 该实体全部实例被移除、桶被删除，且用其旧句柄查询一律被拒绝

#### Scenario: 与技能侧共用同一桶实现

- **WHEN** 检查状态侧与技能侧的桶类型
- **THEN** 两者是**同一模板的不同实例化**（`TPayload` 分别为 `FTcsStateInstance` 与施法运行态），MUST NOT 存在第二份手写的槽位 / 代际 / 空闲栈逻辑

### Requirement: 状态门面（施加、查询、移除与生命周期操作）

> **枚举名订正（2026-10-17，`ARCH-07` D-3 / `G-18`）**：本条及全篇原用的 `EApplyResult` / `EDP_*` / `EAR_*` 三个旧名**在全仓均不存在**。实际是 `EBuffApplyResult` / `EBuffDurationPolicy`（`EBDP_*`）/ `EBuffApplyResult`（`EBAR_*`）——`TcsBuffEnums.h:70` / `:14` / `:70`。
> **根因**：`PLN-REFACTOR-1` Task 4 拆出 `TcsBuff` 时，**三个枚举加了 `Buff` 前缀**（`EBuffApplyResult` / `EBuffDurationPolicy` / `ETcsBuffPeriodRefresh`）**而堆叠策略 struct 没加**（`FStateStackPolicy`），`EOverflowPolicy` / `EStackDurationPolicy` 也保持原名 ⇒ **一次改名没有单一规则，每个类型各自落在三种组合之一**（`ARCH-07` §5 D-4）。
> ⇒ 判断某类型该不该带 `Buff` 前缀，**MUST 看 `Source/TcsBuff/Public/State/TcsBuffEnums.h` 与 `TcsBuffStackPolicy.h`**，MUST NOT 按「看起来该统一」去推断。

`TcsBuff` MUST 提供 `UTcsBuffSubsystem : UWorldSubsystem`，**仅在 Game / PIE / GamePreview 世界实例化**（同族先例）；`Deinitialize` MUST 全量清理（登记表复位 + 全部桶清空 + 待撤销的到期条目撤销）——MUST NOT 留跨世界残影。

**★ 门面拆分（PLN-REFACTOR-1 Task 4，MUST 读）**：施加 / 查询 / 移除 / 等级读口 / 事件与时钟读口**全部住 `TcsBuff`**；而 `TcsState` **另有一个极薄的共享门面** `UTcsStateSubsystem`，它**只持快照参数表反射壳**（`GetParamTableReader()`，两域代码级唯一共用设施）——技能侧求值引用类时长与参数链行时从它取壳。**MUST NOT 把两份门面读成读口重复**：拆分判据 = **该设施是否被两个域在代码级使用**。

操作面：

- **施加**：`ApplyState(Target, DefTag, Source, Instigator, ParamTable, Overrides) -> EBuffApplyResult`——施加方传入的 `Source` 为**可选**（无效则本门面发号）；`Instigator`（发起者实体）与 `ParamTable`（施加方参数表）为**可选**（`Instigator` 无效时取 `Target`、`ParamTable` 为空时引用类源落兜底）；`Overrides` 为施加方参数覆盖，参与快照构建（构造规则见 `state-param-snapshot` 能力）；
- **查询**：`GetState(Handle) -> const FTcsStateInstance*`（脏句柄返回 nullptr 且不 ensure）、`IsStateActive(Handle)`、`ForEachState(Target, TFunctionRef<bool(const FTcsStateInstance&)>)`——**按槽位下标升序稳定遍历**（同输入同输出，可复现验收的前提）；
- **移除**：`RemoveState(Handle, Cause)` / `ExpireState(Handle)`（= `Cause` 为 `Expired` 的移除）/ `UnregisterEntity(Target)`；
- **生命周期操作**：`ExtendDuration(Handle, Delta)` / `SetRemaining(Handle, Seconds)`——MUST 同步到期堆（撤销旧条目 + 按新余量重入堆）；**`Infinite` 时值策略**上调二者 MUST 留 `Warning` 并**无操作**（无到期条目可调）；
- **等级读口**：`SetEntityLevelProvider(TScriptInterface<ITcsEntityLevelProvider>)` / `GetEntityLevelProvider()`——宿主实现"这个实体几级"，注入物以 `UPROPERTY` 持有；**未注入 = `nullptr` 是配置状态不是错误**（等级类源落兜底）。

`ApplyState` 与 `RemoveState` / `GetState` 一类**本轮 MUST NOT 加 `UFUNCTION`**（句柄与 `TMap` 形参今天不可反射；脚本面整体留 `SCRIPT` 系列，登记台账）。

**返回值语义**：`EBuffApplyResult{ EBAR_Applied / EBAR_Refreshed / EBAR_Stacked / EBAR_Rejected }`——四档 MUST 全部可达，判据归 `state-stacking-policies` 能力（无同组实例 ⇒ `Applied`；同组**同来源**未满仓 ⇒ `Refreshed`；同组**异来源**未满仓 ⇒ `Stacked`；满仓 ⇒ `Rejected` 或替换后 `Applied`）；**拒绝原因的具名细分**仍归 R5.5-e（今天只有 `Rejected` 一档 + 日志）。

#### Scenario: 非游戏世界不创建

- **WHEN** 在编辑器预览/检查器世界解析该子系统
- **THEN** 子系统不存在

#### Scenario: 世界反初始化确定性清空

- **WHEN** 某世界的状态门面建过实例后世界被反初始化
- **THEN** 登记表与全部桶被清空、待撤销的到期条目被撤销，旧句柄在新世界一律被拒绝

#### Scenario: 无限时长上调时长操作被拒

- **WHEN** 对 `EBDP_Infinite` 的实例调 `ExtendDuration` / `SetRemaining`
- **THEN** 留 Warning 且实例的时值字段不变（无到期条目可调，属**配置状态**不是错误）

#### Scenario: 遍历稳定且可按谓词提前终止

- **WHEN** 某实体在册三个实例，调 `ForEachState` 并在访问两个后返回 false
- **THEN** 访问序按槽位下标升序、第三次访问不发生；重复调用得到同一顺序

#### Scenario: 未登记定义时施加被拒

- **WHEN** 用一个本世界未登记（或无法解析）的 `DefTag` 调 `ApplyState`
- **THEN** 返回 `EBuffApplyResult::EBAR_Rejected` 并留 Warning，实体桶不新增实例（MUST NOT 出现 ensure）

#### Scenario: 时长操作同步到期堆

- **WHEN** 对一个 `Finite` 实例调 `ExtendDuration` 后让时钟越过原本的到期时刻
- **THEN** 该实例仍在册（旧条目已被撤销并按新余量重入堆）

#### Scenario: 可选形参缺省时的施加路径

- **WHEN** 以未填 `Instigator` 与未提供 `ParamTable` 的形态调 `ApplyState`
- **THEN** 施加照常成功，且快照构建走"发起者取被施加方 + 引用类源落兜底"的缺省路径（不崩溃、不 ensure）

#### Scenario: 未注入等级读口不构成错误

- **WHEN** 未注入 `ITcsEntityLevelProvider` 时查询 `GetEntityLevelProvider`
- **THEN** 返回空（配置状态），门面不留红字

#### Scenario: 四档回执全部可达

- **WHEN** 依次走"建新实例 / 同来源重复施加 / 异来源重复施加 / 满仓拒绝"四类施加
- **THEN** 四类分别返回 `Applied` / `Refreshed` / `Stacked` / `Rejected`（四档均非死档）

### Requirement: 状态定义到运行态的登记口

`UTcsBuffSubsystem` MUST 提供**定义登记口**：`RegisterBuffDef(FGameplayTag DefTag, const FTcsBuffDef& Def)` / `UnregisterBuffDef(FGameplayTag)` / `GetRegisteredBuffDef(FGameplayTag) -> const FTcsBuffDef*`。

> **符号订正（2026-10-17，`ARCH-05` D-4 / `ARCH-07` D-5 / `ARCH-00` §3.1 X-2）**：本条原写 `UTcsStateSubsystem::RegisterStateDef` 一族。三个符号**在全仓从未存在**——`PLN-REFACTOR-1` Task 4 拆出 `TcsBuff` 后，登记口随 Buff 专属语义搬去 `TcsBuff`，而本条停在了拆分前的名字上。**实际实现** = `UTcsBuffSubsystem::RegisterBuffDef` / `UnregisterBuffDef` / `GetRegisteredBuffDef`（`TcsBuffSubsystem.h:141/152/163`）。`UTcsStateSubsystem` **仍然存在**，但只持快照参数表反射壳（`GetParamTableReader()`，见本章「★ 门面拆分」那条）——**它不是登记口的宿主**。三篇模块档案各自独立记过同一处（`G-21` 型），本条按 `G-12`（代码搬走、引用滞留）订正。

**为什么是登记口而不是反向依赖**：定义库（`UTcsDefinitionSubsystem`）住 `TcsIntegration`，而 `TcsIntegration` **依赖** `TcsBuff`——若状态门面去 include `TcsIntegration` 反查定义库即成环。故依赖方向 MUST 保持单向：**定义库把定义写进门的登记口**（同款先例 = 定义库把触发行写进 `UTcsEffectSubsystem::RegisterTriggerRow`）。

**为什么身份是形参**：身份归**资产**声明（`UTcsStateDef::DefTag`——资产身份、`GetPrimaryAssetId()` 的取值来源、作者期校验对象），`FTcsBuffDef` 是**定义内容**；若为"从内容里读身份"再往数据 struct 加一份 `DefTag`，策划就得在同一个资产里把同一个 tag 手填两遍 = **双真相**。故运行期的身份由登记口一次对齐（调用方 = 定义库，其缓存键本就是 `DefTag`）。

登记表 MUST **自持定义副本**（`TUniquePtr` 持有使 `GetRegisteredBuffDef` 返回的指针地址稳定），且 `DefTag` 无效 / 重复登记 MUST 被拒绝并留 Error（不静默覆写）。

#### Scenario: 定义登记后可被施加

- **WHEN** 把某状态定义登记进该世界的状态门面，随后以该 `DefTag` 施加
- **THEN** 施加成功（`Applied`），取回的实例 `DefTag` 与登记时一致

#### Scenario: 重复登记被拒

- **WHEN** 同一 `DefTag` 被登记两次
- **THEN** 第二次被拒绝并留 Error，第一次的定义仍可解析（不静默覆写）

#### Scenario: 未登记的定义解析为 nullptr

- **WHEN** 以未登记过的 `DefTag` 调 `GetRegisteredBuffDef`
- **THEN** 返回 nullptr（正常查询路径，不 ensure）

### Requirement: 状态生命周期事件

`TcsBuff` MUST 原生声明六枚框架事件 tag（`UE_DECLARE_GAMEPLAY_TAG_EXTERN` + `UE_DEFINE_GAMEPLAY_TAG_COMMENT`，常量带模块导出宏——裸 `extern` 会 `LNK2001`），词落在既有 `TcsEvent` 根下（**不新增根**）：

| 常量 | 词 | 广播时机 |
|---|---|---|
| `Tag_TcsEvent_State_Applied` | `TcsEvent.State.Applied` | 施加成功（新实例） |
| `Tag_TcsEvent_State_Refreshed` | `TcsEvent.State.Refreshed` | 重复施加命中同组成活实例（刷新） |
| `Tag_TcsEvent_State_StackChanged` | `TcsEvent.State.StackChanged` | 层数变化 |
| `Tag_TcsEvent_State_Expired` | `TcsEvent.State.Expired` | 到期（自然结束） |
| `Tag_TcsEvent_State_Removed` | `TcsEvent.State.Removed` | 显式移除 / 取消 / 实体注销 |
| `Tag_TcsEvent_State_Periodic` | `TcsEvent.State.Periodic` | 周期到点（需周期计时，归 Task 3） |

载荷 MUST 为 `FTcsBuffEventPayload{ Handle, Entity, DefTag, Source, Instigator, Stacks, Level, Cause }`：`Entity` 为实例的被施加方，广播装配 MUST 从 `Instance.Entity` 填入；`Handle` / `Entity` / `DefTag` / `Source` / `Stacks` / `Level` 恒有效；`Cause`（`EBuffRemoveCause`）**仅 `Expired` / `Removed` 有意义**，其余事件里无意义。`Source` 保留既有非反射运行态字段纪律，其余身份与标量为反射字段。

`TcsBuff` MUST 为该载荷登记触发读取器：`Caster` 取有效 `Instigator`，无效时取 `Entity`；主体身份取反射的 `FTcsBuffInstanceHandle`。读取器与触发行的主体过滤契约归 `effect-trigger` 能力。

广播 MUST 走总线**立即通道**（`PublishImmediate`）——同一提交内到达，装置可在同一帧断言。

#### Scenario: 六枚事件词的归属与形态

- **WHEN** 检查六枚常量
- **THEN** 它们均为本模块原生声明、词形为 `TcsEvent.State.<名>`（3 段，落在既有 `TcsEvent` 根下），无新增根

#### Scenario: 载荷可经总线传递且字段可读

- **WHEN** 订阅者收到任一事件
- **THEN** 载荷为 `FTcsBuffEventPayload`，可读到 `Handle` / `Entity` / `DefTag` / `Source` / `Stacks` / `Level`（`Cause` 只在 `Expired` / `Removed` 上有意义）

#### Scenario: 状态事件提供发起者与实例身份

- **WHEN** 读取状态生命周期载荷，发起者有效或无效
- **THEN** `Caster` 分别取发起者或被施加方 `Entity`，主体身份保持载荷中的实例 Handle；`TcsEffect` MUST NOT 依赖 `TcsBuff` 的类型

### Requirement: 广播点与阶段迁移

**施加的共存结果 MUST 恰好广播一枚生命周期事件**（`Applied` / `Refreshed` 二者之一；替换路径是"旧实例被移除 + 新实例被建出"两笔，不属于"同一条实例的共存结果"）：无同组实例 ⇒ `TcsEvent.State.Applied`；命中同组 ⇒ `TcsEvent.State.Refreshed`，层数变化时另有 `TcsEvent.State.StackChanged`。`ExpireState` ⇒ `Expired`（`Cause = Expired`）、`RemoveState` ⇒ `Removed`（`Cause` 由调用方给）。**周期到点**另有 `TcsEvent.State.Periodic`（周期条目的重复回调，载荷带当前 `Stacks` 与 `Level`）——它是**追加**语义，不替代任一生命周期事件。

**"同组"判据归 `state-stacking-policies` 能力**（组键 = `实体 + DefTag` + 轴附加项）——本能力只保证**共存结果与广播的对应关系**：新建 ⇒ `Applied`；同来源刷新 ⇒ `Refreshed`；异来源叠层 ⇒ `StackChanged` + `Refreshed`（前者先到）；替换 ⇒ `Removed`（旧）+ `Applied`（新）；满仓拒绝 ⇒ 无广播。事件面 MUST NOT 因策略不同而增减（除上述对应关系外）。

**阶段机**：`EBuffPhase{ Inactive / Active / Expiring }`，合法迁移 `Inactive → Active`（施加成功）、`Active → Expiring`（进入移除流程）、`Expiring → Inactive`（槽位释放）、`Expiring → Active`（仍在册实例的刷新过渡态归位）；**非法迁移 = `ensure`**（配置错误语义，与脏句柄的竞态口径分开）。

**撤销顺序**：MUST 先广播后释放槽位——订阅者在回调里仍能 `GetState` 读到该实例（否则"收到移除事件却查不到实例"）。**该顺序的前置两步**（2026-10-05 由"一步"扩为"两步"）：进入撤销流程时 MUST 先（①）按实例的级联锚点摘除属性修正器**并退订该实例的施加期登记物**（内联触发行；行为 Fragment 的订阅退订与①同处，见 `state-behavior-fragments` 的「行为订阅的生命周期接线」），再（②）撤销该实例的到期与周期条目。①排在广播之前的意义 = **该实例自己的触发行 MUST NOT 看见自己的 `Removed` / `Expired`**（"状态在，行为就在"的对偶：状态走了，行为先走）；②的意义 = 条目持句柄，槽位复用后回调仍会到达——先撤条目才不产生无谓回调。

**接线顺序（施加侧的对偶）**：`Applied` 广播**之前** MUST 已完成修正器挂载、内联触发行登记与（若该 Def 配了行为 Fragment）行为订阅——订阅者在 `Applied` 回调里读到的属性已是挂载后的数值，且新登记的触发行**能看见自己的 `Applied`**。

`UnregisterEntity` MUST 对该实体每条在册实例逐条广播 `Removed` 后删桶。

**谁 MUST 调用它（2026-10-17 立，`ARCH-11` U-2 裁决落地）**：`UTcsCombatEntityComponent::EndPlay` MUST 在本帧内调用本方法，且该调用 MUST 排在属性门面 `UnregisterEntity` **之前**（状态的移除链需要属性容器还活着才能按来源撤修正器；反过来会**静默跳过**）。

⇒ **本条此前是一句孤儿义务**：方法的行为写得很清楚，但**没有任何一处说谁必须调它**，于是插件内调用点为零（只有 `TcsDev` 装置调）——与 `entity-query-contract` 的"消费方 MUST 核 `IsAlive`"同型（`ARCH-09` G-26：把责任推给一个未指认的第三方）。完整的四段级联顺序见 `integration-entity` 的「战斗实体组件」需求。

#### Scenario: 实体注销的调用方是组件而非装置

- **WHEN** 一个挂状态的 Actor 离开 Play
- **THEN** 由 `UTcsCombatEntityComponent::EndPlay` 调本方法（不是只有装置调）；该调用排在属性门面注销**之前**，每条在册实例逐条广播 `Removed` 后删桶

**有限重入守卫（STAT-5）**：门面 MUST 仅在真正移除广播窗口将句柄登记到 InRemovalBroadcastHandles；递归 Remove 同句柄 false 静默，不重复广播或释放。MUST NOT 因任意 Expiring 判为正在移除（刷新也暂用此阶段）。既有清理顺序不改变，任意递归修改组合仍未保证。

**广播后身份重查**：Applied/Refreshed 回调可移除自身或注销实体；广播者 MUST 用值快照，之后按句柄重查，不访问旧指针、不恢复已释放实例、不补播失效实例。移除广播返回后同样只释放仍在册且身份一致的槽位。

#### Scenario: 施加广播 Applied 且订阅者能在回调内查到实例

- **WHEN** 订阅 `TcsEvent.State.Applied` 后施加一个状态
- **THEN** 订阅者收到一笔该事件，且在回调内用载荷句柄调 `GetState` 能读到实例（广播早于释放，实例为 `Active`）

#### Scenario: 重复施加广播 Refreshed 而非 Applied

- **WHEN** 对同一实体以同一 `DefTag` **与同一来源句柄**连续施加两次
- **THEN** 第二笔广播的是 `TcsEvent.State.Refreshed`（`ApplyState` 返回 `Refreshed`），`Applied` 只收到一笔

**★ 2026-10-16 `R5.5-e1` 补充：`Cancelled` 首次有内建产生者（本需求已现况化）**：`EBuffRemoveCause::EBRC_Cancelled` 自落地起**只有注释自述「由调用方显式传入」，框架从不产生它**。`R5.5-e1` 的关系表检查器落地后，`TagsToCancel` 挂点会在施加成功路径上按它逐条 `RemoveState(handle, EBRC_Cancelled)` ⇒ 该枚举**第一次由框架自身产生**。MUST 记录的边界：本补充**只增一个产生者**，`Removed` / `Expired` 两档的来源与语义**一字不改**；`TagsToCancel` 为空（既有资产的现状）时**零行为变化**。

#### Scenario: 移除广播 Removed 且原因可读

- **WHEN** 以 `Removed` 与 `Cancelled` 两种原因各移除一个实例
- **THEN** 两笔 `TcsEvent.State.Removed` 的载荷 `Cause` 分别等于给定原因（`Cancelled` 本轮无内建产生者，由调用方显式传入）

#### Scenario: 到期广播 Expired

- **WHEN** 对某实例调 `ExpireState`
- **THEN** 订阅者收到 `TcsEvent.State.Expired` 且载荷 `Cause == Expired`（`Removed` 不广播）

#### Scenario: 实体注销逐条广播 Removed

- **WHEN** 某实体在册两个实例时调 `UnregisterEntity`
- **THEN** 收到两笔 `TcsEvent.State.Removed`，随后该实体全部句柄查询被拒绝

#### Scenario: 非法阶段迁移触发 ensure

- **WHEN** 尝试把已释放（`Inactive`）的实例再次迁入 `Expiring`
- **THEN** 触发 `ensure`（配置错误面），且不产生第二笔移除广播

#### Scenario: 周期事件是追加而非替代

- **WHEN** 一个 `Finite + Period > 0` 的状态跨过一个周期并随后到期
- **THEN** 先收到 `Periodic`（实例仍在册），后收到 `Expired`——`Periodic` 不替代任何生命周期事件

#### Scenario: 移除时先撤条目再广播

- **WHEN** 移除一个带未到期条目的实例后推进时钟越过其原到期时刻
- **THEN** 不出现该实例的第二笔事件（条目已在撤销流程中被撤掉）

#### Scenario: 移除前已退订本实例的触发行

- **WHEN** 一个带内联触发器行的实例被移除或到期（其触发行订阅了 `TcsEvent.State.Removed` / `Expired`）
- **THEN** 退订发生在广播之前——该实例自己的触发行**不会**因自己的死亡事件起链

#### Scenario: Applied 回调可移除自身

- **WHEN** Applied 订阅者按载荷句柄移除自己
- **THEN** 自移除成立，施加流程不再访问失效实例或重复清理，无额外红字

#### Scenario: Refreshed 自移除不被过渡阶段误拒

- **WHEN** Refreshed 回调移除处于刷新过渡态的实例
- **THEN** 自移除成立，刷新流程重查后不恢复已释放实例，无额外红字

#### Scenario: Removed 递归移除静默拒绝

- **WHEN** Removed 回调再次移除同一句柄
- **THEN** 递归调用 false 静默，外层广播和释放各一次，不重复归还槽位
