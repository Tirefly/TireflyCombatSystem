# R3 实施计划二：TcsEffect + TcsTargeting + TcsDamage + TcsIntegration（伤害链竖切）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在计划一之上搭伤害链竖切——手动触发 → WaitDelay 挂起 → 单体选目标 → Damage 流程（官方默认模板裁剪版）→ M2 扣血 → 屏显/Output Log 验收，跑通 R3 剧本检查点 1/5/6/7。

**Architecture:** 四个新 UE 模块，依赖边按 R0 §9（Core←Attribute←Effect←{Damage,Targeting}←Integration；**不进竖切：TcsSkill/TcsCue/TcsState/TcsEditor**）。TcsEffect = 解释器+执行器注册表（注册制分派，D4-14）；TcsDamage = 流程解释器+步骤库+默认模板（D7-5）；跨模块步骤执行器用同一自注册宏。

**Tech Stack:** UE 5.8 C++；验证 = 编译 + 剧本人工检查单；UBT 调用按 unreal-cpp-compile 技能现场探测。

## Global Constraints

- 同计划一全局约束（禁 TDD / 提交门控 / unreal-cpp-style 含模块目录布局（根壳平铺 + Public/Private 分层）与 Tcs 前缀命名 / 依赖铁律 / 确定性 / epsilon 1e-5）。
- **日志**：归 UE 原生分类制（D0-6 v2）——每模块 `DECLARE_LOG_CATEGORY_EXTERN(LogTcs<模块名>, Log, All)` 于**独立通道文件**（`Public/<模块名>LogChannel.h` 声明 + `Private/<模块名>LogChannel.cpp` 定义——使用日志 include LogChannel 头，不 include Module.h）；**TcsCore 零日志设施**；屏显 = 测试装置直调 UE API（插件模块零屏显调用）。
- **模块编译依赖 = 最小编译集**（层级链是方向许可）：TcsEffect{Core,Attribute}、TcsTargeting{Core,Effect}、TcsDamage{Core,Attribute,Effect}（**不内嵌 selector——目标消费 Context.Targets，D4-4 v2 改口**）、TcsIntegration{Core,Attribute,Effect,Targeting,Damage}。
- **策略载体（D3-7 v3）**：策略 = USTRUCT 反射基类 + C++ 虚函数分派（StateTree 同构），Def/步骤以 `TInstancedStruct<Base>` UPROPERTY 成员持有（自动 BaseStruct 限定）；BP 策略扩展通道放弃（R0 §9"蓝图不承诺"承责）；编辑器校验由 Def 资产 IsDataValid 承担。
- **数值配置载体（PV 系列 2026-09-11）**：一切策划书写的数值 = `FTcsParamValue{TInstancedStruct<FTcsParamValueSource> Source}`（TcsCore，默认 Literal；基类虚函数 **Evaluate**——D3-7 v3 同形）；链步骤数值字段/FlowModify Operand/公式参数全量换型；M2 账本运行侧不动（D2-13 零膨胀）。
- 本计划只实现 R3 消费的步骤集：链步骤 = WaitDelay / SelectTargets / Damage；流程默认模板 = CollectStart→BaseDamage→Execute→Completed（Hit/Crit/Element/PreExecute 步骤 struct 一并定义但 R3 模板不组装——宿主后续按需启用）。
- 设计规格来源：`04-module-effects.md`、`09-module-damage.md`（v4）、`07-module-presentation.md`（仅接口认知）、剧本 v2、`2026-09-10-param-value-source-decision-points.md`（PV 系列）；冲突以设计文档+用户拍板为准。设计文档类型名为概念名，实现命名按下述 Tcs 前缀为准。

## File Structure（本计划新建，收窄口径：根仅 Build.cs + Module.h/.cpp，领域代码 Public/Private + 领域子目录 PascalCase，Public 为对外 include 根）

```
  TireflyCombatSystem.uplugin                 （修改：Modules 追加 TcsEffect/TcsTargeting/TcsDamage/TcsIntegration——顺序即依赖序）
  Source/
    TcsEffect/
      TcsEffect.Build.cs                       （根）
      TcsEffectModule.h / .cpp                 （根：模块类）
      Public/
        TcsEffectLogChannel.h                  （日志通道 D0-6 v2：DECLARE_LOG_CATEGORY_EXTERN(LogTcsEffect, Log, All)）
        TcsEffectStep.h                       （FInstancedStruct 容器 + ETcsStepResult）
        TcsEffectChain.h                      （Id/步骤数组/MaxStepsPerFrame 熔断）
        TcsEffectContext.h                    （黑板：Caster/Instigator/EventPayload/Targets/Variables——属性捕获归 Damage 流 Context，见 Flow/）
        TcsChainRun.h                         （池化运行态：PC/状态）
        TcsEffectStepExecutor.h               （执行器签名 + 注册表 + UE_DEFINE_EFFECT_STEP_EXECUTOR 宏）
        TcsEntityQuery.h                      （注入接口：机制层定义，宿主实现）
        TcsEffectSubsystem.h                  （解释器门面）
        TcsStepWaitDelay.h                    （WaitDelay 步骤 struct——Task 1 落地）
      Private/
        TcsEffectLogChannel.cpp                （DEFINE_LOG_CATEGORY(LogTcsEffect)）
        TcsEffectSubsystem.cpp                （解释器 + WaitDelay 执行器 + **Context 默认目标初始化=事件目标**）
    TcsTargeting/
      TcsTargeting.Build.cs                    （根）
      TcsTargetingModule.h / .cpp              （根：模块类）
      Public/
        TcsTargetingLogChannel.h               （DECLARE_LOG_CATEGORY_EXTERN(LogTcsTargeting, Log, All)）
        TcsTargetSelectorStrategy.h           （策略契约：Resolve 纯虚 USTRUCT——D4-4 v2 策略化；载体 D3-7 v3）
        TcsSelSelf.h                          （默认策略 1：Context.Caster）
        TcsSelEventTarget.h                   （默认策略 2：事件载荷目标）
        TcsTargetFilterStrategy.h             （过滤契约：Pass 纯虚——存活/敌对语义宿主实现）
        TcsStepSelectTargets.h                （链步骤：TInstancedStruct 策略 + Filter AND → 写 Context.Targets）
      Private/
        TcsTargetingLogChannel.cpp             （DEFINE_LOG_CATEGORY(LogTcsTargeting)）
        TcsStepSelectTargets.cpp              （执行器）
    TcsDamage/
      TcsDamage.Build.cs                       （根）
      TcsDamageModule.h / .cpp                 （根：模块类）
      Public/
        TcsDamageLogChannel.h                  （DECLARE_LOG_CATEGORY_EXTERN(LogTcsDamage, Log, All)）
        Flow/TcsDamageFlowContext.h           （三层值空间/分类 Tag/FlowSource/CapturedAttrs）
        Flow/TcsFlowAttributes.h              （流程属性黑板：键+修正链+封闭运算）
        Flow/TcsFlowTemplate.h                （有序步骤数组）
        Flow/TcsFlowStepExecutor.h            （流程步骤注册表 + UE_DEFINE_FLOW_STEP_EXECUTOR 宏）
        Flow/Steps/（标准步骤库十 struct + FlowModify/FlowDelegate，各 .h）
        Flow/TcsDamageFlowDelegate.h          （纯 C++ 接口：宿主实现公式）
        Flow/TcsDamageRecord.h
        Chain/TcsStepDamage.h                 （链步骤 Damage/Heal/ModifyFlow 三 struct 定义；**R3 只实现 Damage 执行器**，Heal/ModifyFlow 执行器后续轮）
        TcsDamageSubsystem.h                  （流程解释器门面）
      Private/
        TcsDamageLogChannel.cpp                （DEFINE_LOG_CATEGORY(LogTcsDamage)）
        Flow/Steps/（标准步骤执行器 .cpp）
        Chain/TcsStepDamage.cpp               （本计划实现 Damage 执行器）
        TcsDamageSubsystem.cpp                （流程解释器 + 默认模板注册）
    TcsIntegration/
      TcsIntegration.Build.cs                  （根）
      TcsIntegrationModule.h / .cpp            （根：模块类）
      Public/
        TcsIntegrationLogChannel.h             （DECLARE_LOG_CATEGORY_EXTERN(LogTcsIntegration, Log, All)）
        Entity/TcsCombatEntityComponent.h     （身份锚/查询门面/手动触发 API）
        Entity/TcsEntityQuery.h               （ITcsEntityQuery 默认实现：PIE 枚举）
        TcsDefinitionSubsystem.h              （最小定义加载：链资产/流程模板注册）
      Private/
        TcsIntegrationLogChannel.cpp           （DEFINE_LOG_CATEGORY(LogTcsIntegration)）
        Entity/TcsCombatEntityComponent.cpp
        Entity/TcsEntityQuery.cpp
        TcsDefinitionSubsystem.cpp
  Content/（PIE 测试地图 + 属性 DataTable + 测试链资产——执行期编辑器内建）
```

---

### Task 0: 四模块骨架——**已完成（2026-09-18）**

**Files:** 四个 Build.cs + 四组 `Tcs<名>Module.h/.cpp`（模块壳，不含日志）+ 四组日志通道 `Public/Tcs<名>LogChannel.h`（DECLARE_LOG_CATEGORY_EXTERN(LogTcsEffect/LogTcsTargeting/LogTcsDamage/LogTcsIntegration, Log, All)）+ `Private/Tcs<名>LogChannel.cpp`（DEFINE）+ .uplugin Modules 追加四模块（顺序 TcsEffect/TcsTargeting/TcsDamage/TcsIntegration）

- [x] **Step 1:** 建 Build.cs（依赖边照 Global Constraints 最小编译集；TcsIntegration 加 Engine 子模块按需）
- [x] **Step 2:** .uplugin Modules 列表追加四模块
- [x] **Step 3:** 编译验证（空模块全绿）

> **2026-09-18 落地实施注记**：产物 **20 文件**（4 模块 × `Build.cs` + `Module.h/.cpp` + `Public/<名>LogChannel.h` + `Private/<名>LogChannel.cpp`）+ `.uplugin` 追加四模块（顺序 = 依赖序，排在 TcsAttribute 之后）。依赖边实际：`TcsEffect{Core,CoreUObject,Engine,GameplayTags,TcsCore,TcsAttribute}`、`TcsTargeting{…,TcsEffect}`、`TcsDamage{…,TcsCore,TcsAttribute,TcsEffect}`、`TcsIntegration{…, 其下全部战斗模块}`——引擎侧统一 Core/CoreUObject/Engine/GameplayTags（与既有 `TcsCore`/`TcsAttribute` 同款；`TcsCore` 另有 `DeveloperSettings`）。日志通道按 **D0-6 v2** 独立成文件（使用日志只 include LogChannel 头，不 include Module.h），`LogTcsEffect`/`LogTcsTargeting`/`LogTcsDamage`/`LogTcsIntegration` 四条分类就位。编译：Development Editor 通过（**零警告**），四个 DLL 产出。下一站 **Task 1（TcsEffect：步骤/链/注册表/挂起协议）**。

---

### Task 1: TcsEffect——步骤/链/注册表/挂起协议——**已完成（2026-09-18 实施 / 2026-09-20 验收）**

**Files:** `TcsEffectStep.h / TcsEffectChain.h / TcsEffectContext.h / TcsChainRun.h / TcsEffectStepExecutor.h / TcsEntityQuery.h / TcsStepWaitDelay.h / TcsEffectSubsystem.h(.cpp)`

