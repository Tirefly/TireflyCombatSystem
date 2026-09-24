# Change: 策略载体换裸 `FInstancedStruct`（脚本侧字段可配）

## Why

`TInstancedStruct<T>` 字段在 C# 侧**是空壳**——UnrealSharp 的 `PropertyTranslatorManager` 未为 `UhtTemplateStructProperty` 注册 translator（全库零命中），而 `GetTranslator` 按**精确类型**查表 ⇒ 字段被跳过 ⇒ 绑定产物生成空函数体。

**后果**（2026-09-24 实测暴露，台账 S-6）：

| 类型 | C# 侧现状 |
|---|---|
| `FTcsParamValue.Source`（**全插件数值载体**——链步骤数值/修正器 Operand/流程 Operand 全走它） | 空壳（零字段零构造参数）⇒ 脚本层**配不了任何数值** |
| `FTcsStepSelectTargets.Selector` / `Filters` | 空壳 ⇒ 脚本层**拼不出选目标步骤**（原计划的三步骤链实证因此不可实现） |
| `FTcsAttrModInstance`（修正器） | 空壳 |

**这是比门面反射化（S-1，已交付）更靠前的脚本化障碍**：门面通了之后，C# 仍配不了任何带策略的字段。

**用户 2026-09-24 拍板**：走 TCS 侧换型（裸 `FInstancedStruct` + `meta=(BaseStruct=...)`），**不改 UnrealSharp**（跨仓改动 + 上游未必接受）。

## What Changes

四处 `TInstancedStruct<T>` 字段换为裸 `FInstancedStruct` + `meta=(BaseStruct=...)`：

| 文件 | 字段 | 换型后 |
|---|---|---|
| `TcsCore/Public/Parameter/TcsParamValue.h` | `Source` | `FInstancedStruct` + `BaseStruct="/Script/TcsCore.TcsParamValueSource"` |
| `TcsTargeting/Public/Chain/TcsStepSelectTargets.h` | `Selector` | `FInstancedStruct` + `BaseStruct="/Script/TcsTargeting.TcsTargetSelectorStrategy"` |
| 同上 | `Filters` | `TArray<FInstancedStruct>` + 同 BaseStruct |
| `TcsAttribute/Public/Attribute/TcsAttrModInstance.h`（如含） | — | 随查 |

**规格同步**：`param-value` / `targeting-strategy` 的载体描述改为"裸 `FInstancedStruct` + `BaseStruct` metadata 限定"。

**BREAKING**：**无**（详见下"兼容性论证"）。

## 兼容性论证（三重证据，已源码级核实）

### 证据 1：反射层视为同一物

引擎源码 `StructUtils/InstancedStruct.h:538`（`TInstancedStruct` 私有成员注释）：

> *"TInstancedStruct MUST be the same size as FInstancedStruct, **as the reflection layer treats a TInstancedStruct as a FInstancedStruct**. This means that any reflected APIs (like ExportText) that accept an FInstancedStruct pointer can also accept a TInstancedStruct pointer."*

### 证据 2：UHT 产出的属性结构完全相同

`TcsParamValue.gen.cpp:70` 实测（**换型前**）：

```cpp
const UECodeGen_Private::FStructPropertyParams UHT_STATICS::NewProp_Source = {
    "Source", ..., EPropertyGenFlags::Struct, ..., STRUCT_OFFSET(FTcsParamValue, Source),
    Z_Construct_UScriptStruct_FInstancedStruct,   // ← 指向的就是 FInstancedStruct
    METADATA_PARAMS(...)
};
```

⇒ `TInstancedStruct<T>` 的 `UPROPERTY` **已经是** `FStructProperty` → `FInstancedStruct`；换型后**属性结构逐字段相同**。

### 证据 3：资产序列化的是类型身份，不是容器类型

`FInstancedStruct::Serialize`（`InstancedStruct.cpp:191`）用 `Ar << TObjectPtr<UScriptStruct> SerializedScriptStruct` 写**硬引用**——存的是"内层是哪个 struct"（如 `TcsParamSource_Literal`），**与"外层容器是 `TInstancedStruct` 还是 `FInstancedStruct`"无关**。

