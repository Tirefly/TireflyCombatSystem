## 背景

当前 change 最初是按“保留 `UTcsSTSchema_StateInstance` 作为最小 concrete schema”来推进的，但现在架构方向已经进一步收敛：

1. `UTcsStateInstance` 更适合做共享执行态抽象基类。
2. 当前已经明确存在 concrete 业务运行时语义的只有：
   - Buff 执行态
   - Skill 激活执行态
   - 宿主级 StateComponent 编排树
3. learned skill 数据对象与 skill activation 运行态必须彻底分离。

与此同时，现有命名也出现了两类明显错位：

1. `UTcsSkillInstance` 当前代码实际表示 SkillComponent 托管的 learned skill 数据对象，但名字却像一次执行态实例。
2. `UTcsSTSchema_StateInstance` 继续存在，会把抽象执行基类重新暴露成 concrete editor-facing schema。

本 change 的新设计目标，不再是给 generic `StateInstance` 保留最小 concrete 入口，而是：

- 把 `UTcsStateInstance` 收回到抽象共享执行基类
- 删除 generic `StateInstance` schema
- 让 concrete schema 与 concrete 业务 owner 一一对应
- 同步完成 Skill 数据对象与执行态对象的命名翻面

## 目标 / 非目标

- 目标：将 `UTcsStateInstance` 固化为抽象共享执行态基类。
- 目标：删除 `UTcsSTSchema_StateInstance`，不再保留 generic concrete `StateInstance` schema。
- 目标：将三条 concrete schema 统一命名为：
  - `UTcsSTSchema_StateComponent`
  - `UTcsSTSchema_Buff`
  - `UTcsSTSchema_Skill`
- 目标：将当前 learned skill 数据对象 `UTcsSkillInstance` 更名为 `UTcsSkillEntry`。
- 目标：新增执行态 `UTcsSkillInstance : UTcsStateInstance`，使 Skill 激活运行态与 Buff 运行态命名对齐。
- 目标：让 Skill schema 的 editor-facing 根上下文名与类型名保持一致：`SkillInstance + SkillEntry`。
- 目标：让 `UTcsBuffDefinition` 与 `UTcsSkillDefinition` 成为 concrete 运行时类型选择的静态配置入口。
- 目标：继续保留 `UTcsSTSchema_StateComponent` 的 `LinkSubTree` / `LinkedSubTree` 兼容方向。
- 非目标：在本 change 中完成完整的 Skill 模块业务实现、持久化与复制方案。
- 非目标：一次性完成全部 StateTree 资产迁移与 editor authoring 菜单改线。

## 决策

- 决策：`UTcsStateInstance` 改为抽象共享执行态基类。
  - 原因：它当前承载的主要是共享生命周期、上下文绑定、参数、阶段和 StateTree 驱动底盘，而不是某个明确 concrete 业务域。
  - 原因：当前仓库里已经存在 concrete `UTcsBuffInstance`，并计划引入 concrete `UTcsSkillInstance`；相比之下，让 generic `StateInstance` 继续直接承载业务树，只会让抽象层再次泄漏为默认业务类型。
  - 备选方案：继续让 `UTcsStateInstance` 作为 generic concrete 运行态类型。拒绝，因为这会让抽象共享执行底盘与 concrete 业务运行态继续混在一起。

- 决策：删除 `UTcsSTSchema_StateInstance`，且不再引入任何 concrete `UTcsSTSchema_StateInstance` 替代物。
  - 原因：一旦 `UTcsStateInstance` 被明确为抽象基类，继续保留 generic `StateInstance` schema 只会把抽象类型重新包装成 concrete editor-facing 入口。
  - 原因：长期 concrete schema 应与 concrete 业务 owner 一一对应，而不是继续围绕抽象基类建立菜单和资产入口。
  - 备选方案：保留一个“最小 StateInstance schema”作为共享默认入口。拒绝，因为这会让“抽象共享底盘”与“可直接挂业务树的 concrete 类型”再次混淆。

- 决策：组件树 schema 命名统一为 `UTcsSTSchema_StateComponent`。
  - 原因：它的根对象仍然是 `UTcsStateComponent`，不应借用仓库内已有稳定含义的 `StateSlot` 概念命名。
  - 原因：`FTcsStateSlot` 已经是独立运行时容器名；把组件 schema 命名成 `StateSlot` 会制造“槽位级 schema”错觉。
  - 备选方案：使用 `UTcsStateSchema_StateSlot`。拒绝，因为它会错误暗示根上下文是 slot 而不是 component。