**Interfaces:**
- Consumes: TcsCore 池/总线/时钟。
- Produces:
```cpp
USTRUCT() struct FTcsEffectStep { FInstancedStruct StepData; };   // 步骤类型 = FInstancedStruct 派生 struct（**用途**：单步骤内联挂载/传递的容器形状——链数组直接存 FInstancedStruct，本 struct 供需要"把步骤当字段"的场合（如 SubChain 引用）复用）
USTRUCT() struct FTcsEffectChain { FName ChainId; TArray<FInstancedStruct> Steps; int32 MaxStepsPerFrame = 64; };
enum class ETcsStepResult : uint8 { TSR_Completed, TSR_Running }; // 步内挂起协议（D4-17）
struct FTcsEffectContext { AActor* Caster; AActor* Instigator; FInstancedStruct EventPayload; TArray<TWeakObjectPtr<AActor>> Targets; TMap<FName, double> Variables; };
// **Context 默认目标初始化（D4-4 v2）**：Targets 默认 = EventPayload 事件目标——单步链（灼烧 tick [Damage]）零 SelectTargets 直接消费
using FTcsStepExecute = TFunction<ETcsStepResult(const FInstancedStruct&, FTcsEffectContext&, FTcsChainRun&)>;
// 注册表：FTcsEffectStepExecutorRegistry::Register(FName StepTypeName, FTcsStepExecute)；
// 宏 UE_DEFINE_EFFECT_STEP_EXECUTOR(StepType, ExecutorFn) —— 静态注册器对象，模块加载期自登记（仿 GAMEPLAY_TAG 模式）
// UTcsEffectSubsystem：LoadChainDefs()/ExecuteChain(FName ChainId, FTcsEffectContext) -> FTcsChainRunHandle（池化 FTcsChainRun）
// 挂起-恢复：FTcsChainRun{int32 PC; FTcsEffectContext Context;}——WaitDelay 返回 Running 时 PC 停在原步，到期唤醒后重入
```
- 消费者：Task 3/4/5 的步骤执行器、Integration 触发 API。

- [x] **Step 1: 数据结构四件**（如上；FTcsChainRun 池化走 TcsCore TTcsInstancePool）
- [x] **Step 2: 注册表与宏**（`UE_DECLARE_EFFECT_STEP_EXECUTOR` 配对；注册键 = 步骤 struct 类名 FName）
- [x] **Step 3: 解释器**（`RunFrom(FTcsChainRun&, 从 PC)`：逐步查注册表 → 执行 → Completed 前进 / Running 返回；MaxStepsPerFrame 熔断 ensure）
- [x] **Step 4: WaitDelay 步骤**：`FTcsStepWaitDelay{double Seconds}`；执行器返回 Running 并 Push 到期堆（OwnerId=FTcsChainRunHandle，回调=时钟泵里唤醒重入）
- [x] **Step 5: 编译验证**
- [x] **Step 6: 人工检查**：PIE 起一条 `[WaitDelay 0.5]` 测试链 → 0.5s 后完成（Output Log `LogTcsEffect` 可见）；挂起期间无每帧重入成本

> **2026-09-18 落地实施注记**：规格先行——OpenSpec 提案 **`add-tcseffect-chain-interpreter`**（4 新能力 `effect-chain` / `effect-step-dispatch` / `effect-interpreter` / `entity-query-contract` + `instance-handle-pool` MODIFIED × 2），`openspec validate --strict` 通过。产物 **11 文件**：`Public/Chain/` 六件（`TcsEffectStep.h` / `TcsEffectChain.h` / `TcsEffectContext.h` / `TcsChainRun.h` / `TcsEffectStepExecutor.h` / `TcsStepWaitDelay.h`）+ `Public/Host/TcsEntityQuery.h` + `Public/TcsEffectSubsystem.h` + `Private/Chain/TcsEffectStepExecutor.cpp` / `Private/Chain/TcsStepWaitDelay.cpp` + `Private/TcsEffectSubsystem.cpp`；TcsCore 一处修改（`TTcsInstancePool::Reset()`）；另有临时装置 `Private/Testing/TcsEffectTestRig.h/.cpp`（**不入库**）。
> - **落地口径（相对计划 sketch 的收窄，均已在提案"钉名"表声明）**：①**注册键 = 步骤 struct 反射类型（`const UScriptStruct*`）**，非"类名 FName"——执行期唯一可得的身份就是 `FInstancedStruct::GetScriptStruct()`，指针身份免名字往返且绕开 UHT 对 USTRUCT 反射名去 `F` 前缀的差异；②**静态自注册延迟解析**（静态初始化期只入待解析表，首次 `Find` 才调 `StaticStruct()`）——依据引擎 `FNativeGameplayTag::GetIfAllocated()` 同款纪律；③**运行态持 ChainId 不持链指针**（每步入器按 id 重解析；注销有活动运行态的链被拒）；④**解释器每步入器前与推进 PC 前各重解析运行态指针**（池元素地址不稳定——引擎事实 2026-09-17）；⑤**落点偏差**：WaitDelay 执行器住 `Private/Chain/TcsStepWaitDelay.cpp` 自成一文件（计划写"TcsEffectSubsystem.cpp"）——它是"步骤类型 + 执行器 + 一行宏"的最小完整样本，子系统 `.cpp` 保持解释器单一职责；⑥领域代码住 `Public/Chain/`、宿主契约住 `Public/Host/`（`cpp-module-structure` 规格要求领域子目录），门面与日志通道在 `Public/` 根。
> - **验证**：UBT Development Editor 编译通过（**零警告**；UHT 首次为 TcsEffect 生成产物，四 DLL 产出）；依赖面核对——`Source/TcsEffect/` 只 include 自身与 TcsCore（零领域模块）。装置两条命令：**`Tcs.Test.Effect`**（正路 4 检查：自注册可查 / 起链即挂起 / 延迟判定到期唤醒续走 / 实体查询注入往返——零故意 ensure）+ **`Tcs.Test.Effect.Reject`**（opt-in 拒绝面 5 检查：未知步骤类型断链 / 重复登记拒绝 / 活动运行态拒绝注销 / 悬空句柄静默 / 单帧步数熔断）。
> - **顺延（显式交代，不静默漏做）**：①**Context 默认目标初始化**（D4-4 v2"事件载荷 → `Targets`"）——R3 无事件触发源（手动触发）无法实证，顺延到触发行轮（M3/M5），**已入遗留台账**；R3 由调用方预填 `Context.Targets`。②**链资产类**（`UTcsEffectChainDef`，计划未点名落点）与 ③**实体查询的 `GetLocation`/`IsAlive`**（随 RadiusArea 轮）——见 Task 5/6 与 Task 2 注记。
> - **人工检查（Step 6）已完成——2026-09-20 用户 PIE 两条命令**：**`Tcs.Test.Effect.Reject` → `通过 5 / 失败 0`**（3 条预期 ensure + 2 条预期 Error 一条不多一条不少，五处拒绝面各自命中设计中的代码行：未知步骤类型断链 / 重复登记 @ `RegisterChain:44` / 活动运行态拒绝注销 @ `UnregisterChain:69` / 悬空句柄唤醒静默（无 ensure）/ 单帧步数熔断 @ `RunFrom:197`）；**`Tcs.Test.Effect` → 通过**（起链即挂起 + 延迟判定块给出 Mark 恰好 1 次、运行态已释放，零 Error，`LogTcsEffect` 可见"起链 → 挂起（给出 现在/到期）→ 唤醒 → 完成"）。**装置修复一项**：拒绝面检查 3 刻意留下的挂起运行态会在 ~5s 后自行完成——首轮日志暴露"若该链带 Mark 步，同一会话第二次跑拒绝面时（ensure 不再上报、命令秒回）那次补做会跨命令污染主命令的 Mark 计数"，已改为**只含等待步**。**副产品观测（量化了 opt-in 纪律的理由）**：三条故意 ensure 的栈回溯 + 错误上报实测卡顿约 15s（`StackWalkAndDump` 1.7s + `SendNewReport` 1.8 / 5.9 / 3.5s）。
> - **提案已归档**：`openspec/changes/archive/2026-09-20-add-tcseffect-chain-interpreter`——4 新能力（`effect-chain` / `effect-step-dispatch` / `effect-interpreter` / `entity-query-contract`）+ `instance-handle-pool` 2 条修改并入规格库，规格库 **15 条** `validate --strict` 全绿。
> - **下一站**：plan2 **Task 2（TcsTargeting）**——跨模块自注册的第一个实证点（见该节交接注记）。

---

### Task 2: TcsTargeting——策略契约与默认实现——**已完成（2026-09-20）**

**Files:** `TcsTargetSelectorStrategy.h / TcsSelSelf.h / TcsSelEventTarget.h / TcsTargetFilterStrategy.h / TcsStepSelectTargets.h(.cpp)`

**Interfaces:**
- Consumes: ITcsEntityQuery（TcsEffect 定义，本任务只消费接口）。
- Produces:
```cpp
USTRUCT() struct FTcsTargetSelectorStrategy {   // 策略契约（D4-4 v2 策略化；载体 D3-7 v3——StateTree 同构）
	GENERATED_BODY()
	// Resolve(Context, 注入查询, OutTargets) —— PURE_VIRTUAL 纯虚（struct 载体无 Blueprintable——BP 策略扩展通道放弃，R0"蓝图不承诺"承责）
};
USTRUCT() struct FTcsSelSelf : public FTcsTargetSelectorStrategy { ... };        // 默认实现 1：Context.Caster
USTRUCT() struct FTcsSelEventTarget : public FTcsTargetSelectorStrategy { ... }; // 默认实现 2：事件载荷目标
USTRUCT() struct FTcsTargetFilterStrategy {     // 过滤契约——存活/敌对语义宿主实现
	GENERATED_BODY()
	// Pass(Candidate, Context) —— PURE_VIRTUAL 纯虚（R3 竖切由测试装置实现两个 Filter）
};
USTRUCT() struct FTcsStepSelectTargets {   // 链步骤：策略 Resolve → Filter AND → 写 Context.Targets
	UPROPERTY(EditAnywhere, Category = "SelectTargets") TInstancedStruct<FTcsTargetSelectorStrategy> Selector;
	UPROPERTY(EditAnywhere, Category = "SelectTargets") TArray<TInstancedStruct<FTcsTargetFilterStrategy>> Filters;   // 全过才算通过
};  // TInstancedStruct 自动生成 BaseStruct 限定（编辑器 picker 开箱，StructUtilsEditor）；RadiusArea/FTargetingShape 后置（竖切无消费者，用户拍板）
```
- Context 默认目标初始化 = 事件目标（Effect 侧，单步链零 SelectTargets 直接消费）。
- 消费者：R3 测试链；Damage 步骤读 Context.Targets（不内嵌 selector）。

- [x] **Step 1: 抽象契约 + 两默认策略 + FTcsStepSelectTargets 执行器（宏注册；单体 = 策略 Resolve → Filter AND → 写 Context.Targets）**（两默认策略按 2026-09-20 用户拍板**取消**，见注记）
- [x] **Step 2: 编译验证**

