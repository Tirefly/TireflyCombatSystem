# Change: 落地效果链机制层——链/步骤数据形状、执行器注册表、解释器与挂起协议（effect-chain / effect-step-dispatch / effect-interpreter / entity-query-contract）

## Why
plan2 Task 1（TcsEffect）动工前的规格先行提案。Task 0 只落了四个**空模块壳**（Build.cs / Module / LogChannel）；TcsEffect 作为 R3 竖切的**执行本体**（"事件 → 触发行 → 效果链"的链解释器与步骤分派），目前**一条规格都没有**——链与步骤什么形状、步骤类型怎么找到执行器、解释器怎么挂起与唤醒、等待步骤怎么落到时钟，全在设计文档（04 §2.1/§2.4、D4-3/D4-14/D4-16/D4-17）与计划 sketch 里，尚未变成可执行规格。

本提案把 TcsEffect 的**机制层**（纯机制、不依赖任何领域模块）落成规格，供 Task 2/3/4 的领域步骤（`SelectTargets` / `Damage` / 标准步骤库）**跨模块自注册**进同一张执行器表——这正是 D4-14 依赖层级反转（`Core←Attribute←Effect←{Damage,Targeting,State}←Skill`）在代码与规格上的落地时刻。

## What Changes
- 新增能力规格 **`effect-chain`**：链与步骤的数据形状（`FTcsEffectChain{ChainId, Steps, MaxStepsPerFrame=64}` / `FTcsEffectStep{StepData}`；步骤**无公共基类**，数组不设 BaseStruct 限定）+ 链定义登记表（`RegisterChain` / `UnregisterChain` / `FindChain`，**键 = `ChainId`**；重复登记拒绝、活动运行态的链拒绝注销、查询不 ensure、登记表地址稳定）。
- 新增 **`effect-step-dispatch`**：执行器签名 `FTcsStepExecute = TFunction<ETcsStepResult(const FInstancedStruct&, FTcsEffectContext&, FTcsChainRun&)>`；注册表**键 = 步骤 struct 反射类型**（`const UScriptStruct*`）；**双入口**（C++ 静态宏 + 动态 `Register`）；`UE_DECLARE/DEFINE_EFFECT_STEP_EXECUTOR` 自注册宏（静态初始化期**零 UObject 触达**——延迟解析）；重复登记拒绝；未知步骤类型 → 断链 + Error 日志。
- 新增 **`effect-interpreter`**：`ExecuteChain(ChainId, Context) -> FTcsChainRunHandle`；**池化运行态**（`TTcsInstancePool`，自持 ChainId/PC/Context/宿主弱引用/自身句柄/挂起锚；不持链指针）；**挂起-恢复协议**（`TSR_Completed` 前进 / `TSR_Running` 停原步 + `ResumeRun` 代际校验 + 挂起期零每帧成本 + 恢复重入同一执行器）；**单帧步数熔断**（`MaxStepsPerFrame` 超限 ensure + 断链）；**`FTcsStepWaitDelay{Seconds}`**（首入入到期堆 → Running；到期回调唤醒重入；时间基准 = 战斗时钟 ScaledDt 时轴 —— `slomo` 同比例拉长）。
- 新增 **`entity-query-contract`**：`ITcsEntityQuery` 注入契约（R3 只声明 `EnumerateEntities`；设计名 `ICombatEntityQuery` 的 `GetLocation`/`IsAlive` 随 RadiusArea 轮落地）+ 门面注入点 `SetEntityQuery` / `GetEntityQuery`（`UPROPERTY` 持引用、未注入返回 nullptr、不 ensure）。
- 修订 **`instance-handle-pool`**（MODIFIED × 2）：`TTcsInstancePool` 增 `Reset()`——宿主子系统 `Deinitialize` 的确定性清空口（清空三数组 + 全部旧句柄代际失配 + **按当前已分配槽位数一次性回落池占用统计**），与 `FTcsExpiryHeap::Reset` 同款纪律。**动因**：链运行态池化后需要"世界拆除即清空"的口；用默认构造整体赋值虽也能清空，但会让 `Tcs.Core.PoolSlots` 跨 PIE 会话残留上一生命周期的槽位数（读数失真）。
- **不做的**（非目标，保持 R3 竖切边界）：不做触发行与条件求值器（R3 手动触发）；不做 `WaitEvent` / `Branch` / `Parallel` / `Repeat` / `RunSubChain` / `SetVar` / `OnError` / `ModifyAttribute` 执行器（M3/M5 轮，各留原语位置）；不做打断与链重定向栈（`IChainRedirectResolver`，M5）；不做链资产类（`UTcsEffectChainDef`）与编辑器 picker 限定；不做性能/内存布局优化（Mass 后置）。

