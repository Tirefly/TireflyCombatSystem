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
        FTcsEffectStep.h                       （FInstancedStruct 容器 + ETcsStepResult）
        FTcsEffectChain.h                      （Id/步骤数组/MaxStepsPerFrame 熔断）
        FTcsEffectContext.h                    （黑板：Caster/Instigator/EventPayload/Targets/Variables——属性捕获归 Damage 流 Context，见 Flow/）
        FTcsChainRun.h                         （池化运行态：PC/状态）
        FTcsEffectStepExecutor.h               （执行器签名 + 注册表 + UE_DEFINE_EFFECT_STEP_EXECUTOR 宏）
        ITcsEntityQuery.h                      （注入接口：机制层定义，宿主实现）
        UTcsEffectSubsystem.h                  （解释器门面）
        FTcsStepWaitDelay.h                    （WaitDelay 步骤 struct——Task 1 落地）
      Private/
        TcsEffectLogChannel.cpp                （DEFINE_LOG_CATEGORY(LogTcsEffect)）
        UTcsEffectSubsystem.cpp                （解释器 + WaitDelay 执行器 + **Context 默认目标初始化=事件目标**）
    TcsTargeting/
      TcsTargeting.Build.cs                    （根）
      TcsTargetingModule.h / .cpp              （根：模块类）
      Public/
        TcsTargetingLogChannel.h               （DECLARE_LOG_CATEGORY_EXTERN(LogTcsTargeting, Log, All)）
        FTcsTargetSelectorStrategy.h           （策略契约：Resolve 纯虚 USTRUCT——D4-4 v2 策略化；载体 D3-7 v3）
        FTcsSelSelf.h                          （默认策略 1：Context.Caster）
        FTcsSelEventTarget.h                   （默认策略 2：事件载荷目标）
        FTcsTargetFilterStrategy.h             （过滤契约：Pass 纯虚——存活/敌对语义宿主实现）
        FTcsStepSelectTargets.h                （链步骤：TInstancedStruct 策略 + Filter AND → 写 Context.Targets）
      Private/
        TcsTargetingLogChannel.cpp             （DEFINE_LOG_CATEGORY(LogTcsTargeting)）
        FTcsStepSelectTargets.cpp              （执行器）
    TcsDamage/
      TcsDamage.Build.cs                       （根）
      TcsDamageModule.h / .cpp                 （根：模块类）
      Public/
        TcsDamageLogChannel.h                  （DECLARE_LOG_CATEGORY_EXTERN(LogTcsDamage, Log, All)）
        Flow/FTcsDamageFlowContext.h           （三层值空间/分类 Tag/FlowSource/CapturedAttrs）
        Flow/FTcsFlowAttributes.h              （流程属性黑板：键+修正链+封闭运算）
        Flow/FTcsFlowTemplate.h                （有序步骤数组）
        Flow/FTcsFlowStepExecutor.h            （流程步骤注册表 + UE_DEFINE_FLOW_STEP_EXECUTOR 宏）
        Flow/Steps/（标准步骤库十 struct + FlowModify/FlowDelegate，各 .h）
        Flow/ITcsDamageFlowDelegate.h          （纯 C++ 接口：宿主实现公式）
        Flow/FTcsDamageRecord.h
        Chain/FTcsStepDamage.h                 （链步骤 Damage/Heal/ModifyFlow 三 struct 定义；**R3 只实现 Damage 执行器**，Heal/ModifyFlow 执行器后续轮）
        UTcsDamageSubsystem.h                  （流程解释器门面）
      Private/
        TcsDamageLogChannel.cpp                （DEFINE_LOG_CATEGORY(LogTcsDamage)）
        Flow/Steps/（标准步骤执行器 .cpp）
        Chain/FTcsStepDamage.cpp               （本计划实现 Damage 执行器）
        UTcsDamageSubsystem.cpp                （流程解释器 + 默认模板注册）
    TcsIntegration/
      TcsIntegration.Build.cs                  （根）
      TcsIntegrationModule.h / .cpp            （根：模块类）
      Public/
        TcsIntegrationLogChannel.h             （DECLARE_LOG_CATEGORY_EXTERN(LogTcsIntegration, Log, All)）
        Entity/UTcsCombatEntityComponent.h     （身份锚/查询门面/手动触发 API）
        Entity/UTcsEntityQuery.h               （ITcsEntityQuery 默认实现：PIE 枚举）
        UTcsDefLibrary.h                       （最小定义加载：链资产/流程模板注册）
      Private/
        TcsIntegrationLogChannel.cpp           （DEFINE_LOG_CATEGORY(LogTcsIntegration)）
        Entity/UTcsCombatEntityComponent.cpp
        Entity/UTcsEntityQuery.cpp
        UTcsDefLibrary.cpp
  Content/（PIE 测试地图 + 属性 DataTable + 测试链资产——执行期编辑器内建）
