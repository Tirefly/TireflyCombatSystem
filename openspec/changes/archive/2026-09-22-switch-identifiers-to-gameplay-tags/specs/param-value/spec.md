## MODIFIED Requirements

### Requirement: 值来源策略基类与内置源

TcsCore MUST 提供抽象基类 `FTcsParamValueSource`（USTRUCT，`Evaluate(const FTcsParamEvaluateContext&) const` C++ 虚分派——D3-7 v3 StateTree 同构；实现用默认体 + `meta=(Hidden)`：UHT 为 USTRUCT 无条件生成 TCppStructOps 需可默认构造，纯虚不可编译，plan1 记"待追认"）与内置源最小集（PV-2）：

- `FTcsParamSource_Literal{ double Value }`：恒返回 Value，恒成功（Value 为配置态书写值——无约定列时即规范值，D5-18 v3）；
- `FTcsParamSource_ParamRef{ FGameplayTag Key, double Fallback }`（**2026-09-22 改造：键类型 `FName` → `FGameplayTag`**）：经上下文 `ITcsParamTableReader` 取键值，接口为空或 miss 落 Fallback；引用限同域参数表、Fallback 必填、链式允许——自引用/成环 = 编辑期/加载期报错（DAG 去重属 M8 校验器职责，源内不内置）。

**键类型改 tag 的连带**：`ITcsParamTableReader::TryGetNumericParam` 的键参数 MUST 同步从 `FName` 改为 `FGameplayTag`（键空间一致性——参数表的键与引用它的 `ParamRef::Key` 必须同型，否则无法直接互查）。

基类 MUST 同时提供**约定能力位** `virtual bool AllowsValueConvention() const { return true; }`（D5-18 v3 可配白名单的唯一真相：判据 = "本源书写的数值是否就是结果"）：默认 true（`Literal` 与表型源——书写值直接成为结果）；`FTcsParamSource_ParamRef` MUST 覆写为 false（读到的已是规范值，再转 = 二次转换）；域侧禁配源（如 `TcsAttribute` 的 `AttributeScaled`）同样覆写为 false。能力探测一律走虚分派，**MUST NOT** 在消费方建立源类型中心名单/switch（PV-10 同纪律：宿主自定义源自行声明、零公共代码改动）。

#### Scenario: ParamRef 命中与兜底

- **WHEN** 上下文参数表含 tag K（值 5）与不含 tag M 两种情况，分别以 ParamRef{K, 1} / ParamRef{M, 1} 求值
- **THEN** 前者返回 5，后者返回兜底 1

#### Scenario: 上下文无参数表

- **WHEN** `FTcsParamEvaluateContext` 的 ParamTable 为空，以 ParamRef 源求值
- **THEN** 返回 Fallback

#### Scenario: 约定能力位默认与覆写

- **WHEN** 分别查询 `Literal` 源、`ParamRef` 源、`AttributeScaled` 源是否允许行级值约定列
- **THEN** 依次返回 true、false、false（默认实现不改变既有源行为，是纯增量）
