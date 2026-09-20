## MODIFIED Requirements

### Requirement: 实体查询注入契约

`TcsEffect` MUST 以**注入接口**承载宿主侧实体能力（D4-14：反向依赖一律注入接口击穿——机制层定义契约、宿主/上层实现）：

- `ITcsEntityQuery`（`UINTERFACE`）声明住 `Public/Host/TcsEntityQuery.h`，MUST 提供三支能力（10 §2.3 点名的全集）：
  - `EnumerateEntities(TFunctionRef<void(FTcsCombatEntityHandle)> Visitor)`——**稳定序遍历全部战斗实体的身份句柄**（顺序由宿主定义；调用方若需确定性，宿主须给稳定序）；哪些东西算战斗实体（单位/场景物/陷阱）**是宿主语义**，框架不裁决；
  - `GetLocation(FTcsCombatEntityHandle Entity, FVector& OutLocation) -> bool`——取实体定位（范围选择/表现用；实体无定位或未知时返回 false）；
  - `IsAlive(FTcsCombatEntityHandle Entity) -> bool`——存活判定（**宿主本体论**：死亡规则归宿主）；
- **句柄 → Actor 的映射归宿主**（谁注册实体谁掌握映射）：机制层 MUST NOT 依赖该映射（链/流程只流动句柄——Mass 单位无 Actor 亦成立）；
- 注入点住门面：`SetEntityQuery(const TScriptInterface<ITcsEntityQuery>&)` / `GetEntityQuery()`；引用 MUST 以 `UPROPERTY` 持有（GC 安全——实现是 UObject）；未注入时 `GetEntityQuery()` 返回 nullptr（**不 ensure**——"能力尚未注入"是配置状态）；
- 契约 MUST 为 C++ 专用面（参数含 `TFunctionRef`，蓝图不可表达）——与"蓝图不承诺"（R0 §9）一致；
- **消费者可得的句柄可能已失效**（实体中途销毁）：消费方 MUST 用 `IsAlive` 自行核对，MUST NOT 假设句柄在遍历后仍有效。

#### Scenario: 遍历得到句柄而非 Actor

- **WHEN** 宿主实现被调用 `EnumerateEntities`
- **THEN** 访问者收到的是 `FTcsCombatEntityHandle`（无 Actor 依赖——Mass/无 Actor 实体同样可被枚举）

#### Scenario: 未注入返回空

- **WHEN** 从未注入实体查询实现时调用 `GetEntityQuery()`
- **THEN** 返回 nullptr（无 ensure、无日志噪音）

#### Scenario: 定位与存活查询

- **WHEN** 以合法句柄调用 `GetLocation` / `IsAlive`
- **THEN** 由宿主实现给出结果；未知/已销毁句柄由宿主返回 false（框架不代判）