- 决策：Buff 树 schema 命名统一为 `UTcsSTSchema_Buff`。
  - 原因：Buff 已经是明确业务域，schema 类型名没有必要继续跟着旧的 `*TreeSchema_*Instance` 风格冗长展开。
  - 原因：Buff 运行态根对象仍然是 `UTcsBuffInstance`，editor-facing 根上下文继续使用 `BuffInstance` 即可。

- 决策：当前 `UTcsSkillInstance` 更名为 `UTcsSkillEntry`，并新增 `UTcsSkillInstance : UTcsStateInstance` 作为 Skill 激活执行态。
  - 原因：当前 `UTcsSkillInstance` 实际表示 SkillComponent 托管的 learned skill 数据对象，而不是一次技能执行态。
  - 原因：执行态对象使用 `UTcsSkillInstance` 这个名字，能让 Buff / Skill 两条 concrete activation runtime 线在命名层级上保持一致。
  - 备选方案：保留当前 `UTcsSkillInstance` 语义不动，并另起 `UTcsSkillStateInstance`。拒绝，因为这会继续让最核心的 Skill 数据对象与执行态对象命名错位。

- 决策：Skill 树 schema 命名统一为 `UTcsSTSchema_Skill`，根上下文名同步收敛为 `SkillInstance + SkillEntry`。
  - 原因：editor-facing 根名应与类型名和运行时语义一一对应，避免继续保留 `SkillActivation / SkillLearned` 这一套已被新命名方案取代的术语。
  - 原因：`SkillInstance` 表示本次技能激活执行态，`SkillEntry` 表示 SkillComponent 托管的 learned skill 数据对象，这套词汇更直接。

- 决策：保留 `UTcsStateDefinition` 作为抽象共享定义层，但让 concrete 子定义负责声明 concrete 运行时类型。
  - 原因：`UTcsStateDefinition` 已经是抽象基类，并且已经有 `ResolveStateInstanceClass()` 这条合同，没有必要为了这次收敛额外拆散共享定义字段。
  - 原因：真正需要翻面的是 concrete 类型选择权，而不是把整个共享定义层打碎重建。

- 决策：`UTcsBuffDefinition` 新增 `BuffInstanceClass`，`UTcsSkillDefinition` 新增 `SkillInstanceClass + SkillEntryClass`。
  - 原因：运行时 concrete 类型选择应回到定义层，而不是散落在宿主组件或执行现场。
  - 原因：这也符合架构文档里 SkillDef 负责选择 SkillInstance 派生类的长期方向。

- 决策：`LinkedSubTree` 继续被视为统一方向，且组件树 schema 仍以兼容它为硬约束。
  - 原因：即便 generic `StateInstance` schema 被删除，组件树仍然是最明确的宿主级编排树，不能因为命名和抽象化重构而牺牲组件树能力。

## 风险 / 取舍

- 风险：当前已落地的代码原型是按旧命名和旧 schema 方向实现的。
  - 缓解：在本 change 中先统一 proposal / design / tasks / spec，再以新命名和新分层为准重构代码，而不是在旧实现上继续补丁。

- 风险：删除 `UTcsSTSchema_StateInstance` 后，旧资产和旧 editor authoring 入口会失去直接迁移目标。
  - 缓解：在实现阶段明确三条 concrete schema 对应的资产归属与迁移路径，不再给 generic `StateInstance` 预留兜底入口。

- 风险：`UTcsSkillInstance` / `UTcsSkillEntry` 翻名后，Skill 相关语义需要整体重写，否则极易出现“旧名新义”和“新名旧义”混杂。
  - 缓解：schema 根上下文名、proposal 文案、设计说明和后续代码类型名统一使用 `SkillInstance + SkillEntry`。

- 风险：`UTcsSTSchema_StateComponent` 继续走组件 schema 兼容路线，容易让人误解它就是“普通 state 的替代 concrete 线”。
  - 缓解：在设计与 spec 中明确它是宿主级编排树，不是 generic `StateInstance` 的替代物。

## 迁移计划

1. 先固定新命名与新职责边界：`StateInstance` 抽象化、`StateInstanceSchema` 删除、`SkillEntry / SkillInstance` 翻名、三条 concrete schema 命名统一。
2. 再调整 definition 层：为 Buff / Skill concrete 类型选择提供静态配置入口。
3. 然后重构 runtime 与 schema 实现：让三条 concrete schema 分别接回 `StateComponent`、`BuffInstance`、`SkillInstance + SkillEntry`。
4. 最后再衔接 editor authoring 与旧资产迁移边界。
