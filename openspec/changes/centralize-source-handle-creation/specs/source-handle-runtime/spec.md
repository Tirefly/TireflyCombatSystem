## ADDED Requirements
### Requirement: SourceHandle 创建由静态工厂统一

TCS SHALL 通过 `FTcsSourceHandleFactory` 创建所有带有效非负 ID 的 `FTcsSourceHandle`。该工厂 MUST 是共享 C++ 静态工厂，不得作为 UObject、Subsystem、Component 实例或 DefinitionManager 职责存在。调用方 MUST NOT 在 TCS 业务代码中手工构造或写入非负 SourceHandle ID。

#### Scenario: Root 创建分配首个合法 ID
- **WHEN** TCS 运行时通过 `FTcsSourceHandleFactory` 创建第一个 root `FTcsSourceHandle`
- **THEN** 返回的 handle MUST 具有 `Id == 0`
- **AND** `SourceHandle.IsValid()` MUST 返回 true
- **AND** 后续创建的有效 handle MUST 获得不同的非负 ID

#### Scenario: 旧 StateComponent 分配器已清零
- **WHEN** 在 TCS 运行时代码中搜索 SourceHandle 分配入口
- **THEN** `UTcsStateComponent::CreateSourceHandle` 与 `UTcsStateComponent::NextSourceHandleId` MUST 不再存在
- **AND** `UTcsAttributeComponent`、`UTcsDefinitionManagerSubsystem`、`UTcsAttributeManagerSubsystem` 或 `UTcsStateManagerSubsystem` MUST NOT 提供等价 SourceHandle 分配入口

#### Scenario: 默认构造仍表示无效 handle
- **WHEN** 创建默认 `FTcsSourceHandle()`
- **THEN** 其 `Id` MUST 为负值
- **AND** `IsValid()` MUST 返回 false
- **AND** 默认构造 MUST NOT 消耗工厂 ID

### Requirement: SourceHandle 有效性使用非负 ID

`FTcsSourceHandle::IsValid()` SHALL 等价于 `Id > -1`。所有业务有效性分支 MUST 使用 `IsValid()`，不得用 `SourceHandle.Id > 0` 或其他会排除 `Id == 0` 的判断表达有效性。

#### Scenario: Id 0 参与正常生命周期清理
- **WHEN** 一个来源 handle 的 `Id == 0`
- **THEN** 该 handle MUST 被视为有效
- **AND** State removal、Modifier cleanup、事件归因与索引清理路径 MUST 继续执行

#### Scenario: 负数 ID 仍表示无效
- **WHEN** 一个来源 handle 的 `Id < 0`
- **THEN** `SourceHandle.IsValid()` MUST 返回 false
- **AND** 需要有效来源的运行时请求 MUST 拒绝该 handle

#### Scenario: 业务代码不手写 Id 大于零判断
- **WHEN** 审查 TCS runtime 源码中的 SourceHandle 有效性分支
- **THEN** 代码 MUST 使用 `SourceHandle.IsValid()`
- **AND** MUST NOT 使用 `SourceHandle.Id > 0`、`SourceHandle.Id != -1` 或等价的手写有效性分支

### Requirement: SourceHandle 因果链由 Root 和 Child API 构建

`FTcsSourceHandleFactory` SHALL 提供 Root 与 Child 两类创建 API。Root API 创建没有父来源的 handle；Child API MUST 从父 handle 继承 `CausalityChain`，并追加直接父来源的 `FPrimaryAssetId`。调用方 MUST NOT 手工拼接 `CausalityChain` 后再创建有效 handle。

#### Scenario: Root SourceHandle 没有父链
- **WHEN** 调用方通过 Root API 创建来源 handle
- **THEN** 返回 handle 的 `CausalityChain` MUST 不包含父来源条目
- **AND** 工厂仍 MUST 分配唯一非负 ID

#### Scenario: Child SourceHandle 追加直接父来源
- **WHEN** 调用方通过 Child API 传入有效父 handle 与直接父来源 Definition Id
- **THEN** 返回 handle 的 `CausalityChain` MUST 等于父 handle 的 `CausalityChain` 后追加该直接父来源 Definition Id
- **AND** 返回 handle MUST 获得新的唯一非负 ID
- **AND** 调用方 MUST NOT 自行传入完整 child chain

#### Scenario: 无效 Child 输入不创建有效 handle
- **WHEN** Child API 收到无效父 handle 或无效直接父来源 Definition Id
- **THEN** 工厂 MUST 返回无效 `FTcsSourceHandle`
- **AND** 工厂 MUST NOT 消耗新的 SourceHandle ID

### Requirement: SourceHandle 不提供运行时对象反查注册表

`FTcsSourceHandle` SHALL 只作为值对象承载 ID、Instigator、SourceTags 与 CausalityChain。TCS MUST NOT 在 SourceHandle、SourceHandle 工厂、GenericLibrary、DefinitionManager 或全局 subsystem 中维护 `SourceHandle.Id -> UObject` 的对象注册表或反查机制。

#### Scenario: 工厂不保存对象映射
- **WHEN** `FTcsSourceHandleFactory` 创建一个有效 handle
- **THEN** 工厂 MUST NOT 保存 `HandleId -> UTcsStateInstance`、`HandleId -> UTcsSkillEntry`、`HandleId -> BuffInstance`、`HandleId -> Equipment` 或其他 UObject 映射

#### Scenario: 领域模块自行持有来源上下文
- **WHEN** Attribute、Skill、Buff 或后续伤害流程需要 `SourceStateInstance`、`SourceSkillEntry` 或业务来源对象
- **THEN** 这些对象 MUST 通过所属领域的运行时上下文、显式参数或本地索引取得
- **AND** MUST NOT 通过 SourceHandle 全局反查取得

### Requirement: Blueprint SourceHandle 入口只做工厂转发

`UTcsGenericLibrary` SHALL 只提供 SourceHandle 创建的 Blueprint 转发入口。Blueprint 转发 MUST 直接委托 `FTcsSourceHandleFactory`，不得持有独立 ID 计数器、对象缓存、注册表或与 C++ 工厂不同的创建语义。

#### Scenario: Blueprint Root 转发不持有状态
- **WHEN** Blueprint 调用 `UTcsGenericLibrary` 的 Root SourceHandle 创建入口
- **THEN** 该入口 MUST 直接委托 `FTcsSourceHandleFactory`
- **AND** `UTcsGenericLibrary` MUST NOT 持有 SourceHandle ID 计数器

#### Scenario: Blueprint Child 转发使用相同因果链规则
- **WHEN** Blueprint 调用 `UTcsGenericLibrary` 的 Child SourceHandle 创建入口
- **THEN** 该入口 MUST 使用与 C++ Child API 相同的父链继承与直接父来源追加规则
- **AND** 它 MUST NOT 允许 Blueprint 调用方手工传入完整 child `CausalityChain`

#### Scenario: Blueprint 转发不授权预测客户端生成最终 handle
- **WHEN** 后续网络或本地预测 change 引入预测阶段调用路径
- **THEN** Blueprint SourceHandle 转发入口 MUST NOT 被视为预测客户端生成最终 authority `FTcsSourceHandle` 的授权
- **AND** 最终 authority SourceHandle 仍 MUST 由 authority 创建路径通过 `FTcsSourceHandleFactory` 生成
