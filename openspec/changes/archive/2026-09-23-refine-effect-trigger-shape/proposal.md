# Change: 触发行形态精修（分层 / 改名 / 删 Cues / 条件注册表）

## Why

`add-effect-trigger-row`（2026-09-23 归档）落下的触发行形状有四处需要修正，其中两处是用户审阅后指出的**架构问题**、一处是**调研结论推翻了原实现**：

1. **配置与运行期混在一个 struct 里**（用户指出）：`FTcsTriggerRow` 同时装"能进资产的配置"（`EventTag`/`Conditions`/…）与"绝对不能进资产的运行期簿记"（`Source` —— 运行期发号的句柄，跨会话/跨机不同）。**一个 struct 里混了两类字段，让"它能不能进资产"没有干净答案。**
2. **`Effects` 名字不直观**（用户指出）：它就是一个链 id，应叫 `EffectChainId`。
3. **`Cues` 字段无消费者**（用户指出）：TcsCue 模块整体未敲定（`Source/TcsCue` 不存在），留一个填了没有任何消费者的字段 = 给策划一个假控件（与同批删除的修正器族死字段 `Tag` 同款理由）。
4. **条件求值走硬编码 if-else，且形态选错了**：原实现是 `EvaluateTriggerConditions` 里逐个 `GetPtr<具体类型>` 的 if-else 链——**宿主新增条件类型必须改插件源码**。更严重的是：我在评审时曾建议"改成 USTRUCT 虚分派基类"，而 `2026-09-23-scripting-language-ustruct-research.md` §5–§6 给出引擎级论证**推翻了这个方向**——虚分派依赖 vtable，vtable 来自 UHT 为 C++ 类型生成的 `TCppStructOps<T>`；C# 定义的结构体没有 C++ 类型 → `CppStructOps == nullptr` → 实例内存 `Memzero` 起步、vtable 指针位为 0 → 调用即**野调用崩溃**。故虚分派在脚本侧**物理不可达**，而**注册表分派可达**（§6："步骤系统已经用注册表分派 → 只要补一个反射可达的注册入口，C# 就能注册执行器"）。

## What Changes

- `effect-trigger` 的「触发行数据形状」需求 → **MODIFIED**：改为**定义/运行期分层**（`FTcsEffectTriggerDef` + `FTcsEffectTriggerInstance`），`Effects` → `EffectChainId`，删 `Cues`，`Source` 移入运行期侧。
- `effect-trigger` 的「触发条件最小集」需求 → **MODIFIED**：条件求值改为**条件求值器注册表**（`FTcsTriggerConditionRegistry` + 自注册宏），内置条件也走同一注册表。
- 新增「触发行实例与级联退订」需求：运行期形态的字段与 `Source` 语义（含"施加状态时 Source = 状态实例句柄"这条 Buff 行为机制）。

## 影响面

- **Affected specs**: `effect-trigger`（MODIFIED × 2 + ADDED × 1）
- **Affected code**:
  - 删：`Public/Trigger/TcsTriggerRow.h`、`Public/Trigger/TcsTriggerConditions.h`、`Private/Trigger/TcsTriggerConditions.cpp`
  - 新：`Public/Trigger/TcsEffectTrigger.h`（`FTcsEffectTriggerDef` + `ETcsExecutionGate`）
  - 新：`Public/Trigger/TcsEffectTriggerInstance.h`（`FTcsEffectTriggerInstance` + `FTcsEffectTriggerHandle`）
  - 新：`Public/Trigger/TcsTriggerCondition.h` + `Private/Trigger/TcsTriggerCondition.cpp`（注册表 + 内置条件 + 求值助手）
- **不改**：`TcsEffect.Build.cs`（零新依赖边）；既有链/解释器/注册表。

## 关键决策与依据

| 决策 | 依据 |
|---|---|
| 条件走**注册表**而非虚分派基类 | CS 调研 §5–§6（虚分派脚本侧物理不可达）；且与步骤执行器同构（TCS 内部本有两套分派机制，条件归注册表侧不再加深分裂） |
| 内置条件也走同一注册表 | **不分内外两套路径**——两套路径必然导致行为分歧（一处改了另一处忘） |
| `Def` / `Instance` 分层 | 配置 vs 运行期簿记是两类字段；与既有 Def 双轨制同款分工 |
| 命名 `FTcsEffectTriggerDef` / `FTcsEffectTriggerInstance` | 用户拍板"参考已有几个 Def 的命名规则"；与 `UTcsEffectChainDef` / `FTcsAttributeDefData` 族内一致 |
| 删 `Cues`（保留 `InterruptPriority`） | 用户只点名删 `Cues`；`InterruptPriority` 是 plan3 已接受的"只存不裁"字段 |

## 非目标

- **不做**条件求值器的**反射注册入口**（`TFunction` 不可反射）：该欠账与步骤执行器注册表**同批**解决（CS 调研 §7.6 的 G-2），不在此处单独开一个反射入口（否则两处口径不一）。
- **不做** `FTcsTriggerContext` 的扩展机制（PV-1 式结构体继承）：今天零消费者；Buff 生命周期参数（`StateHandle`/`Stacks`/`Level`）要等 M3 落地才有真实数据源。
- **不做**触发行的资产载体（`UTcsEffectTriggerDef`）与 DefLibrary 发现路径：载体是独立一块（含 DefLibrary 扩展），单列 Task。
