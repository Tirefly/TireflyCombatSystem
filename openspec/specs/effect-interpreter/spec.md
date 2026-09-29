# effect-interpreter Specification

## Purpose
定义链的执行语义：池化运行态与驱动、步内挂起-恢复协议、单帧步数熔断、等待步骤，以及供宿主脚本调用的门面反射面。
## Requirements
### Requirement: 链执行与池化运行态

`UTcsEffectSubsystem` MUST 提供 `ExecuteChain(FGameplayTag ChainId, FTcsEffectContext Context) -> FTcsChainRunHandle`（起链：立即执行至首个挂起点或走完；**2026-09-22 改造：`ChainId` 类型 `FName` → `FGameplayTag`**）：

- 运行态 `FTcsChainRun` **池化**（`TTcsInstancePool`，D0-2 代际句柄防悬空）——控制流状态住数据、**不活在调用栈里**（D4-3 异步语义）；
- `FTcsChainRun` MUST 自持：`ChainId`（**不是链定义指针**——每步入器按 id 重解析；登记表变更不得使运行中链悬空）、`PC`、`Context`（黑板）、宿主门面弱引用与自身句柄（唤醒回入口）、挂起锚；
- 返回句柄**仅在链未走完时有效**：链同步走完即释放运行态（句柄随之失效）；`IsRunActive(Handle)` 是活性查询入口；
- 未登记的链 id MUST 拒绝：Error 日志 + 无效句柄（不 ensure、不崩溃——执行期配置错误，不用 ensure 刷屏）；
- 运行态指针 MUST NOT 跨步骤执行持有：**每次进入步骤执行前按句柄重解析**（引擎事实 2026-09-17：池元素住连续缓冲，新增其他运行态即搬移；步骤执行期间做任何池新增都会让缓存指针失效）。

#### Scenario: 全即时步骤的链同步走完

- **WHEN** 执行一条不含挂起步骤的链
- **THEN** `ExecuteChain` 返回时全部步骤已执行、运行态已释放（`IsRunActive` 为 false）

#### Scenario: 未登记链被拒

- **WHEN** 执行一个未登记的 ChainId（有效 tag 但未登记）
- **THEN** 返回无效句柄 + Error 日志（不崩溃、不 ensure）

#### Scenario: 挂起期间登记新链不影响续走

- **WHEN** 一条链挂起期间又登记了若干其他链定义，随后该链被唤醒
- **THEN** 唤醒重入按 id 解析到自己的定义并正常续走（运行态未持链指针，登记表增长不使其悬空）

### Requirement: 挂起-恢复协议（步内挂起）

步骤执行器 MUST 以 `ETcsStepResult{ TSR_Completed, TSR_Running }` 表达**步内挂起**（D4-17，与 PC 挂起、事件订阅并列的第三种挂起形态）：

- `TSR_Completed` → PC 前进；`TSR_Running` → **PC 停在原步**、运行态保持活动、解释器立即让出控制权（MUST NOT 原地自旋）；
- 挂起中的运行态 MUST NOT 产生每帧成本——门面 MUST NOT 是 Tickable 子系统，唤醒一律由唤醒源驱动（R3 落到期堆一路；事件/子链完成随各自轮次）；
- 唤醒回入口 `ResumeRun(FTcsChainRunHandle Handle)`：MUST 做**句柄代际校验**，悬空/已释放/世界将拆的句柄静默返回（挂起条目与运行态的竞态是正常路径，不是契约违规）；
- 恢复 MUST 从运行态 PC **重入同一执行器**（由步骤依据运行态上的挂起锚自辨"首入 / 被唤醒"）；
- 走完最后一步即释放运行态；释放后旧句柄 MUST 失效（不得复活已结束的链）。

#### Scenario: 挂起步不前进 PC

- **WHEN** 链首步返回 `TSR_Running`
- **THEN** 该步 PC 不变、运行态保持活动、后续步骤不执行、解释器立即返回

#### Scenario: 唤醒后从原步续走

- **WHEN** 唤醒源按句柄调用 `ResumeRun`
- **THEN** 从挂起步重入（该步据挂起锚判定为完成）→ PC 前进 → 后续步骤继续执行

#### Scenario: 悬空句柄唤醒静默

- **WHEN** 用一个已释放（代际失配）的句柄调用 `ResumeRun`
- **THEN** 静默返回（无 ensure、无 Error 日志）

