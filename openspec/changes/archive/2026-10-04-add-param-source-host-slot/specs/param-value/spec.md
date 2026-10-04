# Delta: param-value

> **本 delta 含一条 MODIFIED（补 `Instigator` 字段）**——起草期原以为"上下文零改动"，实施期编译暴露：`Instigator` **不在** Task 0 补的那两个字段里（只有 `Subject` 与 `EffectiveLevel`），而发起者等级源必须读它。裁定 = **补进基础上下文**（与 `Subject` 同族的主体字段；`FTcsParamSource_AttributeScaled` 的父注本就写着它随等级源同批进 Core 上下文），**而不是**塞进 `TcsState` 的派生上下文。
>
> **宿主等级读口仍不进 Core 上下文**（调研 R-1 原设想的第三字段**不采用**）：它的唯一消费者是 `TcsState` 的四个等级源，Core 不该持有只有上层使用的域读口——改用既立的"派生上下文 + 源内 checked cast"机制（先例 `FTcsAttributeEvaluateContext` 持 `ITcsAttributeProvider`）。**两者的分界判据**：字段本身是**通用主体身份**（发起者/被施加方，跨域都会问）⇒ 进 Core；字段是**某域的读口**（属性读口、等级读口）⇒ 进该域派生上下文。

## MODIFIED Requirements

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

## ADDED Requirements

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
