# param-value Specification

## Purpose
定义全插件统一的数值配置载体与其值来源策略（内置 Literal / ParamRef），以及宿主脚本可承载的反射可见求值上下文——它是链步骤数值、修正器操作数与流程操作数的共同底座。

## Requirements

### Requirement: 统一数值配置载体

TcsCore MUST 提供 header-only 载体 `FTcsParamValue{ FInstancedStruct Source }`（全插件统一"数值配置"载体，取代 D2-12 FTcsParamScalar）：默认构造后 Source 初始化为 `FTcsParamSource_Literal`（值 0）；MUST 提供纯 C++ 求值便利转发 `Evaluate(const FTcsParamEvaluateContext& Ctx)`（**不标 `UFUNCTION()`**——不进脚本可达面）——`Source.IsValid()` 为假时兜 0（防 Source 失效后误用）。

- **载体形态（2026-09-24 换型，提案 `switch-strategy-carrier-to-plain-instanced-struct`）**：字段 MUST 为**裸 `FInstancedStruct`**（**MUST NOT** 用 `TInstancedStruct<FTcsParamValueSource>`），并以 `meta = (BaseStruct = "/Script/TcsCore.TcsParamValueSource")` 限定编辑器 picker；
- **换型理由**：`TInstancedStruct<T>` 字段在宿主脚本层（UnrealSharp/C#）**导出为空壳**（绑定产物无字段读写代码）⇒ 脚本层配不了任何数值（台账 SCRIPT-6）；裸 `FInstancedStruct` 是引擎官方文档注释给出的写法（`InstancedStruct.h:20-27`），且 **StateTree 用的正是裸形态**（D3-7 v3 宣称的"StateTree 同构"以裸形态才成立）；
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

### Requirement: 值来源策略基类与内置源

TcsCore MUST 提供抽象基类 `FTcsParamValueSource`（USTRUCT，`Evaluate(const FTcsParamEvaluateContext&) const` C++ 虚分派——D3-7 v3 StateTree 同构；实现用默认体 + `meta=(Hidden)`：UHT 为 USTRUCT 无条件生成 TCppStructOps 需可默认构造，纯虚不可编译，plan1 记"待追认"）与内置源最小集（PV-2）：

- `FTcsParamSource_Literal{ double Value }`：恒返回 Value，恒成功（Value 为配置态书写值——无约定列时即规范值，D5-18 v3）；
- `FTcsParamSource_ParamRef{ FGameplayTag Key, double Fallback }`（**2026-09-22 改造：键类型 `FName` → `FGameplayTag`**）：经上下文 `ITcsParamTableReader` 取键值，接口为空或 miss 落 Fallback；引用限同域参数表、Fallback 必填、链式允许——自引用/成环 = 编辑期/加载期报错（DAG 去重属 M8 校验器职责，源内不内置）。**键的根（2026-10-01 换根）**：`TcsStateParam`——消费角色 = 参数表读取（`ITcsParamTableReader::TryGetNumericParam`），声明方 = 宿主 `Config/DefaultGameplayTags.ini`，形态 `TcsStateParam.<键>`（2 段）；根段注册表与归属规则见 `gameplay-tag-governance` 能力。

**键类型改 tag 的连带**：`ITcsParamTableReader::TryGetNumericParam` 的键参数 MUST 同步从 `FName` 改为 `FGameplayTag`（键空间一致性——参数表的键与引用它的 `ParamRef::Key` 必须同型，否则无法直接互查）。**键的归属同样 MUST 一致**：参数表侧的键与 `ParamRef::Key` MUST 同住 `TcsStateParam` 根（同型 + 同根才是同一个键空间）；该根下的**具体键由宿主声明**，插件 MUST NOT 声明任何参数表键。**存在性校验的分工（2026-10-01 收口）**：**运行期不校验**——miss 落 `Fallback` 是本源（PV-2）的既定设计，运行期 MUST NOT 因"键可能不存在"而 ensure 或拒绝求值；**编辑器期的存在性校验归 M8 校验器**（与 `ParamRef` 的 DAG 去重同属 M8 职责）。即"静默降级"只是**运行期**语义，配置错误 MUST 在**作者期**被报出，MUST NOT 被当作规范默许的常态（否则与"拼错的 tag 在解析处 ensure、不静默降级"的既有口径相背，并让 M8 丢掉一项职责）。

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

#### Scenario: 参数表键的根

- **WHEN** 检查宿主写在 `Config/DefaultGameplayTags.ini` 里的参数表键形态
- **THEN** 形如 `TcsStateParam.<键>`（2 段）；技能激活运行态与状态实例读的是**同一个键空间**（"State" 取广义——不是 `TcsState` 模块专属），故两者同根

#### Scenario: 运行期不校验、作者期校验

