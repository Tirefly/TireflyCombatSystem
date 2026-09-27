# Change: 宿主脚本插槽（三类扩展点的反射可达化）

## Why

2026-09-24，用户提出：宿主项目的客制化伤害流程、TargetSelector 等语义不值得进入 TCS 插件 C++，是否可以由 C#/AS/Luau/TS/蓝图实现？

现有扩展点使用虚分派或 `TFunction`，脚本类型没有 C++ vtable，注册表值也无法反射；直接暴露原签名会导致脚本实现不可达。TCS 因此提供基于 UE 原生反射（`UObject` / `UFUNCTION(BlueprintNativeEvent)`）的语言无关插槽，保留 C++ 内置快路径，同时把宿主专属语义留给脚本。

实现必须避开不可反射上下文：插槽传反射句柄/视图，运行态经门面按句柄访问器读写；非反射 `FTcsDamageFlowContext` 不能直接作为 `UFUNCTION` 形参，插槽签名所用句柄必须为 `BlueprintType`。

## What Changes

### ① 目标选择 / 过滤插槽（虚分派 → 反射分派）

- 新增 `TcsTargeting/Public/Host/TcsTargetSelectorHost.h` / `TcsTargetFilterHost.h`：`UINTERFACE` + `BlueprintNativeEvent`
  - `ITcsTargetSelectorHost::ResolveTargets(FTcsCombatEntityHandle Caster, FTcsCombatEntityHandle Instigator, TArray<FTcsCombatEntityHandle>& OutTargets)`
  - `ITcsTargetFilterHost::PassTarget(FTcsCombatEntityHandle Candidate, FTcsCombatEntityHandle Caster, FTcsCombatEntityHandle Instigator) -> bool`
- 新增策略子类（USTRUCT 转发器，**持 `TScriptInterface` 转发**——先例 `FTcsFlowDelegate` 步骤已持 `TScriptInterface<ITcsDamageFlowDelegate>`）：
  - `FTcsSelHostDelegate : FTcsTargetSelectorStrategy`（持 `TScriptInterface<ITcsTargetSelectorHost>`）
  - `FTcsFilterHostDelegate : FTcsTargetFilterStrategy`（持 `TScriptInterface<ITcsTargetFilterHost>`）
- 语义：转发器**只转发**——框架默认实现（`FTcsSelSelf`）与 C++ 子类策略保持原样（**双轨并存**）。

### ② 步骤执行器插槽（`TFunction` → UObject 基类）

- 新增 `UTcsStepExecutor : UObject`（`Abstract, Blueprintable`，住 `TcsEffect/Public/Chain/`）：
  - `UFUNCTION(BlueprintNativeEvent) ETcsStepResult Execute(FGameplayTag ChainId, FTcsChainRunHandle Run, const FInstancedStruct& StepData)`
- `ETcsStepResult` 升格 `UENUM()`（作反射返回类型的必要条件）。
- `FTcsEffectStepExecutorRegistry` 新增**反射注册入口**（经门面）：`UTcsEffectSubsystem::RegisterStepExecutor(UScriptStruct* StepStruct, UTcsStepExecutor* Executor)`——内部包成 `TFunction` 转发进既有注册表（**键与查表逻辑零改动**）。
- **GC 可见持有**：门面以 `UPROPERTY TArray<TObjectPtr<UTcsStepExecutor>>` 持有已登记执行器（T-8 教训同款：裸注册表持不住对象引用 ⇒ 静默回收 ⇒ 表现为"步骤不生效"）。

### ③ 伤害流程插槽（自定义伤害流程）

- 新增 `FTcsDamageFlowContextView`（`USTRUCT(BlueprintType)`，**反射只读视图**）——把 `FTcsDamageFlowContext` 中**可反射的数据面**摘出：参与者句柄、`FormulaParams`、`ClassificationTags`、请求字段（`BaseDamageInput` / `TargetAttrKey`）；**不含**黑板（含 `TFunction` 的消耗策略）、不含 `Owner` 弱引用（宿主经自身 `GetWorld()` 取门面）。
- `ITcsDamageFlowDelegate` 5 方法**换签名**（`const FTcsDamageFlowContext&` → `const FTcsDamageFlowContextView&`）+ 加 `UFUNCTION(BlueprintNativeEvent)` + 声明 `_Implementation`；C++ 调用点改 `Execute_*`。
- `UTcsDamageSubsystem` 注册面反射化：`RegisterTemplate` / `UnregisterTemplate` 加 `UFUNCTION()`（无 specifier，同 S-1 口径）。

