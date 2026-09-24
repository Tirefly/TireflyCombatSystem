# Change: 脚本反射面（门面 API 反射化 + 运行态句柄反射化）

## Why

LAC 已接入 UnrealSharp（上游 main @ `46e9c2c1`，UE 5.8，`Script/LegendAutoChessCS` 实测热重载通过）。用户 2026-09-24 拍板：**要让项目成员（含策划）能用 C# 脚本编写技能/Buff/伤害流程逻辑**，动机三条（摆脱 C++ 编译 / 摆脱蓝图低扩展性 / 写全新语义），但不排斥 C++——"非常适合放在 TCS 插件基建或 LAC C++ 底层的，肯定没问题"。

`2026-09-23-csharp-tcs-logic-authoring-research.md` 用 **glue 产物实证**（直接读 TCS 已生成的 76 个 `.generated.cs`，非源码推断）得出分层结论：

| 层 | 结论 | 证据 |
|---|---|---|
| 数据面 | ✅ 通 | C# 可定义纯数据 USTRUCT、可组装 `IList<T>` 送进链 |
| **门面调用面** | ❌ **不通** | `TcsEffectSubsystem.generated.cs` **只有两个属性、零个方法**——`RegisterChain` / `ExecuteChain` / `RegisterTriggerRow` / `SetEntityQuery` 全部未导出 |
| 行为登记面 | ❌ 不通 | 三张注册表的 `Register` 形参是 `TFunction`（本提案**不做**，见非目标） |

**本提案只做"门面调用面"**（台账 S-1）——它零障碍、可独立验收，且是"能注册链 → 能起链 → 能查活性"最小闭环的全部内容。做完之后 C# 即可用现有原语拼链（`Damage`/`WaitDelay` 等），这已能支撑相当一部分技能/Buff 逻辑。

## What Changes

- **门面 API 反射化**（`UTcsEffectSubsystem` + `UTcsAttributeSubsystem` + `UTcsCombatEntityComponent`）：为"能被脚本调用"的方法补 `UFUNCTION()`，使 UnrealSharp 生成可调用的 C# 方法。
- **运行态句柄反射化 + 展平**（`FTcsChainRunHandle` / `FTcsEffectTriggerHandle`）：加 `USTRUCT()` **并把内部字段展平**为可 `UPROPERTY` 的 `Index`/`Generation`——这是实测修正（见下"实测发现"）。
- **新增脚本层起链入口** `ExecuteChainForCaster(FGameplayTag, FTcsCombatEntityHandle)`：`ExecuteChain` 的形参 `FTcsEffectContext` 是非反射 struct，无法加反射标记（UHT 报错）。
- **新增查询口** `IsChainRegistered(FGameplayTag)`：`FindChain` 返回裸 struct 指针，反射层表达不了。
- **实体注册面反射化**（`RegisterUnit` / `UnregisterUnit` / `AddAttribute` / `SetBaseValue` / `EvaluateCurrent` / `PeekPending`）：脚本层起链的**必要前置**——没有注册实体就没有合法 `Caster`（实测：硬编码句柄会触发"单位未注册" ensure）。
- 规格同步：`effect-chain` / `effect-interpreter` / `effect-trigger` 增补"反射面"要求。

**BREAKING**：无（纯增量——加反射标记、展平句柄内部字段；方法签名与既有语义均不变）。

## 实测发现（2026-09-24，首次 PIE 实测后修正）

**"能导出 ≠ 能往返"** —— 首轮实现按"`USTRUCT()` + 非 `UPROPERTY` 内部字段"的形态（照 `FTcsEffectTriggerHandle` 先例），实测暴露：

| 环节 | 实测结果 |
|---|---|
| 门面反射面 | ✅ **通**——C# 成功注册链、起链、链挂起、被到期堆唤醒、跑完（`UFunction::Invoke` 路径） |
| 句柄往返 | ❌ **不通**——`IsRunActive` 返回 false |