### Requirement: 单帧步数熔断

**单次进入执行**的步数 MUST 受链定义 `MaxStepsPerFrame`（默认 64）限制——超限即视为运行中链失控（自激/循环）：

- 超限 MUST 熔断：ensure 提示 + Error 日志（含链 id 与上限）+ 释放运行态（断链）；
- 熔断判定 MUST 与"链正常走完"区分——正常走完是零诊断噪音的正常路径。

#### Scenario: 超限熔断

- **WHEN** 一条链的单次进入执行步数超过其 `MaxStepsPerFrame`
- **THEN** 停止执行、留 ensure + Error 日志（含链 id 与上限）、运行态释放

#### Scenario: 上限内零噪音

- **WHEN** 链的单次进入执行步数不超过上限
- **THEN** 正常走完或挂起，无任何熔断诊断输出

### Requirement: 等待步骤（WaitDelay）

`TcsEffect` MUST 提供控制流步骤 `FTcsStepWaitDelay{ double Seconds; }`（D4-16 本模块 9 个原语之一；挂起-恢复协议的首个实证载体）：

- **首入**：按**战斗时钟**（`UTcsClockSubsystem`，唯一取时入口 D0-5）入到期堆，到期时刻 = 当前 `Elapsed` + `Seconds`，随后返回 `TSR_Running`；
- **等待期零每帧重入成本**（不轮询、不自 tick——到期由时钟泵回调驱动，见 `clock-expiry-heap`）；
- **到期唤醒**：回调按运行态句柄调 `ResumeRun` → 本步判为完成 → 续走；
- **时间基准 = 战斗时钟（ScaledDt 时轴）**：`slomo` 减速时等待同比例拉长（R3 剧本检查点 7 的成立依据）；`pause` 冻结期间时钟不推进 → 等待不兑现；
- **缺时钟设施**（世界或时钟子系统不可得）时 MUST 降级为"本步按完成处理" + Error 日志（不挂死、不崩溃）。

#### Scenario: 到点后才续走

- **WHEN** 起一条 `[WaitDelay 0.5, 后续步]` 的链
- **THEN** 后续步在到期时刻之后才执行；等待窗口内该链零步骤执行

#### Scenario: 减速时等待同比例拉长

- **WHEN** 以 `slomo 0.1` 运行同一条 `[WaitDelay 0.5]` 链
- **THEN** 等待按战斗时间 0.5s（实时约 5s）兑现——到期基准是战斗时钟 `Elapsed` 而非实时秒

#### Scenario: 无时钟设施时降级

- **WHEN** 运行态所属世界取不到时钟子系统
- **THEN** 本步按完成处理 + Error 日志（不挂起、不崩溃）

### Requirement: 门面反射面（脚本层可达）

`UTcsEffectSubsystem` 的公开门面方法 MUST 对**宿主脚本层**（UnrealSharp / BP 反射）可达——这是 D4-17"语言无关执行器"预留的落地，也是"宿主用 C# 编写技能/Buff 逻辑"的前提（2026-09-24 用户拍板）。

**口径（2026-09-24 实证定稿）**：

