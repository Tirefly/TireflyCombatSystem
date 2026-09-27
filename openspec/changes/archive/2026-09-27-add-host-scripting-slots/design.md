# Design: 宿主脚本插槽

## Context

TCS 的扩展点当前有三种分发机制，它们的**脚本可达性完全不同**（2026-09-24 源码级核实，台账 S-8）：

| 机制 | 用于 | 脚本可达 | 物理原因 |
|---|---|---|---|
| 虚分派（vtable） | `FTcsTargetSelectorStrategy` / `FTcsTargetFilterStrategy` / `FTcsParamValueSource` | ❌ 不可达 | 脚本定义的 struct **没有 C++ 类型** ⇒ `CppStructOps == nullptr` ⇒ vtable 指针位为 0 ⇒ 调用即野函数指针 |
| 注册表 + `TFunction` | 步骤执行器 / 条件求值器 / 载荷读取器 | ⚠️ 差一层签名 | 键（`UScriptStruct*`）可反射，值（`TFunction`）不可 |
| UObject + `BlueprintNativeEvent` | `UTcsEventHandler` / `ITcsAttributeProvider` | ✅ 可达（已实测） | UE 原生反射分发（`UFunction::Invoke`） |

**关键认识**：第三类是 **UE 自己的反射系统，不是某个脚本语言专属**——AngelScript / Luau / Puerts(TS) / 蓝图全部支持 ⇒ 建在它上面的插槽**天然语言无关**，正是 D4-17"语言无关执行器"要的底座。

**用户质询驱动**（2026-09-24）：客制化、只服务宿主业务、不值得进插件的语义（伤害流程、TargetSelector），是否也必须写 C++？——本设计是对该问题的回答。

### 两条硬约束（决定所有插槽签名）

**约束 1：非反射 struct 不能作 `UFUNCTION` 形参**

`FTcsDamageFlowContext`（`TcsDamageFlowContext.h:28`）与 `FTcsEffectContext`（`TcsEffectContext.h:24`）都是**纯 C++ struct**（无 `USTRUCT`）。出现在 `UFUNCTION` 签名里 ⇒ UHT 报 `Unable to find 'struct' with name ...`。
既有实证：`UTcsEffectSubsystem::ExecuteChain` 正因此**无法**标记，只能另开 `ExecuteChainForCaster`。

**约束 2：`BlueprintNativeEvent` 触发蓝图参数校验**

`BlueprintNativeEvent` 含 `EFunctionFlags.BlueprintEvent` ⇒ UHT 走 `checkForBlueprint`（`UhtFunction.cs:859`）⇒ 逐参数要求 `IsParameterSupportedByBlueprint`（`UhtFunction.cs:1043-1053`）；非 `BlueprintType` 的 USTRUCT 拿不到该 cap（`UhtStructProperty.cs:110-121`）。

**约束 2 的连带**：`FTcsChainRunHandle` 当前是 `USTRUCT()` 无 `BlueprintType`（`effect-chain` 规格明文"MUST NOT 加"）⇒ 要作插槽形参必须先放宽该条。

## Goals / Non-Goals

**Goals**

1. 三类扩展点（目标选择/过滤、步骤执行器、伤害流程）**宿主用任意 UE 脚本语言可扩展，零 C++ 改动**；
2. **不改既有分发结构**——C++ 快路径（虚分派 / 自注册宏 / `TFunction`）原样保留，插槽只做加法；
3. 避开"上下文反射化"这块最贵的骨头（台账 S-3 因此降为可选）；
4. 机制选择**不扩大蓝图承诺面**（R0 §9 不动）。

**Non-Goals**

1. 不做上下文 / 运行态整体反射化（S-3）——仅"脚本必须直接持有/构造上下文 struct"时才需要；
2. 不做 `ITcsEntityQuery` 换签名（S-5）——访问器已覆盖"读写目标集"主诉求；`EnumerateEntities` 的 `TFunctionRef` 换型另轮；
3. 不内嵌任何脚本引擎（D4-17 不动）；
4. 不把框架内置步骤/策略改写成 UObject 形式（**双轨并存**，不是替换）。

## Decisions

### D1：目标选择/过滤 —— 用"USTRUCT 转发器 → UObject 反射接口"，而不是改基类分派

