# entity-query-contract Specification

## Purpose
定义实体查询的注入契约——框架向宿主索取「世界里有哪些实体、它们各自在哪、是否存活」的唯一接口，并明确句柄语义、稳定遍历序与不依赖 Actor 的边界；同时规定**注入面全仓只有一处**，让"同一个能力被多处持有"的静默分歧结构上不可能。

## Requirements

### Requirement: 实体查询注入契约

`TcsCore` MUST 以**注入接口**承载宿主侧实体能力（D4-14：反向依赖一律注入接口击穿——机制层定义契约、宿主/上层实现）：

- `ITcsEntityQuery`（`UINTERFACE`）声明住 `TcsCore/Public/Host/TcsEntityQuery.h`，MUST 提供四支能力（10 §2.3 点名的全集 + R6 追加的"实体可操作性"一问）：
  - `EnumerateEntities(TFunctionRef<void(FTcsCombatEntityHandle)> Visitor)`——**稳定序遍历全部战斗实体的身份句柄**（顺序由宿主定义；调用方若需确定性，宿主须给稳定序）；哪些东西算战斗实体（实体/场景物/陷阱）**是宿主语义**，框架不裁决；
  - `GetLocation(FTcsCombatEntityHandle Entity, FVector& OutLocation) -> bool`——取实体定位（范围选择/表现用；实体无定位或未知时返回 false）；
  - `IsAlive(FTcsCombatEntityHandle Entity) -> bool`——存活判定（**宿主本体论**：死亡规则归宿主）；
  - `IsEntityReady(FTcsCombatEntityHandle Entity) -> bool`——**可操作性判定**（框架能不能在这个实体上操作；与 `IsAlive` 是**正交轴**：后者答"宿主认为它活着吗"，本问答"框架能不能在它身上操作"）。**R6 追加（Q-11 裁定，2026-10-06）**：设计 `06-module-integration.md` §4 指定的门禁第一道原词就是"实体状态 **Ready**"，而持有该状态机的 `UCombatWorldRegistrySubsystem` **属 R7、今天不存在** ⇒ R6 必须有替身，本方法与设计 1:1 对应。**R7 落地后的转发纪律**：世界注册表（`GetEntityState`）落地后，本方法 MUST 改为 `GetEntityState(handle) == Ready` 的**转发**，MUST NOT 成为与状态机并列的**第二真相**；日期锚点 = 2026-10-08。
- **句柄 → Actor 的映射归宿主**（谁注册实体谁掌握映射）：机制层 MUST NOT 依赖该映射（链/流程只流动句柄——Mass 实体无 Actor 亦成立）；
- 注入点住门面：`SetEntityQuery(const TScriptInterface<ITcsEntityQuery>&)` / `GetEntityQuery()`；引用 MUST 以 `UPROPERTY` 持有（GC 安全——实现是 UObject）；未注入时 `GetEntityQuery()` 返回 nullptr（**不 ensure**——"能力尚未注入"是配置状态）；
- 契约 MUST 为 C++ 专用面（参数含 `TFunctionRef`，蓝图不可表达）——与"蓝图不承诺"（R0 §9）一致；
- **消费者可得的句柄可能已失效**（实体中途销毁）：消费方 MUST 用 `IsAlive` 自行核对，MUST NOT 假设句柄在遍历后仍有效。

**为什么契约住 `TcsCore` 而不是某个域模块**（2026-10-26 重判，MUST 读）：契约的归属判据是**消费者的最近公共祖先（LCA）**，不是"哪个模块名字听起来更底层"。四个消费者是 `TcsEffect`（持注入槽位）、`TcsSkill`（门禁用 `IsEntityReady`）、`TcsTargeting`（打分用 `GetLocation`）、`TcsIntegration`（提供实现），它们的 LCA = `TcsCore` —— 而 `TcsCore` 本来就持有契约唯一依赖的类型 `FTcsCombatEntityHandle`（`Handle/TcsCombatEntityHandle.h`），故四个消费者的 `Build.cs` **零改动**。此前声明在 `TcsEffect` 下，使 `TcsEffect` 从"恰好持有槽位的门面"错位成"契约的主人"。

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

### Requirement: 注入面唯一

**实体查询的注入槽位 MUST 全仓只有一处**——`UTcsEffectSubsystem::SetEntityQuery` / `GetEntityQuery`；任何**读**该能力的模块 MUST 经这一处取，MUST NOT 各自持有私有槽位。

**为什么这条必须成为需求**（缺陷史，MUST 读）：`UTcsSkillSubsystem` 曾持有自己的 `EntityQuery` 槽位，与 `UTcsEffectSubsystem` 那份**互不感知**。后果是两个静默失效形态：

- **宿主只注入其一**（接了效果门面、漏了技能门面）⇒ 技能侧门禁**恒为假** ⇒ 施法一律被拒；
- **宿主分别注入两个不同实例** ⇒ 组件登记走效果那份、门禁走技能那份 ⇒ **映射查不到该句柄** ⇒ 同样是"实体明明好好的却报实体未就绪"。

两者的读数是同一句话——**误导性症状 + 静默错配**，本仓反复拦的那一类。判据：**当"同一个能力被 N 处持有"时，N 处的分歧是静默的，而静默分歧不可能靠文档约束住**（台账 `WAIT-12` 曾把"只文档化装配要求"列为候选解法，已否决——靠人记住不是契约）。⇒ 正确处置是**把 N 减到 1**，让分歧**结构上不可能**，而不是补齐 N 处的同步逻辑。

**与「槽位为什么由 `TcsEffect` 持有」的关系**：契约已搬到 `TcsCore`，故"槽位住 `TcsEffect`"不再有契约归属上的理由，只剩**历史与改动成本**的理由。本轮**不改槽位归属**（一次只动一件事），本条只约束**数量为一**；"槽位是否该搬去 `TcsCore` 的独立子系统"登记为已知语义债，超出本需求范围。

#### Scenario: 技能侧经共享槽位取查询而非自持

- **WHEN** 技能门禁第一道需要判定某实体是否可操作
- **THEN** 它 MUST 经 `UTcsEffectSubsystem::GetEntityQuery()` 取查询实现，**MUST NOT** 从 `UTcsSkillSubsystem` 的私有成员取

#### Scenario: 注入面数量为一可反查

- **WHEN** 全仓检索 `SetEntityQuery` 的**声明与实现**
- **THEN** 只命中 `UTcsEffectSubsystem` 一处；出现第二处即为违反本需求（装置侧的**调用**不算，那正是本需求的正常用法）