> **2026-09-20 落地实施注记**：规格先行——OpenSpec 提案 **`add-tcstargeting-strategies`**（新能力 `targeting-strategy`，4 条需求；`validate --strict` 通过）。产物 **5 文件**：`Public/Targeting/TcsTargetSelectorStrategy.h` / `Public/Targeting/TcsTargetFilterStrategy.h`（两份抽象契约）+ `Public/Chain/TcsStepSelectTargets.h` + `Private/Chain/TcsStepSelectTargets.cpp`（执行器 + **跨模块自注册**）；另有临时装置 `Private/Testing/TcsTargetingTestRig.h/.cpp`（**不入库**）。
> - **R3 范围收窄（2026-09-20 用户拍板）**：**框架零默认选择器**——`FTcsSelSelf` 与 `FTcsSelEventTarget` **都不进 R3**（与"框架零默认 Filter"同一条纪律：选谁是宿主/内容语义）。EventTarget 依赖的"事件载荷 → 目标"通路随触发行轮（台账 **R5-1**）；Self 零依赖、随时可补，但不先钉一个可能被竖切实际用法否掉的默认。**R3 竖切的选择器与过滤器均由测试装置提供**。
> - **抽象手法（相对计划 sketch 的修正，必读）**：计划写"`Resolve` 用 `PURE_VIRTUAL` 纯虚"——**照抄会在 Development 编不过**：`PURE_VIRTUAL` 在 `CHECK_PUREVIRTUALS` 开启时展开为 `= 0`（`CoreMiscDefines.h:100-102`），而 USTRUCT 抽象类会撞 UHT 生成的 `TCppStructOps<T>`（C2259，引擎实证）。落地改用**中性默认实现 + `meta = (Hidden)`**（基类不进编辑器类型 picker——`SInstancedStructPicker.cpp:104` 按 Hidden 过滤）；同款先例 = 本仓 `FTcsParamValueSource`。
> - **其余落地口径**：`Resolve(Context, ITcsEntityQuery*, OutTargets)` 的**注入查询不进 Context**（走门面注入点，经 `Run.Owner` 取）；`OutTargets` 与 `Context.Targets` **同型**（`TWeakObjectPtr`，弱引用纪律贯通）；**执行器清空、策略只填充**；**未配 Selector ≠ 选中空集**（前者目标集原样 + Warning，不静默清空、不 ensure）；`EntityQuery` **可空**（契约明文，策略降级 + Warning，禁解引用）；悬空候选跳过；遍历/过滤**保序**（D0-1）。
> - **验证**：UBT Development Editor 编译**零警告**（一次通过）；**机制层零改动自检通过**（`git diff -- Source/TcsEffect` 为空——跨模块自注册不需要动 Task 1 的任何文件，Task 1 的注册契约经受住了首个跨模块消费者）；依赖面 grep 零越界（只 include 自身 + TcsEffect 的 `Chain/`/`Host/` + 引擎）。
> - **人工检查已完成——2026-09-20 用户 PIE 两条命令均通过**：`Tcs.Test.Targeting`（正路：跨模块自注册可查 / 选择→过滤 AND→保序写回 / 空 Filter 全过——零 ensure、零 Error）与 `Tcs.Test.Targeting.Reject`（opt-in 边界：未配 Selector 目标集原样 / 未注入查询降级空集——只产预期 Warning）。装置夹具用"临时 spawn 3 个 Actor + 注入装置查询"造确定性场景，命令尾自动清理。
> - **提案已归档**：`openspec/changes/archive/2026-09-20-add-tcstargeting-strategies`——新能力 `targeting-strategy`（**3 条需求**：选择器契约 / 过滤器契约 / SelectTargets 步骤与执行器）并入规格库，规格库 **16 条** `validate --strict` 全绿。
> - **归档前的一处规格修正（用户指正）**：初稿曾在规格里写一条"**R3 范围：框架不提供默认选择器**"（MUST NOT）——用户指出这与设计文档 `10-module-targeting.md` §2.1"默认实现（框架提供）：Self / EventTarget"**冲突**，且把"推迟"固化成"取消"、将来还得写 MODIFIED 撤销。已删除该条，规格只留行为契约；两个选择器后置的事实住在**非规格载体**三处（proposal 顺延节 / 本注记 / 台账 R5-1）。**纪律**：规格装"系统行为真相"，"某一轮做到哪儿"归计划注记与台账——两边不抢对方的活。
> - **遗留**：`IRelationResolver`（阵营判定注入契约，10 §2.3 设计名）plan2 全篇未点名落点、R3 零消费者 → **已入台账 T-5**（触发条件 = 第一个需要阵营判定的宿主实现出现）。

> **2026-09-18 Task 1 交接注记**：`ITcsEntityQuery` 已随 Task 1 落地（`Source/TcsEffect/Public/Host/TcsEntityQuery.h`），**R3 只声明 `EnumerateEntities(TFunctionRef<void(AActor*)>)`**——竖切的 Self / EventTarget 两条策略都不需要枚举，故本任务**不消费**查询接口（`GetLocation` / `IsAlive` 与 RadiusArea 轮一起补）；注入点已在门面就位（`UTcsEffectSubsystem::SetEntityQuery` / `GetEntityQuery`，`TScriptInterface` GC 安全持有）。执行器形状照 `Private/Chain/TcsStepWaitDelay.cpp`（步骤 struct + 执行器函数 + 一行 `UE_DEFINE_EFFECT_STEP_EXECUTOR`）——**跨模块自注册的第一个实证点就在本任务**。

---

### Task 3: TcsDamage——流程机制层（解释器/黑板/协议/裁决）——**已完成（2026-09-20）**

**Files:** `Flow/TcsDamageFlowContext.h / Flow/TcsFlowAttributes.h / Flow/TcsFlowTemplate.h / Flow/TcsFlowStepExecutor.h / TcsDamageSubsystem.h(.cpp)`

**Interfaces:**
- Consumes: TcsCore（Source/总线——日志走本模块 LogTcsDamage 通道）、TcsAttribute（provider 接口——读 Current/ApplyModifier/RemoveBySource）、TcsEffect（宏同构认知，不依赖其注册表）。
- Produces:
```cpp
USTRUCT() struct FTcsFlowTemplate { FName TemplateId; TArray<FInstancedStruct> Steps; };   // 流程=数据（D7-5）
struct FTcsDamageFlowContext
{
	AActor* Attacker; AActor* Instigator; TArray<TWeakObjectPtr<AActor>> Targets;
	TMap<FName, double> FormulaParams;                 // 公式参数初值（只读原料，键=项目词表）
	FTcsFlowAttributes Blackboard;                     // 流程属性黑板（键→修正链，M2 聚合语义）
	TArray<FGameplayTag> ClassificationTags;           // 来源标签启动写入 / 元素标签步骤解析写入
	FTcsSourceHandle FlowSource;                       // 作用域修改器锚点（流程结束 RemoveBySource）
	TMap<FTcsAttributeName, double> CapturedAttrs;       // AttrCapture 快照（默认 Live，命中读快照）
};
// FTcsFlowAttributes：Read(Key)（修正链 M2 求值）/ Submit(Key, Op, Operand, SortKey, FTcsConsumePolicy)（收集事件响应的落点）
// 流程步骤执行器：UE_DEFINE_FLOW_STEP_EXECUTOR(StepType, Fn)——注册表与 Effect 宏同构、独立实例
// UTcsDamageSubsystem：RunTemplate(FName TemplateId, FTcsDamageFlowContext&)（单帧同步完成，无挂起）
// 收集事件协议：任意步骤可发 Combat.Damage.Collect.<Step>（总线立即通道，payload=Context 引用包装）
```
- 消耗裁决机制：候选收集（收集≠消费）→ Execute 步裁决（SortKey 选一）→ 成功才 OnConsumed。

- [x] **Step 1: 三数据结构 + FTcsFlowAttributes**（键→修正链求值；Submit 带 SortKey/消耗策略）
- [x] **Step 2: 流程解释器**（按模板顺序执行；步骤执行器注册表+宏；Context 池化）
- [x] **Step 3: 收集事件协议**（步骤边界发事件——立即通道同步分发，响应链提交落黑板）
- [x] **Step 4: 编译验证**

> **2026-09-20 落地实施注记**：规格先行——OpenSpec 提案 **`add-tcsdamage-flow-layer`**（新能力 `damage-flow`，**6 条需求**：模板登记 / 三层值空间上下文 / 流程属性黑板 / 步骤注册表与自注册宏 / 同步单帧解释器 / 收集事件协议；`validate --strict` 通过）。产物 **8 文件**：`Public/Flow/` 五件（`TcsDamageFlowContext.h` / `TcsFlowAttributes.h` / `TcsFlowTemplate.h` / `TcsFlowStepExecutor.h` / `TcsDamageFlowCollectEvent.h`）+ `Public/TcsDamageSubsystem.h` + `Private/Flow/TcsFlowStepExecutor.cpp` / `TcsFlowAttributes.cpp` + `Private/TcsDamageSubsystem.cpp`；另有临时装置 `Private/Testing/TcsDamageTestRig.h/.cpp`（**不入库**）。
> - **落地口径（相对计划 sketch 的收窄，皆在提案钉名表声明）**：①步骤执行器签名钉为 **`bool` 中止通道**（`FTcsFlowStepExecute = TFunction<bool(const FInstancedStruct&, FTcsDamageFlowContext&)>`——流程无挂起，故不用挂起枚举）；②**收集事件 Tag 改用公约名** `Tcs.Event.Damage.*` + 原生声明（设计文档旧写法 `Combat.Damage.Collect.<Step>` 早于 2026-09-18 Tag 公约；本任务只声明流程开始事件，各步骤收集事件随标准步骤库逐条声明 = Task 4）；③载荷 = **上下文指针包装**（设计写"C++ 引用包装"——总线载荷须反射可见，引用不可反射）；④黑板**只存不裁**（`FTcsConsumePolicy` 落字段；裁决归 Task 4 的 Execute 步骤）；⑤黑板 **R3 无值域收口**（工作值不是角色属性）；⑥**不做 Context 池化**（门面签名即调用方提供上下文）；⑦**不做模板重定向栈**（`FFlowRedirect` 随 M3 状态轮）；⑧`Read` **只调用共享纯函数 `FoldTcsAttributeBands`**（D5-5 v3 三处共用的中间一处——台账 R6-1 已勾销）。
> - **验证**：编译**零警告**（一次通过）；**折叠器复用自检**（`Source/TcsDamage/` 仅 `TcsFlowAttributes.cpp` 一处调用共享折叠，无私建第二份）；**零改动自检**（`git diff -- Source/TcsEffect Source/TcsTargeting Source/TcsAttribute Source/TcsCore` 为空）；依赖面 grep 零越界。
> - **人工检查已完成——2026-09-20 用户 PIE 两条命令均通过**：`Tcs.Test.Damage.Flow`（步骤自注册可查 / 模板登记 / 同步单帧执行 / 顺序 A→B / 黑板读数 15 = 提交 10 + 收集响应 5 / 收集事件同步到达 / 收集重置回 0）与 `Tcs.Test.Damage.Flow.Reject`（未登记模板 / 未知步骤类型 / 步骤返回 false 中止——2 Error + 1 Warning 均为预期）。
> - **提案已归档**：`openspec/changes/archive/2026-09-20-add-tcsdamage-flow-layer`（新能力 `damage-flow` 6 条需求并入规格库）。
> - **同批句柄化（用户 2026-09-20 审阅后拍板）**：链/流程上下文的参与者与实体查询契约全部改 **`FTcsCombatEntityHandle`**（提案 `switch-combat-contexts-to-entity-handles` → 归档 `2026-09-20-switch-combat-contexts-to-entity-handles`；`effect-chain` 加"效果链上下文"需求、`entity-query-contract` / `targeting-strategy` / `damage-flow` 三份改型；规格库 **17 条** `validate --strict` 全绿）；三套装置（Effect / Targeting / Damage）随之复跑全过。**编辑器授权约束**：句柄 MUST NOT 进内容资产（运行期发号、跨会话/跨机不同；要指定实体用"稳定标识 FName + 运行期宿主解析"）。**过网结构纪律**（禁 `TFunction`/`TMap`/`TSet`、`FInstancedStruct` 内层须可复制）已落 `openspec/project.md`；调研笔记 `2026-09-20-replication-posture-research.md`。
> - **遗留**：**流程来源发号器与本进程其他发号器的 Id 碰撞面**——已登记台账 **T-6**（触发条件 = M6 宿主适配轮统一为进程唯一发号）。

