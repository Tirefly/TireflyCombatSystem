## MODIFIED Requirements

### Requirement: 流程上下文（三层值空间）

`FTcsDamageFlowContext`（黑板，**纯运行态结构**）MUST 承载 09 §2.1 的三层值空间定位：

- **参与者一律为实体身份句柄**（`FTcsCombatEntityHandle`）——`Attacker` / `Instigator` 为单句柄，`Targets` 为句柄数组；**MUST NOT 使用 `AActor*` / `TWeakObjectPtr<AActor>`**（D3-1 Actor 无关性；06 §33"Mass 适配核心零改动"的前提）；"目标还在不在"经注入接口 `IsAlive` 询问宿主，框架不持有 Actor 生命周期引用；
- **公式参数初值**：`TMap<FName, FTcsParamValue> FormulaParams`——**只读原料**（键 = 项目词表，**MUST NOT 用下标**；宿主公式按名取用）；
- **流程属性黑板**：`FTcsFlowAttributes Blackboard`（伤害计算的工作值）；
- `TArray<FGameplayTag> ClassificationTags`（来源标签启动写入 / 元素标签由 Element 步骤写入——词表归项目，插件只搬运与匹配）；
- `FTcsSourceHandle FlowSource`——**每流程唯一**的作用域修改器归属锚点（**MUST 由 `RunTemplate` 在起流程时分配**；流程结束 `RemoveBySource` 级联摘除）；
- `TMap<FTcsAttributeName, double> CapturedAttrs`——属性捕获快照（读默认 Live、命中读快照；**快照填充归标准步骤**，本层只落字段与语义）；
- 生命期：由调用方构造、`RunTemplate` 就地使用，**MUST NOT 跨帧持有**；R3 MUST NOT 引入上下文池化（零消费者不预建）。

#### Scenario: 参与者是句柄而非 Actor

- **WHEN** 检查 `FTcsDamageFlowContext` 的参与者字段
- **THEN** 全部为 `FTcsCombatEntityHandle`（无任何 Actor 类型）——无 Actor 实体（未来 Mass）同样可跑流程

#### Scenario: 起流程分配唯一来源锚点

- **WHEN** 连续执行两次 `RunTemplate`
- **THEN** 两次的 `Context.FlowSource` 互异且有效（非零）——作用域修改器可被精确级联摘除

#### Scenario: 公式参数按名取用

- **WHEN** 上下文里 `FormulaParams` 含键 `DmgCoeff`
- **THEN** 消费方按名取值（结构上不存在下标访问路径）