- **决策**：新增 `ITcsTargetSelectorHost` / `ITcsTargetFilterHost`（`UINTERFACE + Blueprintable + BlueprintNativeEvent`），配套新增 `FTcsSelHostDelegate` / `FTcsFilterHostDelegate`（`FTcsTargetSelectorStrategy` / `FTcsTargetFilterStrategy` 的子类，持 `TScriptInterface` 转发）。
- **理由**：虚分派对脚本类型**物理不可达**（vtable 指针位为 0，引擎层面无解）——转发是**唯一出路**，不是偏好。
- **Alternatives considered**：
  - *把基类改成 `UObject`*——会动全部既有 C++ 策略子类与 `FInstancedStruct` 配置面（资产形状变化），代价远超收益；且内置策略走 UObject 会引入反射分派开销（热路径）。
  - *要求宿主写 C++ 策略子类*（现状）——正是用户质询要解决的痛点，未解决。

### D2：步骤执行器 —— 用 UObject 基类替代 `TFunction` 作注册值，而不是换 `TFunction` 签名（S-2）

- **决策**：新增 `UTcsStepExecutor : UObject`（`BlueprintNativeEvent Execute`）；门面提供 `RegisterStepExecutor(UScriptStruct*, UTcsStepExecutor*)`，内部包成 `TFunction` 转发进既有注册表。
- **理由**：**不动注册表结构、不动 C++ 快路径**。键（`UScriptStruct*`）本就可反射，缺的只是"值"那一层——UObject 基类补上它，改动面最小。
- **Alternatives considered**：
  - *把 `Register` 的 `TFunction` 形参换成反射可见委托*（台账 S-2 原方案）——同样可达，但要动三张注册表（步骤/条件/载荷读取器）的签名，且**条件求值器与载荷读取器的消费者都是热路径**；UObject 路线可覆盖主要场景，故 S-2 维持"定位降级"。
  - *要求宿主写 C++ 执行器*（现状）——同上，痛点未解决。
- **连带**：`ETcsStepResult` MUST 升格 `UENUM()`（作 `BlueprintNativeEvent` 返回类型，非反射枚举拿不到 blueprint cap）。

### D3：伤害流程 —— 5 方法换签名 + 上下文投影为反射视图，而不是整体反射化上下文

- **决策**：新增 `FTcsDamageFlowContextView`（`USTRUCT(BlueprintType)`）——从 `FTcsDamageFlowContext` **单向投影**可反射数据面（参与者句柄 / `FormulaParams` / `ClassificationTags` / 请求字段）；5 个 delegate 方法形参由 `const FTcsDamageFlowContext&` 换为该视图 + 加 `UFUNCTION(BlueprintNativeEvent)` + 声明 `_Implementation`；C++ 调用点改 `Execute_`。
- **理由**：视图只摘可反射面，**成本比整体反射化低一个数量级**，且满足插槽的全部读取需求。
- **Alternatives considered**：
  - *把 `FTcsDamageFlowContext` 整体反射化*——**物理不可**：其黑板 `FTcsFlowAttributes` → `FTcsFlowAttributeSubmit` → `FTcsConsumePolicy::OnConsumed`（`TFunction<void()>`）是递归闭包（`TcsFlowAttributes.h:44-46` 自注、UHT 实证）。真前置是"上下文/黑板分层"（台账 S-3），另轮。
  - *保留 `const FTcsDamageFlowContext&` 形参、只加宏*（台账 S-8 的原始设想）——**做不到**（约束 1）。
- **视图不含 `Owner` 的理由**：`TWeakObjectPtr` 非脚本友好，且宿主脚本实现本身就是 UObject，可经自身 `GetWorld()` 取门面（LAC 侧 `UTcsDevDamageFormula` 的机械修复即此路）。
- **`_Implementation` 声明的作用**：UHT 检测到接口类内的声明则**不生成默认 stub**（`UhtFunction.cs:681` 的 `ImplFound`）⇒ 中性默认实现住接口声明处，"普通项目零 delegate"（PV-7）不受影响。同款先例 = 引擎 `ISequencerAnimationOverride`（`SequencerAnimationOverride.h:31-44`）。

### D4：门面按句柄访问器 —— "传句柄、不传上下文"

- **决策**：`UTcsEffectSubsystem` 新增 6 个访问器（`GetRunTargets` / `SetRunTargets` / `TryGetRunVariable` / `SetRunVariable` / `GetRunCaster` / `GetRunInstigator`），全部 `UFUNCTION()` 无 specifier。
- **理由**：**这是绕开 S-3 的关键**——插槽签名只需句柄（`BlueprintType` 标量容器），上下文经访问器按需读写。脚本**永远不需要持有 `FTcsEffectContext`**。
- **Alternatives considered**：
  - *把 `FTcsEffectContext` 反射化*——它含 `FInstancedStruct EventPayload` 与 `TMap` 变量表，反射化本身可行，但 `effect-chain/spec.md:63` 明文"MUST NOT 为反射类型（纯运行态，非配置数据）"，须同批修订；且更贵的 `FTcsDamageFlowContext` 仍需视图路线。**访问器路线一举两得**。

