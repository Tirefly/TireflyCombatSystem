## MODIFIED Requirements

### Requirement: 流程步骤注册表与自注册宏

`TcsDamage` MUST 提供**与 TcsEffect 执行器注册表同构、但独立实例**的流程步骤注册表（D7-5"流程 = 数据模板 + 步骤库"）：

- 执行器签名：`FTcsFlowStepExecute = TFunction<bool(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)>`——**返回 false = 中止流程剩余步骤**（流程失败/打断；已产生的副作用不回滚——止于未来）；
- 注册键 = 步骤 struct 的**反射类型**（同 Effect 口径）；**双入口**（静态自注册宏 + 动态 `Register`）；
- 宏对：`UE_DECLARE_FLOW_STEP_EXECUTOR(ExecutorFn)` / `UE_DEFINE_FLOW_STEP_EXECUTOR(StepType, ExecutorFn)`——**静态初始化期零 UObject 触达**（只入待解析表、首次查询才解析反射类型；依据与 Effect 侧同：引擎 `FNativeGameplayTag::GetIfAllocated()` 纪律）；
- 同类型重复登记 MUST 拒绝（ensure + 保留首个）；未知步骤类型在执行期 MUST 中止流程 + Error 日志（含步骤序号与类型名）；
- **宿主脚本可登记流程步骤（2026-09-24 新增，台账 SCRIPT-8）**：`UTcsDamageSubsystem` MUST 提供 `UFUNCTION() bool RegisterStepExecutor(UScriptStruct* StepStruct, UTcsFlowStepExecutor* Executor)`——**与 Effect 侧同款插槽手法**（UObject 基类替代 `TFunction` 作注册值），新增 `UTcsFlowStepExecutor : UObject`（`UCLASS(Abstract, Blueprintable)`）带 `UFUNCTION(BlueprintNativeEvent) bool Execute(const FInstancedStruct& StepData, const FTcsDamageFlowContextView& Context)`；
  - **为什么流程侧也需要**："流程阶段构成 = 项目知识"（D7-5）——宿主自研的流程步骤（如"先算护盾再生再结算"）正是最典型的"客制化、只服务宿主业务、不值得进插件"的语义；
  - **形参用视图不用原上下文**：同 `damage-primitive` 的 `FTcsDamageFlowContextView`（`FTcsDamageFlowContext` 是非反射纯 C++ struct，出现在 `UFUNCTION` 签名里会让 UHT 报错）；
  - **GC 可见持有**：门面 MUST 以 `UPROPERTY TArray<TObjectPtr<UTcsFlowStepExecutor>>` 持有已登记执行器（同 `Templates` 的 WAIT-8 教训：裸容器持不住对象引用 ⇒ 静默回收 ⇒ 表现为"步骤不生效"）；
  - **双轨并存**：C++ 自注册宏路径 MUST 保持原样。
- **寿命语义（2026-09-29 新增，`DEC-04` 裁定 ⑤；修 `LEDGER-reflection` R-2 跨世界寿命缺陷）**：本注册表与 Effect 侧同为**进程级单例**且 MUST 保持如此，其**动态**登记项 MUST 受同款寿命约束——**MUST 记录宿主对象弱引用 + 登记世界弱引用**；静态自注册的纯函数项 MUST NOT 受寿命约束；
  - **失效判据**（三者任一即失效）：宿主弱引用为空（对象已被 GC）/ 世界弱引用为空 / 条目世界 ≠ 查询方世界；
  - **查询侧校验**：`Find` MUST 接受可选的世界校验入参（**调用方持有世界时 MUST 显式传入**）；跨世界失效 MUST 视为未命中（返回 nullptr）并移除该条目 + 留含类型名的 **Warning** 日志，MUST NOT 静默按"未登记"处理（否则"世界已更换"会被表现成"步骤类型写漏"，而后者走"未知步骤类型 → 中止 + Error"路径，两者混为一谈会显著抬高排障成本）；
  - **拒绝门收窄（"同世界活对象重复"）**：既有条目已按上述判据失效时，新登记 MUST **替换**该条目而 MUST NOT 拒绝——这是"失效条目毒化后续所有 PIE"的根因所在；仅当既有条目有效且属同世界（或为静态自注册项）时才 MUST 拒绝（ensure + 保留首个）；
  - **显式移除入口**：MUST 提供按键移除**动态**条目的入口，MUST NOT 能移除静态自注册项；缺省不调用时正确性 MUST NOT 受影响（失效判据须自足——对应用户 2026-09-27 裁定"TCS 侧兜底优先，注销作退路"）；
  - **`Deinitialize` 收口**：门面 MUST 撤销**本世界**登记的动态条目；但门面的 `UPROPERTY` GC 可见持有 MUST 保留——两者互补而非替代（强持有管"别被收走"，弱引用管"收走了/换世界了就失效"）。

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

#### Scenario: 同一世界内重复登记被拒

- **WHEN** 对同一类型在**同一世界**内、既有登记**仍有效**时再次 `Register`
- **THEN** 拒绝并保留首个登记，留 ensure 提示

#### Scenario: 第二个世界的同类型登记替换失效条目

- **WHEN** 第一个世界登记的流程步骤执行器随该世界结束而失效，第二个世界对同类型再次登记
- **THEN** 替换成功、MUST NOT 触发"重复登记"拒绝、MUST NOT 产生 ensure 红字；随后流程分派抵达第二个世界的执行器

#### Scenario: 宿主执行器被回收后不被解引用

- **WHEN** 动态登记的流程步骤执行器已无其他强引用并被 GC，随后流程走到该步骤
- **THEN** 该条登记判为失效（查询返回 nullptr），流程按"未知步骤类型"中止并留 Error；MUST NOT 解引用已回收对象、MUST NOT 崩溃

#### Scenario: 门面 Deinitialize 撤销本世界动态条目

- **WHEN** 某世界登记的动态条目在该世界门面 `Deinitialize` 后再次查询
- **THEN** 返回 nullptr（本世界登记被撤销）；静态自注册项不受影响