- 方法标记 MUST 为 **`UFUNCTION()` 无 specifier**——**MUST NOT** 加 `BlueprintCallable`：UHT 对 `BlueprintCallable | BlueprintEvent` 校验参数蓝图可表达性（`UhtFunction.cs:859`/`:1043`），而本门面的形参含 `FTcsEffectChain` / `FTcsEffectTriggerInstance` 等 **`USTRUCT()` 非 `BlueprintType`** 载体，加 specifier 会被 UHT 拒绝编译；
- **蓝图侧因此不可见，这是有意接受**（R0 §9"蓝图不承诺"）——声明处 MUST 以注释写明，避免被误读为漏写；
- 可见性依赖 C++ 侧 `public` 访问级别（UHT 据此授予 `EFunctionFlags.Public`，UnrealSharp 才生成 `public` 方法）；**实现后 MUST 以生成的 glue 产物验证**（读 `*.generated.cs` 确认修饰符为 `public`），不得仅凭推断；
- 覆盖范围（本轮）：链登记（`RegisterChain`/`UnregisterChain`/`FindChain`）、触发登记（`RegisterTriggerRow`/`UnregisterTriggerRow`/`SetTriggerGateTag`/`IsTriggerGateTagLit`/`GetTriggerRowCount`/`SetTriggerRandomSeed`）、执行（`ExecuteChain`/`ResumeRun`/`IsRunActive`）、能力注入（`SetEntityQuery`/`GetEntityQuery`）；
- **形参含非反射裸 struct 的方法不在本轮**（如 `UnregisterTriggerRowsBySource(const FTcsSourceHandle&)`）——需先反射化该 struct，属台账 SCRIPT-1 的 B 类未覆盖项；
- 反射面**只开"调用"这一扇门**：MUST NOT 被理解为"脚本可提供行为"——行为登记（三张注册表的 `Register`）的反射入口是独立欠账（台账 SCRIPT-2），其形参 `TFunction` 不可反射。**（2026-09-24 补：该欠账的替代路径已由台账 SCRIPT-8 的"步骤执行器插槽"给出——UObject 基类不动 `TFunction` 签名即可让脚本登记执行器，见 `effect-step-dispatch`）**；
- **按句柄的上下文访问器（2026-09-24 新增，台账 SCRIPT-8）**：门面 MUST 提供下列方法，**全部 `UFUNCTION()` 无 specifier**——它们是"传句柄、不传上下文"手法的落地，使宿主脚本能在不反射化 `FTcsEffectContext` 的前提下读写运行态：

  | 方法 | 语义 |
  |---|---|
  | `TArray<FTcsCombatEntityHandle> GetRunTargets(FTcsChainRunHandle)` | 读运行态目标集（悬空句柄返回空数组——不 ensure，时序竞态） |
  | `bool SetRunTargets(FTcsChainRunHandle, const TArray<FTcsCombatEntityHandle>&)` | 写运行态目标集（悬空句柄返回 false + Warning） |
  | `bool TryGetRunVariable(FTcsChainRunHandle, FGameplayTag Key, double& OutValue)` | 读链内变量（miss 返回 false，出参内容未定义） |
  | `bool SetRunVariable(FTcsChainRunHandle, FGameplayTag Key, double Value)` | 写链内变量 |
  | `FTcsCombatEntityHandle GetRunCaster(FTcsChainRunHandle)` | 读施法者句柄（悬空句柄返回无效句柄） |
  | `FTcsCombatEntityHandle GetRunInstigator(FTcsChainRunHandle)` | 读发起者句柄（同上） |

  **MUST NOT 提供 `FTcsEffectContext` 整体读写口**——该 struct 是非反射纯 C++ 类型（`TcsEffectContext.h:24`），且含 `FInstancedStruct EventPayload`（反射）与 `TMap` 变量表；整体暴露即等于要求上下文反射化（台账 SCRIPT-3），而访问器路线的全部收益正是**避开**它。

#### Scenario: 脚本层可调用门面方法

- **WHEN** 检查门面 API 的生成绑定产物（UnrealSharp glue）
- **THEN** 上列方法全部出现且修饰符为 `public`（脚本层可直接调用）

#### Scenario: 门面方法不加蓝图 specifier

- **WHEN** 检查门面方法的 `UFUNCTION` 标记
- **THEN** 全部为无 specifier 形式，且声明处注释写明"形参含非 BlueprintType struct + 蓝图不承诺"的理由

#### Scenario: 运行态句柄可被脚本接住并传回

- **WHEN** 脚本层调 `ExecuteChain` 接住返回的 `FTcsChainRunHandle`，随后把它传回 `IsRunActive`
- **THEN** 编译通过与运行期均成立（句柄是反射可见类型；`FTcsChainRunHandle` MUST 为 `USTRUCT(BlueprintType)` 且字段展平——见 `effect-chain`）

#### Scenario: 脚本层经句柄访问器读写运行态

- **WHEN** 脚本层在挂起链的运行态句柄上调用 `GetRunTargets` / `SetRunVariable` / `GetRunCaster`
- **THEN** 读到本次执行的真实值、写入的值对后续步骤可见（脚本无需持有 `FTcsEffectContext`）

#### Scenario: 访问器对悬空句柄安全

- **WHEN** 以已释放的句柄调用任一访问器
- **THEN** 读口返回空值/无效句柄、写口返回 false + Warning（**不 ensure**——代际竞态是正常路径，口径同 `ResumeRun`）
