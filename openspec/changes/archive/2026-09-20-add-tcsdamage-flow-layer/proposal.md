# Change: 落地伤害流程机制层——流程模板/上下文/流程属性黑板/步骤注册表与解释器/收集事件协议（damage-flow）

## Why
plan2 Task 3（TcsDamage）动工前的规格先行提案。Task 1 落了效果链机制层（执行器注册表 + 解释器 + 挂起协议），Task 2 落了目标选择策略层（**跨模块自注册实证**）；TcsDamage 是竖切里**第三个也是最后一个"机制层"模块**，其全部内容目前只有设计文档措辞（`09-module-damage.md` v4 + D7-1~D7-7 + D5-5 v3），没有任何可执行规格。

本任务是"**流程管线宿主化**"（D7-5）的机制侧落地：插件**不预设流程阶段**——它只交付"模板 = 数据、步骤 = 类型 + 自注册执行器、黑板 = 流程工作值、收集 = 事件协议"这套机器；标准十阶段降级为标准步骤库 + 官方默认模板（Task 4）。

## What Changes
- 新增能力规格 **`damage-flow`**（6 条需求）：
  1. **流程模板与登记表**：`FTcsFlowTemplate{TemplateId, Steps}` + `RegisterTemplate` / `UnregisterTemplate` / `FindTemplate`（键 = TemplateId，重复登记拒、未登记查询不 ensure、地址稳定持有）。
  2. **流程上下文（三层值空间）**：`Attacker` / `Instigator` / `Targets` / `FormulaParams`（`FTcsParamValue`，**按名取用、永不用下标**）/ `Blackboard` / `ClassificationTags` / **`FlowSource`（每流程唯一，起流程时分配）** / `CapturedAttrs`；R3 不做上下文池化。
  3. **流程属性黑板**：键 → 修正链；`Submit` / `Read`（**折叠 MUST 调用 TcsAttribute 的共享 `FoldTcsAttributeBands`**——D5-5 v3 三处共用）/ `Reset`；`FTcsConsumePolicy` **只存不裁**（裁决归 Execute 步骤）；**R3 不做值域收口**（黑板是工作值不是角色属性）。
  4. **流程步骤注册表 + 自注册宏**：`FTcsFlowStepExecute = TFunction<bool(const FInstancedStruct&, FTcsDamageFlowContext&)>`；`UE_DECLARE/DEFINE_FLOW_STEP_EXECUTOR`（**静态初始化期零 UObject 触达**——照 Effect 侧的延迟解析设计）；键 = 步骤反射类型；双入口；未知类型中止 + Error。
  5. **流程解释器**：`RunTemplate(TemplateId, Context&)` **同步单帧走完、无挂起**（09 §3：瞬时流程无异步语义）；门面非 Tickable；模板只读。
  6. **收集事件协议**：`TcsDamage` **原生声明**收集事件 Tag（**按 `Tcs.Event.<域>.<事件名>` 公约，域 = Damage**）+ 门面 `PublishCollectEvent` 走总线**立即通道**；载荷 = 上下文指针包装（进程内瞬态）。
- **不做的**（非目标，保持 R3 边界）：不做任何公式（命中/暴击/元素 = delegate 挂点或宿主步骤，**基础伤害值 = 链步骤参数解算输入**——PV-7/D7-2 收窄后流程零计算）；不做标准步骤库与官方默认模板（**Task 4**）；不做 `ITcsDamageFlowDelegate` / `FTcsDamageRecord` / `FTcsStepDamage` 链步骤（**Task 4**）；不做流程模板重定向栈（`FFlowRedirect`，D7-7——无状态/装备声明消费者，随 M3 状态轮）；不做上下文池化（无消费者）；不做值域策略（`IValueDomainPolicy` 轮）；不做元素克制表/护盾系统/伤害数字 UI/DoT。

## 顺延与落点裁决（提案显式交代）

1. **收集事件 Tag 采用公约名，不采用设计文档旧写法**：09 §2.2/§2.3 写 `Combat.Damage.Collect.<Step>`，那是 2026-09-18 Tag 公约（`Tcs.Event.<域>.<事件名>` + 原生声明 + 不进项目 Tag 表）之前的口径。本提案按公约落为 `Tcs.Event.Damage.Collect<StepName>`（逐条原生声明）。**理由**：项目侧声明会让"项目漏配 Tag 表 → 事件静默丢失"复活，而原生声明是插件的既有硬规则。
2. **`RunTemplate` 的步骤签名钉为返回 `bool`**（true = 继续 / false = 中止剩余步骤）：设计只写"流程失败/打断 → 无人消费"，未钉执行器的中止通道；不钉死则每个步骤各写各的失败语义。
3. **R3 不落上下文池化**（计划 sketch 写"Context 池化"）：门面签名即"调用方提供上下文"，池化无消费者且会引入生命周期纪律；随性能/宿主化轮。
4. **黑板无值域收口**：设计把流程属性黑板的价值域策略括注为"走 `IValueDomainPolicy` 同款形态"，而该策略接口 R3 不在（属性侧 `AVD_Custom` 亦只留逃逸位）——黑板作为**流程工作值**本就没有边界概念，本提案明示"R3 无收口"。
5. **`AttrCapture` 只落字段与语义**：`CapturedAttrs` 的**快照填充**发生在流程启动（标准步骤的配置行为）→ 归 Task 4；本任务只保证字段与"读默认 Live、命中读快照"的语义被钉住。
6. **文件落点**（照 `cpp-module-structure` 与 TcsDamage 计划）：`Public/Flow/`（上下文/黑板/模板/注册表）+ `Public/TcsDamageSubsystem.h` 与 `Private/TcsDamageSubsystem.cpp`（门面）+ `Private/Flow/`（实现）；日志走既有 `LogTcsDamage` 通道。