- **WHEN** 配置里引用了宿主 ini 尚未声明的参数表键
- **THEN** 运行期按 miss 落 `Fallback`（不 ensure、不崩溃、不拒绝求值）；该配置错误由 M8 校验器在**作者期**报出（运行期的静默降级不构成"该键合法"的默许）

### Requirement: 反射可见求值上下文

`FTcsParamEvaluateContext` MUST 是 `USTRUCT(BlueprintType)`（**类型反射可见 + 蓝图可配置**）——**禁止 `TFunction`/`std::function` 等无 `USTRUCT` 宏的成员**（宿主脚本扩展通道，PV-1；含这类成员会让宿主类型无法加 `USTRUCT()`——UHT 报错）。

当前持四个字段：参数表只读访问 `TScriptInterface<ITcsParamTableReader> ParamTable`（可空——空时 `ParamRef` 源落 `Fallback`）、被施加方实体 `FTcsCombatEntityHandle Subject`（**2026-10-04 补齐**：状态/技能实例所依附的单位；0 = 无效）、发起者实体 `FTcsCombatEntityHandle Instigator`（**2026-10-05 补齐**："谁施加的"——可与被施加方不同；0 = 未填，域侧源按缺省口径处理）、生效等级 `int32 EffectiveLevel`（**2026-10-04 补齐**：0 = 无等级语义）；领域扩展 = 结构体继承 + 源内 checked cast（PV-1，cast 失败面由 M8 校验覆盖）。

**取值归属**：三个主体/等级字段一律由**装配上下文的调用方**填写（谁起链/谁施加状态谁负责），值来源（策略）MUST NOT 自行反查实例或全局表——源只读上下文。措辞口径见 `Documents/combat-system-design/ledger/reflection-terminology.md`。

**域读口不进本类型**：域特化的读取契约（属性读口、等级读口等）MUST 挂在**该域的派生上下文**上（先例 `FTcsAttributeEvaluateContext::Provider`）——本类型只承载跨域通用的主体身份与等级位。

USTRUCT 没有内建类型查询，故 PV-1 的"源内 checked cast"MUST 由**类型标识虚函数**承载：`virtual const UScriptStruct* GetScriptStruct() const { return StaticStruct(); }`（GAS `FGameplayEffectContext::GetScriptStruct` 同款机制，2026-09-16 实证）——派生上下文覆写为自身类型，域侧源取上下文后以 `GetScriptStruct()->IsChildOf(<域上下文>::StaticStruct())` 判定（**支持多层派生**：更具体的上下文不被拒），不匹配即落兜底（不崩溃、不 ensure）。

#### Scenario: 上下文可被宿主脚本读写

- **WHEN** 宿主（UnrealSharp/BP）构造或检查求值上下文
- **THEN** 全部字段均可经脚本层读写（无闭包类成员阻断 `USTRUCT()` 生成）——含 `Subject` / `Instigator` / `EffectiveLevel` 三个主体字段

#### Scenario: 派生上下文的类型判定

- **WHEN** 域侧源收到的是基础 `FTcsParamEvaluateContext`（或另一域的派生上下文），而它期望属性域上下文
- **THEN** `IsChildOf` 判定为假，源落兜底路径；收到属性域上下文（或其更具体的派生）时判定为真，进入域读取路径

#### Scenario: 等级源只读上下文、不反查实例

- **WHEN** 等级类参数源（读 `Context.Subject` / `Context.EffectiveLevel`）在上下文未填这两个字段（`Subject.Id == 0` / `EffectiveLevel == 0`）时求值
- **THEN** 源按"无等级语义"路径落兜底（`Evaluate` 不崩溃、`GetIndexForLevel` 返回 `INDEX_NONE`），且**不**向任何子系统反查该实例的等级

#### Scenario: 未填发起者时发起者等级源落兜底

- **WHEN** 上下文填了 `Subject` 与等级读口但 `Instigator` 未填（`Instigator.Id == 0`），发起者等级源求值
- **THEN** 落该源兜底路径（不崩溃、不 ensure、不去读 `Subject` 的等级顶替）

### Requirement: 可枚举值来源基类

TcsCore MUST 提供可选能力基类 `FTcsParamEnumerableSource : FTcsParamValueSource`（`USTRUCT(meta = (Hidden))` + 模块导出宏，与值来源策略同族、同住参数面）——成员为**一个中性默认实现**：

- `virtual int32 GetIndexForLevel(int32 Level) const { return INDEX_NONE; }`：等级 → 档位下标（`INDEX_NONE` = 无对应档）。

**索引解析的唯一真相在源**（PV-10）：展示/视图层 MUST NOT 自行推算下标，防"tooltip 高亮 10%、实打 7.5%"这类双口径；`Evaluate` 与 `GetIndexForLevel` MUST 同源（同一档位口径）。基类 MUST NOT 预建策略接口（如 `IValueDomainPolicy`）——零消费者不预建；消费方 MUST NOT 建立"源类型 × 可枚举"中心名单或 switch（能力探测走虚分派，宿主自定义源零公共代码改动）。**PV-10 原案的 `Enumerate(Level, OutValues, OutCurrentIndex)` 及其派生形态"表项总数"均不在本需求内**：两者的消费者都是展示层的 `FTcsParamView_Series`（住 TcsNotation），落地时随真实消费方补（台账 `STAT-2`）。

