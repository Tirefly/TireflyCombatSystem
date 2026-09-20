## 1. Implementation

### 池清空口（TcsCore，本任务为消费方）

- [x] 1.0 `Source/TcsCore/Public/Pool/TcsInstancePool.h`：新增 `Reset()`——按当前已分配槽位数一次性回落 `FTcsCorePoolStats` 后清空三数组（旧句柄全部代际失配）；与 `FTcsExpiryHeap::Reset` 同款纪律（不逐槽清理实例内容，外部资源由调用方在 Reset 前自清）

### 数据结构（Public/Chain/）

- [x] 1.1 `Public/Chain/TcsEffectStep.h`：`ETcsStepResult{TSR_Completed = 0, TSR_Running = 1}`（协议枚举，无 `Custom` 位）+ `FTcsEffectStep{ FInstancedStruct StepData; }`（USTRUCT，`TCSEFFECT_API`）
- [x] 1.2 `Public/Chain/TcsEffectChain.h`：`FTcsEffectChain{ FName ChainId; TArray<FInstancedStruct> Steps; int32 MaxStepsPerFrame = 64; }`（USTRUCT；注释写明"步骤无公共基类、数组不设 BaseStruct 限定、未知类型由解释器拒绝"）
- [x] 1.3 `Public/Chain/TcsEffectContext.h`：`FTcsEffectContext{ Caster / Instigator / EventPayload / Targets / Variables }`（纯运行态黑板，非反射结构；注释写明 `CapturedAttrs` 归 Damage 流 Context、注入引用归门面注入点）
- [x] 1.4 `Public/Chain/TcsChainRun.h`：`FTcsChainRunTag` + `FTcsChainRunHandle{ TTcsInstanceHandle<FTcsChainRunTag> Inner; IsValid(); }` + `FTcsChainRun{ ChainId / PC / Context / Owner / Self / PendingExpiry / bHasPendingExpiry }`（注释写明：不持链定义指针；指针 MUST NOT 跨步骤执行缓存）

### 执行器注册表与自注册宏

- [x] 1.5 `Public/Chain/TcsEffectStepExecutor.h`：`FTcsStepExecute = TFunction<ETcsStepResult(const FInstancedStruct&, FTcsEffectContext&, FTcsChainRun&)>`
- [x] 1.6 同文件：`FTcsEffectStepExecutorEntry{ UScriptStruct* (*GetStepStruct)(); FTcsStepExecute Executor; }`（待解析项）+ `FTcsEffectStepExecutorRegistrar`（静态自注册器，构造即入待解析表——**零 UObject 触达**）
- [x] 1.7 `Private/Chain/TcsEffectStepExecutor.cpp`：`Get()`（函数局部静态单例）/ `AddPending` / `Register`（动态入口，重复登记 ensure + 拒绝）/ `Find`（未命中返回 nullptr 不 ensure；**首次调用解析待解析表**）
- [x] 1.8 同头文件：`UE_DECLARE_EFFECT_STEP_EXECUTOR(ExecutorFn)` / `UE_DEFINE_EFFECT_STEP_EXECUTOR(StepType, ExecutorFn)`（注册器名 = `ExecutorFn` + `StepExecutorRegistrar`；`&StepType::StaticStruct` 作 getter）

### 宿主契约（Public/Host/）

- [x] 1.9 `Public/Host/TcsEntityQuery.h`：`UINTERFACE(MinimalAPI) UTcsEntityQuery` + `class ITcsEntityQuery`（`virtual void EnumerateEntities(TFunctionRef<void(AActor*)> Visitor) = 0;`；注释写明 R3 只声明遍历、`GetLocation`/`IsAlive` 随 RadiusArea 轮）

### 解释器与门面