### ④ 门面按句柄访问器（①/② 的前置，也是"绕开 S-3"的关键）

`UTcsEffectSubsystem` 新增（全部 `UFUNCTION()` 无 specifier）：

| 方法 | 作用 |
|---|---|
| `TArray<FTcsCombatEntityHandle> GetRunTargets(FTcsChainRunHandle)` | 读运行态目标集 |
| `void SetRunTargets(FTcsChainRunHandle, const TArray<FTcsCombatEntityHandle>&)` | 写运行态目标集 |
| `bool TryGetRunVariable(FTcsChainRunHandle, FGameplayTag Key, double& OutValue)` | 读链内变量 |
| `void SetRunVariable(FTcsChainRunHandle, FGameplayTag Key, double Value)` | 写链内变量 |
| `FTcsCombatEntityHandle GetRunCaster(FTcsChainRunHandle)` / `GetRunInstigator(FTcsChainRunHandle)` | 读主体句柄 |

**手法 = 传句柄、不传上下文** ⇒ **对"宿主专属语义脚本化"这一目标，上下文反射化（台账 S-3）不再是必经之路**（S-3 降为可选）。

### ⑤ 句柄可反射性放宽（**MODIFIED 既有规格**）

`FTcsChainRunHandle` 由 `USTRUCT()` 改为 **`USTRUCT(BlueprintType)`**（`FTcsEffectTriggerHandle` 同批）。

**理由（三重）**：
1. **机制要求**：插槽走 `BlueprintNativeEvent`，UHT 强制所有形参蓝图可表达（约束 2）——句柄是插槽签名的组成部分，非 `BlueprintType` 则插槽编译不过；
2. **先例一致**：`FTcsCombatEntityHandle` 本就是 `USTRUCT(BlueprintType)`（同为零内嵌的展平句柄），两条句柄分道扬镳无技术依据；
3. **不扩大承诺面**：原"MUST NOT"的顾虑是"成为 `BlueprintCallable` 的合法形参 ⇒ 意外扩大蓝图承诺面"——但**消费句柄的门面方法仍是 `UFUNCTION()` 无 specifier（蓝图不可见）**，蓝图能"看见类型"却**无任何可调用的门面**。插槽接口确实蓝图可实现，但那是 R0 §9 已接受的"恰好蓝图也能用"（台账 S-8 原文），不是新增承诺。

### ⑥ 附带修正：规格漂移

`effect-interpreter` 的门面反射面需求中"`FTcsChainRunHandle` MUST 为 `USTRUCT()`——内部 `TTcsInstanceHandle` 字段不加 `UPROPERTY`"一句是**S-1 展平前的旧文本**，与已落地的"两字段皆 `UPROPERTY`"矛盾 ⇒ 同批订正。

---

## 兼容性

| 面 | 影响 | 处置 |
|---|---|---|
| **C++ 宿主实现 `ITcsDamageFlowDelegate`** | **BREAKING**（签名换型） | 机械修复：`UTcsDevDamageFormula` 覆写改签名；取门面由 `Context.Owner` 改自身 `GetWorld()`（3 行） |
| **资产（链/模板）** | **无影响** | 改的是 C++ 接口签名与新增类型，不碰任何已序列化字段 |
| **既有注册表 / 解释器** | **无影响** | 键（`UScriptStruct*`）与查表逻辑零改动；`TFunction` 执行器路径原样保留（**双轨**） |
| **C++ 策略子类（`FTcsSelSelf` 等）** | **无影响** | 基类契约与虚分派原样保留，只**新增**转发子类 |

---

## 影响面