**根因**：`TTcsInstanceHandle<T>` 是**模板类型、无法作 `UPROPERTY`** ⇒ 绑定产物生成空壳（`ToNative`/`FromNative` 函数体为空）⇒ C# 接住句柄时读不到值、传回时写全零 ⇒ 池给 `{Index=0, Generation=1}` 而收到 `{0, 0}` ⇒ **代际失配判无效**。

**修法**：句柄**展平**为两个 `UPROPERTY int32` 字段（`Index`/`Generation`），经 `GetInner`/`SetInner` 与池句柄互转（无效值 `-1` 与 `0xFFFFFFFF` 位模式相同，往返无损）。先例 = `FTcsCombatEntityHandle`（同为展平的反射句柄）。

**同根因连带**：`FTcsEffectTriggerHandle` 是同一形态 ⇒ `RegisterTriggerRow` → `UnregisterTriggerRow` 的往返**同样是坏的**（此前未暴露，因 C# 根本调不到这些方法）。本提案同批修（同根因、同修法，分两次做等于同一件事做两遍）。

**教训（可复用）**：判"某类型能否被脚本往返使用"时，**不能只看它是否被导出**——必须检查绑定产物里 `ToNative`/`FromNative` 是否**有实际字段读写代码**。空壳类型能通过编译、能当类型用，但**值过不去**。

## 关键技术约束（已源码级核实）

### 约束 1：不能加 `BlueprintCallable`

UHT 会校验蓝图参数类型（`UhtFunction.cs:859` `checkForBlueprint = ...BlueprintCallable | BlueprintEvent`；`:1043-1053` 报 `Type '...' is not supported by blueprint`），而 TCS 的载体 struct 是 `USTRUCT()` **非 `BlueprintType`**（`FTcsEffectChain` / `FTcsEffectTriggerInstance` / `FTcsFlowTemplate`），不满足 `IsParameterSupportedByBlueprint`（`UhtStructProperty.cs:113`）。

⇒ **用 `UFUNCTION()` 无 specifier**（`checkForBlueprint` 为 false，UHT 不校验参数），**UnrealSharp 仍会导出**——实测先例：`TcsAsyncAction_ListenForCombatEvent.h:90` 的 `UFUNCTION()` `HandleBusEvent` 已生成完整 C# 调用代码。

**代价（有意接受）**：无 specifier 的 `UFUNCTION` **蓝图侧不可见**——符合 R0 §9"蓝图不承诺"。MUST 在头注释里写明这是**有意选择**而非漏写。

### 约束 2：C# 可见性由 `EFunctionFlags.Public` 决定

`FunctionExporter.DetermineProtectionMode()`（`:1207-1225`）：`Public | BlueprintCallable` → `public`；否则 `protected`/`private`。**实测**：`HandleBusEvent` 生成的是 **`private`**（它是 `UFUNCTION()` 无 specifier）。

⇒ TCS 门面方法**本就在 `public:` 区**，UHT 据此给 `EFunctionFlags.Public` ⇒ C# 侧生成 `public` 方法。**此点 MUST 在实现后用生成的 glue 实证**（读 `.generated.cs` 确认修饰符）。

### 约束 3：句柄只需 `USTRUCT()`，不加 `BlueprintType`

`FTcsChainRunHandle` 内部是 `TTcsInstanceHandle<FTcsChainRunTag>`（非 UPROPERTY 裸模板成员）——**先例 = `FTcsEffectTriggerHandle`**（`USTRUCT()` 包 `TTcsInstanceHandle<FTcsEffectTriggerTag>`，C# 侧生成可持有传递的空壳结构，实测已导出）。故照抄该形态即可，零新风险。

## 影响面

- **Affected specs**: `effect-chain`（MODIFIED × 1：链与步骤数据形状 → 补运行态句柄反射性）、`effect-interpreter`（MODIFIED × 1：链执行与池化运行态 → 补门面反射面）、`effect-trigger`（MODIFIED × 1：登记表 → 补门面反射面）
- **Affected code**:
  - `Source/TcsEffect/Public/Chain/TcsChainRun.h`（`FTcsChainRunHandle` 加 `USTRUCT()`）
  - `Source/TcsEffect/Public/TcsEffectSubsystem.h`（门面方法补 `UFUNCTION()`）
  - `Source/TcsIntegration/Public/Entity/TcsCombatEntityComponent.h`（`ExecuteChainById` 补 `UFUNCTION()`）
