## MODIFIED Requirements

### Requirement: 统一数值配置载体

TcsCore MUST 提供 header-only 载体 `FTcsParamValue{ FInstancedStruct Source }`（全插件统一"数值配置"载体，取代 D2-12 FTcsParamScalar）：默认构造后 Source 初始化为 `FTcsParamSource_Literal`（值 0）；MUST 提供纯 C++ 求值便利转发 `Evaluate(const FTcsParamEvaluateContext& Ctx)`（不进 UHT 反射面）——`Source.IsValid()` 为假时兜 0（防 Source 失效后误用）。

- **载体形态（2026-09-24 换型，提案 `switch-strategy-carrier-to-plain-instanced-struct`）**：字段 MUST 为**裸 `FInstancedStruct`**（**MUST NOT** 用 `TInstancedStruct<FTcsParamValueSource>`），并以 `meta = (BaseStruct = "/Script/TcsCore.TcsParamValueSource")` 限定编辑器 picker；
- **换型理由**：`TInstancedStruct<T>` 字段在宿主脚本层（UnrealSharp/C#）**导出为空壳**（绑定产物无字段读写代码）⇒ 脚本层配不了任何数值（台账 S-6）；裸 `FInstancedStruct` 是引擎官方文档注释给出的写法（`InstancedStruct.h:20-27`），且 **StateTree 用的正是裸形态**（D3-7 v3 宣称的"StateTree 同构"以裸形态才成立）；
- **代价（明示接受）**：丢编译期类型限定（原本 `TInstancedStruct` 的 `enable_if` 拦异族赋值）⇒ 改由**运行期**校验兜底（`FInstancedStruct::GetPtr<T>()` 做 `IsChildOf` 检查、不符返回 nullptr，调用点 MUST 判空）——TCS 既有调用点已按该模式书写；
- **兼容性**：换型**不破坏已存资产**——引擎保证 `TInstancedStruct` 与 `FInstancedStruct` 同尺寸且"反射层视为同一物"（`InstancedStruct.h:538`），UHT 产出的属性结构逐字段相同（`FStructProperty` → `FInstancedStruct`），资产序列化的是**内层类型身份**（`Ar << TObjectPtr<UScriptStruct>`）而非容器类型。

#### Scenario: 默认载体即字面量源

- **WHEN** 默认构造 `FTcsParamValue` 并以空上下文调用 `Evaluate`
- **THEN** 返回 Literal 源的 Value（默认 0）

#### Scenario: 空载体兜 0

- **WHEN** `Reset()` 后 Source 为空，调用 `Evaluate`
- **THEN** 返回 0 且不崩溃

#### Scenario: 载体字段对脚本层可读写

- **WHEN** 检查 `FTcsParamValue` 的宿主脚本绑定产物（UnrealSharp glue）
- **THEN** `Source` 字段生成真实的读写代码（`ToNative`/`FromNative` 含 `FInstancedStructMarshaller` 调用，**非空函数体**）——脚本层可构造并配置该载体

#### Scenario: 异族类型赋值的兜底

- **WHEN** 把非 `FTcsParamValueSource` 派生的 struct 塞进 `Source`（编译期不再拦）
- **THEN** 运行期取用（`GetPtr<T>()`）返回 nullptr 而非野调用；调用点按既有纪律判空并留日志

#### Scenario: 已存资产换型后原样加载

- **WHEN** 用换型后的代码打开换型前保存的 Def 资产（内含 `TInstancedStruct` 序列化数据）
- **THEN** 内层类型正确反序列化（步骤/参数源的配置保持原值，非空、非默认）