---

### Task 4: 标准步骤库 + 官方默认模板 + Damage 链步骤——**已完成（2026-09-21）**

**Files:** `Flow/Steps/（标准步骤库十 struct + FlowModify/FlowDelegate，各 .h，执行器分 .cpp）`、默认模板注册（Subsystem 初始化）、`Flow/TcsDamageFlowDelegate.h / Flow/TcsDamageRecord.h`、`Chain/TcsStepDamage.h(.cpp)`

**Interfaces:**
- Produces:
```cpp
// 十个标准步骤 struct+执行器（行为按 09 §2.2 表：CollectStart 发事件重置收集；BaseDamage **接收输入值写黑板 BaseDamage**——输入=Damage 步骤 DamageBase（参数账本解算，PV-7），流程零计算；
// Execute 免疫/减伤裁决 → 宿主护盾 hook → M2 事务扣血（键=步骤配置 AttrKey）→ 成功才消费；Completed 发事件+FTcsDamageRecord）
// 通用数据步骤：FTcsFlowModify{TargetKey, Op, Operand: FTcsParamValue（黑板键引用保留为流程域自身选项——PV-6）, SortKey}、FTcsFlowDelegate{TargetKey, Delegate 接口位}
// 每步骤 Conditions：TArray<FInstancedStruct>（复用 D4-5 最小集**数据谓词**形状——HasAllTags/Chance 等为纯数据条件，非策略对象，不适用策略形态一元化；R3 只实现 HasAllTags/Chance 两个条件的求值，其余结构留位）
UINTERFACE(MinimalAPI) class UTcsDamageFlowDelegate;   // GetBaseHitRate/GetBaseCritRate/ResolveElement/CalculateBaseDamage（**降级逃生口**——PV-7 D7-2 收窄）/ModifyShield
USTRUCT() struct FTcsDamageRecord { uint64 FlowId; AActor* Source; AActor* Target; double Base; double Final; double Executed; bool bKill; ... };  // 完成时 Damage.Record 事件 + 环形缓冲
USTRUCT() struct FTcsStepDamage { FName FlowTemplateId;   // 空=默认模板；目标消费 Context.Targets（不内嵌 selector——D4-4 v2）
    FTcsParamValue DamageBase;               // 基础伤害值输入（PV-7：参数账本解算结果经 ParamRef 引用——流程零计算，D7-2 收窄）
    TMap<FName, FTcsParamValue> FormulaParams; TMap<FName, ETcsAttrCaptureFrom> AttrCaptureList;
    TScriptInterface<UTcsDamageFlowDelegate> Delegate; FTcsAttributeName HealthAttrKey; };  // 执行器：构 Context → RunTemplate
```
- R3 默认模板（注册为 Default）：`CollectStart → BaseDamage → Execute → Completed`（Hit/Crit/Element/PreExecute struct 定义、不组装）。
- **PV-7 R3 落地注记**：竖切无 TcsSkill 参数账本——`FTcsStepDamage.DamageBase` 用 `FTcsParamSource_Literal` 直配测试值；ParamRef/参数链复合（"攻击力×倍率" = Add(AttributeScaled)+Mul(ParamRef)）随 M5/TcsSkill 轮接入（链行来源 = `SkillDef.ParamChainRows`——PV-9 已采纳）。

- [x] **Step 1: 十标准步骤 struct+执行器**（宏注册；BaseDamage/Execute 为核心，其余最小实现——发事件/写黑板）
- [x] **Step 2: FTcsFlowModify/FTcsFlowDelegate 数据步骤**（含 Conditions 求值挂点——R3 实现 HasAllTags/Chance）
- [x] **Step 3: 默认模板注册**（Subsystem Initialize 组装并登记 Default）
- [x] **Step 4: ITcsDamageFlowDelegate + FTcsStepDamage 链步骤执行器**（宏注册进 **Effect** 注册表——跨模块注册的第一个实证）
- [x] **Step 5: FTcsDamageRecord**（Completed 步骤填充 → 总线 Damage.Record 立即事件 + 环形缓冲）
- [x] **Step 6: 编译验证**

> **2026-09-21 落地实施注记**：规格先行——OpenSpec 提案 **`add-tcsdamage-steps-and-primitive`**（两个新能力：`damage-step-library` 4 条需求 / `damage-primitive` 3 条需求；`validate --strict` 通过）。产物 **10 文件**：`Public/Flow/TcsFlowStepConditions.h`（条件助手 + 两个谓词）、`Public/Flow/TcsDamageFlowDelegate.h`、`Public/Flow/TcsDamageRecord.h`、`Public/Flow/Steps/TcsFlowSteps.h`（十步 struct）、`Public/Flow/Steps/TcsFlowDataSteps.h`、`Public/Chain/TcsStepDamage.h` + `Private/Flow/Steps/TcsFlowStepsCore.cpp`（核心四步）/ `TcsFlowStepsRest.cpp`（其余六步 + 两数据步骤）/ `Private/Chain/TcsStepDamage.cpp`（链原语 + 跨模块注册）+ 门面扩（默认模板注册 / 记录环形缓冲 / 8 个收集 Tag）；另有临时装置 `Private/Testing/TcsDamagePrimitiveRig.h/.cpp`（**不入库**）。
> - **落地口径（相对计划 sketch 的收窄，皆在提案钉名表声明）**：①十步命名 `FTcsFlow<Name>`（`FTcsFlowStep*` 前缀已被 Task 3 注册表族占用）；②**基值以 `Add` 提交**（"基值"在折叠式里就是 `ΣAdd` 的一员——与 M2 属性同式同值；**MUST NOT 用覆盖带**写基值，覆盖带会盖掉收集到的修正）；结果类键（`Executed`/`Absorbed`/`Kill`）才用 `Override`；③`FTcsStepDamage` 收窄为 `{FlowTemplateId, DamageBase, FormulaParams}`——`Delegate`/`HealthAttrKey`/`Conditions` 三字段移除（前者归流程模板配置、后者归 M4a 链侧条件轮；避免双真相与死字段）；④**上下文补 `Owner` 门面弱引用**（句柄化后没有 Actor 可借道取世界——与链运行态同款）；⑤记录 `int64` + `BlueprintType`（UHT 不支持 `uint64` 属性）；⑥`FTcsConsumePolicy` 含 `TFunction` 不可反射 → **数据步骤不带消耗策略**（消耗型提交只能来自 C++ 步骤/事件响应）。
> - **验证**：编译**零警告**；跨模块注册自检（`FTcsStepDamage` 在 Effect 注册表）；依赖面零越界；零公式自检（唯一命中是"调用宿主 delegate"，正是纪律要求的形态）。装置 `Tcs.Test.Damage.Primitive`（正路 7 检查：12 类执行器自注册 / 链原语跨模块注册 / 默认模板四步 + 扣血 100→70 / 记录事件与环形缓冲 / delegate 逃生口 ×2 + 收集改写 -20% → 16 / Conditions 跳过 / 收集事件逐点触发）+ `.Reject`（opt-in 3 检查：未知条件 / 空键 / 未配 AttrKey——3 条预期 Warning）。
> - **顺延（显式交代）**：**AttrCapture 整套**（`ETcsAttrCaptureFrom` + 捕获填充/读取）——R3 竖切零消费者（无属性修正型修改器），已入台账（触发条件 = M5 账本轮或第一个属性修正型修改器）；`FTcsFlowRedirect`（模板重定向栈，D7-7）随 M3 状态轮（**已入台账 R4-3**）。
> - **PIE 首轮（2026-09-21）：通过 3 / 失败 4——4 项全是真缺陷，已修**：①**输入通道错**（链步骤预写黑板 → 被 `CollectStart.Reset()` 冲掉；改为"不写黑板、由 `BaseDamage` 步骤自提交"后又与链侧预写**重复计数**，实测 10+20=30）→ 修法 = **上下文补请求字段** `BaseDamageInput`（输入是"请求"的一部分，不随收集重置清除）；②**默认模板永不扣血**（`FTcsFlowExecute.AttrKey` 留空——插件组装的模板不可能知道项目词表）→ 修法 = 上下文补 `TargetAttrKey` 请求字段 + 属性键解析"步骤级优先、否则用请求键"；③装置缺陷一项（检查 7 只订阅了 2 个收集 Tag，另 5 条无人接收）→ 订阅全量。**方法论**：这三处都属于"设计文档没写到字段级、实现自选通道"的地方——PIE 是唯一能照出来的环节（编译期与静态自检全绿）。
> - **PIE 次轮（2026-09-21）：通过 6 / 失败 1** ——余下 1 项为**键名不匹配**：装置把减伤候选提交到 `FinalDamage`，而执行步骤读 `BaseDamage`（改键时留下的不一致）；顺带暴露记录里 `Final` 一直读废弃键（日志显示 `Final=0.000`）。**修法 = 统一契约键**：伤害量只有 `BaseDamage` 一个键（基值以 Add 落在它上面、收集修正也叠在它上面），**废除 `FinalDamage` 键**；`Record.Base` = 调用方输入、`Record.Final` = 该键折叠后的最终值。
> - **PIE 三轮全过（2026-09-21）**：首轮 **3/4**（输入通道错 / 默认模板无属性键 / 装置订阅面窄）、次轮 **6/1**（契约键不匹配：装置提交 `FinalDamage` 而执行读 `BaseDamage`；并暴露记录 `Final` 字段一直读废弃键）、三轮 **7/0** + 拒绝面 **3/0**。**两轮修复的实质**：①**上下文补请求字段** `BaseDamageInput` / `TargetAttrKey`（输入与目标属"请求"，不随收集重置清除——黑板 = 收集产物、请求字段 = 调用方输入）；②**统一契约键**：伤害量只有 `BaseDamage` 一个键（基值以 Add 落在其上、收集修正叠其上），**废除 `FinalDamage` 键**；`Record.Base` = 调用方输入、`Record.Final` = 该键折叠后的最终值。**方法论**：三处缺陷全属"设计文档没写到字段级、实现自选通道"——编译期与静态自检全绿，**只有 PIE 能照出来**。
> - **同批追加：入库调试命令 `Tcs.Damage.DumpRecords`**（提案 `add-damage-record-inspection` → 归档；新能力 `damage-record-inspection`）——用户质询"策划怎么看 InputDmg 与 FinalDmg 的差别"：现状是"能看见（记录日志）但没有工具（`GetRecentRecords` 是 C++ API、既有命令全在临时装置）"。命令打印环形缓冲表格（序号/Flow/源/目标/**Input**/**Final**/**差额**/Executed/Absorbed/暴击/击杀），差额 = `Final − Input` = 修改器净影响；住**正式模块**（第一条入库的调试命令）。**逐键归因刻意不做**（09 §5 非目标）→ M8 Explain 登记台账 **R8-6**（设计有定义、无 Task 认领的又一处遗漏）。
> - **待办**：**Task 6 顺带验"宿主自研流程"**（用户 2026-09-21 定：定义一个 2–3 步、完全不用插件默认步骤的新流程——验零插件改动 + 零默认步骤依赖，可选地仍落 M2 扣血）。

---

### Task 5: TcsIntegration——CombatEntity 接线与查询　**【已完成 2026-09-21】**