- [x] 1.10 `Public/TcsEffectSubsystem.h`：`UWorldSubsystem` 门面（`DoesSupportWorldType` = Game/PIE/GamePreview；**非 Tickable**——挂起零每帧成本）
- [x] 1.11 门面登记面：`RegisterChain` / `UnregisterChain` / `FindChain`（键 = `ChainId`；重复登记与空 id 拒绝；**活动运行态的链拒绝注销**；`TMap<FName, TUniquePtr<FTcsEffectChain>>` 地址稳定持有）
- [x] 1.12 门面执行面：`ExecuteChain(FName, FTcsEffectContext) -> FTcsChainRunHandle` / `ResumeRun(FTcsChainRunHandle) -> bool` / `IsRunActive(FTcsChainRunHandle) const`
- [x] 1.13 门面注入面：`SetEntityQuery(const TScriptInterface<ITcsEntityQuery>&)` / `GetEntityQuery() const`（`UPROPERTY` 持引用；未注入返回 nullptr）
- [x] 1.14 `Private/TcsEffectSubsystem.cpp`：解释器 `RunFrom(Handle)`——按 id 解析链 → 逐步查注册表 → 执行 → `Completed` 前进 / `Running` 让出；**每次入器前与推进 PC 前重解析运行态指针**；超 `MaxStepsPerFrame` 熔断（ensure + Error + 断链）；未知类型断链（Error + 释放）；走完释放运行态
- [x] 1.15 同文件：`Deinitialize` 确定性清理（`RunPool.Reset()` + 清登记表与注入引用）；`LogTcsEffect` 记起链/挂起/唤醒/完成（Log 级）与逐步骤（Verbose 级）

### WaitDelay（控制流步骤 + 自注册样本）

- [x] 1.16 `Public/Chain/TcsStepWaitDelay.h`：`FTcsStepWaitDelay{ double Seconds = 0.5; }`（USTRUCT）
- [x] 1.17 `Private/Chain/TcsStepWaitDelay.cpp`：执行器（首入 → `UTcsClockSubsystem::PushExpiry(Elapsed + Seconds, ...)` → 置挂起锚 → `TSR_Running`；被唤醒 → 清锚 → `TSR_Completed`）+ `UE_DEFINE_EFFECT_STEP_EXECUTOR(FTcsStepWaitDelay, ExecuteStepWaitDelay)`；缺时钟设施降级（Error + 本步按完成）
- [x] 1.18 到期回调：按运行态句柄调 `ResumeRun`（**代际校验**——悬空/已释放静默返回）

### 临时测试装置（`Private/Testing/`，**不入库**）

- [x] 1.19 `Private/Testing/TcsEffectTestRig.h/.cpp`：测试步骤 `FTcsTestStepMark{ FString Label; }` + 执行器（屏显 + 日志）+ 宏自注册（**跨文件自注册实证**）；孤儿步骤 `FTcsTestStepOrphan`（故意无执行器）；测试实体查询 `UTcsTestEntityQuery`（空遍历）
- [x] 1.20 `Tcs.Test.Effect`（正路，**零故意 ensure/Error**）：检查 1 自注册可查 → 起链 → 检查 2 首步挂起（运行态活动 + Mark 未执行）→ 检查 3 延迟判定（+1.0s 战斗时间：Mark 恰好执行 1 次且运行态已释放）→ 检查 4 实体查询注入往返；屏显分节排版 + 固定行号
- [x] 1.21 `Tcs.Test.Effect.Reject`（**opt-in** 拒绝面，屏显先声明）：未知步骤类型断链 / 重复登记拒绝 / 活动运行态拒绝注销 / 悬空句柄唤醒静默 / 单帧步数熔断（上限 3 的 5 步链）

## 2. Verification

- [x] 2.1 UBT Development Editor 编译通过（零警告；UHT 对 TcsEffect 重新生成产物、四 DLL 产出）
- [x] 2.2 依赖面核对：`TcsEffect` 不 include 任何领域模块头（`grep -rn "#include" Source/TcsEffect/ | grep -i "tcs"` 仅命中自身 + TcsCore）
- [ ] 2.3 用户 PIE 人工检查：`Tcs.Test.Effect` 后 Output Log `LogTcsEffect` 可见"起链 → 挂起（到期时刻）→ 唤醒 → 完成"，屏显 Mark 行晚于命令约 0.5s 战斗时间出现；`slomo 0.1` 下等待同比例拉长（剧本检查点 7 的本地预演）
- [x] 2.4 文档回写：plan2 Task 1 勾选 + 实施注记（含落点偏差与顺延项）；plan2 Task 5/6 注记（链资产类落点 + 接口 U 类名与实现类名撞名）；遗留台账补一条（Context 默认目标初始化）；README 检查点状态
