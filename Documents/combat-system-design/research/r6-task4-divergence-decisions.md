# Task 4 提案重写前置：三处"设计文本 vs 既有实现"的落地裁决点

> 本文记录重写 `add-skill-param-chain` 提案前**必须先定**的三处，全部实测取证。
> 起因：用户要求「ParamModifier 的计算过程与 AttributeModifier **同样**」+「Apply/Removal **准确无误**」。
> 要做到"同样"，就必须先确认**既有实现的真实形状**，而不是照设计文本抄——本文即那三处差异。

---

## 差异 1（已定）：双形状 —— 定义侧 `FTcsParamValue` / 账本侧**已解析值**

**用户裁决 = A（照 M2 双形状）**，账本侧名 **`FTcsNumericParamModInstance`**。

证据：`TcsAttrModInstance.h:105`（`FTcsAttrModOperandDef`，`USTRUCT`，`FTcsParamValue Literal`）
vs `:156`（`FTcsAttrModOperand`，**纯 C++**，`double Literal`）+ `:152-153` 注释
「账本只为聚合热路径服务，不进反射面」「Literal 恒为已解析规范值（物化器单点转换保证）」。
设计裁定依据 `decisions-log.md:87`（**D2-13 Operand 双形状，B 方案，用户拍板**）。

---

## 差异 2（已答用户提问）：布尔侧 `FBoolSwitchModifier` 设计已预留

设计原文 `spec/05-module-skill.md:56`：`FBoolSwitchModifier{SwitchKey, Value, Source}`（D5-5 v2 命名统一）。
批次表 `plan-r6-skill-layer.md:202`：归 **`R6.5-e`**（与 `GateCheck` 条件求值器同批）；
`TcsBoolSwitchRow.h` 文件头自证「布尔修正器归 R6.5」。
**极简三字段是设计**（布尔无 Σ/Π 代数、无可比大小、无值约定）——详见研究文档 §6。

---

## 差异 3（★ 新发现，待裁决）：`SortKey` 与 `OverridePriority` 的关系

### 3.1 既有实现的真实形状：**两个独立字段**

| 形状 | `SortKey` | `OverridePriority` |
|---|---|---|
| 模板行 `FTcsAttrModDefTableRow` | `:79` `int32 SortKey = 0` | `:88` `int32 OverridePriority = 0` |
| 账本条目 `FTcsAttrModInstance` | `:207` | `:204` |
| 映射 `MakeFromDef` | `:188` `Instance.SortKey = Row.SortKey` | `:187` `Instance.OverridePriority = Row.OverridePriority` |
| **折叠器读的** | **不读**（`TcsAttributeBandFold.h:17`「SortKey 不参与折叠」） | **读**（`:134`/`:137` `IsStrongerTcsOverride(Entry.Value, Entry.OverridePriority, …)`） |

### 3.2 设计文本却只给 M5 一个 `SortKey`，并说它"退化为带权"

`spec/05-module-skill.md:56` 的 `FTcsNumericParamModifier` 字段表 = `{ParamKey, Op, Operand, Source, SortKey, CompeteGroup, ValueConvention}`
——**没有 `OverridePriority`**；同句又写「**`SortKey` 退化为带权、不再承担排序语义**」，
以及「"取优先级最高"= **Override 组取最大值**（与 M2 一致；D5-5 v3 取代原 `Override+SortKey` 口径）」。

计划 Task 4 Step 1 照抄了设计（只列 `SortKey`），Step 2 则写「摊平成 `FTcsAttributeBandEntry{Op, Value, OverridePriority = SortKey}`」
——即**把两者合并**。

### 3.3 三句设计文本互相冲突（这就是要裁的）

1. 「SortKey 退化为带权、不再承担排序语义」——可 `SortKey` **今天在 M2 也不承担排序语义**（折叠器不读它）；
   真正承担"强弱排座次"的是 `OverridePriority`。⇒ 这句像是在描述**另一个字段**。
2. 「Override 组取**最大值**」vs `IsStrongerTcsOverride` 的三级比较（优先级 → 同优先级策略 → 有符号值，`:57` 注释）
   ——**不是**"取最大值"，而是"优先级大者胜，打平才按策略比数值"。⇒ 设计这句与实现不符。
3. 「D5-5 v3 取代原 `Override+SortKey` 口径」——**取代的是口径，但实现里 `SortKey` 字段仍在**
   （且 M2 的 `SortKey` 真消费者在**别处**：`TcsDamage` 的消耗裁决 `TcsFlowStepsCore.cpp:141`，与折叠无关）。

### 3.4 三个可选处置（MUST 定一个再重写提案）

| 方案 | 形状 | 与 M2 同形？ | 代价 |
|---|---|---|---|
| **甲** | M5 行加 `int32 OverridePriority`，与 M2 逐字段对齐；`SortKey` 一并保留作审计位 | **完全同形**（可直用 `FTcsAttributeBandEntry` 的三级比较） | 行多一个字段（但那正是 M2 的形状） |
| **乙** | M5 只用 `SortKey`，折叠时 `OverridePriority = SortKey`（照计划现状） | 形状不同形（缺字段） | 语义漂移：M5 的 Override 胜负与 M2 **不同判据**，违反用户需求①「同样的聚合流程」 |
| **丙** | M5 不用 `SortKey`，只留 `OverridePriority` | 字段名同形、少一个审计位 | M2 有 `SortKey`；少它是"更干净"但与 M2 不完全同形 |

**推荐 = 甲**。理由：用户需求①的字面要求是"**同样的聚合计算流程**"，
而"同样"最可靠的判据是**能直接喂进同一个 `FTcsAttributeBandEntry` 而不需字段改名/合并**——
乙需要把 `SortKey` 冒充 `OverridePriority`（两个语义不同的字段被合并，正是设计文本那三句冲突的根源）。
丙虽干净，但 M2 的 `SortKey` 在 M5 也有真实用途（同带内审计展示），删它没有根据。

**★ 但 MUST 提醒**：选甲意味着**与设计文本 `spec/05-module-skill.md:56` 的字段表不一致** ⇒
重写提案时 MUST 同批把设计文本按 D5-5 v3 的实际口径**划改**（保留原文 + 注明订正理由），
否则会留下"设计 7 字段 / 实现 8 字段"的第二真相。