**Files:** `Entity/TcsCombatEntityComponent.h(.cpp) / Entity/TcsPieEntityQuery.h(.cpp) / TcsDefinitionSubsystem.h(.cpp) / Chain/TcsEffectChainDef.h(.cpp)`

**Interfaces:**
- Consumes: 全前序模块。
- Produces:
```cpp
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UTcsCombatEntityComponent : public UActorComponent
	// 身份锚：BeginPlay 门禁（定义库就绪）→ RegisterUnit；EndPlay → 映射摘除 + UnregisterUnit
	// 查询门面：GetCurrent(FTcsAttributeName)/ApplyModifier(...)/RemoveBySource(...)——D2-1 裸 FName 不进属性 API
	// D2-15（2026-09-17 裁决）：本组件是 AttributeSet 的**引用点**（实体侧配置）——R3 竖切无 Set 资产
	//   （属性由测试装置直接添加）；ApplyAttributeSet/ClearAttributeSet 属 R7（台账 R7-1）
	// 触发 API：ExecuteChainById(FName ChainId)（手动触发——R3 不做触发行）
};
UCLASS() class UTcsPieEntityQuery : public UObject, public ITcsEntityQuery   // PIE 枚举：遍历带该组件的 Actor
{
	virtual void EnumerateEntities(TFunctionRef<void(FTcsCombatEntityHandle)>) override;   // 稳定序 = 登记序
	virtual bool GetLocation(FTcsCombatEntityHandle, FVector& Out) override;
	virtual bool IsAlive(FTcsCombatEntityHandle) override;
	// 宿主侧唯一的"句柄 ↔ Actor"映射点（组件注册时登记 / 注销时移除）
	// **文件名与类名都必须与契约区分**（UHT 全项目头名唯一 + U 类名占用——编译实证见实施注记）
};
UCLASS() class UTcsDefinitionSubsystem : public UGameInstanceSubsystem   // 概念名 DefLibrary（定义库）
{
	// 发现：IAssetRegistry::GetAssetsByClass（不依赖 PrimaryAssetTypesToScan 注册——属 M6 轮，见台账 R7-3）
	// 缓存：链定义（Const）+ 失败清单；ResolveChain(FName)
	// 装配：订阅 FWorldDelegates::OnPostWorldInitialization → 每世界装配进该世界 UTcsEffectSubsystem（幂等）
	// 就绪：IsRuntimeReady / GetFailureList（单出口 = Initialize 内置位并派发——R3 同步版）
};
UCLASS(BlueprintType) class UTcsEffectChainDef : public UPrimaryDataAsset   // 链资产（资产轨，2026-09-21 拍板）
{
	// { FName ChainId; FTcsEffectChain Chain; } + 显式 PrimaryAssetType + GetPrimaryAssetId() 名取 ChainId
	// IsDataValid：空 id / 双真相（Chain.ChainId != ChainId）→ Invalid
};
```

- [x] **Step 1: UTcsCombatEntityComponent**（三职责最小实现）
- [x] **Step 2: UTcsPieEntityQuery**（ITcsEntityQuery 默认实现，注入 Effect 层）
- [x] **Step 3: UTcsDefinitionSubsystem 最小版**（就绪单出口；链资产发现 + 缓存 + 装配到世界）
- [x] **Step 3b: UTcsEffectChainDef**（链资产类——收口交接注记①）
- [x] **Step 4: 编译验证**

> **2026-09-21 Task 5 实施注记（落地记录 + 三处引擎事实）**：
> ①**交付清单**：`Chain/TcsEffectChainDef.h(.cpp)`、`Entity/TcsPieEntityQuery.h(.cpp)`、`Entity/TcsCombatEntityComponent.h(.cpp)`、`TcsDefinitionSubsystem.h(.cpp)`、`TcsIntegration.Build.cs`（加 `AssetRegistry` 依赖）、装置 `Private/Testing/TcsIntegrationTestRig.cpp`（**不入库**）。
> ②**两处编译实证（均为引擎硬门槛，已入技能库）**：
>   - **UHT 要求全项目头文件名唯一**：实现头原名 `Entity/TcsEntityQuery.h` 与 TcsEffect 的契约头 `Host/TcsEntityQuery.h` 撞名 → UHT 直接报 `Two headers with the same name is not allowed`（**不是 include 歧义、是 UHT 全局头名注册表去重**）。故实现头改名 `TcsPieEntityQuery.h`（类名 `UTcsPieEntityQuery` 同时避开契约的 U 类占用）。提案原写"同名不同模块、头注释写明即可"——**该判断有误，已修正**。
>   - **`BlueprintType` 类上不能给"非 BlueprintType 结构体"字段加 `BlueprintReadOnly`**：`UPROPERTY(EditAnywhere, BlueprintReadOnly) FTcsEffectChain Chain` → UHT 报 `Type 'FTcsEffectChain' is not supported by blueprint`。修法 = 去掉 `BlueprintReadOnly` 保留 `EditAnywhere`（细节面板可编辑性只看 `CPF_Edit`，与 BlueprintType 无关——2026-09-20 已核），策划侧零损失、TcsEffect 零改动。
> ③**运行时注册组件会走 BeginPlay**（装置夹具依赖）：`RegisterComponentWithWorld` → `AActor::HandleRegisterComponentWithWorld` 判 `HasActorBegunPlay() || IsActorBeginningPlay()`（`Actor.cpp:6429`）为真才调 `Component->BeginPlay()`（`:6449`）——故 PIE 运行期动态挂组件可正常完成注册。
> ④**装置检查面**（`Tcs.Test.Integration`，**2026-09-21 用户 PIE 实测 7/0 全绿 + 零红字**）：定义库就绪 / 查询注入 / 组件身份锚（3 Actor 全注册）/ 查询门面（Health=100）/ 遍历吐句柄（3 个 + 两次顺序一致）/ 句柄解析定位与存活（含未登记句柄返回 false）/ 手动触发链扣血（100→70，`Record` Base=Final=Executed=30）。边界命令 `Tcs.Test.Integration.Reject`（**3/0**）：检查 A 门禁正向对照（定义库就绪 + 就绪时注册通过）；检查 B **故意**未登记链 id → Error 拒绝（红字为预期）。
> ④b **首轮 8/0 含一条红字（用户两次指出后修正——本轮最值得记的一条）**：检查 7（"未登记链 id → Error 拒绝"）原住**主命令**，于是常规验收每次都有 `Error: UTcsEffectSubsystem::ExecuteChain: 链 ... 未登记——拒绝起链` 刷屏。**这不是缺陷，是设计错误**：故意触发失败输出的检查 MUST 独立成 opt-in 命令——本项目 **2026-09-18 已为"故意 ensure"立过同一原理**（见 README 检查点段），本轮**以 Error 日志形式复犯**。更值得警惕的是我当时的处理：在汇总里写"检查 7 的 Error 日志为**预期**"——**用一句注解把设计缺陷正当化了**，等于承认"常规验收可以有红字"。修正后主命令回归**零红字**（检查项 8 → 7），`.Reject` 自带"红字为预期"屏显声明。**纪律已泛化入 `unreal-development-workflow` 技能**：判据 = "常规验收命令的输出应当零红字，跑完只看 PASS/FAIL 行；任何以'某条 Error 是预期的'为注解的输出，都说明该检查放错了命令"；命名约定 `Tcs.Test.<域>`（零红字）/ `Tcs.Test.<域>.Reject`（自带声明）。
> ⑤**验证覆盖面缺口（须在 Task 6 补上，勿被"全绿"掩盖）**：**DefLibrary 的自动发现路径未被实证**——`GetAssetsByClass` → 缓存 → 装配到世界这条链，在 R3 **没有内容资产文件**（全库无 `UTcsEffectChainDef` 实例），故装置检查 6 走的是**手动 `RegisterChain`**（与 Task 1-4 同款），而**不是**"资产被发现 → 自动登记 → 触发成功"。受影响的验收面：①本提案的 Scenario「链资产自动登记」与「新世界初始化后链仍可查（跨世界装配）」；②**验收剧本检查点 6（"零 C++ 加链"）**——它正是靠"策划只建资产、不改代码"来验数据驱动线。Task 6（内容资产版装置 + PIE 地图）会创建真实链资产，届时**这两条 Scenario + 检查点 6 应一并验收**（已写入 Task 6 Step 5）；`IsLoadingAssets()` / `WaitForCompletion()` 的异步扫描门也将在那时首次真实生效。
> ⑥**未在本任务（归属已明）**：AttributeSet 全套（R7-1，含 `ApplyAttributeSet`/`ClearAttributeSet`）；加载层三策略 + 异步（R7-3）；流程模板资产化（T-8）；`PrimaryAssetTypesToScan` 注册（R7-3）。

> **2026-09-18 Task 1 交接注记（两项需在本任务收口）**：
> ①**链资产类落点**：Task 1 只交付登记 API（`RegisterChain` / `UnregisterChain` / `FindChain`，键 = `FTcsEffectChain::ChainId`）与执行入口，**没有链资产类**——本计划 File Structure 与本任务清单都未点名 `UTcsEffectChainDef` 的落点。**已于 2026-09-21 拍板（用户）**：采用**资产轨**——`UTcsEffectChainDef : UPrimaryDataAsset`，字段 `{ FName ChainId; FTcsEffectChain Chain; }`，`static const FPrimaryAssetType PrimaryAssetType` 显式声明 + 覆写 `GetPrimaryAssetId()` 使**名取 `ChainId`**（遵 2026-09-17 的 Def 身份/命名标准：`<族>Def` 去 `Asset` 后缀、身份 = `[PrimaryAssetType, DefId]`、资产可改名移动不破坏解析）。**否决**：DataTable 行轨（链带嵌套 `FInstancedStruct` 步骤数组，单元格内嵌多态结构编辑体验差——行轨留给扁平词表）；双轨（M8 的双轨同步器负担不值得）。**登记时机**：**暂定 DefLibrary 发现并加载链资产后自动调 `RegisterChain`**（与"链资产发现 → DefLibrary、执行 → Effect"的既定分工一致）——开发时再议。
> ②**接口 U 类名撞名**：Task 1 的 UINTERFACE 按 UE 约定占用了 `UTcsEntityQuery` / `ITcsEntityQuery` 这一对名（`ITcsEntityQuery::UClassType` 即 `UTcsEntityQuery`）。本任务 Step 2 原写"`UCLASS() class UTcsEntityQuery : public ITcsEntityQuery`"——**同名将导致两个模块各有一个 `UTcsEntityQuery` U 类**（反射库里重名、同 TU include 两者即重定义）。实现类请改名（如 `UTcsPieEntityQuery`）并同步本行文字。
> ③**实体映射点（2026-09-20 句柄化后新增）**：实体查询实现（`ITcsEntityQuery` 三支能力：遍历吐**句柄** / `GetLocation` / `IsAlive`）是**宿主侧唯一的"句柄↔Actor"映射点**——`UTcsCombatEntityComponent` 注册实体时记录"自己拿到的句柄"并在此映射；机制层与内容资产都不碰映射（**句柄不得进内容资产**——授权约束已入规格侧提案钉名表）。装置里已有该写法的最小样板（自建映射 + 三支能力实现）。

