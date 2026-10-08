## MODIFIED Requirements

### Requirement: 实体查询注入契约

`TcsEffect` MUST 以**注入接口**承载宿主侧实体能力（D4-14：反向依赖一律注入接口击穿——机制层定义契约、宿主/上层实现）：

- `ITcsEntityQuery`（`UINTERFACE`）声明住 `Public/Host/TcsEntityQuery.h`，MUST 提供四支能力（10 §2.3 点名的全集 + R6 追加的"实体可操作性"一问）：
  - `EnumerateEntities(TFunctionRef<void(FTcsCombatEntityHandle)> Visitor)`——**稳定序遍历全部战斗实体的身份句柄**（顺序由宿主定义；调用方若需确定性，宿主须给稳定序）；哪些东西算战斗实体（单位/场景物/陷阱）**是宿主语义**，框架不裁决；
  - `GetLocation(FTcsCombatEntityHandle Entity, FVector& OutLocation) -> bool`——取实体定位（范围选择/表现用；实体无定位或未知时返回 false）；
  - `IsAlive(FTcsCombatEntityHandle Entity) -> bool`——存活判定（**宿主本体论**：死亡规则归宿主）；
  - `IsEntityReady(FTcsCombatEntityHandle Entity) -> bool`——**可操作性判定**（框架能不能在这个实体上操作；与 `IsAlive` 是**正交轴**：后者答"宿主认为它活着吗"，本问答"框架能不能在它身上操作"）。**R6 追加（Q-11 裁定，2026-10-06）**：设计 `06-module-integration.md` §4 指定的门禁第一道原词就是"实体状态 **Ready**"，而持有该状态机的 `UCombatWorldRegistrySubsystem` **属 R7、今天不存在** ⇒ R6 必须有替身，本方法与设计 1:1 对应。**R7 落地后的转发纪律**：世界注册表（`GetEntityState`）落地后，本方法 MUST 改为 `GetEntityState(handle) == Ready` 的**转发**，MUST NOT 成为与状态机并列的**第二真相**；日期锚点 = 2026-10-08。
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

#### Scenario: 实体可操作性判定与存活判定彼此独立

- **WHEN** 宿主实现被调用 `IsEntityReady`（与 `IsAlive` 分别调用同一批句柄）
- **THEN** 两者各自返回宿主给出的结果，**互不代为回答**——死亡但可操作（复活技能 / 死亡触发的被动）与存活但不可操作（待销毁尸体）两种组合都能被表达；`IsAlive` MUST NOT 被改写成 `IsEntityReady` 的别名