```

---

### Task 0: 四模块骨架

**Files:** 四个 Build.cs + 四组 `Tcs<名>Module.h/.cpp`（模块壳，不含日志）+ 四组日志通道 `Public/Tcs<名>LogChannel.h`（DECLARE_LOG_CATEGORY_EXTERN(LogTcsEffect/LogTcsTargeting/LogTcsDamage/LogTcsIntegration, Log, All)）+ `Private/Tcs<名>LogChannel.cpp`（DEFINE）+ .uplugin Modules 追加四模块（顺序 TcsEffect/TcsTargeting/TcsDamage/TcsIntegration）

- [ ] **Step 1:** 建 Build.cs（依赖边照 Global Constraints 最小编译集；TcsIntegration 加 Engine 子模块按需）
- [ ] **Step 2:** .uplugin Modules 列表追加四模块
- [ ] **Step 3:** 编译验证（空模块全绿）

---

### Task 1: TcsEffect——步骤/链/注册表/挂起协议

**Files:** `FTcsEffectStep.h / FTcsEffectChain.h / FTcsEffectContext.h / FTcsChainRun.h / FTcsEffectStepExecutor.h / ITcsEntityQuery.h / FTcsStepWaitDelay.h / UTcsEffectSubsystem.h(.cpp)`

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

- [ ] **Step 1: 数据结构四件**（如上；FTcsChainRun 池化走 TcsCore TTcsInstancePool）
- [ ] **Step 2: 注册表与宏**（`UE_DECLARE_EFFECT_STEP_EXECUTOR` 配对；注册键 = 步骤 struct 类名 FName）
- [ ] **Step 3: 解释器**（`RunFrom(FTcsChainRun&, 从 PC)`：逐步查注册表 → 执行 → Completed 前进 / Running 返回；MaxStepsPerFrame 熔断 ensure）
- [ ] **Step 4: WaitDelay 步骤**：`FTcsStepWaitDelay{double Seconds}`；执行器返回 Running 并 Push 到期堆（OwnerId=FTcsChainRunHandle，回调=时钟泵里唤醒重入）
- [ ] **Step 5: 编译验证**
- [ ] **Step 6: 人工检查**：PIE 起一条 `[WaitDelay 0.5]` 测试链 → 0.5s 后完成（Output Log `LogTcsEffect` 可见）；挂起期间无每帧重入成本

---

### Task 2: TcsTargeting——策略契约与默认实现

**Files:** `FTcsTargetSelectorStrategy.h / FTcsSelSelf.h / FTcsSelEventTarget.h / FTcsTargetFilterStrategy.h / FTcsStepSelectTargets.h(.cpp)`

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

- [ ] **Step 1: 抽象契约 + 两默认策略 + FTcsStepSelectTargets 执行器（宏注册；单体 = 策略 Resolve → Filter AND → 写 Context.Targets）**
- [ ] **Step 2: 编译验证**

---

### Task 3: TcsDamage——流程机制层（解释器/黑板/协议/裁决）

**Files:** `Flow/FTcsDamageFlowContext.h / Flow/FTcsFlowAttributes.h / Flow/FTcsFlowTemplate.h / Flow/FTcsFlowStepExecutor.h / UTcsDamageSubsystem.h(.cpp)`

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

- [ ] **Step 1: 三数据结构 + FTcsFlowAttributes**（键→修正链求值；Submit 带 SortKey/消耗策略）
- [ ] **Step 2: 流程解释器**（按模板顺序执行；步骤执行器注册表+宏；Context 池化）
- [ ] **Step 3: 收集事件协议**（步骤边界发事件——立即通道同步分发，响应链提交落黑板）
- [ ] **Step 4: 编译验证**

---

### Task 4: 标准步骤库 + 官方默认模板 + Damage 链步骤

**Files:** `Flow/Steps/（标准步骤库十 struct + FlowModify/FlowDelegate，各 .h，执行器分 .cpp）`、默认模板注册（Subsystem 初始化）、`Flow/ITcsDamageFlowDelegate.h / Flow/FTcsDamageRecord.h`、`Chain/FTcsStepDamage.h(.cpp)`

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

- [ ] **Step 1: 十标准步骤 struct+执行器**（宏注册；BaseDamage/Execute 为核心，其余最小实现——发事件/写黑板）
- [ ] **Step 2: FTcsFlowModify/FTcsFlowDelegate 数据步骤**（含 Conditions 求值挂点——R3 实现 HasAllTags/Chance）
- [ ] **Step 3: 默认模板注册**（Subsystem Initialize 组装并登记 Default）
- [ ] **Step 4: ITcsDamageFlowDelegate + FTcsStepDamage 链步骤执行器**（宏注册进 **Effect** 注册表——跨模块注册的第一个实证）
- [ ] **Step 5: FTcsDamageRecord**（Completed 步骤填充 → 总线 Damage.Record 立即事件 + 环形缓冲）
- [ ] **Step 6: 编译验证**

---

### Task 5: TcsIntegration——CombatEntity 接线与查询

**Files:** `Entity/UTcsCombatEntityComponent.h(.cpp) / Entity/UTcsEntityQuery.h(.cpp) / UTcsDefLibrary.h(.cpp)`

**Interfaces:**
- Consumes: 全前序模块。
- Produces:
```cpp
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UTcsCombatEntityComponent : public UActorComponent
	// 身份锚：RegisterUnit 到属性系统；查询门面：GetCurrent(FTcsAttributeName)/ApplyTestModifier(...)/RemoveTestSource(...)——D2-1 裸 FName 不进属性 API
	// 触发 API：ExecuteChainById(FName ChainId)（手动触发——R3 不做触发行）
	// 挂 ITcsEntityQuery 提供方（注册进 Effect 层注入点）
};
UCLASS() class UTcsEntityQuery : public ITcsEntityQuery   // PIE 枚举：遍历带该组件的 Actor，存活过滤
{
	virtual void EnumerateEntities(TFunctionRef<void(AActor*)>) override;
};
UCLASS() class UTcsDefLibrary : public UGameInstanceSubsystem   // 定义资产发现/登记（**加载职责边界**：DefLibrary 只做资产发现与注册，UTcsEffectSubsystem.LoadChainDefs 只消费已登记定义执行——链资产发现→DefLibrary，执行→Effect）
{
	virtual void OnDefLibraryReady();   // 单出口 OnReady（M6 双层引导的最小版）
};
```

- [ ] **Step 1: UTcsCombatEntityComponent**（三职责最小实现）
- [ ] **Step 2: UTcsEntityQuery**（ITcsEntityQuery 默认实现，注入 Effect 层）
- [ ] **Step 3: UTcsDefLibrary 最小版**（OnReady 单出口；链定义加载）
- [ ] **Step 4: 编译验证**

---

### Task 6: 测试装置 + PIE 地图（内容资产）

**Files:** `Content/R3_TestMap.umap`、测试单位 Actor BP（挂组件）、`属性表 R3_Attributes`（DataTable：Health/MaxHealth/Attack/Armor）、测试公式 delegate（**C++ 测试实现，落点 = `Source/TcsIntegration/Testing/` 测试装置目录**（plan1 Task 6 同款约定）：`max(1, Attack−Armor)`）、测试链资产（`WaitDelay 0.5 → SelectTargets(单体) → Damage`，纯数据）

- [ ] **Step 1: 属性 DataTable 4 行**（FName 键 + Base + Bounds——验 D2-1 数据行形态）
- [ ] **Step 2: 测试链资产**（链=纯数据资产——检查点 6 的"零 C++ 加链"实证主体）
- [ ] **Step 3: 测试公式 delegate + 2 个测试单位 + 地图**（编辑器内建，人工步骤；验收信号 = 测试装置订阅属性广播直调 `GEngine->AddOnScreenDebugMessage`）
- [ ] **Step 4: 编译 + 打开 PIE 就绪**

---

### Task 7: 端到端验收（检查点 1/5/6/7 + 日志附加）

- [ ] **Step 1: 检查点 1 数值**：触发链 → 屏显 `damage = max(1, Attack−Armor)`；Health clamp 到 0
- [ ] **Step 2: 检查点 5 铁律 grep**：反向 include 逐模块枚举——`Select-String -Path Source\TcsCore\* -Pattern 'TcsAttribute|TcsNotation|TcsEffect|TcsTargeting|TcsDamage|TcsIntegration'` 应零命中；同法抽查 TcsEffect 不得含 TcsDamage/TcsTargeting；实例数据文件无 TSubclassOf；依赖边对照 R0 §9 表（最小编译集口径）
- [ ] **Step 3: 检查点 6 数据驱动线**：复制测试链资产改 Damage 参数数值 → 触发生效，**零 C++ 改动**
- [ ] **Step 4: 检查点 7 ScaledDt**：`slomo 0.1` 下 WaitDelay 到期减速；pause 冻结（Output Log `LogTcsCore`/`LogTcsEffect` 时间戳对照）
- [ ] **Step 5: 日志附加**：UE 原生分类控制生效（`log LogTcsDamage Verbose` 开启流程追踪、恢复默认）；屏显仅测试装置产生（插件模块零屏显调用）
- [ ] **Step 6: 全量编译（Development + Shipping 配置各一次）→ 停点交付人工检查单全项**（提交经用户授权）
