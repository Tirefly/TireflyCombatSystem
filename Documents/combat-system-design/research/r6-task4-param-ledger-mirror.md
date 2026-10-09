# 研究：技能参数账本（Task 4）——M2 属性侧的可镜像结构

> 起因：用户对 Task 4 提案提出两条硬需求：
> ① **ParamModifier 对 SkillParam 的修改，计算过程 MUST 与 AttributeModifier 有同样的聚合流程**；
> ② **ParamModifier 的 Apply 流程与 Removal 流程 MUST 准确无误**。
>
> 本文是**照既有实现读出来的镜像清单**（不是我的设计），用于重写提案。
> 全部结论带 `文件:行号`，可复核。

---

> **实施注记（2026-10-09，按 `docs-convention` §7「冻结」只追加、正文不改）**：本文正文出现的
> `ModifierSlots` / `ParamSlots` 应读作 **`AttrModInstances`** / **`NumericParamModInstances`**
> （提案 `refactor-rename-modifier-ledger-fields`）。**改名判据** = 容器字段名 MUST 取元素类型名的
> 复数形式（`FTcsAttrModInstance` → `AttrModInstances`；`FTcsNumericParamModInstance` →
> `NumericParamModInstances`），**`Slot` 一词保留给"可按下标寻址、可复用、带代际的空位"**。
> 旧名沿袭自 AbilityKit 的"修改器槽位自由链表"，而本仓早已拆掉槽壳、元素直接就是修正器条目。
> 详见 `PLN-R6` 的「本次改名」落地记录与提案 `refactor-rename-modifier-ledger-fields`。


## 1. ★ 我原提案的核心错误：把"双形状"合成了一个

M2 属性侧**刻意保留两套形状**（设计裁定 **D2-13「Operand 双形状（B 方案，用户拍板）」**，
依据 `decisions/log/decisions-log.md:87`）：

| 面 | 类型 | `Literal` 字段类型 | 为什么 |
|---|---|---|---|
| **定义侧** | `FTcsAttrModOperandDef`（`USTRUCT(BlueprintType)`，`TcsAttrModInstance.h:105`） | `FTcsParamValue`（`UPROPERTY`，`:127`） | 模板/Def 要能配"等级表等参数源"，且要进编辑器与反射 |
| **账本侧** | `FTcsAttrModOperand`（**纯 C++ struct，无 `USTRUCT`**，`TcsAttrModInstance.h:156`） | **`double Literal`**（`:162`）——「恒为已解析规范值（物化器单点转换保证，账本不做二次猜测——零膨胀）」（`:153`） | 账本**只为聚合热路径服务，不进反射面**（`:152` 原文） |

**⇒ 账本侧不含任何 `FInstancedStruct` / 任何对象引用**，因此账本**天然零 GC 负担**。

`TcsAttrModInstance.h:102` 另有一句限定：**「两侧同形说明：`OPK_AttributeScaled` 不物化（live 求值），故两侧字段一致」**
——即"同形"是**按种类**的结论，不是"整套只有一个形状"。

### 我原提案错在哪

我起草的 `FTcsNumericParamModifier` 把 `Operand` 定为 `FTcsParamValue`（多态、可持
`TScriptInterface`），并让它**同时充当**"定义侧行"与"账本侧条目"。后果正是我随后自己撞上的
那个矛盾：`TcsSkillSubsystem.h:89` / `.cpp:113` 明文写着「**账本条目（`FTcsLearnedSkillEntry`）
按纪律 MUST NOT 持任何 `UObject` 引用，故无需补引用**」，而我的形状让账本真持了对象引用。

**⇒ 正确解法不是"给账本补 ARO"，而是照 M2 的做法把两侧**分开**：账本侧存已解析的 `double`。**

---

## 2. 计算流程的镜像清单（对应用户需求 ①）

M2 的求值链（读自 `TcsAttributePipeline.cpp` 的 `Recalculate` 与 `TcsAttributeBandFold.h`）：

| 步骤 | M2 属性侧 | M5 参数链应对应 |
|---|---|---|
| ① 收集 | 遍历 `Instance.ModifierSlots`，逐条转 `FTcsAttributeBandEntry`（`TcsAttributePipeline.cpp:229-231`） | 遍历条目参数链的**账本槽位**，逐条转同一 `FTcsAttributeBandEntry` |
| ② 折叠 | 调 **`FoldTcsAttributeBands`**（`TcsAttributeBandFold.h:115`）——**全插件唯一** | 调**同一个**函数（`STAT-1` 收束） |
| ③ 收口 | 值域收口 `FTcsAttributeBounds`（属性特有） | 参数侧无值域概念（参数是纯数）⇒ **跳过** |
| ④ 写回/广播 | `CachedCurrent` + epsilon `1e-5` 变化广播 | 参数账本缓存当前值；变化面按需（本轮可只落求值） |

