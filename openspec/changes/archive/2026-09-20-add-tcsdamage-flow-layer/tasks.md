## 1. Implementation

### 数据结构（Public/Flow/）

- [ ] 1.1 `Public/Flow/TcsDamageFlowContext.h`：`FTcsDamageFlowContext{ Attacker / Instigator / Targets / FormulaParams(TMap<FName, FTcsParamValue>) / Blackboard(FTcsFlowAttributes) / ClassificationTags / FlowSource(FTcsSourceHandle) / CapturedAttrs(TMap<FTcsAttributeName, double>) }`（纯运行态结构，非反射；注释写明三层值空间定位、"按名取用永不用下标"、"读默认 Live 命中读快照"）
- [ ] 1.2 `Public/Flow/TcsFlowTemplate.h`：`FTcsFlowTemplate{ FName TemplateId; TArray<FInstancedStruct> Steps; }`（USTRUCT，模板=数据 D7-5）
- [ ] 1.3 `Public/Flow/TcsFlowAttributes.h`：`FTcsConsumePolicy{ MaxUses / Cooldown / SortKey / OnConsumed 位 }` + `FTcsFlowAttributes`——`Submit(Key, Op, Operand(FTcsParamValue), SortKey, ConsumePolicy)` / `Read(Key)` / `Reset()` / 候选访问（供 Task 4 的 Execute 裁决读回）；**`Read` 内部调用 `FoldTcsAttributeBands`**（`Attribute/TcsAttributeBandFold.h`）——注释写明"D5-5 v3 三处共用，MUST NOT 私建第二份"；`SortKey` 不入求值顺序；R3 无值域收口

### 步骤注册表与自注册宏（同构 Effect，独立实例）

- [ ] 1.4 `Public/Flow/TcsFlowStepExecutor.h`：`FTcsFlowStepExecute = TFunction<bool(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)>` + `FTcsFlowStepExecutorEntry`（待解析项）+ `FTcsFlowStepExecutorRegistrar` + `FTcsFlowStepExecutorRegistry`（`Get` / `AddPending` / `Register` / `Find`；键 = 步骤反射类型；首次查询解析待解析表）+ 宏对 `UE_DECLARE_FLOW_STEP_EXECUTOR` / `UE_DEFINE_FLOW_STEP_EXECUTOR(StepType, ExecutorFn)`
- [ ] 1.5 `Private/Flow/TcsFlowStepExecutor.cpp`：注册表实现（**静态初始化期零 UObject 触达**——照 `TcsEffectStepExecutor.cpp` 的延迟解析设计；重复登记 ensure + 保留首个）

### 门面与解释器

- [ ] 1.6 `Public/TcsDamageSubsystem.h` + `Private/TcsDamageSubsystem.cpp`：`UWorldSubsystem`（**非 Tickable**）——`RegisterTemplate` / `UnregisterTemplate` / `FindTemplate`（键 = TemplateId，`TMap<FName, TUniquePtr<>>` 地址稳定持有；重复登记拒、未登记查询 nullptr）+ `RunTemplate(FName, FTcsDamageFlowContext&)`（**同步单帧**逐步执行；步骤返回 false 或未知类型 → 中止 + Error；起流程分配 `FlowSource`）+ `PublishCollectEvent(步骤标识, Context)`
- [ ] 1.7 收集事件 Tag：`Public/Flow/TcsDamageFlowCollectEvent.h`——**原生声明** `Tcs.Event.Damage.Collect<StepName>`（`UE_DECLARE_GAMEPLAY_TAG_EXTERN` / `UE_DEFINE_GAMEPLAY_TAG`）+ 载荷 `FTcsDamageFlowCollectEvent{ FTcsDamageFlowContext* Context; }`（注释写明进程内瞬态、MUST NOT 跨帧）；广播走总线 **立即通道**
- [ ] 1.8 `Deinitialize` 确定性清理（模板表与注入清空）

### 临时测试装置（`Private/Testing/`，**不入库**）

- [ ] 1.9 `Private/Testing/TcsDamageTestRig.h/.cpp`：装置步骤三件（`FTcsTestFlowMark{Label}` 记顺序 / `FTcsTestFlowSubmit{Key,Op,Value}` 提交黑板 / `FTcsTestFlowHalt` 恒返回 false）+ 装置订阅者（订阅收集事件 → 在回调里 `Submit`，验证"同步到达 + 响应可提交"）+ 宏自注册
- [ ] 1.10 `Tcs.Test.Damage.Flow`（正路，**零故意 ensure/Error**）：登记模板 `[Mark → Submit → Collect 步骤 → Mark]` → 执行 → 断言步骤顺序、黑板读数（共享折叠器结果）、收集事件同步到达且响应写入可见、`FlowSource` 唯一非零；边界：未登记模板拒绝（Error 属预期 → 归到 .Reject）
- [ ] 1.11 `Tcs.Test.Damage.Flow.Reject`（**opt-in**）：未登记模板 / 未注册步骤类型 / 步骤返回 false 三条中止路径（均为 Error 或 Warning，屏显先声明）

## 2. Verification

- [ ] 2.1 UBT Development Editor 编译通过（零警告）
- [ ] 2.2 **折叠器复用自检**：黑板实现只调用 `FoldTcsAttributeBands`（无第二份带式折叠；grep 佐证）
- [ ] 2.3 **零改动自检**：`git diff --stat -- Source/TcsEffect Source/TcsTargeting Source/TcsAttribute` 为空（流程层不反向侵入既有模块）
- [ ] 2.4 依赖面核对：`Source/TcsDamage/` 不 include `TcsTargeting/TcsState/TcsSkill/TcsIntegration`
- [ ] 2.5 用户 PIE 人工检查：装置命令输出（步骤顺序 / 黑板读数 / 收集事件同步到达 / 中止语义）
- [ ] 2.6 文档回写：plan2 Task 3 勾选 + 实施注记；台账 R6-1 勾销（折叠器三处共用的中间一处落地）；README 检查点状态
