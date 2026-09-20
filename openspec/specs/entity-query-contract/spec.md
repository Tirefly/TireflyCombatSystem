# entity-query-contract Specification

## Purpose
TBD - created by archiving change add-tcseffect-chain-interpreter. Update Purpose after archive.
## Requirements
### Requirement: 实体查询注入契约

`TcsEffect` MUST 以**注入接口**承载宿主侧实体能力（D4-14：反向依赖一律注入接口击穿——机制层定义契约、宿主/上层实现）：

- `ITcsEntityQuery`（`UINTERFACE`）声明住 `Public/Host/TcsEntityQuery.h`，R3 只声明 `EnumerateEntities(TFunctionRef<void(AActor*)> Visitor)`（**稳定序遍历**全部战斗实体）；设计名 `ICombatEntityQuery` 的其余成员（`GetLocation` / `IsAlive`）随"范围选择（RadiusArea）"消费者落地（后置，本轮零消费者不预建）；
- 注入点住门面：`SetEntityQuery(const TScriptInterface<ITcsEntityQuery>&)` / `GetEntityQuery()`；引用 MUST 以 `UPROPERTY` 持有（GC 安全——实现是 UObject）；
- 未注入时 `GetEntityQuery()` MUST 返回 nullptr（**不 ensure**——"能力尚未注入"是配置状态，不是契约违规）；
- 契约 MUST 为 C++ 专用面（参数含 `TFunctionRef`，蓝图不可表达）——与"蓝图不承诺"（R0 §9）一致。

#### Scenario: 注入后可查回

- **WHEN** 注入一个 `ITcsEntityQuery` 实现对象后调用 `GetEntityQuery()`
- **THEN** 返回该实现（可调用 `EnumerateEntities`），且经 GC 安全持有

#### Scenario: 未注入返回空

- **WHEN** 从未注入实体查询实现时调用 `GetEntityQuery()`
- **THEN** 返回 nullptr（无 ensure、无日志噪音）