**关键同式点**：`Op` 复用同一个 `ETcsAttributeOp`（五带）、`OverridePriority` 语义同
（"仅 `TAO_Override` 读"，`TcsAttrModInstance.h:202-204`）、`SortKey` 同
（"带序唯一真相在 `Op`……折叠 MUST NOT 依赖本字段"，`:206`）。

**⇒ "同样的聚合计算流程" = 同一折叠器 + 同一带枚举 + 同一 override 三级比较**。
M2 已经是 `Source/TcsSkill/` 外的两处用户之一（另两处 = M2 自己 + `TcsDamage/TcsFlowAttributes.cpp:45`），
M5 接入即第三处、`STAT-1` 全三处闭合。

---

## 3. Apply / Removal 流程的镜像清单（对应用户需求 ②）

M2 的账本架构（`TcsAttributePipeline.h` / `_Cascade.cpp`）——**这就是需求②要的"准确无误"**：

### 3.1 Apply（`TcsAttributePipeline.cpp:85-120`）

```
ApplyModifier(Unit, Modifier):
  Store = ResolveStore(Unit)                     ← 单位未注册 → Warning + false（不 ensure）
  Instance = Store->FindInstance(Modifier.Target) ← 目标无实例 → Log + false（D2-14 允许动态增删）
  Instance->ModifierSlots.Add(Modifier)           ← 账本追加
  Instance->bDirty = true                         ← 标脏
  if (Store->BatchDepth == 0)                     ← 批外 = 隐式批
      PushEvalStack → Recalculate → PopEvalStack  ← 立即重算
```

### 3.2 Removal（`TcsAttributePipeline_Cascade.cpp:11-73`）——**四处细节都是"准确无误"的实质**

1. **按 `Source` 匹配摘除**：`Slots.RemoveAll(Modifier.Source == Source)`，**返回摘除条数**（`:22-27`）；
2. **扫描面含冻结暂存区**（`:46-58`）：注释原文——*"来源可能在属性被冻结期间结束——只在槽位里找会让其修正器**永久滞留**，属性被解冻时**凭空多出数值**，比丢数值更难查"*；
3. **无匹配 = 正常路径**：`RemovedCount == 0` **不 ensure**（`:60-64`）；
4. **批纪律**：批内只标脏、批外立即 flush（`:67-70`）。

### 3.3 事务批（`BeginBatch`/`Commit`/`FlushDirty`，`.cpp:122-169`）

- `BeginBatch` 嵌套计数；**内层提交不 flush，最外层才统一重算**（`:152`）；
- `Commit` 对未配对 `BeginBatch` **ensure**（`:143`）；
- `FlushDirty` 多轮扫描至无脏，`MaxFlushPasses = 64` 是**安全网而非环判据**（`:22-24` 注释明确）。

### 3.4 物化边界（`TcsStateModifierMaterializer.cpp:19-113`）

- **`Out.Reset()` 起手**（`:26`）——"重建语义：刷新路径与首挂共用本函数"；
- **值约定在物化边界转唯一一次**，能力位判据归**源自身**（`:76-83`，PV-10）；
- **`Source` = 实例的级联锚点**（`:108`，`MakeFromDef` 的第三参）。

---

## 4. 名词归属（防我另造名）

- 设计给账本条目定的字段名 = **`参数修正链集`**（`spec/05-module-skill.md:44`，`FLearnedSkillEntry` 字段表内）。
- **全仓零命中**的我拟名：`FTcsParamModInstance` / `FTcsNumericParamModInstance` / `FTcsParamChain` / `FTcsParamLedger`。
- 既有族命名惯例（`TcsAttrModInstance.h:184-186`）：`TcsAttrMod*` 前缀 = 修正器族；
  `Def` = 模板、`Instance` = 账本条目 ⇒ 技能侧应对应为 **`FTcsSkillParamMod*`** 或
  沿用 `TcsNumericParam*`（计划 Task 4 用的家族名）。**本命名需用户确认**（见下）。

---

## 5. 用户裁决（2026-10-09，本轮）

| 点 | 裁决 |
|---|---|
| 账本侧形状 | **A：照 M2 做双形状**（定义侧 `FTcsParamValue`，账本侧已解析 `double`/同款） |
| 账本侧命名 | **`FTcsNumericParamModInstance`**（对齐 `FTcsAttrModInstance` 的 Def/Instance 分野） |
| Apply 作用目标 | **A：落在条目上**——每个已学条目持自己的参数修正器槽位（照 `FTcsAttributeInstance::ModifierSlots`） |

---

## 6. ★ 用户追加提问：布尔开关也要有 ModifierInstance 吧？

**回答：是，设计早已预留，且形状是刻意的「极简非对称」。**

### 6.1 设计原文（`spec/05-module-skill.md:56`，逐字）

