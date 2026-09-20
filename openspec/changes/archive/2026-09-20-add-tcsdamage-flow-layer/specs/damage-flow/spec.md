## ADDED Requirements

### Requirement: 流程模板与登记表

`TcsDamage` MUST 以**数据模板**承载瞬时流程（D7-5 流程管线宿主化：阶段构成本身是项目知识，插件不预设）：

- `FTcsFlowTemplate{ FName TemplateId; TArray<FInstancedStruct> Steps; }`（USTRUCT）——有序步骤数组，元素形状同 `FEffectStep`（步骤无公共基类，类型合法性由执行器注册表在执行期判定）；
- `UTcsDamageSubsystem`（世界级子系统门面）MUST 提供登记表：`RegisterTemplate` / `UnregisterTemplate` / `FindTemplate`，**键 = `TemplateId`**；拒绝面（ensure + false）：`TemplateId` 为空、同 id 重复登记（不得静默覆写）；`FindTemplate` 未登记返回 nullptr（**不 ensure**——正常查询路径）；
- `RunTemplate` 遇未登记模板 MUST 拒绝：Error 日志 + 不执行（执行期配置错误，不 ensure）；
- 登记表 MUST 以地址稳定方式持有模板（`TMap<FName, TUniquePtr<…>>` 或同效手段）——解释器在一次执行期间持模板引用。

#### Scenario: 登记后可按 id 查到

- **WHEN** `RegisterTemplate` 一条 `TemplateId = "Flow_Test"` 的模板后 `FindTemplate("Flow_Test")`
- **THEN** 返回该模板，步骤数组与登记内容一致

#### Scenario: 重复登记被拒

- **WHEN** 对同一 `TemplateId` 再次 `RegisterTemplate`
- **THEN** 返回 false、原模板不被覆写、留 ensure 提示与日志

#### Scenario: 未登记模板被拒绝执行

- **WHEN** `RunTemplate` 一个未登记的 `TemplateId`
- **THEN** 不执行任何步骤 + Error 日志（不崩溃、不 ensure）

### Requirement: 流程上下文（三层值空间）

`FTcsDamageFlowContext`（黑板，**纯运行态结构**）MUST 承载 09 §2.1 的三层值空间定位：

- `AActor* Attacker` / `AActor* Instigator` / `TArray<TWeakObjectPtr<AActor>> Targets`；
- **公式参数初值**：`TMap<FName, FTcsParamValue> FormulaParams`——**只读原料**（键 = 项目词表，**MUST NOT 用下标**；宿主公式按名取用）；
- **流程属性黑板**：`FTcsFlowAttributes Blackboard`（伤害计算的工作值）；
- `TArray<FGameplayTag> ClassificationTags`（来源标签启动写入 / 元素标签由 Element 步骤写入——词表归项目，插件只搬运与匹配）；
- `FTcsSourceHandle FlowSource`——**每流程唯一**的作用域修改器归属锚点（流程结束 `RemoveBySource` 级联摘除；**MUST 由 `RunTemplate` 在起流程时分配**）；
- `TMap<FTcsAttributeName, double> CapturedAttrs`——属性捕获快照（读默认 Live、捕获命中读快照；**快照填充归标准步骤（AttrCapture 配置），本任务只落字段与语义**）。

R3 MUST NOT 引入上下文池化（门面签名 = 调用方提供上下文；池化随性能/宿主化轮——零消费者不预建）。

#### Scenario: 起流程分配唯一来源锚点

- **WHEN** 连续执行两次 `RunTemplate`
- **THEN** 两次的 `Context.FlowSource` 互异且有效（非零）——作用域修改器可被精确级联摘除

#### Scenario: 公式参数按名取用

- **WHEN** 上下文里 `FormulaParams` 含键 `DmgCoeff`
- **THEN** 消费方按名取值（结构上不存在下标访问路径——键永不用下标）

### Requirement: 流程属性黑板

`FTcsFlowAttributes`（流程工作值的容器：键 + 每键修正链）MUST 提供：

- **提交**：`Submit(Key, Op, Operand, SortKey, ConsumePolicy)`——Operand 为 `FTcsParamValue`（PV 系列载体；黑板键引用保留为流程域自身 Operand 选项）；`SortKey` **只用于消耗裁决、MUST NOT 参与求值顺序**；
- **读取**：`Read(Key)` 按 **M2 同款带式语义**求值——**折叠 MUST 调用 TcsAttribute 的共享纯函数 `FoldTcsAttributeBands`**（D5-5 v3：M2 属性聚合 / M5 参数链 / 本容器**三处共用，MUST NOT 私建第二份**）；
- **收集重置**：`Reset()`——清空全部键的收集（标准步骤 `CollectStart` 的落点）；
- **消耗策略位**：`FTcsConsumePolicy`（`MaxUses` / `Cooldown` / `SortKey` 等字段；`OnConsumed` 回调位）——**本容器只存不裁**：裁决（SortKey 选一 → 成功才消费）归 `Execute` 步骤（Task 4，D7-4"收集 ≠ 消费"）；
- **R3 不做值域收口**：黑板是流程工作值、**不是角色属性**——无边界/值域模式概念（`IValueDomainPolicy` 同款逃逸位随值域策略轮，见 09 §2.1 括注）。

