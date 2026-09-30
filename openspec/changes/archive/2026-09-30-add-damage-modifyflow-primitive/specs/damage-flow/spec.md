## MODIFIED Requirements

### Requirement: 流程属性黑板

`FTcsFlowAttributes`（流程工作值的容器：键 + 每键修正链）MUST 提供：

- **键类型 = `FGameplayTag`**（**2026-09-22 改造：`FName` → `FGameplayTag`**）——契约键（`BaseDamage` / `Executed` / `Absorbed` / `Kill` / `Hit` / `Crit` / `ExecuteCandidates`）属**标准步骤库的契约**，由**插件原生声明**（`Tcs.Flow.Key.*`，常量名逐点换下划线）；项目自定义键自由，由**项目 ini** 声明；
- **提交**：`Submit(Key, Op, Operand, Consume)`——Operand 为 `FTcsParamValue`（PV 系列载体；黑板键引用保留为流程域自身 Operand 选项）；`SortKey` **只用于消耗裁决、MUST NOT 参与求值顺序**（`SortKey` 是消耗策略的字段，不是本方法的独立形参——本参数列表以源码为准校正）；
- **读取**：`Read(Key)` 按 **M2 同款带式语义**求值——**折叠 MUST 调用 TcsAttribute 的共享纯函数 `FoldTcsAttributeBands`**（D5-5 v3：M2 属性聚合 / M5 参数链 / 本容器**三处共用，MUST NOT 私建第二份**）；
- **收集重置**：`Reset()`——清空全部键的收集（标准步骤 `CollectStart` 的落点）；
- **消耗策略位**：`FTcsDamageModifierConsumePolicy`（**2026-09-30 改名**：原 `FTcsConsumePolicy`——"DamageModifier" 指名"伤害修改器"这一提交者身份，与走运算带聚合的**属性**修正器明确区分）——字段集合限为 `{ MaxUses; Cooldown; SortKey; }` 的**纯数据**，**MUST NOT 含 `TFunction` / 闭包成员**（`OnConsumed` 回调位**已移除**，2026-09-30：消费行为改由**事件语义**表达，见 `damage-primitive` 的「消耗策略为纯数据可反射结构」需求）；**本容器只存不裁**：裁决（SortKey 选一 → 成功才消费）归 `Execute` 步骤（D7-4"收集 ≠ 消费"）；
- **R3 不做值域收口**：黑板是流程工作值、**不是角色属性**——无边界/值域模式概念（`IValueDomainPolicy` 同款逃逸位随值域策略轮，见 09 §2.1 括注）。

#### Scenario: 同键多笔提交求和且与顺序无关

- **WHEN** 对同一键提交两笔 `Add`（或同一组含不同运算带的提交按两种排列）
- **THEN** `Read` 结果一致（带序由 `Op` 决定、顺序无关——共享折叠函数的既有保证）

#### Scenario: 收集重置

- **WHEN** 提交若干修正后调用 `Reset`
- **THEN** 该容器全部键回到空收集状态（`Read` 返回折叠初值 0）

#### Scenario: 消耗策略只存不裁

- **WHEN** 提交时携带 `FTcsDamageModifierConsumePolicy`（如 `MaxUses = 1`）
- **THEN** `Read` 行为不受策略影响，策略可被后续裁决步骤读回（消费语义不在本容器）

#### Scenario: 消耗策略不含闭包

- **WHEN** 检查 `FTcsDamageModifierConsumePolicy` 的成员
- **THEN** 全部为可序列化的纯数据字段（无 `TFunction`）——该结构因此可作 `UPROPERTY`（链步骤 / 数据步骤得以携带消耗策略），且不再阻断 `FTcsDamageFlowContext` 的逐字段反射化
