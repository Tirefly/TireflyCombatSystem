## MODIFIED Requirements

### Requirement: 跨域在飞运行态查询契约（`ITcsStateLiveQuery`）

「A 域要读 B 域的在跑运行态」MUST 经**共享层定义的单一契约**中转，**MUST NOT** 让任一域认识另一域：

- `ITcsStateLiveQuery`（`UINTERFACE`，住 `Public/State/TcsStateLiveQuery.h`）MUST 由 `TcsState` 定义、由**域自己向下注册**（`UTcsStateSubsystem::SetLiveQuery`），**MUST NOT** 由宿主注入——判据：这**不是**宿主能力（宿主没有理由知道「插件内部两个域要互通」），而是框架自身机制的缺失；若要求宿主接线，则每个宿主都要接一次，而**漏接的表现是关系门静默失效**（本仓最忌的失效形态）；
- **形状 MUST 是一对，不是单半**：`FTcsLiveStateView` 同时交出「**事实半**」`CategoryTags`（这个在跑实例**是什么**）与「**声明半**」`Row`（这个在跑实例**约束谁**）。两半 **MUST NOT** 合并进一个类型——行答「我约束谁」（声明）、词面答「我是什么」（事实），把 `CategoryTags` 塞进 `FTcsRelationRow` 会让同一格承载两种语义（本仓反复拦的混装形态）。**该形状经三次修正，前两版各少一半**，MUST NOT 回到只交类别词或只交行的形态；
- **两个方法**：`ForEachLiveState(Entity, Visitor)` MUST 按**稳定序遍历**该实体当前在册的运行态并逐个交出视图（`Visitor` 返回是否继续）；`GetLiveCount(Entity) const` MUST 返回在册实例数（未注册实体返回 0）。**遍历期间 MUST NOT 增删实例**（会改槽位与空闲栈）——调用方若需增删，先收集再动（两段式）；
- **未注册时 MUST 降级而非失败**：某域未注册 ⇒ 该域**零贡献**（不是「整条查询失败」）；调用方 MUST 按「该域无在跑实例」处理（关系门恒过的保守侧）并**留一条具名日志**，**MUST NOT** 静默当真、**MUST NOT** 因此拒绝——缺注册不该变成随机的业务失败；
- **契约 MUST 为 C++ 专用面**（形参含 `TFunctionRef`，蓝图不可表达），与 `ITcsEntityQuery` / `ForEachState` 的先例一致；
- **它与 `ITcsEntityQuery` 的分工**：那个读**世界**（遍历 / 定位 / 存活——宿主本体论，框架不认识「世界有哪些实体」）；本契约读的是**框架自己的运行态**（哪些运行态在跑），故实现方是域而非宿主。

> **收录订正（2026-10-24）**：本契约于 `f626582`（`R5.5-e1` 关系表族检查）已落地并被**真实消费者**使用（`FTcsSkillOps_Relation::CheckRelationGate` 的第 ③ 向判定），但**本能力自 `R5.5-e1` 起从未收录它**——`ITcsStateLiveQuery` / `FTcsLiveStateView` / `GetLiveQuery` / `SetLiveQuery` / `ForEachLiveState` / `GetLiveCount` 六个符号在 `openspec/specs/` 全库**零命中**。性质同 `G-6`（`FTcsPrimaryAssetQuery` 有唯一真实消费者却零收录）/ `G-17`（`IsChainRegistered` 同款）：**有实现者、有消费者，而最高权威层查不到**。
> **连带订正两处错误的排他性断言**：`plugin-descriptor:29` 与 `buff-instance-lifecycle:140` 都写「技能侧对状态侧的**唯一**代码级依赖是 `GetParamTableReader()`」——该断言 **已假**（真实依赖共 7 类：`FTcsStateInstance` 基底 / `FTcsParamSnapshot` / `UTcsStateParamTableReader` / `FTcsStateRelation` / `FTcsRelationRow` / `ITcsStateLiveQuery` / `FTcsLiveStateView`）。两句都成文于 `PLN-REFACTOR-1` Task 4/5（`948e98d` / `c0bbeae`），而本契约晚至 `f626582` 才进代码 ⇒ **不是写错，是「写下排他性断言之后世界变了、没人回头改」**。⚠ **结论仍成立**（`TcsSkill.Build.cs` 确无 `TcsBuff`），但**论证依据已失效**：照原文判断「新增一个跨域读口会不会破坏这条边界」会得到错误答案。

#### Scenario: 跨域接口不依赖任一域

- **WHEN** 检查 `ITcsStateLiveQuery` 与 `FTcsLiveStateView` 的声明所在模块与其形参类型
- **THEN** 二者都住 `TcsState`（共享层），且形参只含共享层自己定义的值类型（`FTcsCombatEntityHandle` / `FTcsRelationRow` / `FGameplayTagContainer`）——**MUST NOT** 出现任一域的 Def 类型或运行态记录类型（出现即制造包含环）

#### Scenario: 视图两半缺一即判据缺失

- **WHEN** 消费方要同时判「在跑实例是否落进我的禁区」（需事实半）与「在跑实例是否覆盖我」（需声明半）
- **THEN** 一次遍历即可拿到两半；**只交类别词**会使后者无法表达、**只交行**会使前者无法表达——两种残缺形态都被本 Scenario 判为不合规

#### Scenario: 未注册时降级放行且可诊断

- **WHEN** 状态域尚未注册实现时，技能侧调用关系门
- **THEN** 关系门按「无在跑实例」放行并**留一条具名日志**（含实体与定义）；**MUST NOT** 静默当真、**MUST NOT** 因此拒绝该次激活、**MUST NOT** ensure

#### Scenario: 遍历期间不改动在册集合

- **WHEN** 消费方在 `ForEachLiveState` 的 `Visitor` 内尝试增删该实体的运行态
- **THEN** 该行为被禁止（调用方 MUST 先收集再动）；判据 = 遍历依赖桶的槽位与空闲栈，中途增删会让遍历命中已归还的槽位

#### Scenario: 在册计数是可读的观测面

- **WHEN** 对某实体调用 `GetLiveCount`
- **THEN** 返回该实体当前在册的运行态数（未注册实体返回 0）；该口是**观测面**（装置断言用），不参与任何判定——**消费者 MUST 在头注释登记「哪个装置 + 该装置可整体摘除」两件事**