> **2026-09-21 Task 5 前置讨论落档（四条拍板 + 一条纪律，动工前已定）**：
> ①**定义库类名 = `UTcsDefinitionSubsystem`**（两轮收口：设计侧原名 `UCombatDefLibrarySubsystem` → 先拍 `UTcsDefLibrary` → 再定本名）。判据：①族内风格一致（`Tcs` + 域 + `Subsystem`）；②**避开"Registry"**——该词已被中央注册表（可变运行态）占用，且文档另有"定义注册表"/"链定义登记表"两处混用，不再加重负担。`DefLibrary` 保留为**概念名**（沿用设计词汇，与"概念名 ↔ 实现类名"既有惯例一致）。06 文档 §2.1 已回写修正（v4 增补条）。
> ②**发现机制 = `IAssetRegistry::GetAssetsByClass` 按类扫描**（否决"现在就注册 `PrimaryAssetTypesToScan`"与"硬引用清单"）：不提前替 M6 拍板注册时机；扫描失败显式可见（**已核引擎语义**：未注册类型走 AssetManager 的 `GetPrimaryAssetIdList` / `GetPrimaryAssetPath` 会**静默返回空**，无 log 无 ensure——`AssetManager.cpp:2134-2152`；而 `GetPrimaryAssetId()` 本身是纯函数不依赖注册，`DataAsset.cpp:73-122`）。`PrimaryAssetTypesToScan` 注册 + 切换 AssetManager 入台账 **R7-3**（M6 轮，AttributeSet 依赖它）。`Build.cs` 须加 `AssetRegistry` 依赖（`Engine.Build.cs:98` 那条是过渡期临时依赖）。
> ③**登记时机 = 缓存 + 世界初始化装配**（否决"ready 时登记一次"与"组件惰性触发"）：**跨级方向问题**——DefLibrary 是 GameInstance 级（跨世界存活），而 `UTcsEffectSubsystem` 是 **World 级**（每世界重建）；"ready 时对当时的 World 登记一次"会在关卡切换后丢失全部链登记（R3 单地图 PIE 测不出，宿主首次切关卡必撞）。方案 = DefLibrary 缓存链定义（Const）+ 订阅 `FWorldDelegates::OnPostWorldInitialization` 在每个世界装配（幂等），正是 06 文档职责原话"**把定义库装配到世界**"。**时机已核**：`UWorld::InitWorld` 中 `InitializeSubsystems()`（`World.cpp:2447`）**早于** `OnPostWorldInitialization.Broadcast`（`2601`），故装配时 EffectSubsystem 已可用；委托声明见 `World.h:4488`（`DECLARE_MULTICAST_DELEGATE_TwoParams(FWorldInitializationEvent, UWorld*, const UWorld::InitializationValues)`）/`:4539`。
> ④**`FInstancedStruct` 承载步骤的资产稳定性已核实**（本任务选资产轨的前提）：类型身份是**包 import 表索引**（`TObjectPtr<const UScriptStruct>` 序列化 → `FPackageIndex`，`InstancedStruct.cpp:199-234` / `:254-276`），解析走 linker 同步 import 路径（`LinkerLoad.cpp:5374`），**非名字字符串**；数据体走 **tagged property stream**（`SerializeItem` → `SerializeTaggedProperties`，`Class.cpp:3338-3421`）→ **加字段取默认值、删字段被 `Tag.Size` 安全跳过**（`Class.cpp:1956-1966`、`:2016-2018`）。**两条纪律**：a) 步骤 struct **MUST NOT** 加 `Atomic` / `Immutable` specifier（会退化成 `SerializeBin` 裸二进制，增删字段不安全）；b) **步骤 struct 类名改名 = 旧资产失联**（该步骤变空 + `LogCore` Warning，`InstancedStruct.cpp:237-248`），补救 = `[CoreRedirects] +StructRedirects`（须在 cook 前生效——cooked 构建下 `FixupLinkerImportMap` 被 `RequiresCookedData()` 短路，`LinkerLoad.cpp:2168`）。落到我们系统里类型失联是**可观测**的：链解释器 `Step.GetScriptStruct()` 空 → 无执行器 → Error + 断链（`TcsEffectSubsystem.cpp:205-215`），不会静默少打一次伤害。

> **2026-09-21 增补：两个解释器、两套资产——对照说明（用户质询"链资产到底用来干嘛"的落档）**
>
> 用户的困惑合理：**链资产**与**流程模板**长得像（都是"有序步骤数组 + FInstancedStruct"），但它们是**两个解释器**的两套句子，服务不同问题域：
>
> | | **效果链**（`FTcsEffectChain` / `UTcsEffectChainDef`） | **伤害流程模板**（`FTcsFlowTemplate`，待改名 `FTcsDamageFlowTemplate`） |
> |---|---|---|
> | 解释器 | `UTcsEffectSubsystem`（World 级，**可挂起/唤醒**、跨帧） | `UTcsDamageSubsystem::RunTemplate`（**同步单帧**、无挂起） |
> | 步骤集 | **15 原语**（D4-16）：控制流 6 + 战斗 6 + 表现与元 3；R3 只落 `WaitDelay`/`SelectTargets`/`Damage` | **标准十阶段**（D7-5）：CollectStart→PreHit→Hit→Crit→Element→BaseDamage→AfterDamage→PreExecute→Execute→Completed；R3 十步执行器**已全部落地** |
> | 回答的问题 | **"这个技能/事件要做什么"**——顺序、分支、等待、多目标、多段 | **"一次伤害怎么结算"**——命中/暴击/元素/减伤/免疫/扣血的固定流程 |
> | 谁的内容 | 策划创作（技能链、触发效果） | 插件自带官方默认模板（项目可整表替换，09 §2.2） |
> | 资产形态 | **Task 5 落地**（`UTcsEffectChainDef`） | **T-8 台账**（等第一个真实内容需求） |
>
> **两者的连接点 = `FTcsStepDamage`（Damage 链原语）**：链走到 Damage 步时，它构造 `FTcsDamageFlowContext`（搬运 `BaseDamageInput` / `TargetAttrKey`）→ 调 `RunTemplate` 起一次伤害结算 → 流程走完返回，链继续下一步（`TcsStepDamage.cpp:24-68`）。所以：
>
> - **链是"句子"**："等 0.5 秒 → 选目标 → 造成 30 点伤害"——策划天天改；
> - **流程模板是"伤害结算的句子"**："先算命中、再算暴击、再算元素、再算减伤、最后扣血"——插件给标准版，项目要改顺序/加阶段时才动；
> - **步骤是"词汇"**（`FTcsStepDamage` / `FTcsFlowHit` 等）：**不是资产、不可配置**——扩展词汇 = 写新 step struct + 执行器（R0 §8 Authoring 第三层）。
>
> 三者关系：**链（技能编排）→ 链原语 Damage（搬运）→ 流程模板（结算编排）→ 流程步骤（结算词汇）**。资产化的合法边界：**句子可资产化**（链资产已做、流程模板待 T-8），**词汇不可资产化**（步骤 struct 恒为 C++）。

---

### Task 6: 测试装置 + PIE 地图（内容资产）　**【已完成 2026-09-22：内容资产 + PIE 验收全项通过】**

**Files（落地后的实际形态——与计划原文有四处偏离，见实施注记①）**：内容资产住**宿主项目** `Content/TcsDev/`（4 条 `UTcsAttributeDef` 属性资产 / 1 条 `UTcsEffectChainDef` 链资产 / 1 张地图 + 2 单位）；切片代码住**宿主项目独立模块** `Source/TcsDev/`（公式 delegate / 自研流程 2 步 / 装配 / 屏显观测 / `Tcs.Test.Slice.*` 命令）；插件侧仅新增 `FTcsSelSelf` 选择器 + 三处导出宏修正。

- [x] **Step 1: 属性定义资产 4 条**（`UTcsAttributeDef` 资产轨——**非**计划原文的 DataTable，见注记①）——*已建并实测装载 4/4*
- [x] **Step 2: 测试链资产**（`UTcsEffectChainDef`：`WaitDelay 0.5 → SelectTargets(Self) → Damage`——检查点 6 的"零 C++ 加链"实证主体）——*已建；检查 3 实测 3/3 步类型全可解析*
- [x] **Step 3: 测试公式 delegate + 2 个测试单位 + 地图**（公式落宿主 `Source/TcsDev/`；验收信号 = `UTcsDevScreenObserver` 订阅属性/记录广播直调 `GEngine->AddOnScreenDebugMessage`）——*地图 `L_TcsDev_Slice` 已建（含 2 个单位）；屏显观测实测生效*
- [x] **Step 4: 编译 + 打开 PIE 就绪**（插件与宿主各一次 UBT Development Editor，**零警告**）
- [x] **Step 5（2026-09-21 Task 5 追加）：DefLibrary 自动发现路径验收**——命令 `Tcs.Test.Slice.Run` 的检查 1b/2/3 已就位；拒绝面（瞬态双真相资产 / 空身份 / 未登记链）在 `.Reject`。**PIE 实测通过（见注记⑨）**。