## 顺延与落点裁决（提案显式交代，避免静默漏做）

1. **计划 sketch 的 `LoadChainDefs()` 拆为三入口**：`RegisterChain`（登记）+ `UnregisterChain` + `FindChain`（执行期解析）。**链资产发现归 Task 5 的 `UTcsDefLibrary`**（计划 Task 5 已定"链资产发现 → DefLibrary、执行 → Effect"）；本提案只保证登记 API 与资产载入路径兼容。链资产类 `UTcsEffectChainDef` 计划文档未点名落点（**有 Task 归属，不入台账**）→ 落点与"接口 U 类名 vs Task 5 实现类名撞名"两项写入 plan2 Task 5/6 注记。
2. **Context 默认目标初始化（D4-4 v2）= 事件载荷 → `Targets`**：R3 无事件触发源（手动触发），无法实证 → **顺延到触发行轮（M3/M5）**（无 Task 归属，入遗留台账）；R3 由调用方预填 `Context.Targets`（Task 5 组件按实体填）。
3. **落点偏差**：`WaitDelay` 执行器住 `Private/Chain/TcsStepWaitDelay.cpp`（自成一文件——它同时是"领域/本地步骤经宏自注册"的首个实证样本），**不是**计划所写的 `TcsEffectSubsystem.cpp`；子系统 `.cpp` 只留解释器与登记表。
4. **文件落点**：领域代码住 `Public/Chain/` 与 `Private/Chain/`（`cpp-module-structure` 规格要求"对外头文件放 `Public/<领域子目录>/`，PascalCase"），宿主契约住 `Public/Host/`，门面与日志通道在 `Public/` 根（与 `TcsAttributeSubsystem.h` 同款）。

## Impact
- Affected specs: `effect-chain`（新建能力）、`effect-step-dispatch`（新建能力）、`effect-interpreter`（新建能力）、`entity-query-contract`（新建能力）、`instance-handle-pool`（MODIFIED × 2：池 `Reset` 清空口 + 占用统计随重置回落）。
- Affected code：
  - `Source/TcsEffect/`（新模块首次落代码）：
    - 新增 `Public/Chain/TcsEffectStep.h`（`ETcsStepResult` + `FTcsEffectStep`）、`Public/Chain/TcsEffectChain.h`、`Public/Chain/TcsEffectContext.h`、`Public/Chain/TcsChainRun.h`（`FTcsChainRunTag`/`FTcsChainRunHandle`/`FTcsChainRun`）、`Public/Chain/TcsEffectStepExecutor.h`（签名 + 注册表 + 待解析表 + 注册器 + 两宏）、`Public/Chain/TcsStepWaitDelay.h`、`Public/Host/TcsEntityQuery.h`、`Public/TcsEffectSubsystem.h`；
    - 新增 `Private/Chain/TcsEffectStepExecutor.cpp`（注册表实现：待解析表延迟解析）、`Private/Chain/TcsStepWaitDelay.cpp`（执行器 + 宏自注册）、`Private/TcsEffectSubsystem.cpp`（登记表 + 解释器 + 运行态池 + 注入点）；
    - 临时测试装置 `Private/Testing/TcsEffectTestRig.h/.cpp`（**不入库**，plan1 Task 6 同款约定）：`Tcs.Test.Effect`（正路：自注册可查 → 起链即挂起 → +1.0s 延迟判定到期唤醒续走 → 实体查询注入往返）+ `Tcs.Test.Effect.Reject`（opt-in 拒绝面：未知步骤类型断链 / 重复登记拒绝 / 活动运行态拒绝注销 / 悬空句柄静默 / 单帧步数熔断——**故意触发 ensure 与 Error 的检查一律独立成 opt-in 命令**）。
  - `Source/TcsCore/Public/Pool/TcsInstancePool.h`（修改：新增 `Reset()`）。
- 决策依据：04 §2.1（15 原语与步骤容器）/§2.4（异步语义四项）/§3（入口服务与解释器）；D4-3、D4-4 v2、D4-14、D4-15 v2、D4-16、D4-17；D0-2（池化代际句柄）、D0-5（时钟泵）、D0-6 v2（日志通道）；R0 §8（分离宪法：链 = 数据）、§9（最小编译集与模块表）；引擎事实 2026-09-17（`TMap`/池元素地址不稳定性）。
- 验证：UBT Development Editor 编译（零警告）+ 临时装置定向检查 + 用户 PIE 人工检查（`Tcs.Test.Effect` 的 LogTcsEffect 时间戳：挂起 → 0.5s 后完成）。

