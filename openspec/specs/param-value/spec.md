# param-value Specification

## Purpose
TBD - created by archiving change add-tcscore-param-value. Update Purpose after archive.
## Requirements
### Requirement: 统一数值配置载体

TcsCore MUST 提供 header-only 载体 `FTcsParamValue{ TInstancedStruct<FTcsParamValueSource> Source }`（全插件统一"数值配置"载体，取代 D2-12 FTcsParamScalar）：默认构造后 Source 初始化为 `FTcsParamSource_Literal`（值 0）；MUST 提供纯 C++ 求值便利转发 `Evaluate(const FTcsParamEvaluateContext& Ctx)`（不进 UHT 反射面）——`Source.IsValid()` 为假时兜 0（防 Source 失效后误用）。

#### Scenario: 默认载体即字面量源

- **WHEN** 默认构造 `FTcsParamValue` 并以空上下文调用 `Evaluate`
- **THEN** 返回 Literal 源的 Value（默认 0）

#### Scenario: 空载体兜 0

- **WHEN** `Reset()` 后 Source 为空，调用 `Evaluate`
- **THEN** 返回 0 且不崩溃

### Requirement: 值来源策略基类与内置源

TcsCore MUST 提供抽象基类 `FTcsParamValueSource`（USTRUCT，`Evaluate(const FTcsParamEvaluateContext&) const` C++ 虚分派——D3-7 v3 StateTree 同构；实现用默认体 + `meta=(Hidden)`：UHT 为 USTRUCT 无条件生成 TCppStructOps 需可默认构造，纯虚不可编译，plan1 记"待追认"）与内置源最小集（PV-2）：

- `FTcsParamSource_Literal{ double Value }`：恒返回 Value，恒成功（Value 为配置态书写值——无约定列时即规范值，D5-18 v3）；
- `FTcsParamSource_ParamRef{ FName Key, double Fallback }`：经上下文 `ITcsParamTableReader` 取键值，接口为空或 miss 落 Fallback；引用限同域参数表、Fallback 必填、链式允许——自引用/成环 = 编辑期/加载期报错（DAG 去重属 M8 校验器职责，源内不内置）。

#### Scenario: ParamRef 命中与兜底

- **WHEN** 上下文参数表含键 K（值 5）与不含键 M 两种情况，分别以 ParamRef{K, 1} / ParamRef{M, 1} 求值
- **THEN** 前者返回 5，后者返回兜底 1

#### Scenario: 上下文无参数表

- **WHEN** `FTcsParamEvaluateContext` 的 ParamTable 为空，以 ParamRef 源求值
- **THEN** 返回 Fallback

### Requirement: 反射可见求值上下文

`FTcsParamEvaluateContext` MUST 是反射可见 USTRUCT——**禁止 TFunction/std::function 等不可反射成员**（宿主脚本扩展通道，PV-1）；当前仅持参数表只读访问 `TScriptInterface<ITcsParamTableReader> ParamTable`（可空）；领域扩展 = 结构体继承 + 源内 checked cast（PV-1，cast 失败面由 M8 校验覆盖）；`Subject`（FCombatEntityHandle）与 `EffectiveLevel`（int32）随 TcsState 等级源同批补齐（既定偏差——等级源/属性源是其唯一消费者）。

#### Scenario: 上下文可被宿主脚本读写

- **WHEN** 宿主（UnrealSharp/BP）构造或检查求值上下文
- **THEN** 全部字段反射可见可读写（无闭包类成员阻断）

