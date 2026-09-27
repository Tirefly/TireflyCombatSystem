## MODIFIED Requirements

### Requirement: 流程步骤注册表与自注册宏

`TcsDamage` MUST 提供**与 TcsEffect 执行器注册表同构、但独立实例**的流程步骤注册表（D7-5"流程 = 数据模板 + 步骤库"）：

- 执行器签名：`FTcsFlowStepExecute = TFunction<bool(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)>`——**返回 false = 中止流程剩余步骤**（流程失败/打断；已产生的副作用不回滚——止于未来）；
- 注册键 = 步骤 struct 的**反射类型**（同 Effect 口径）；**双入口**（静态自注册宏 + 动态 `Register`）；
- 宏对：`UE_DECLARE_FLOW_STEP_EXECUTOR(ExecutorFn)` / `UE_DEFINE_FLOW_STEP_EXECUTOR(StepType, ExecutorFn)`——**静态初始化期零 UObject 触达**（只入待解析表、首次查询才解析反射类型；依据与 Effect 侧同：引擎 `FNativeGameplayTag::GetIfAllocated()` 纪律）；
- 同类型重复登记 MUST 拒绝（ensure + 保留首个）；未知步骤类型在执行期 MUST 中止流程 + Error 日志（含步骤序号与类型名）；
- **宿主脚本可登记流程步骤（2026-09-24 新增，台账 S-8）**：`UTcsDamageSubsystem` MUST 提供 `UFUNCTION() bool RegisterStepExecutor(UScriptStruct* StepStruct, UTcsFlowStepExecutor* Executor)`——**与 Effect 侧同款插槽手法**（UObject 基类替代 `TFunction` 作注册值），新增 `UTcsFlowStepExecutor : UObject`（`UCLASS(Abstract, Blueprintable)`）带 `UFUNCTION(BlueprintNativeEvent) bool Execute(const FInstancedStruct& StepData, const FTcsDamageFlowContextView& Context)`；
  - **为什么流程侧也需要**："流程阶段构成 = 项目知识"（D7-5）——宿主自研的流程步骤（如"先算护盾再生再结算"）正是最典型的"客制化、只服务宿主业务、不值得进插件"的语义；
  - **形参用视图不用原上下文**：同 `damage-primitive` 的 `FTcsDamageFlowContextView`（`FTcsDamageFlowContext` 是非反射纯 C++ struct，出现在 `UFUNCTION` 签名里会让 UHT 报错）；
  - **GC 可见持有**：门面 MUST 以 `UPROPERTY TArray<TObjectPtr<UTcsFlowStepExecutor>>` 持有已登记执行器（同 `Templates` 的 T-8 教训：裸容器持不住对象引用 ⇒ 静默回收 ⇒ 表现为"步骤不生效"）；
  - **双轨并存**：C++ 自注册宏路径 MUST 保持原样。

#### Scenario: 自注册后即时可执行

- **WHEN** 某模块用宏登记一个流程步骤类型（该模块已加载）
- **THEN** 注册表可查到其执行器，模板中的该步骤被解释器分派执行

#### Scenario: 未知步骤类型中止流程

- **WHEN** 模板含未注册执行器的步骤
- **THEN** 该流程中止（后续步骤不执行）+ Error 日志给出步骤序号与类型名

#### Scenario: 步骤返回 false 即中止

- **WHEN** 某步骤执行器返回 false
- **THEN** 后续步骤不执行（流程视为失败），已执行的步骤副作用保留

#### Scenario: 脚本层登记流程步骤并生效

- **WHEN** 宿主脚本层定义 `UTcsFlowStepExecutor` 子类实现 `Execute`，用自定义步骤 struct 类型经 `RegisterStepExecutor` 登记，并把该步骤放进模板
- **THEN** 流程跑到该步时分派到脚本执行器，流程行为由脚本决定（含返回 false 中止流程）