> **2026-09-21 Task 6 实施注记（落地记录：四处口径收窄 + 两处框架缺陷 + 一处装置缺陷）**
>
> **① 四处落点/形态偏离计划原文（均经用户 2026-09-21 拍板）**：
> - **切片代码落宿主独立模块 `Source/TcsDev/`**（用户指令："所有写入宿主项目 LAC 的代码，都统一放到 `TcsDev` 目录下，不作为 LAC 正式内容"）——计划原文写 `Source/TcsIntegration/Testing/`（装置目录）。**判据**：独立模块 = 可整体不构建/可整体删除/**LAC 正式模块零改动**（`git diff -- Source/LegendAutoChess/` 实测为空）；若落 `Source/LegendAutoChess/TcsDev/` 子目录，则 LAC 正式模块 `Build.cs` 必须加 TCS 依赖，与"不作为正式内容"相悖。
> - **属性定义走资产轨（`UTcsAttributeDef`），非计划原文的 DataTable**：2026-09-17 双轨制定案后，DataTable 是**编辑期载体**、**运行期一律走资产**（`attribute-types` 规格明文"运行期零 DataTable 加载路径"）。计划原文写于该定案之前。
> - **内容资产住宿主 `Content/TcsDev/`**（非插件 `Content/`）：竖切是宿主项目的样例；且内容引用的类型必须长于内容（链资产引用 `FTcsSelSelf` 等插件类型，成立）。
> - **`FTcsSelSelf` 提前进 R3**（Task 2 曾拍"R3 框架零默认选择器"）：**动因 = 内容资产的类型稳定性**——`TInstancedStruct` 存类型身份（包 import 表索引），链资产若引用切片类型则切片退役后该步**静默变空**（降级只剩一条 `LogCore` Warning）。Task 2 那条口径当年**被明确排除在规格之外**（用户要求"规格只装系统真相"），故本任务是**纯 ADDED**、无需撤销任何已归档规格文字；设计 `10 §2.2` 本就写"框架提供 Self/EventTarget"（EventTarget 仍留 R5，依赖载荷→目标通路）。
>
> **② 两处跨模块链接缺陷（本任务照出的真实框架缺陷，已修）**——共同根因：**此前从无跨模块消费者**，故潜伏至今。
> - **原生 Tag 声明缺导出宏**：`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**（`NativeGameplayTags.h:31`，无 dllexport）→ 宿主引用即 `LNK2001`。修法：`TcsAttributeChangedEvent.h`（1）+ `TcsDamageFlowCollectEvent.h`（9）+ `TcsDamageRecord.h`（1）改 `extern TCS<模块>_API FNativeGameplayTag ...`（定义处零改动）。**判据**：框架事件 Tag 的设计意图正是"供宿主订阅"。
> - **`FTcsFlowAttributes` 缺导出宏**：`Submit`/`Read` 是**宿主自研流程步骤的公共调用面**（既定用法）→ `LNK2019`。修法：`FTcsFlowAttributes` 与 `FTcsFlowAttributeSubmit` 加 `TCSDAMAGE_API`。
> - **存量面**：其余公共 struct（`FTcsDamageFlowContext` / `FTcsConsumePolicy` / `FTcsAttributeStore` 等）纯内联或仅同模块使用，暂无需导出——**随首个跨模块消费者出现再逐个补**（已登记台账 **T-9**）。
>
> **③ 一处装置设计缺陷（自查发现，非编译报错）**：拒绝面检查 A 初稿拿 `RegisterChain` 的返回值当"双真相被拒"判据——**错**：`RegisterChain` 只校验 `ChainId` 非空与重复，**不做双真相校验**（双真相是**资产层**纪律，由 `UTcsEffectChainDef::IsDataValid` 与 `DefLibrary::DiscoverChainDefs` 承担）。该写法会**真的登记一条坏链并报 false pass**。修法：改为断言 `IsDataValid` 返回 `Invalid` 且错误数 > 0，并注释写明两条校验路径的分工。
>
> **④ 屏显接线（规格要求，初稿漏做）**：`UTcsDevScreenObserver` 初稿只定义类**没订阅**。修法：`UTcsDevBootstrap::SubscribeScreenObserver` 订阅两个原生事件（立即通道），观测者由 Bootstrap 持 `UPROPERTY` 强引用（总线订阅表持**弱引用**，不阻止 GC——`TcsEventBus.h:65`）。
>
> **⑤ 内容资产的 `FlowTemplateId` 与公式的关系（会误导验收的坑，已写进建立指南 §4.4）**：切片装配登记的 `Default_Slice` 带公式 delegate → `CalculateBaseDamage` 把链上 `DamageBase` **替换**为 `max(1, Attack−Armor)`。故**带公式模板下改 `DamageBase` 不改变扣血量**——检查点 6 的"改数值零 C++ 生效"须把链指向**无公式的 `Default`**（或改属性资产走公式输入）。指南给两种做法并建议各跑一次。
>
> **⑥ 装置退役的执行（用户 2026-09-21 拍板"六套全删"）**：`Source/*/Private/Testing/` 六套 12 文件已删（该目录本就不入库，故 git 无删除痕迹——与"临时件不入库"约束一致）。**显式代价（不藏）**：其中 `TcsCoreTestRig` / `TcsAcceptanceRig`（TcsAttribute）属 plan1，覆盖检查点 2/3/4 与核心句柄/事件/时钟——**内容版装置替代不了**这些覆盖（它们测的是"批内 0 条广播""同属性两次变更只 1 条广播"这类机制内省），删除后 R3 内**无回归网**；其结论已随 plan1 归档，Task 7 不重验。
>
> **⑦ 验证状态**：**编译零警告**（插件 + 宿主，Development Editor）；**LAC 正式模块零改动**（`git diff -- Source/LegendAutoChess/` 实测为空）。**内容资产与 PIE 验收已于 2026-09-22 全项通过**（见注记⑨）——本注记原记的"未执行"状态已失效，保留以见演进。
>
> **⑧ 未在本任务**：AttributeSet 全套（R7-1）；加载层三策略 + `PrimaryAssetTypesToScan`（R7-3）；流程模板资产化 + 模板登记表 GC 持有（T-8）；属性资产的 DefLibrary 多族发现（**决策点已定**：本任务走"切片侧自行扫描装载"，DefLibrary 保持只管链资产的既有规格——若将来纳入需开 `integration-entity` 增量提案）。
>
> **⑨ 2026-09-22 内容资产 + PIE 验收（全项通过；含一处装置缺陷修复 + 两处新实证）**：
>
> **验收结果（全部实测，非推断）**：
> - **`Tcs.Test.Slice.Run`：10/10 全绿、零红字**（检查 0/1a/1b/2/3/4/5/6/7a + 延迟判定 7b）。关键读数：属性装载 4/4；链资产发现 1 个；`Attack=30.0 / Armor=5.0 / Health=100.0`；宿主自研流程扣血 `100.0 → 75.0`（`max(1, 30−5)`）；链起链成功且 `WaitDelay` 到期后续走扣血生效。
> - **`Tcs.Test.Slice.Reject`：3/3 全过**——双真相资产被 `IsDataValid` 拒（1 条错误）/ 空 `ChainId` 被登记 API 拒 / 未登记链 id 触发被拒（Error 日志 + 无效句柄，不崩溃）。三条预期红字均命中，且**只**出现在本 opt-in 命令内。
> - **检查点 6 端到端（零 C++ 改数值）：通过**——链资产 `FlowTemplateId` 改指无公式的 `Default`、`DamageBase` 30 → 45，重跑后扣血 **25.0 → 45.0**（严格跟随资产数值）。对照：带公式的 `Default_Slice` 下 `DamageBase` 被 `max(1, Attack−Armor)` 替换，故改它不影响扣血（注记⑤预告的坑，实测复现）。
> - **§4.3 跨世界装配：通过**——PIE 运行中控制台 `open L_TbnsShowcase` 切到另一插件的地图，重跑 `Tcs.Test.Slice.Run` **10/10 全绿**，检查 1b/2 仍通过。**决定性证据**：切图后日志只有 `已装配到世界 L_TbnsShowcase——链定义 1/1 条`，**没有**"发现链资产"行 → 定义库（GameInstance 级）未重扫、直接拿缓存装配进新世界（旧世界 `CleanupWorld` → 新世界装配，同进程同 GameInstance）。
>
> **⑨-A 一处装置缺陷（读码自查发现，已修 + 编译 + 实证）——检查 7b 读错实体**：链上的 `SelectTargets(Self)` 执行器是"**清空后写回**"（`TcsStepSelectTargets.cpp`：`Context.Targets = MoveTemp(PassedTargets)`），会把目标集**替换**为施法者自身；故该链的扣血落在**施法者**身上。而初稿的延迟判定读的是**目标**的 Health——**7b 会恒 FAIL**（目标的生命只被检查 6 改过，本链一点没碰它），且 7a 文案已写"目标 = 施法者自身"、7b 却读目标，**断言与语义自相矛盾**。修法：`ChainHealthBefore` 与延迟回调的读取对象由 `TargetHandle` 改为 `CasterHandle`，文案同步改为"施法者"。**实证**：修复后 7b 读到 `施法者 Health 100.0 → 55.0（实际扣 45.0）`——修复前该行必然显示 0 伤害。
> - **教训归属**：这是"装置全绿 ≠ 新能力已验证"的**镜像情形**——装置**恒红**同样不是被测对象的问题。判据：断言读的实体必须与"被测语义"一致；链上选择器改写了目标集时，判定点必须跟着改写。
>
> **⑨-B 两处新实证（Task 5 遗留缺口已闭合）**：
> - **DefLibrary 自动发现路径首次真实生效**：Task 5 注记⑤列的三条受影响验收面（Scenario「链资产自动登记」/「新世界初始化后链仍可查」/ 检查点 6）**全部随本轮验收闭合**——检查 1b 实证"未调 `RegisterChain` 而链被缓存"，检查 2 实证"世界装配可查"，且 `IsLoadingAssets()` / `WaitForCompletion()` 异步扫描门首次在有资产的真实场景下生效。
> - **关卡加载期组件注册路径（装置走不到的路径）**：用户在地图 `L_TcsDev_Slice` 里摆了 2 个单位，日志实测 `已注册实体 StaticMeshActor_1/2（句柄 1/2）`——**关卡加载期 `BeginPlay` 注册**，与装置"运行时 `SpawnActor` + `AddInstanceComponent`"是**两条不同路径**。该路径通了，且**就绪门禁在真实路径上成立**（装配 `18.15.42.567` 早于组件注册 `:595`，无 Warning 跳过）。这是本轮验收中"装置永远覆盖不到"的覆盖面增量。
>
> **⑨-C 验收操作方式（可复用）**：本轮由 agent 经 **UE MCP** 直接驱动——`DataAssetTools.create` + `ObjectTools.set_properties` 建资产（`FInstancedStruct` 用 `{"_structType": "/Script/<模块>.<StructName>", ...}` 写入），`EditorAppToolset.StartPIE/StopPIE` 控 PIE，命令经 Output Log 底部 `Cmd` 输入行（Slate ref）提交，结果读 `LogsToolset.GetLogEntries`。**注意**：编辑器内**原生模态对话框（如 Save Level As）会阻塞游戏线程 → 所有 MCP 调用超时**，须由人工操作或绕 Win32 输入。
>
> **⑨-D 归档**：`openspec archive add-content-slice-rig --yes` 已执行——delta（`targeting-strategy` ADDED × 1：内置 Self 选择器）应用到 `openspec/specs/targeting-strategy/spec.md`；归档目录 `openspec/changes/archive/2026-09-21-add-content-slice-rig/`。规格库自检 `openspec validate --specs --strict` **22/22 全绿**。
>
> **⑨-E 一条验收判据需修正（记录在案，不藏）**：任务清单 5.5 原写"宿主自研流程生效 **且插件 `git diff --stat` 为空**"——**该字面判据已失效**：本轮照出两处真实框架缺陷（原生 Tag 与 `FTcsFlowAttributes` 缺导出宏）并新增 `FTcsSelSelf`，插件 diff **不空**。判据的**真实意图**（"宿主自研流程无需插件为它改动"）**已达成**——检查 6 实证 `Slice_Flow` 两个宿主自研步骤零插件默认步骤依赖生效；插件侧的改动是**框架存量缺陷修复**（宿主作为首个跨模块消费者照出），与"宿主自研能力"无关。教训：**判据要写它真正要拦的东西**——"diff 为空"拦的是"框架被迫为宿主特化"，而不是"框架有 bug 要修"。

> **2026-09-18 Task 1 交接注记**：Step 2 的"测试链资产"依赖 Task 5 定下的链资产载体（`UTcsEffectChainDef` 或行轨——见 Task 5 注记①）；检查点 6"零 C++ 加链"的成立条件 = 链资产在编辑器内可创作 + 经 `RegisterChain` 登记，登记 API 已在 Task 1 就位。竖切测试链的步骤形状：`FTcsStepWaitDelay{0.5}` → `FTcsStepSelectTargets`（Task 2）→ `FTcsStepDamage`（Task 4）。**验收信号走测试装置直调 UE API**（插件模块零屏显调用，D0-6 v2）。
>
> **2026-09-21 Task 5 收口注记（载体已定，本注记的前置问题已解）**：链资产载体 = **`UTcsEffectChainDef`（资产轨）**，类已落地（`Source/TcsIntegration/Public/Chain/TcsEffectChainDef.h`）；创作方式 = 编辑器内建 `UTcsEffectChainDef` 资产、填 `ChainId`（与 `Chain.ChainId` 一致）、在 `Chain.Steps` 里配步骤；**登记无需代码**——`UTcsDefinitionSubsystem` 在 GameInstance 初始化时自动发现（`GetAssetsByClass`）并缓存，每个世界初始化时装配进该世界的 `UTcsEffectSubsystem`。故本 Task 的"零 C++"是**真的零代码**：策划建资产即可，连 `RegisterChain` 都不用调（见 Step 5）。**注意**：Task 5 的装置命令走的是手动登记，自动路径由本 Task Step 5 首次实证。

---

### Task 7: 端到端验收（检查点 1/5/6/7 + 日志附加）　**【已完成 2026-09-22：六步全通过】**

- [x] **Step 1: 检查点 1 数值**：触发链 → 屏显 `damage = max(1, Attack−Armor)`；Health clamp 到 0——***通过**（见注记①；公式数值与 clamp 到 0 两个半边均实证，多链按 id 选链亦实证）*
- [x] **Step 2: 检查点 5 铁律 grep**：反向 include 逐模块枚举——`Select-String -Path Source\TcsCore\* -Pattern 'TcsAttribute|TcsNotation|TcsEffect|TcsTargeting|TcsDamage|TcsIntegration'` 应零命中；同法抽查 TcsEffect 不得含 TcsDamage/TcsTargeting；实例数据文件无 TSubclassOf；依赖边对照 R0 §9 表（最小编译集口径）——***通过**（见注记②）*
- [x] **Step 3: 检查点 6 数据驱动线**：复制测试链资产改 Damage 参数数值 → 触发生效，**零 C++ 改动**——*Task 6 已验（扣血 25→45），本轮标为已覆盖*
- [x] **Step 4: 检查点 7 ScaledDt**：`slomo 0.1` 下 WaitDelay 到期减速；pause 冻结（Output Log `LogTcsCore`/`LogTcsEffect` 时间戳对照）——***通过**（见注记④；两半边均实证——`slomo 0.1` 实测实时:战斗 = 10:1，`pause` 实测 `Elapsed` 完全冻结）*
- [x] **Step 5: 日志附加**：UE 原生分类控制生效（`log LogTcsDamage Verbose` 开启流程追踪、恢复默认）；屏显仅测试装置产生（插件模块零屏显调用）——***通过**（判据需修正，见注记③）*
- [x] **Step 6: 全量编译（Development + Shipping 配置各一次）→ 停点交付人工检查单全项**（提交经用户授权）——***两配置均通过**：Shipping 首跑照出 2 个真实缺陷（已修，见注记⑤）；Development 最终一次真实 UBT 编译写回基线 DLL（见注记⑥）*

> **2026-09-22 Task 7 执行注记（Step 2/3/5/6 完成 + Step 1 核心结论 + 两处判据修正）**
>
> **① Step 1（检查点 1）通过**——新建 `DA_FormulaChain`（`ChainId` = `Formula_Chain`，步骤 `WaitDelay(0.5)` → `SelectTargets(Self)` → **5×**`Damage`，模板 `Default_Slice` 带公式）后跑 `Tcs.Test.Slice.Run Formula_Chain`，**10/10 全绿（即时 9/0 + 7b PASS）**：
>
> **判读点逐条命中（干净重跑，装置与源码一致）**：
>
> | 判读点 | 实测 | 证明 |
> |---|---|---|
> | 「受测链 = Formula_Chain」提示行 | 出现 | 新命令签名（`FAutoConsoleCommandWithWorldAndArgs`）生效 |
> | 链名 = `Formula_Chain` | 正确 | **按 id 精确选链**生效（不再是"取第一条"的 TSet 哈希序） |
> | 7b 读**施法者**（非"目标"） | `链 Formula_Chain：施法者 Health 100.0 → 0.0` | Task 6 的 7b 修复生效 |
> | 实际扣 **100.0** 而非 125 | 钳住 | **clamp 到 0 生效**（5×25=125 被钳在 0） |
> | `DamageBase = 30.0` 而扣 100 | 一致 | **公式替换生效**（`max(1,30−5)=25`，非原样 30） |
> | 7b `[PASS]` | ✓ | 装置与语义一致 |
>
> **伤害记录（5 次全同，即公式的逐次证据）**：
>
> | # | 输入 | 最终 | 执行 | 施法者 Health |
> |---|---|---|---|---|
> | 1 | 30.000 | **25.000** | 25.000 | 100 → 75 |
> | 2 | 30.000 | **25.000** | 25.000 | 75 → 50 |
> | 3 | 30.000 | **25.000** | 25.000 | 50 → 25 |
> | 4 | 30.000 | **25.000** | 25.000 | 25 → **0** `[击杀]` |
> | 5 | 30.000 | **25.000** | 25.000 | **0**（钳住）`[击杀]` |
>
> - **公式 `max(1, Attack−Armor)` 生效**：输入 30 但执行 **25**（差额 −5 = 护甲）✓
> - **Health clamp 到 0 生效**：第 4 次落到 **0**，第 5 次**仍为 0**（未变 −25）✓
> - **多链共存 + 按 id 选链**（原验收只有单链）：同会话两跑对照——`Formula_Chain` → 扣 100.0、`Slice_Chain`（默认参数）→ 扣 45.0，**各自与自己的资产配置一致**；检查 1a `2 个` / 1b `2/2` / 2 `2/2` / 3 `10/10 步` ✓
> - **未验到的半边（如实记）**：公式的**下限**半边（`Attack ≤ Armor` 时取 1）本链验不到——`Self` 选择器把目标集换成施法者，而施法者 Attack=30 / Armor=5 恒得 25；要验下限需让 Armor > Attack（动共享属性资产），**留待需要时单做**。
>
> **② Step 2（检查点 5 铁律）通过**——精确 `grep "^#include"` 复验：`TcsCore` 零反向 include、`TcsEffect` 零 `TcsDamage`/`TcsTargeting`、`TcsAttribute` 零上层模块、全库零 `TSubclassOf`；依赖边与 R0 §9 表一致。**方法要点**：粗 grep（不限 `^#include`）会把**注释文本**里提到的模块名当成违规命中（本次初跑各命中 2–3 处，全是文档注释），必须用 `^#include` 精确匹配——否则会误判。
>
> **③ Step 5 的判据需修正（原文与已归档规格冲突）**——原判据"屏显仅测试装置产生（**插件模块零屏显调用**）"**与 `damage-record-inspection` 规格冲突**：该规格**明文要求** `Tcs.Damage.DumpRecords` 走屏显（`MUST ... + 屏显（GEngine->AddOnScreenDebugMessage）`）。判据写于该命令入库之前（Task 4 追加交付），未随规格更新。**判据的真实意图** = "竖切验收信号不得由插件代劳"；按此重验**通过**：插件三处屏显调用**全在 `DumpDamageRecords` 一个函数内**（规格要求），验收信号由装置产生（`TcsDevScreenObserver` 订阅总线 + `TcsDevSliceRig` 判定行各自打屏显）。——**同 `MEM-20260922-02`（判据要写它真正要拦的东西）**。
>
> **④ Step 4（检查点 7 ScaledDt）通过——观测方法 = 日志时间戳对照（零新代码）**：plan1 验检查点 7 用的 `Tcs.Test.Clock` 装置**已随六套临时装置退役删除**（Task 6 注记⑥只记了"检查点 2/3/4 回归网消失"，**漏记检查点 7 的观测工具也一并消失**）。本轮改用**零新代码**方案：开 `log LogTcsCore Verbose`，日志每条自带**实时时间戳**、正文含 `Elapsed`（战斗时间），两者对照即验 ScaledDt。
>
> **观测载体**：`UTcsClockSubsystem::HandleWorldTickStart`（`TcsClockSubsystem.cpp:54-74`）的 Verbose 日志 `泵推进 Frame=N DeltaSeconds=X Elapsed=Y`——其中 `:63` 是 pause 语义的显式实现：`const double RawDeltaSeconds = World->IsPaused() ? 0.0 : DeltaSeconds;`
>
> **slomo 半边（实测，`slomo 0.1`）**：
>
> | 实时时间戳 | 实时增量 | `DeltaSeconds` | `Elapsed` 增量 |
> |---|---|---|---|
> | 05:21:24:137 | — | 0.033336 | 47.067995 |
> | 05:21:25:805 | 1.668s | 0.033333 | +0.066666 |
> | 05:21:26:804 | 0.999s | 0.033333 | +0.033333 |
> | 05:21:27:805 | 1.001s | 0.033333 | +0.033333 |
>
> **换算**：实时每秒 3 帧（帧间隔 ≈0.333s），每帧战斗时间 0.033333s → 实时 1.0s = 战斗 **0.1s** → **实时:战斗 = 10:1** ✓ 与 `slomo 0.1`（`TimeDilation = 0.1`）严格一致——**`ScaledDt` 语义成立**（战斗时钟按时间膨胀缩放，而非按实时推进）。
>
> **pause 半边（实测）**：
>
> | 时间戳 | `DeltaSeconds` | `Elapsed` | Frame |
> |---|---|---|---|
> | 05:22:28:805 | **0.000000** | 49.601378 | 472 |
> | 05:22:32:471 | **0.000000** | **49.601378**（一字未变） | 483 |
>
> **跨 12 帧 / 3.7 秒实时，`Elapsed` 完全冻结**（`DeltaSeconds` 全 0），而 `Frame` 仍递增——正是 `:63` 的实现语义：**泵仍在走（帧号推进），但战斗时间冻结** ✓
>
> **副产物**：`WaitDelay` 的到期基准就是 `Clock.Elapsed`（`PushExpiry` 用它算 DueTime），故时钟冻结 ⇒ **到期队列同步冻结**（剧本原文要验的正是这条链：World 暂停/降速 → 到期队列同步冻结/减速，消费者 = 测试链的 WaitDelay 步骤）。
>
> **方法评价**：本方案零新代码、不需重启编辑器、精度足够（每帧一条日志）；代价是需人工对照时间戳。**比新写观测命令更划算**——后者要写码 + 真实编译 + 重启。将来若需自动化判定，再考虑把它做成 opt-in 命令。
>
> **⑤ Step 6 双配置编译——Shipping 照出 2 个真实缺陷（已修）**：两者都住 `Source/TcsDev/Private/Dev/TcsDevSliceRig.cpp`，**都是 Task 6 引入、Development 下不可能发现**（`WITH_EDITOR=1` 时成员存在）：
> - **`IsDataValid` 无 `WITH_EDITOR` 保护**（`:568`）→ Shipping 报 `C2737「必须初始化 const 对象」`——**真因被诊断掩盖**（`IsDataValid` 是 `#if WITH_EDITOR` 成员，Shipping 下不存在，调用点解析失败后被报成 const 未初始化）。修法：整块加 `#if WITH_EDITOR` + `#else` 屏显"检查 A 跳过"。
> - **`SetActorLabel` 无保护**（`:132`）→ 报 `C2039「不是 AActor 的成员」`。修法：加 `#if WITH_EDITOR`（只影响编辑器显示名，Shipping 跳过即可）。
> - **插件侧零问题**（三处 `IsDataValid` 定义都正确包在 `#if WITH_EDITOR` 内）。**教训**：Shipping 编译是**唯一**能照出"编辑器专用 API 泄漏"的检查——两轮 PIE 全绿也照不出。
>
> **⑥ 陈旧二进制的陷阱（首跑假 FAIL → 真实编译后全绿，值得记）**：首跑 7b 报 `[FAIL] 目标 Health 75.0 → 75.0`，**不是装置逻辑错、也不是被测对象错**——是**编辑器重启后加载的基线 DLL 比源码旧**。机制：`Live Coding` 的 patch（`*.patch_N.exe`）是**运行时内存补丁、不写回基线 DLL**（`Binaries/Win64/UnrealEditor-TcsDev.dll` 仍是 00:34 版），故**重启后 patch 作废、回到更旧的基线**；而"改命令注册（`FAutoConsoleCommand` → `WithWorldAndArgs`）"这类**静态初始化期改动 Live Coding 本就应用不了**（它只替换函数体）。**处置**：关闭编辑器 → 跑一次真实 UBT Development 编译（写回基线 DLL，`13:08` > 源码 `11:38`）→ 重启编辑器 → 重跑，**10/10 全绿**。**判据**：装置行为与源码不符时，先核对**基线 DLL 时间戳 vs 源码时间戳**，别急着怀疑逻辑——与 `MEM-20260826-03`（陈旧构建信号）同源。**要跑通须一次真实 UBT 编译**，Live Coding 不够。
