## MODIFIED Requirements

### Requirement: 反射可见求值上下文

`FTcsParamEvaluateContext` MUST 是 `USTRUCT(BlueprintType)`（**类型反射可见 + 蓝图可配置**）——**禁止 `TFunction`/`std::function` 等无 `USTRUCT` 宏的成员**（宿主脚本扩展通道，PV-1；含这类成员会让宿主类型无法加 `USTRUCT()`——UHT 报错）；当前持三个字段：参数表只读访问 `TScriptInterface<ITcsParamTableReader> ParamTable`（可空——空时 `ParamRef` 源落 `Fallback`）、被施加方实体 `FTcsCombatEntityHandle Subject`（**2026-10-04 补齐**：状态/技能实例所依附的单位；0 = 无效）、生效等级 `int32 EffectiveLevel`（**2026-10-04 补齐**：0 = 无等级语义）；领域扩展 = 结构体继承 + 源内 checked cast（PV-1，cast 失败面由 M8 校验覆盖）。**取值归属**：`Subject` 与 `EffectiveLevel` 由**装配上下文的调用方**填写（谁起链/谁施加状态谁负责），值来源（策略）MUST NOT 自行反查实例或全局表——源只读上下文。措辞口径见 `Documents/combat-system-design/ledger/reflection-terminology.md`。

USTRUCT 没有内建类型查询，故 PV-1 的"源内 checked cast"MUST 由**类型标识虚函数**承载：`virtual const UScriptStruct* GetScriptStruct() const { return StaticStruct(); }`（GAS `FGameplayEffectContext::GetScriptStruct` 同款机制，2026-09-16 实证）——派生上下文覆写为自身类型，域侧源取上下文后以 `GetScriptStruct()->IsChildOf(<域上下文>::StaticStruct())` 判定（**支持多层派生**：更具体的上下文不被拒），不匹配即落兜底（不崩溃、不 ensure）。

#### Scenario: 上下文可被宿主脚本读写

- **WHEN** 宿主（UnrealSharp/BP）构造或检查求值上下文
- **THEN** 全部字段均可经脚本层读写（无闭包类成员阻断 `USTRUCT()` 生成）——含 `Subject` 与 `EffectiveLevel` 两个新增字段

#### Scenario: 派生上下文的类型判定

- **WHEN** 域侧源收到的是基础 `FTcsParamEvaluateContext`（或另一域的派生上下文），而它期望属性域上下文
- **THEN** `IsChildOf` 判定为假，源落兜底路径；收到属性域上下文（或其更具体的派生）时判定为真，进入域读取路径

#### Scenario: 等级源只读上下文、不反查实例

- **WHEN** 等级类参数源（读 `Context.Subject` / `Context.EffectiveLevel`）在上下文未填这两个字段（`Subject.Id == 0` / `EffectiveLevel == 0`）时求值
- **THEN** 源按"无等级语义"路径落兜底（`Evaluate` 不崩溃、`GetIndexForLevel` 返回 `INDEX_NONE`），且**不**向任何子系统反查该实例的等级

## ADDED Requirements

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