## Impact
- Affected specs: `damage-flow`（新建能力）——**无 MODIFIED/REMOVED**（TcsDamage 此前无规格；TcsEffect/TcsTargeting 的契约按需求零改动）。
- Affected code（`Source/TcsDamage/`）：
  - 新增 `Public/Flow/TcsDamageFlowContext.h`（上下文 + 三层值空间）、`Public/Flow/TcsFlowAttributes.h`（黑板 + `FTcsConsumePolicy`）、`Public/Flow/TcsFlowTemplate.h`、`Public/Flow/TcsFlowStepExecutor.h`（签名 + 注册表 + 待解析表 + 注册器 + 两宏）、`Public/TcsDamageSubsystem.h`；
  - 新增 `Private/Flow/TcsFlowStepExecutor.cpp`（注册表实现）、`Private/TcsDamageSubsystem.cpp`（模板登记表 + 解释器 + 收集事件广播）；
  - 临时测试装置 `Private/Testing/TcsDamageTestRig.h/.cpp`（**不入库**）：装置步骤（记录执行顺序 + 可配置返回 false）+ 一个订阅收集事件的装置订阅者 + 命令 `Tcs.Test.Damage.Flow`（正路：登记模板 → 执行 → 步骤顺序与黑板读数；含"未登记模板拒绝"与"步骤返回 false 中止"）+ 可选 `.Reject`（未注册步骤类型中止——故意触发 Error）。
  - **零改动**：TcsEffect / TcsTargeting / TcsAttribute 全部文件。
- 决策依据：`09-module-damage.md` v4（§2.1 三层值空间 / §2.2 流程模板与步骤库 / §2.3 修改器唯一通道 / §3 收集≠消费）；D7-1~D7-7（尤其 D7-5 流程管线宿主化、D7-6 修改器唯一通道、**D7-2/PV-7 流程零计算**）；**D5-5 v3（折叠器三处共用）**；D4-14（注册制分派）、D2-2（来源级联）、D0-1（确定性）、D0-4（单线程）；2026-09-18 Tag 命名公约。
- 验证：UBT Development Editor 编译（零警告）+ 装置定向检查（模板登记/查询、步骤顺序、黑板提交与读取、收集事件同步到达、中止语义、未登记模板拒绝）+ **折叠器复用自检**（grep 确认黑板不含第二份带式折叠实现）。

## 提案内钉名（plan / 设计文档未钉或需收窄，供审阅否决）

| 项 | 钉法 | 依据 |
|---|---|---|
| 步骤执行器签名 | `FTcsFlowStepExecute = TFunction<bool(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)>` | 计划 sketch 只写"注册表与 Effect 宏同构、独立实例"，未钉签名；设计只说"流程失败/打断 → 无人消费"——用 `bool` 把"中止"通道钉在签名上（Effect 侧是挂起协议所以返回枚举，流程无挂起故用布尔） |
| 收集事件 Tag | `Tcs.Event.Damage.Collect<StepName>`（逐条原生声明） | 设计文档写 `Combat.Damage.Collect.<Step>`（早于 2026-09-18 公约）；公约 + 原生声明是既有硬规则（项目侧声明会让"漏配 Tag 表 → 静默丢事件"复活） |
| 收集事件载荷 | `FTcsDamageFlowCollectEvent{ FTcsDamageFlowContext* Context; }`（USTRUCT，裸指针非 UPROPERTY） | 设计写"payload = Context 引用包装"——总线载荷是 `FInstancedStruct`（须反射可见），C++ 引用不可反射；用瞬态裸指针包装 + 契约明文"派发期有效、MUST NOT 跨帧"（同步立即通道保证安全） |
| `FTcsConsumePolicy` 字段 | `MaxUses`（int32，0=不限）/ `Cooldown`（double 秒，0=无）/ `SortKey`（int32）/ `OnConsumed` 回调位 | 设计写 `{MaxUses/Cooldown/OnConsumed/SortKey}` 未钉类型；`SortKey` 与裁决语义一致（Execute 步骤选一，D7-4） |
| 黑板的值域 | **R3 无收口**（无边界/值域模式字段） | 黑板是流程工作值不是角色属性；设计把值域策略括注为 `IValueDomainPolicy` 同款形态，而该接口 R3 不在（属性侧同样只留逃逸位）——明示比留空好 |
| 上下文池化 | **R3 不做** | 计划 sketch 写"Context 池化"，但门面签名即"调用方提供上下文"，无消费者且引入生命周期纪律 |
| 模板重定向 | **R3 不做**（`FFlowRedirect` 随 M3 状态轮） | D7-7 的消费者是"状态/装备声明换流程"，R3 无状态模块（TcsState 不在竖切） |

## 检查点
落点验收 = UBT 编译（Development Editor，零警告）+ 装置定向检查（登记/查询/重复登记拒绝、步骤顺序、黑板提交-读取-重置、收集事件同步到达并可提交、步骤返回 false 中止、未登记模板与未注册步骤类型的拒绝面）+ **折叠器复用自检**（黑板无第二份带式折叠）+ **零改动自检**（`git diff -- Source/TcsEffect -- Source/TcsTargeting` 为空）。标准步骤库、官方默认模板、`ITcsDamageFlowDelegate`、`FTcsDamageRecord`、`FTcsStepDamage` 不在本提案（Task 4）。
