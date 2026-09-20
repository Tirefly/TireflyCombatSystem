## Context

TcsEffect 是 R3 竖切的执行本体：机制层只认识"链 = 数据、步骤 = 类型 + 执行器"，对领域零感知（D4-14）。Task 2/3/4 的领域步骤（`SelectTargets`（TcsTargeting）、`Damage`/`Heal`/`ModifyFlow`（TcsDamage）、标准流程步骤库）都要跨模块注册进本模块的执行器表，因此本任务的关键不是"能跑一条链"，而是**跨模块登记契约**与**挂起-恢复协议**两件事的形状一次钉对——它们一旦被领域模块依赖，改动成本就外溢到四个模块。

约束：单游戏线程（D0-4）、时间只经战斗时钟（D0-5）、日志走本模块通道（D0-6 v2）、依赖边 `TcsEffect{CsCore, TcsAttribute}` 不得含任何领域模块、禁 TDD（验证 = 编译 + 定向人工检查）。

## Goals / Non-Goals

- Goals：链/步骤数据形状；执行器注册表 + 双入口 + 一行自注册宏；解释器与运行态池化；步内挂起（`TSR_Running`）与到期唤醒；单帧步数熔断；宿主实体能力注入点。
- Non-Goals：触发行与条件求值器；`WaitEvent`/`Branch`/`Parallel`/`Repeat`/`RunSubChain`/`SetVar`/`OnError`/`ModifyAttribute` 执行器；打断与链重定向栈；链资产类与编辑器 picker 限定；网络姿态实现；性能优化。

## Decisions

- **注册键用 `const UScriptStruct*` 而不是 FName**。执行期只拿得到 `FInstancedStruct::GetScriptStruct()`；用指针省一次哈希，并绕开 UHT 的 USTRUCT 反射名去 `F` 前缀问题（`FTcsStepWaitDelay` 的反射名是 `TcsStepWaitDelay`）。*被否*：FName 键（名字往返 + 前缀陷阱）；`UClass` 键（步骤是 struct 不是 class）。
- **静态自注册 = 延迟解析**。宏展开的注册器在模块静态初始化期只把 `{步骤类型 getter, 执行器}` 挂入待解析表，注册表**首次 `Find` 时**才调 `StaticStruct()` 建键。依据：引擎 `FNativeGameplayTag` 构造函数对同一问题使用 `UGameplayTagsManager::GetIfAllocated()`（静态初始化期不假定 UObject 设施已就绪）。*被否*：宏里直接 `StepType::StaticStruct()->GetFName()`（静态初始化期触反射，风险高且与 DLL 加载顺序耦合）；模块 `StartupModule` 手工登记（D4-14 明确要消掉这份样板）。
- **运行态池化用 `TTcsInstancePool`，但解释器不缓存运行态指针**：每次进入步骤执行前按句柄重解析。依据：池元素住 `TArray` 连续缓冲、扩容即搬移（引擎事实 2026-09-17，属性模块同类缺陷已实证一次）。*被否*：为运行态另造地址稳定的池（新机制、无第二个消费者；且"重解析"恰好也是"按 id 重解析链定义"的同款纪律）。
- **运行态持 `ChainId`，不持链定义指针**：每次进入执行按 id 解析。*被否*：运行态持 `const FTcsEffectChain*`（登记表搬移或注销即悬空；快照链定义则每步拷贝开销）。
- **挂起锚住在运行态上**（`PendingExpiry` + `bHasPendingExpiry`），步骤据此自辨"首入 / 被唤醒"；解释器不代步骤记状态。依据：D4-17"状态存脚本侧"的同款分层——控制流状态住运行态、步骤私有状态住步骤实现。*被否*：解释器为 `WaitDelay` 特判（机制层硬编码语义，违背 D4-14）。
- **挂起期间零每帧成本**：门面不是 Tickable，唤醒全部由唤醒源驱动（R3 = 到期堆回调）。这既是性能要求，也是"挂起"与"轮询"的语义分界。
- **熔断用 ensure + 断链**：`MaxStepsPerFrame` 是失控护栏而非限流器，正常链路（R3 三步）远低于上限；走完链是零诊断噪音的正常路径。
- **`Context` 只持 R3 需要的五项**（`Caster`/`Instigator`/`EventPayload`/`Targets`/`Variables`）：04 §2.1 的 `CapturedAttrs` 与注入引用按计划归 Damage 流 Context（属性捕获）与门面注入点（实体查询），不重复进效果链黑板。

## Risks / Trade-offs

- **步骤执行内新增运行态会让缓存指针失效** → 解释器每步入器前重解析；写进 `TcsChainRun.h` 注释作为调用纪律。
- **同类型重复登记（两个领域模块登记同一 struct）** → 拒绝 + ensure（保留首个），避免"谁后加载谁生效"的隐蔽覆盖。
- **`WaitDelay` 的到期条目在运行态释放后仍会触发** → 唤醒按句柄做代际校验，悬空即静默返回（不复活已结束的链）；条目本身由堆在回调后回收，无泄漏。
- **世界拆除时的顺序依赖**（时钟子系统先 `Deinitialize` 清堆 / 效果子系统先清池） → 两个方向都安全：池清空后句柄代际失配 → 静默；堆清空后回调不再触发。
- **编辑器手工配链可挂任意 struct**（D4-16 无公共基类，picker 无法限定） → 运行期由注册表拒绝并留 Error 日志（不静默），作者侧校验（链资产 `IsDataValid`）随资产类在后续轮补。

## Open Questions

- 链资产类 `UTcsEffectChainDef` 的落点（`ChainId` 与资产 `DefId` 的对应、编辑器步骤 picker 的限定手段）——留给 plan2 Task 5/6（已记遗留台账）。
- 打断（`InterruptPriority` → 取消）与 `ResumeSource` 多唤醒源的取消语义——M5 轮；本任务的挂起锚字段为其预留，不提前实现。