#### Scenario: 同键多笔提交求和且与顺序无关

- **WHEN** 对同一键提交两笔 `Add`（或同一组含不同运算带的提交按两种排列）
- **THEN** `Read` 结果一致（带序由 `Op` 决定、顺序无关——共享折叠函数的既有保证）

#### Scenario: 收集重置

- **WHEN** 提交若干修正后调用 `Reset`
- **THEN** 该容器全部键回到空收集状态（`Read` 返回折叠初值 0）

#### Scenario: 消耗策略只存不裁

- **WHEN** 提交时携带 `FTcsConsumePolicy`（如 `MaxUses = 1`）
- **THEN** `Read` 行为不受策略影响，策略可被后续裁决步骤读回（消费语义不在本容器）

### Requirement: 流程步骤注册表与自注册宏

`TcsDamage` MUST 提供**与 TcsEffect 执行器注册表同构、但独立实例**的流程步骤注册表（D7-5"流程 = 数据模板 + 步骤库"）：

- 执行器签名：`FTcsFlowStepExecute = TFunction<bool(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)>`——**返回 false = 中止流程剩余步骤**（流程失败/打断；已产生的副作用不回滚——止于未来）；
- 注册键 = 步骤 struct 的**反射类型**（同 Effect 口径）；**双入口**（静态自注册宏 + 动态 `Register`）；
- 宏对：`UE_DECLARE_FLOW_STEP_EXECUTOR(ExecutorFn)` / `UE_DEFINE_FLOW_STEP_EXECUTOR(StepType, ExecutorFn)`——**静态初始化期零 UObject 触达**（只入待解析表、首次查询才解析反射类型；依据与 Effect 侧同：引擎 `FNativeGameplayTag::GetIfAllocated()` 纪律）；
- 同类型重复登记 MUST 拒绝（ensure + 保留首个）；未知步骤类型在执行期 MUST 中止流程 + Error 日志（含步骤序号与类型名）。

#### Scenario: 自注册后即时可执行

- **WHEN** 某模块用宏登记一个流程步骤类型（该模块已加载）
- **THEN** 注册表可查到其执行器，模板中的该步骤被解释器分派执行

#### Scenario: 未知步骤类型中止流程

- **WHEN** 模板含未注册执行器的步骤
- **THEN** 该流程中止（后续步骤不执行）+ Error 日志给出步骤序号与类型名

#### Scenario: 步骤返回 false 即中止

- **WHEN** 某步骤执行器返回 false
- **THEN** 后续步骤不执行（流程视为失败），已执行的步骤副作用保留

### Requirement: 流程解释器（同步单帧）

`UTcsDamageSubsystem::RunTemplate(FName TemplateId, FTcsDamageFlowContext& Context)` MUST：

- 按模板顺序**同步执行全部步骤、单帧内完成**——MUST NOT 挂起（瞬时流程无异步语义，区别于效果链的挂起点；09 §3）；
- 门面 MUST NOT 是 Tickable（流程无每帧成本）；
- 每次执行 MUST 只读模板（MUST NOT 改写共享模板数据）。

#### Scenario: 单帧同步走完

- **WHEN** 执行一条全部步骤都返回 true 的模板
- **THEN** `RunTemplate` 返回时全部步骤已执行完毕（无挂起、无延迟步骤）

#### Scenario: 模板不可变

- **WHEN** 同一模板被连续执行两次
- **THEN** 第二次行为与第一次一致（模板数据未被执行过程改写）

### Requirement: 收集事件协议

`TcsDamage` MUST 以总线**立即通道**承载"步骤边界发事件、外部响应提交修正"的协议（09 §2.2/§3）：

- 事件 Tag MUST 按 **`Tcs.Event.<域>.<事件名>` 命名公约**由本模块**原生声明**（域段 = `Damage`）——**设计文档旧写法 `Combat.Damage.Collect.<Step>` 不采用**（该写法早于 2026-09-18 的 Tag 公约与"原生声明、不进项目 Tag 表"口径；按 Step 名逐条声明）；
- 门面 MUST 提供广播入口 `PublishCollectEvent(步骤标识, FTcsDamageFlowContext&)`：经总线立即通道**同步**派发（响应方在调用返回前收到）；
- 载荷 MUST 为**上下文指针包装**（`FTcsDamageFlowCollectEvent{ FTcsDamageFlowContext* Context; }`）——进程内瞬态指针，MUST NOT 跨帧持有；响应方在回调内 `Submit` 即可（收集 ≠ 消费）。

#### Scenario: 立即通道同步到达

- **WHEN** 步骤执行器调用 `PublishCollectEvent`
- **THEN** 订阅方的响应在调用返回前已执行完毕（响应内提交的黑板修正对本步骤后续读取可见）

#### Scenario: 载荷指针仅在派发期有效

- **WHEN** 订阅方在回调外保存载荷中的上下文指针
- **THEN** 属误用（契约明文 MUST NOT 跨帧持有）——框架不为其保活