## 提案内钉名（plan / 设计文档未钉，供审阅否决）

| 项 | 钉法 | 依据 |
|---|---|---|
| 注册表键 | `const UScriptStruct*`（步骤 struct 反射类型），非 FName | 执行期唯一可得的类型身份是 `FInstancedStruct::GetScriptStruct()`；指针身份免一次哈希，且绕开 UHT 对 USTRUCT 反射名的 `F` 前缀差异（`FTcsStepWaitDelay` 的反射名是 `TcsStepWaitDelay`——写名字必踩）。计划 Step 2 写"注册键 = 步骤 struct 类名 FName"，此处按执行期实际形状收窄 |
| 静态自注册时机 | 注册器只入**待解析表**；首次 `Find` 才调 `StepType::StaticStruct()` 解析 | 静态初始化期触 UObject 是雷区——引擎自身 `FNativeGameplayTag` 用 `GetIfAllocated()` 规避（`NativeGameplayTags.cpp`）；同时免掉 DLL 静态初始化顺序问题 |
| 运行态池与重解析 | `TTcsInstancePool<FTcsChainRun, FTcsChainRunTag>`（计划指定）+ **解释器每步入器前按句柄重解析指针** | 池元素住 `TArray` 连续缓冲、扩容即搬移（引擎事实 2026-09-17）——步骤执行内可能新增运行态（嵌套链），缓存指针会悬空 |
| 运行态持 `ChainId` 而非链指针 | `FTcsChainRun::ChainId` + 每次进入执行按 id 解析 | 同款理由：登记表增长/搬移不使运行态悬空；注销有活动运行态的链被拒绝（双保险） |
| 登记 API 命名 | `RegisterChain` / `UnregisterChain` / `FindChain`（动词纪律：`Resolve` 只用于"从标识符取对象"） | 计划 sketch 的 `LoadChainDefs()` 语义（"消费已登记定义"）由登记三入口承担；资产发现归 DefLibrary |
| `WaitDelay` 落点 | `Private/Chain/TcsStepWaitDelay.cpp`（执行器 + 宏自注册），非子系统 `.cpp` | 该文件是"步骤类型 + 执行器 + 一行宏"的最小完整样本（Task 2/3/4 领域步骤照抄此形状）；子系统 `.cpp` 保持解释器单一职责 |
| 实体查询成员面 | R3 只声明 `EnumerateEntities(TFunctionRef<void(AActor*)>)`；`GetLocation`/`IsAlive` 后置 | R3 默认策略 Self/EventTarget 不需要遍历（零消费者不预建）；RadiusArea 轮按需补，接口名与设计名 `ICombatEntityQuery` / 实现名 `ITcsEntityQuery` 的对应关系照 10 文档保留 |
| `ETcsStepResult` 值 | `TSR_Completed = 0` / `TSR_Running = 1`（显式初值，无 `Custom` 位） | 与计划 sketch 一致；协议枚举非策略枚举，不适用 `Custom = 1` 逃逸位规约 |
| 池清空口 | `TTcsInstancePool::Reset()`（TcsCore；不是"默认构造整体赋值"） | 默认构造赋值同样能清空数组，但会漏掉 `FTcsCorePoolStats` 的占用回落 → `Tcs.Core.PoolSlots` 跨 PIE 会话残留上一生命周期的槽位数（读数失真）；`FTcsExpiryHeap::Reset` 已有同款"清空即回落统计"的先例 |
| 运行态池重解析 | 解释器每步入器前 + 推进 PC 前各重解析一次 | 步骤执行器可能在执行中新增运行态（后轮 RunSubChain/Parallel 场景）——池扩容搬移元素，缓存指针会悬空（引擎事实 2026-09-17） |

## 检查点
落点验收 = UBT 编译通过（Development Editor，**零警告**）+ 临时装置定向检查（自注册可查、挂起不前进 PC、唤醒续走、悬空句柄静默、熔断、未知类型断链、登记拒绝面）+ 用户 PIE 人工检查（`Tcs.Test.Effect`：起链 → 0.5s 后完成，Output Log `LogTcsEffect` 时间戳可对）。`WaitEvent`/并行/子链等其余原语、触发行、打断与重定向栈不在本提案。