实测扫过项目 6 个内容资产：`DA_SliceChain` / `DA_FormulaChain` 确实含 `TInstancedStruct` 数据（内层类型名 `TcsParamSource_Literal` / `TcsSelSelf` / `TcsStepSelectTargets` 等明文可见）。

⇒ **换型后已存资产原样加载**。**实测验证**列入验收（Task 5）。

## 代价（诚实列全）

| 代价 | 具体影响 | 补偿 |
|---|---|---|
| **丢编译期类型限定** | `Source = SomeRandomStruct` 能编译（原本 `enable_if` 拦） | 运行期仍有校验：`FInstancedStruct::GetPtr<T>()` 做 `IsChildOf` 检查，不符**返回 nullptr**（非野调用）——TCS 既有代码已在用 `GetPtr` + 判空模式（`TcsStepSelectTargets.cpp:29`） |
| **picker 限定靠手写 metadata** | 每个字段手写 `meta=(BaseStruct="/Script/...")`，**写错则 picker 静默不限**（`TryFindTypeSlow` 找不到返回 nullptr，不报错） | 列入验收：编辑器里逐个字段确认 picker 只列该族；M8 校验矩阵可加"BaseStruct metadata 存在性"检查（台账 R8-1 范围内） |
| **`GetMutable<T>` 语义变化** | `TInstancedStruct<T>` 的 `GetMutable<T>()` 带 `enable_if`；裸版的**无编译期限定**（但仍 `check(Struct->IsChildOf(...))`） | 调用点无需改动（语法一致）；`TcsFlowStepsCore.cpp:56` 等写操作照常 |

## 影响面

- **Affected specs**: `param-value`（MODIFIED × 1）、`targeting-strategy`（MODIFIED × 1）
- **Affected code**（4 字段定义 + 调用点）：
  - `TcsCore/Public/Parameter/TcsParamValue.h`（字段 + `Evaluate` 转发）
  - `TcsTargeting/Public/Chain/TcsStepSelectTargets.h`（两字段）
  - `TcsTargeting/Private/Chain/TcsStepSelectTargets.cpp`（`GetPtr` 调用点）
  - `TcsAttribute/Private/Attribute/TcsAttrModDef.cpp`（`Source.IsValid()` / `Get()` / `GetScriptStruct()`）
  - `TcsDamage/Private/Flow/Steps/TcsFlowStepsCore.cpp` / `TcsFlowStepsRest.cpp`（`GetMutable<Literal>`）
- **不改**：任何资产文件（兼容性论证保证）；`Build.cs`；`TInstancedStruct` 在**非字段**位置的用法（若有）

## 非目标

- **不改 UnrealSharp**（补 `UhtTemplateStructProperty` translator）——用户已否决该路。
- **不做**上下文反射化（台账 S-3）/ 注册表签名换型（S-2）——独立欠账。
- **不引入**新的策略位类型。

## 验收方式

1. **编译**：UBT Development 通过；
2. **glue 产物**：读 `TcsParamValue.generated.cs` / `TcsStepSelectTargets.generated.cs`——**确认字段真可读写**（`ToNative`/`FromNative` 含 `FInstancedStructMarshaller` 调用，而非空函数体）。**这是本提案的核心判据**（"能导出 ≠ 能往返"，见 `MEM-20260924-04`）；
3. **资产兼容性实测**：编辑器打开 `DA_SliceChain` / `DA_FormulaChain`，确认内层类型正确反序列化（步骤/参数源的 picker 显示原名，非空）；
4. **picker 限定**：逐个字段确认只列该族类型（`FTcsParamValueSource` 派生 / `FTcsTargetSelectorStrategy` 派生）；
5. **端到端**：**补跑三步骤链实证**（原计划「选目标 → 伤害 → 等待 → 伤害」，此前因 `SelectTargets` 空壳不可实现）——`TcsChainProbe` 扩展为该链并跑通。