### D5：`FTcsChainRunHandle` 放宽为 `BlueprintType`（MODIFIED 既有规格）

- **决策**：`USTRUCT()` → `USTRUCT(BlueprintType)`（`FTcsEffectTriggerHandle` 同批）。
- **理由**：约束 2 要求插槽形参蓝图可表达。**放宽的承诺面代价为零**——消费句柄的门面方法仍为无 specifier（蓝图不可见）；蓝图能"看见类型"却无任何可调用的门面。插槽接口确实蓝图可实现，但那是 R0 §9 已接受的"恰好蓝图也能用"，不是新增承诺。
- **先例**：`FTcsCombatEntityHandle` 本就是 `USTRUCT(BlueprintType)`（同为展平句柄）——两条句柄分道扬镳无技术依据。

## Risks / Trade-offs

| 风险 | 影响 | 缓解 |
|---|---|---|
| **`Execute_` 比虚表直调慢** | 插槽路径每次调用走 `FindFunction` + `ProcessEvent` | **双轨并存**：内置策略/步骤仍走 C++ 快路径，插槽只给宿主扩展（非热路径）。这是设计取舍，非缺陷 |
| **`ITcsDamageFlowDelegate` 换签名 = BREAKING** | C++ 宿主实现需机械修复 | 本仓唯一实现是 LAC 侧 `UTcsDevDamageFormula`（测试装置），3 行改动；设计文档与规格同批订正 |
| **脚本执行器被 GC 回收** | 表现为"步骤不生效"而非崩溃（极难排查） | 门面 MUST 以 `UPROPERTY` 持有（T-8 教训的正面应用，已写入规格） |
| **`Blueprintable` 让蓝图也能实现插槽** | 表面像"扩大蓝图承诺面" | R0 §9 不动：插槽是**机制选择**（UE 反射是唯一语言无关底座），蓝图"恰好也能用"但不作为设计目标、不进验收 |
| **转发器多一层间接** | 配置面多一个类型（`FTcsSelHostDelegate` vs 直接配脚本类） | 不可避免（虚分派物理约束）；转发器可被编辑器 picker 选中，配置体验与普通策略一致 |
| **视图与原上下文可能漂移** | 新增字段时忘了同步视图 | 视图是**只读投影**、无反向写回（无双真相）；M8 校验矩阵可加"视图字段覆盖度"检查 |
| **AS/Luau/TS 未经实测** | 本提案只实测 C# | 机制上同为 UE 反射底座，理论上同样支持；**各自需实测确认**（已写入台账口径） |

## Migration Plan

**顺序**（用户 2026-09-24 认可）：③ 伤害流程（最便宜，流程侧 12 执行器已全实现）→ ④ 门面访问器（①/② 前置）→ ① 选择器/过滤 → ② 步骤执行器。

**每步可独立编译、独立验证**（无跨步的半成品状态）。

**回滚**：全部改动是**加法 + 一处签名换型**。加法部分回滚 = 删新增文件；换型部分回滚 = 还原 5 处签名与调用点（`TcsDevDamageFormula` 同批还原）。

**验收门**（顺序不可颠倒）：

1. UBT Development 编译通过（**UHT 是插槽签名合法性的主判据**）；
2. **glue 产物实测**——`TcsDamageFlowDelegate.generated.cs` 从"零方法空壳"变为真实方法体（换型前已存档对照）；各插槽接口方法可覆写；门面方法为 `public`。**"能导出 ≠ 能往返"**纪律：产物非空只是必要条件；
3. **端到端 PIE**：C# 实现 `CalculateBaseDamage` 生效（**唯一硬证据**）+ 访问器往返成立；
4. 零红字。

## Open Questions

1. **AS / Luau / TS 实测**——本提案只验 C#。机制上共享 UE 反射底座，但各自绑定层质量未知（尤以"接口实现"这一能力为要点）。建议列为后续独立调研（不影响本提案落地）。
2. **`ITcsEntityQuery` 的 `EnumerateEntities` 换型**（S-5 残余）——脚本"遍历全世界"的诉求未覆盖；本提案只给"读写本次运行的目标集"。待真实需求出现时再评估（形态：`TFunctionRef` → 反射回调接口或"返回句柄数组"）。
3. **视图字段覆盖度校验**——是否入 M8 校验矩阵（归 R8-1）；与 S-6 的 `BaseStruct` metadata 校验同族。
