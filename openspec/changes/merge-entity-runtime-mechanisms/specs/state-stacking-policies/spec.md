## MODIFIED Requirements

### Requirement: Custom 决策 Fragment

`EGB_Custom` 的分组与决策 MUST 由 `FTcsStateStackDecisionFragment` 接管：`USTRUCT(meta = (Hidden))`，三个决策方法**全部中性默认实现**（MUST NOT `= 0`、MUST NOT `PURE_VIRTUAL`——同一份代码在 Development 与 Shipping 下行为不同）：

**★ 自 2026-10-14 起本类型是共享层统一决策接口的「状态侧薄型」（本节为接口化后的改述，MUST 读）**：三个决策方法的**契约与中性默认实现上提到 `FTcsEntityAdmission`**（`USTRUCT(meta = (Hidden))` + `virtual ~() = default`），`FTcsStateStackDecisionFragment` **继承它**并**不再自行声明这三个方法**。理由 = 该决策空间与技能侧的"每实体/每次执行"实例化分流**同型**，两处各写一份即漂移源（详见 `entity-runtime-mechanism` 能力的「统一重申请决策接口」需求）。

- `virtual bool IsSameGroup(...) const`：默认 = 内置 `None` 语义（同实体 + 同 `DefTag`）；
- `virtual bool ShouldAccept(...) const`：默认 = 接受；
- `virtual int32 ResolveStacks(...) const`：默认 = 沿用内置规则（同来源不变 / 异来源 +1）；
- **形参类型（接口化后）**：三方法收 `FTcsEntityAdmissionRequest` 与 `FTcsEntityAdmissionExisting`（**通用视图**），取代原先的 `FTcsStateStackRequest` / `FTcsStateInstance`——因为共享层 MUST NOT 认识域记录类型。域侧由**适配层**逐字段直取填充（**MUST NOT 掺入语义**；判据全在策略里）。**视图的字段取舍判据 = "两域共有且语义相同"**：`DefTag` / `Entity` / `Source` / `Instigator` / `Stacks`。**注意 `Stacks` 对技能侧恒 1**（它无层数语义）——该字段只为状态侧的分组与层数决策而存在；
- **`IsSameGroup` 的默认实现 MUST 仍等于"同实体 + 同 `DefTag`"**（与接口化前**逐字相同**，行为中性可核）。

策略实例归 **Def 资产**持有（`CustomDecision` 字段）；`FTcsStateInstance` MUST NOT 持有任何策略载体（值语义记录是池化与操作复制的地基）。

`GroupBy = EGB_Custom` 而载荷缺失或类型不符 MUST 退化为内置 `None` 语义并留 `Warning`（**配置错误**语义——与满仓拒绝的 `Log` 分开，后者是业务结果）。

框架 MUST NOT 提供任何具体分组或决策策略实现（分组词与业务判据是宿主词汇；框架只给中性默认与逃逸位）。

#### Scenario: 宿主策略被真实调用

- **WHEN** 宿主提供一个 `IsSameGroup` 恒假的样本策略并以 `EGB_Custom` 施加两次
- **THEN** 两次都建出新实例（两笔 `Applied`），且样本被真实调用（日志可证）

#### Scenario: 载荷缺失时退化

- **WHEN** `GroupBy = EGB_Custom` 但 `CustomDecision` 为空
- **THEN** 按内置 `None` 语义处理并留 `Warning`（不崩溃、不 `ensure`）

#### Scenario: 实例不持有策略

- **WHEN** 检查 `FTcsStateInstance` 的字段集合
- **THEN** 无策略载体（策略住 Def 资产，实例只留数据）

#### Scenario: 决策契约只有一份（接口化后的新增判据）

- **WHEN** 检查 `FTcsStateStackDecisionFragment` 与技能侧默认策略类的决策方法声明
- **THEN** 两者**均继承自 `FTcsEntityAdmission`**、**MUST NOT 各自声明一份**三方法契约（编译期可核：子类型里不存在 `IsSameGroup` / `ShouldAccept` / `ResolveStacks` 的 `virtual … override` 声明，除非它确实要覆盖）；且**状态侧与技能侧的同一档语义（如同实体不共组）由同一处默认实现给出**

#### Scenario: 接口化不改状态侧默认行为（行为中性对照）

- **WHEN** 不挂任何自定义策略、按内置路径施加两次（同来源 / 异来源各一次）
- **THEN** 回执与层数读数与接口化**之前逐字相同**——同来源 `Refreshed`（层数不变）、异来源 `Stacked`（层数 +1）、满仓按溢出政策 `Reject` / `Replace`；即本次上提**只换契约载体、不改任何判据**
