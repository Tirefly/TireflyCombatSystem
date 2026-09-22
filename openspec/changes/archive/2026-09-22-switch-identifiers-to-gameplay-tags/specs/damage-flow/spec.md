## MODIFIED Requirements

### Requirement: 流程模板与登记表

`TcsDamage` MUST 以**数据模板**承载瞬时流程（D7-5 流程管线宿主化：阶段构成本身是项目知识，插件不预设）：

- `FTcsFlowTemplate{ FGameplayTag TemplateId; TArray<FInstancedStruct> Steps; }`（USTRUCT；**2026-09-22 改造：`TemplateId` 类型 `FName` → `FGameplayTag`**）——有序步骤数组，元素形状同 `FEffectStep`（步骤无公共基类，类型合法性由执行器注册表在执行期判定）；
- `UTcsDamageSubsystem`（世界级子系统门面）MUST 提供登记表：`RegisterTemplate` / `UnregisterTemplate` / `FindTemplate`，**键 = `TemplateId`**；拒绝面（ensure + false）：`TemplateId` **无效**（`!TemplateId.IsValid()`）、同 id 重复登记（不得静默覆写）；`FindTemplate` 未登记返回 nullptr（**不 ensure**——正常查询路径）；
- `RunTemplate` 遇未登记模板 MUST 拒绝：Error 日志 + 不执行（执行期配置错误，不 ensure）；
- 登记表 MUST 以地址稳定方式持有模板（`TMap<FGameplayTag, TUniquePtr<…>>` 或同效手段；**2026-09-22 改造：键类型改 tag**）——解释器在一次执行期间持模板引用。

**模板 id 的来源（2026-09-22）**：流程模板是**项目知识**（D7-5"阶段构成本身是项目知识"），故 `TemplateId` 属**项目词汇**——由项目 `Config/DefaultGameplayTags.ini` 声明（`Tcs.Flow.Template.<Id>`）。**例外**：插件自登记的官方默认模板 `Default` 属**框架词汇**，由插件原生声明（`Tag_Tcs_Flow_Template_Default`）。

#### Scenario: 登记后可按 id 查到

- **WHEN** `RegisterTemplate` 一条 `TemplateId = Tcs.Flow.Template.Flow_Test` 的模板后 `FindTemplate`（同一 tag）
- **THEN** 返回该模板，步骤数组与登记内容一致

#### Scenario: 重复登记被拒

- **WHEN** 对同一 `TemplateId` 再次 `RegisterTemplate`
- **THEN** 返回 false、原模板不被覆写、留 ensure 提示与日志

#### Scenario: 未登记模板被拒绝执行

- **WHEN** `RunTemplate` 一个未登记的 `TemplateId`（有效 tag 但未登记）
- **THEN** 不执行任何步骤 + Error 日志（不崩溃、不 ensure）

### Requirement: 流程上下文（三层值空间）

`FTcsDamageFlowContext` MUST 承载三层值空间（09 §2.1）：账本（技能参数，**不住这里**——由链步骤解算后经请求字段传入）、**公式参数初值**、**流程属性黑板**。

- **公式参数初值**：`TMap<FGameplayTag, FTcsParamValue> FormulaParams`——**只读原料**（键 = 项目词表，**MUST NOT 用下标**；宿主公式按名取用；**2026-09-22 改造：键类型 `FName` → `FGameplayTag`**）；
- **请求字段**（上下文请求，不随收集重置清除）：`BaseDamageInput: double`（链步骤解算结果）与 `TargetAttrKey: FGameplayTag`（扣血属性键；**2026-09-22 改造：原 `FTcsAttributeName`**）；
- `TMap<FGameplayTag, double> CapturedAttrs`——属性捕获快照（读默认 Live、命中读快照；**快照填充归标准步骤**，本层只落字段与语义；**2026-09-22 改造：键类型改 tag**）。

#### Scenario: 请求字段不随收集重置

- **WHEN** 流程跑过 `CollectStart`（重置黑板）后读取 `BaseDamageInput`
- **THEN** 值仍在（请求字段不受收集重置影响——这是"输入 vs 收集产物"分离的落点）

### Requirement: 流程属性黑板

`FTcsFlowAttributes`（流程工作值的容器：键 + 每键修正链）MUST 提供：

- **键类型 = `FGameplayTag`**（**2026-09-22 改造：`FName` → `FGameplayTag`**）——契约键（`BaseDamage` / `Executed` / `Absorbed` / `Kill` / `Hit` / `Crit` / `ExecuteCandidates`）属**标准步骤库的契约**，由**插件原生声明**（`Tcs.Flow.Key.*`，常量名逐点换下划线）；项目自定义键自由，由**项目 ini** 声明；
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

### Requirement: 流程解释器（同步单帧）

`UTcsDamageSubsystem::RunTemplate(FGameplayTag TemplateId, FTcsDamageFlowContext& Context)` MUST（**2026-09-22 改造：`TemplateId` 类型 `FName` → `FGameplayTag`**）：

- 按模板顺序**同步执行全部步骤、单帧内完成**——MUST NOT 挂起（瞬时流程无异步语义，区别于效果链的挂起点；09 §3）；
- 门面 MUST NOT 是 Tickable（流程无每帧成本）；
- 每次执行 MUST 只读模板（MUST NOT 改写共享模板数据）。

#### Scenario: 单帧同步走完

- **WHEN** 执行一条全部步骤都返回 true 的模板
- **THEN** `RunTemplate` 返回时全部步骤已执行完毕（无挂起、无延迟步骤）

#### Scenario: 模板不可变

- **WHEN** 同一模板被连续执行两次
- **THEN** 第二次行为与第一次一致（模板数据未被执行过程改写）
