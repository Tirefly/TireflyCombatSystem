# 领域草稿 · Action（原 Effect）

> **族外草稿，不是实现依据。** 领域层正式文档尚未开始；本文收录 Action 域开始前已达成的方向共识，供开正式文档时**逐条裁决后**搬入。**未列入本文的都不算已裁决。**
>
> 来源：原 `PhaseB-前置裁决.md`（2026-11-04 按域拆分）。文中的 ✅ 表示「讨论中已由用户确认」，**不等于已进台账**——升入族内前仍须逐条走审核清单。
>
> **范围**：本文只装 Action 域自身的机制。跨全族的词汇纪律已升族内，见 [`03-词汇与命名.md`](../tcs-remake-v2/03-词汇与命名.md)；已进族内的内容见下文就地标注。
>
> **跨域提示**：这套设计是在「实例要不要 UObject」的讨论中长出来的，原文记为**牵动「状态域 / 效果域 / 技能域」三份文档的形状**（「效果域」即今日的 Action 域）。故 State 与 Skill 两域开文档时必须回看这里，尤其是 §2.3（步骤对象 per-run 实例化）与 §3（实例化策略）——它们决定了「谁持有 UObject 实例」。

---

## 0. 起点：问题是什么

V1 的 `EffectChain` / `EffectStep` 理念可用，但**所有步骤都是 `USTRUCT`**，宿主想用蓝图 / 反射脚本扩展新实现时只能「借壳」——注册表键是 `UScriptStruct*`，脚本造不出新的步骤种类，于是所有脚本步骤共用一个通用 struct，执行器按参数分发。

宿主自写带状态的复杂逻辑，更无处安放：蓝图不能定义新类型，**它的自定义状态只能存在 `UCLASS` 的变量里**。

---

## 1. 逻辑落地：单步链 ✅

> **「复杂逻辑单开一个 UObject 实例」= 一条只含一个 UObject 步骤的链。**

**依据**：`FTcsChainRun` 本来就是「控制流状态住数据、不活在调用栈里，故挂起可以跨帧」（V1 `TcsChainRun.h:100`），配合事件等待与恢复，一个步骤完全能表达「进入 → 等事件 → 被唤醒继续 → 结束」。

**Why Not 另立一套实例机制**：两套运行时意味着两套挂起、两套取消、两套调试，且「什么时候用哪个」变成宿主的判断题。

| 需求 | 表达方式 |
|---|---|
| 简单 Buff / Skill | 多步链，步骤是 `USTRUCT` |
| 特别复杂的逻辑 | **单步链**，那一步是 `UObject` |
| 流程步骤（WaitDelay / WaitEvent / Parallel） | 保持 `USTRUCT`，不升格 |

---

## 2. 步骤类型：Struct / Object 二选一 ✅

### 2.1 注册表键统一到 `UStruct*`

| 类型 | 继承关系 | 引擎位置 |
|---|---|---|
| `UScriptStruct` | `public UStruct` | `UObject/Class.h:1774` |
| `UClass` | `public UStruct` | `UObject/Class.h:3893` |

⇒ **一张 `TMap<const UStruct*, FExecutor>` 同时管两种步骤**，不需要两张表、不需要类型标签。

### 2.2 步骤引用

```cpp
struct FTcsStepRef
{
    // USTRUCT 步骤：类型 + 数据（FInstancedStruct 自带反射类型）
    FInstancedStruct       StructStep;

    // UOBJECT 步骤：只存类型；运行时按实例化策略创建
    TSubclassOf<UObject>   ObjectStep;
};
```

### 2.3 实例化：每次运行创建一个对象 ✅

步骤对象 **MUST NOT 由链定义共享**——共享对象无法持有 per-run 状态，同一技能并发施法会串状态。

> **定义只存类型，运行时实例化。**

**连带代价**：`FTcsChainRun` 要持步骤对象的强引用，而池元素住普通 `TArray`、GC 看不见 ⇒ 子系统 MUST 在 `AddReferencedObjects` 里遍历池。V1 踩过同款（`UTcsDamageSubsystem::AddReferencedObjects` 修的 WAIT-8）。

> ✅ **这一条已在族内**：审核清单第 46 项，落于《运行时实例》§1.2.1「池元素的 `UObject` 引用」。本文不重复裁决，升文档时直接引用。

### 2.4 对象池复用：全局机制 ✅

> **复用 MUST 是全局行为，MUST NOT 暴露成每个定义的开关。**

理由：让某个定义选择「每次新建」的唯一理由是它的 Reset 不干净，那该修 Reset，不该给它一个更慢的选项。

### 2.5 流程步骤接 UObject 步骤

`WaitDelay` / `WaitEvent` 本身不变（靠 PC 前进，不持有后继引用）。

**要变的是**：`Parallel` 的子步骤列表、`Branch` 的分支目标——这些引用 MUST 是 `FTcsStepRef`。

---

## 3. 实例化策略 ✅

```cpp
enum class ETcsInstancingScope : uint8
{
    InstancePerRun,      // 每次运行一个实例
    InstancePerEntity,   // 每 (实体, 定义) 一个实例
};
```

**不做 `NonInstanced`**：TCS 的执行底座决定了它不可行。

### 3.1 `bRetriggerableInRun`（默认 `false`）

**两种策略下都有效**，语义不同：

| Scope | `false` | `true` |
|---|---|---|
| `InstancePerRun` | 已有运行中时拒绝新触发 | **开启全新的运行**（新实例对象） |
| `InstancePerEntity` | 已有运行中时拒绝新触发 | **重置当前运行**（同一实例对象） |

> `InstancePerEntity + true` 时，状态由定义作者自行重置；框架不加保护。

### 3.2 回收时机由 Scope 推导 ✅

| Scope | 何时归还对象池 |
|---|---|
| `InstancePerRun` | 该次运行结束时 |
| `InstancePerEntity` | 实体失去该定义时（死亡 / 移除 / 替换） |

> **MUST NOT 另设「回收策略」字段**：它与 Scope 会组成互相矛盾的组合。

### 3.3 `InstancePerEntity` 的持有容器（派生结论）

`InstancePerRun` 的对象挂在 `FTcsChainRun` 上，随运行态走。

`InstancePerEntity` 的对象**不属于任何一次运行**，需要独立持有：

- 容器：`TMap<FTcsCombatEntityHandle, TObjectPtr<UObject>>`，挂在子系统上
- 移除路径：实体死亡 / 失去定义 / 定义被替换
- GC：该容器 MUST 被 `AddReferencedObjects` 覆盖

---

## 4. 命名

> 本域的 `Effect` → `Action`、`Run` → `Instance` 两张命名表，以及 `Def` / `DefAsset` 后缀统一、`Instance` 裸词禁令，**已升族内**：见 [`03-词汇与命名.md`](../tcs-remake-v2/03-词汇与命名.md) §1–§4。本文不再复述，避免两处不同步。

---

## 5. 本文的来源

- 讨论起点：Buff / Skill 实例是否需要是 `UObject`
- 关键事实：`USTRUCT` 可反射、蓝图可使用，但**蓝图不能继承它**；能派生新实现的只有 `UCLASS`。另注：蓝图**可以**新建 User Defined Struct 资产（`UUserDefinedStruct : UScriptStruct`），那是数据类型，不是行为。
- V1 已有的一半：`UTcsStepExecutor : UObject`（`BlueprintNativeEvent`），用 UObject 替代不可反射的 `TFunction` 作注册值