> **参数双表与修正器两结构**：`FTcsNumericParamModifier{ParamKey, Op: Add/PercentAdd/Mul/FlatAdd/Override…, Operand: FTcsParamValue…, Source, SortKey, CompeteGroup, ValueConvention}` 与
> **`FBoolSwitchModifier{SwitchKey, Value, Source}`**（D5-5 v2 命名统一，原 `FLogicGateModifier`）

拍板记录（`log/log-03-skill-m5.md:58`，D5-5 v2）：

> **BoolSwitch 命名统一**（用户抓出失步）：`FLogicGateModifier`→`FBoolSwitchModifier{SwitchKey, Value, Source}`；服务 `IsGateSet`→`IsSwitchSet`——D5-5「Gate 命名避让」当初只执行了一半（表名已避让、修改器结构名未跟），无特殊考虑

### 6.2 它为什么只有三个字段（不对称是**设计**，不是遗漏）

| 数值侧字段 | 布尔侧 | 理由 |
|---|---|---|
| `Op`（五带） | **无** | 五带是**数值聚合**的代数（求和/连乘/百分比）。布尔是"设成真/假"，没有 `Σ`/`Π` 的对应物 ⇒ 无需带枚举 |
| `SortKey` | **无** | `SortKey` 在数值侧也只作"同带内展示/审计位"（带序唯一真相在 `Op`）⇒ 无带则无位 |
| `CompeteGroup` | **无** | 竞争组是"组内解析值最大者进折叠"——**需要可比大小**；布尔只有两值，无"最大"语义 |
| `ValueConvention` | **无** | 值约定是"书写值时转规范值"（Percent/OneMinus/Negate），**纯数值记法**，布尔无此概念 |
| — | `Value`（`bool`） | 布尔侧独有的"设成什么" |

**⇒ 布尔侧不对应"带式折叠"，而是"来源注销级联撤销 + 取值竞争"**——这是它字段极简的根据。

### 6.3 双表在两侧的既有落地（实测）

- 数值行：`FTcsNumericParamRow`（住 `TcsState`，技能继承白拿）；
- 布尔行：**`FTcsBoolSwitchRow`**（`Source/TcsSkill/Public/Def/TcsBoolSwitchRow.h:18`，`USTRUCT(BlueprintType)`）——字段实测为 `{Key: FGameplayTag, Base: bool, Mode: ETcsParamMode}`（**同样有 `Mode`**，`:18` 文件头的注释原文：「`GateCheck` 与**布尔修正器归 R6.5**」⇒ 该头**自己就写明**布尔修正器是既定的后续交付物）。

### 6.4 归属与轮次（**维持原判，不并入 Task 4**）

- 批次表 `plan-r6-skill-layer.md:202`：**`R6.5-e`** = 「**`FBoolSwitchModifier` + `GateCheck` 条件**：布尔开关的修正器通道与 `TcsEffect` 侧第三个条件求值器（读 `BoolSwitches`）」，状态 ❌ 未开工，依据 = 「R6（`FTcsBoolSwitchRow` 形状已落 `TcsSkill`）+ `TRIG-5` 第 ⑤ 项」；
- `TcsBoolSwitchRow.h` 文件头亦自证「布尔修正器归 R6.5」。

**⇒ 本轮（Task 4）只做数值侧参数账本；布尔侧的 `FBoolSwitchModifier`（及其账本条目 `FTcsBoolSwitchModInstance`）归 `R6.5-e`。**
**这样切分的实质理由**：布尔修正器**没有聚合计算**（无折叠器可共用），它的价值全在"与 `GateCheck` 条件求值器配对"——而 `GateCheck` 求值器住 `TcsEffect` 条件系统，属 R6.5-e 的同批交付。**先落一个零消费者的布尔修正器 = 零消费者预建**（本仓反复拦的形态）。

**★ 但有一条 MUST 记录的形状预留**：Task 4 若把"条目持参数修正器槽位"定为**单槽位数组**（`TArray<FTcsNumericParamModInstance> ParamSlots`），则 R6.5-e 的布尔槽位**应是并列的第二个数组**（`TArray<FTcsBoolSwitchModInstance> BoolSlots`）而非挤进同一数组 —— 理由同 M2：`ModifierSlots` 之所以能只用一个数组，是因为五带**同属一个代数**；布尔与数值**不同代数**，混装会让"折叠"必须按元素类型分派（M2 无此概念）。**该预留只写进注释与提案，MUST NOT 现在就建空数组**（零消费者不预建）。


---

## 6. 本轮不做（维持原判，不受镜像影响）

- 冷却 / Cost（`R6.5`）；`FBoolSwitchModifier`（`R6.5-e`）；
  `UTcsSkillModDef` 模板分派（`R6.5-f`，且**实测该类型全仓零命中**）；
  `FTcsEntrySelector` 的 `Custom` 档实现（`R6.5-g`）；描述视图（R8）。