#### Scenario: 基类默认实现不构成能力承诺

- **WHEN** 直接查询 `FTcsParamEnumerableSource` 的中性默认实现
- **THEN** `GetIndexForLevel(任意 Level)` 返回 `INDEX_NONE`——基类自身不宣称"可枚举"，未覆写的派生源走"无档位语义"路径

#### Scenario: 档位口径同源

- **WHEN** 一个等级数组源同时被求值（`Evaluate`）与查询档位（`GetIndexForLevel`）
- **THEN** 两者落在**同一档**（下标一致）——不存在"求值用一套、展示用另一套"的双口径

### Requirement: 宿主参数源插槽

TcsCore MUST 提供 `ITcsParamSourceHost`（`UINTERFACE(MinimalAPI, Blueprintable)`），使宿主（脚本 / 蓝图 / 任意 UE 脚本语言）能定义**新的数值求值语义**而不触碰任何 C++ 求值路径。两个 `UFUNCTION(BlueprintNativeEvent)`：

- `double Evaluate(const FTcsParamEvaluateContext& Context)`——在给定上下文下求出规范值；
- `bool AllowsValueConvention()`——本源是否允许配置行级"值约定"列（D5-18 v3 白名单的唯一真相）。

TcsCore MUST 同时提供 `FTcsParamSource_HostDelegate : FTcsParamValueSource`（USTRUCT 策略子类）：持 `UPROPERTY TScriptInterface<ITcsParamSourceHost> Host`，两个虚函数**纯转发**（`Execute_Evaluate` / `Execute_AllowsValueConvention`）+ **空 Host 守卫**（分别返回 0 / true）。**两个都必须转发**：漏掉能力位会让宿主新源在 `ValueConvention` 白名单校验上拿到错误的默认能力位。

**为什么是"接口 + 转发器"两层**：脚本定义的 struct 无 C++ 类型 ⇒ `CppStructOps == nullptr` ⇒ vtable 位为 0 ⇒ 虚分派**物理不可达**。转发器把宿主实现接进既有虚分派体系，**内置路径保持 C++ 快路径不变**，插槽只服务宿主扩展。

**载体与调用点零改动**：`FTcsParamValue.Source` 是裸 `FInstancedStruct`，照装转发器；既有 `Evaluate` 调用点走 `GetPtr<FTcsParamValueSource>()` + 判空，转发器作为子类自然命中。

**上下文形参用基础类型**：宿主源收到的是 `FTcsParamEvaluateContext&`；需要域侧读口的宿主源自按既有扩展机制做 checked cast（PV-1）——插槽本身**不承诺**任何具体域的读口。

**落地核验（"能导出 ≠ 能往返"）**：本需求除编译通过外，MUST 核验 ①glue 产物里两个方法生成了**真实方法体**（非空壳）；②宿主脚本实例配进 `FTcsParamValue.Source` 后求值真的走脚本实现。验证流程 MUST 复用既有宿主脚本探针形态（两段式命令驱动：C++ 驱动基类 + C# 子类），MUST NOT 另写一套。

#### Scenario: 空 Host 的守卫值

- **WHEN** 一个未填 `Host` 的 `FTcsParamSource_HostDelegate` 被求值并查询能力位
- **THEN** `Evaluate` 返回 0、`AllowsValueConvention` 返回 true（空 Host 是配置缺口，不崩溃、不 ensure）

#### Scenario: 转发器可作参数值来源装入载体

- **WHEN** 把填了宿主实现的转发器塞进 `FTcsParamValue.Source`
- **THEN** `FTcsParamValue::Evaluate` 命中它（`GetPtr<FTcsParamValueSource>()` 判空通过）并返回宿主给出的值

#### Scenario: 能力位经转发器真实到达

- **WHEN** 宿主实现把 `AllowsValueConvention` 覆写为 false，经转发器查询能力位
- **THEN** 返回 false（不是转发器的缺省 true）——`ValueConvention` 白名单校验据此判定

#### Scenario: glue 产物含真实方法体

- **WHEN** 检查宿主脚本绑定产物中接口的两个方法
- **THEN** 两者生成真实方法体（**非空壳**）——脚本层可覆写且 C++ 的 `Execute_*` 能调到脚本实现

#### Scenario: 宿主源参与快照构建

- **WHEN** 某状态 Def 的某参数行使用宿主源，施加该状态
- **THEN** 快照内该键的值 = 宿主源在本次上下文下求出的值（值约定列的转换按宿主声明的能力位决定）
