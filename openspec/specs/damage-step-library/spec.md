# damage-step-library Specification

## Purpose
TBD - created by archiving change add-tcsdamage-steps-and-primitive. Update Purpose after archive.
## Requirements
### Requirement: 标准步骤库（十步）

`TcsDamage` MUST 以内建步骤 struct + 执行器提供 `09 §2.2` 的标准十阶段，**全部经 `UE_DEFINE_FLOW_STEP_EXECUTOR` 自注册**（住 `Public/Flow/Steps/` 与 `Private/Flow/Steps/`）：

- `FTcsFlowCollectStart`：发流程开始事件（原生 Tag `Tcs.Event.Damage.FlowStarted`）+ **重置收集**（`Blackboard.Reset()`）；
- `FTcsFlowPreHit`：发收集事件 → 应用（宿主挂点：修改命中率/注册免疫候选）；
- `FTcsFlowHit`：基础命中率（delegate `GetBaseHitRate`）→ 修改器 → 判定，结果写黑板（键 `Hit`，1/0）；
- `FTcsFlowCrit`：基础暴击率（`GetBaseCritRate`）→ 修改器 → 判定（键 `Crit`）；
- `FTcsFlowElement`：delegate `ResolveElement` 解析元素 → 写入 `ClassificationTags`；
- `FTcsFlowBaseDamage`：**接收输入值**（`Context.BaseDamageInput`——**请求字段，不随 `CollectStart` 的收集重置清除**；链步骤解算结果经此传入，PV-7/D7-2：基础值来源 = 链步骤配置，流程不得自行推导）→ 可经 delegate 逃生口 → 以 **`Add`** 提交到 `OutputKey`（默认 `BaseDamage`）——**MUST NOT 用覆盖带写基值**（覆盖带盖掉一切，会抹掉收集到的修正）；
- `FTcsFlowAfterDamage`：发收集事件 → 应用（宿主挂点："伤害 +50"）；
- `FTcsFlowPreExecute`：发收集事件 → **只收集**免疫/减伤候选（收集 ≠ 消费，D7-4）；
- `FTcsFlowExecute`：免疫/减伤**裁决**（按消耗策略的 `SortKey` 选一）→ 宿主护盾 hook（delegate `ModifyShield`）→ **M2 事务扣血**（属性键 = **步骤级 `AttrKey` 优先、否则用 `Context.TargetAttrKey`**——插件组装的官方默认模板不可能知道项目词表（`Health` 是项目侧的），故"打哪个属性"必须由请求方给出；两者皆空 → 不扣血 + Warning）→ **成功才消费**（调用命中候选的 `OnConsumed`）；
- `FTcsFlowCompleted`：发完成事件 + 填充并发布 `FTcsDamageRecord`（见 `damage-primitive`）。

**执行期行为契约**：正路共用的黑板键名（`Hit` / `Crit` / `BaseDamage` / `FinalDamage`）属**标准步骤库的契约**（M8 校验与 Explain 认识），项目自定义键自由 `FName`；步骤 MUST NOT 自行推导基础伤害（零公式纪律）。

#### Scenario: 默认四步跑通

- **WHEN** 执行 `CollectStart → BaseDamage → Execute → Completed`
- **THEN** 收集被重置、`BaseDamage` 取到输入值、`Execute` 对目标属性做了事务扣血、完成事件与记录都已产出

#### Scenario: 十步均可被解释器分派

- **WHEN** 查询流程步骤注册表
- **THEN** 十步的执行器全部可查（模块加载即自注册，零启动代码）

### Requirement: 步骤 Conditions 挂点

流程步骤 MUST 支持"条件不过则跳过"（09 §2.2 的**每步骤可带 Conditions**）：