- **不改**：`Build.cs`（零新依赖边）；任何方法签名；任何既有语义。

## 关键决策与依据

| 决策 | 依据 |
|---|---|
| 用 `UFUNCTION()` 无 specifier 而非 `BlueprintCallable` | UHT 蓝图参数校验（约束 1）；蓝图本就不在承诺范围（R0 §9） |
| `FTcsChainRunHandle` 加 `USTRUCT()` 不加 `BlueprintType` | 先例 `FTcsEffectTriggerHandle` 已是该形态；句柄是运行期身份词，无需蓝图暴露 |
| 句柄内部字段**不加** `UPROPERTY` | 与 `FTcsEffectTriggerHandle.Inner` 一致（裸模板无法作 UPROPERTY）；C# 侧是可持有传递的空壳——**足以完成"接住句柄 → 传回查询"的闭环** |
| 本轮**不做** `ITcsEntityQuery` / `ITcsDamageFlowDelegate` 反射化 | 前者形参含 `TFunctionRef`（C 类，需换签名）；后者形参含非反射 `FTcsDamageFlowContext`（依赖上下文分层）——均属台账 S-4/S-5，独立评估 |

## 非目标

- **不做**三张注册表的反射注册入口（`Register` 换签名）——台账 S-2，`TFunction` 形参须先定反射委托形态，且"什么能被脚本提供"取决于上下文反射化方案（S-3）。
- **不做**上下文/运行态反射化（`FTcsEffectContext` / `FTcsChainRun` / `FTcsDamageFlowContext`）——台账 S-3。`FTcsDamageFlowContext` 深处嵌 `TFunction`（`FTcsConsumePolicy::OnConsumed`）⇒ **物理不可能直接反射化**，真前置是上下文/黑板分层设计。
- **不做** `TInstancedStruct<T>` 字段的 C# 导出缺口——台账 S-6（新发现）。该缺口使 `FTcsParamValue` / `FTcsStepSelectTargets` 等含策略位的 struct 在 C# 侧是空壳 ⇒ **本提案的实证范围据此收窄**（见下）。
- **不做** `ITcsEntityQuery`（S-5）/ `ITcsDamageFlowDelegate`（S-4）反射化。

## 验收方式（实证而非推断）

本提案的验收**不依赖"编译通过"**，而依赖 **C# 侧真跑通一条链**：

1. 编译通过（UBT，Development 配置）；
2. **读生成的 glue 确认**：`TcsEffectSubsystem.generated.cs` 出现 `public` 方法（验证约束 2）；`TcsChainRunHandle.generated.cs` 生成（验证约束 3）；
3. **C# 实证**：在 `Script/LegendAutoChessCS/` 写一个 C# 类，**注册一条链 → 起链 → 接住句柄 → 查活性**。链内容用 `FTcsStepDamage`（其字段全反射可配）+ `FTcsStepWaitDelay`（`double Seconds` 可配）——**不用 `FTcsStepSelectTargets`**（它含 `TInstancedStruct` 字段，C# 侧是空壳，见 S-6）；
4. 编辑器实测：热重载后该 C# 逻辑仍可调用（验证注册面不因重载失效——门面 API 是 UObject 方法，天然稳定）。

**第 3 步的收窄说明**：原计划"三步骤链（选目标→伤害→等待→伤害）"因 `FTcsStepSelectTargets` 在 C# 侧是空壳而**不可实现**。改用"`Damage` + `WaitDelay`"仍能证明门面通道打通（注册→起链→句柄→活性），只是链的内容受 S-6 限制。**S-6 是比本提案更靠前的脚本化障碍**，已在台账登记。