- **Affected specs**：`damage-primitive`（MODIFIED × 1）、`targeting-strategy`（ADDED × 2）、`effect-step-dispatch`（ADDED × 1）、`effect-chain`（MODIFIED × 1）、`effect-interpreter`（MODIFIED × 1）、`entity-query-contract`（ADDED × 1，见下）
- **Affected code**：
  - `TcsCore`：`Handle/TcsChainRun.h`（句柄 `BlueprintType`）；`Parameter/TcsParamValueSource.h`（如需）
  - `TcsEffect`：`Chain/TcsChainRun.h`、`Chain/TcsStepExecutor.h`（新增 UObject 基类）、`TcsEffectSubsystem.h/.cpp`（访问器 + 注册口 + GC 持有）、`Chain/TcsEffectStep.h`（`ETcsStepResult` 升格 `UENUM`）
  - `TcsTargeting`：新增 `Host/` 两接口 + 两转发策略；`Chain/TcsStepSelectTargets.cpp`（转发器的调用点）
  - `TcsDamage`：`Flow/TcsDamageFlowContextView.h`（新）、`Flow/TcsDamageFlowDelegate.h`（换签名 + 反射标记）、`TcsDamageSubsystem.h/.cpp`（注册面反射化）、`Private/Flow/Steps/TcsFlowStepsCore.cpp` / `TcsFlowStepsRest.cpp`（5 处调用点改 `Execute_`）
  - `TcsDev`（LAC 侧）：`Dev/TcsDevDamageFormula.h/.cpp`（覆写签名 + 取门面方式）
- **不改**：资产文件；`Build.cs`（无新模块依赖）；注册表内部实现

---

## 非目标

- **不做**上下文/运行态整体反射化（台账 S-3）——本提案的"传句柄 + 反射视图"手法使其**降为可选**；仅在"脚本必须直接持有/构造上下文 struct"时才有必要（届时须先做上下文/黑板分层）。
- **不做** `ITcsEntityQuery` 换签名（台账 S-5）——本提案的 `GetRunTargets` 等访问器已覆盖"脚本读/写目标集"这一主要诉求；实体遍历（`EnumerateEntities`）的 `TFunctionRef` 换型另轮。
- **不做** `TInstancedStruct` 相关（S-6 已消费）；**不做**注册表 `TFunction` 签名换型（台账 S-2——本提案的 ② 已给出更省的替代路径，S-2 维持"定位降级"）。
- **不内嵌任何脚本引擎**（D4-17 不动）：插槽只提供 UE 反射面的"插座"，语言选择归宿主。
- **不承诺蓝图支持**（R0 §9）：插槽的 `BlueprintNativeEvent` 是**机制选择**（UE 反射是唯一语言无关的分发底座），蓝图"恰好也能用"但不作为设计目标、不进验收。

---

## 验收方式

1. **编译**：UBT Development 通过（UHT 是主判据——插槽签名是否合法由它裁决）；
2. **glue 产物实测**（核心判据，**"能导出 ≠ 能往返"** 纪律）：
   - `TcsDamageFlowDelegate.generated.cs`：5 方法生成**真实方法体**（含 `CallGetNativeFunctionFromClassAndName` / 参数偏移），**非空壳**（换型前是零方法空壳，已实测存档）；
   - `TcsTargetSelectorHost.generated.cs` / `TcsTargetFilterHost.generated.cs` / `TcsStepExecutor.generated.cs`：方法可覆写（`FunctionFlags.BlueprintEvent`）；
   - `TcsEffectSubsystem.generated.cs`：5 个访问器为 `public`；
3. **端到端 PIE 实证**（LAC 侧 `TcsChainProbe` 扩展）：
   - **C# 实现 `ITcsDamageFlowDelegate::CalculateBaseDamage`** → 注册进流程模板 → 链步骤触发 → 断言伤害值走了 C# 公式（**这是"自定义伤害流程可用 C# 写"的唯一硬证据**）；
   - C# 侧读 `GetRunTargets` 确认访问器往返成立；
4. **零红字**：日志无 ensure / Error（配置错误除外——验收路径不得含配置错误）。

---

## 实施顺序（用户 2026-09-24 认可）

1. **③ 伤害流程**——最便宜且流程侧 12 执行器已全实现；
2. **④ 门面访问器**——①/② 的前置；
3. **① 选择器/过滤器**；
4. **② 步骤执行器**（连带 `ETcsStepResult` 升格与 GC 持有）。