- 每个步骤 struct MUST 持**同名字段** `TArray<FInstancedStruct> Conditions`（元素为**纯数据谓词** struct——非策略对象，故不适用策略形态一元化）；
- 共享求值助手 `ShouldRunFlowStep(const TArray<FInstancedStruct>& Conditions, const FTcsDamageFlowContext& Context) -> bool`（住 `Public/Flow/TcsFlowStepConditions.h`）；**每个步骤执行器 MUST 首行调用**（步骤无公共基类（D4-16），故为纪律而非基类钩子）；
- 不过 → **跳过该步并继续流程**（返回 true，不是中止）；未知条件类型 → 视为不过 + Warning 日志（不静默通过）；
- R3 实现两个求值器：`FTcsConditionHasAllTags{ TArray<FGameplayTag> Tags }`（对 `ClassificationTags` 判定）与 `FTcsConditionChance{ double Probability }`（**概率型条件显式声明随机源**：宿主未注入随机源时退化为确定性判定，保证可复现——D0-1 的例外必须显式）。

#### Scenario: 条件不过即跳过

- **WHEN** 某步骤的 `Conditions` 含 `FTcsConditionHasAllTags{Tags=[火]}` 而上下文的 `ClassificationTags` 不含火
- **THEN** 该步不执行、流程继续走下一步

#### Scenario: 未知名条件不静默通过

- **WHEN** `Conditions` 含没有求值器的 struct 类型
- **THEN** 该步被跳过 + Warning 日志（不静默通过、不崩溃）

### Requirement: 通用数据步骤

`TcsDamage` MUST 提供两个"编辑器拼流程零 C++"的数据步骤（D7-5）：

- `FTcsFlowModify{ FName TargetKey; ETcsAttributeOp Op; FTcsParamValue Operand; TArray<FInstancedStruct> Conditions; }`——数据化黑板写入（"破甲阶段" = 一个数据步骤）；Operand 为 `FTcsParamValue`（黑板键引用保留为流程域自身的 Operand 选项，随其来源策略轮落地）。**MUST NOT 携带消耗策略**：`FTcsConsumePolicy` 含 `TFunction OnConsumed` 回调（纯 C++、不可反射、不可作 UPROPERTY）——消耗型提交只能来自 C++ 步骤或事件响应；数据步骤只做纯数值写入；
- `FTcsFlowDelegate{ FName TargetKey; TScriptInterface<UTcsDamageFlowDelegate> Delegate; ... }`——数据化委托调用（轻量公式挂法）；
- 两者的执行器 MUST 同样遵守 Conditions 契约与自注册契约。

#### Scenario: 数据步骤无需 C++ 即可改黑板

- **WHEN** 模板里放一个 `FTcsFlowModify{TargetKey=FinalDamage, Op=Mul, Operand=1.2}`
- **THEN** 该键的后续读取带上这一笔（无任何 C++ 改动）

### Requirement: 官方默认模板

`TcsDamage` MUST 在门面初始化时组装并登记**官方默认模板**（D7-5：标准十阶段只是插件自带的标准件，**可整表替换**）：

- 内容：`CollectStart → BaseDamage → Execute → Completed`；
- 标识：以约定 `TemplateId`（`Default`）登记；`FTcsStepDamage` 的 `FlowTemplateId` 为空时 MUST 解析到它；
- 其余六步（`PreHit` / `Hit` / `Crit` / `Element` / `AfterDamage` / `PreExecute`）的 struct 与执行器同批提供，但 **R3 不组装进默认模板**；
- 模板为**数据**，宿主的自定义模板可整表替换默认模板的行为（登记同 id 时以宿主显式登记为准——门面 MUST NOT 拒绝宿主覆盖 `Default`？**否**：按既有口径重复登记一律拒绝；宿主以自己的 id 登记并在链步骤里指定）。

#### Scenario: 空模板 id 落到默认模板

- **WHEN** `FTcsStepDamage.FlowTemplateId` 为空
- **THEN** 执行器解析到官方默认模板并跑通四步

#### Scenario: 标准件可替换

- **WHEN** 宿主登记自己的 `TemplateId` 并在链步骤里指定
- **THEN** 流程按宿主的模板执行（官方默认模板不受影响，仍可被其他链使用）

