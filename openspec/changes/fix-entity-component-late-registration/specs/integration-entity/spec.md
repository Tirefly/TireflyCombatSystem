## ADDED Requirements

### Requirement: 实体组件「定义未就绪」不是永久失效

`UTcsCombatEntityComponent` 在 `BeginPlay` 时若定义库尚未就绪，MUST **订阅就绪单出口并在就绪后补注册**，**MUST NOT 直接 return 并永久失效**。

- **两条路径 MUST 都覆盖**：`BeginPlay` 时**已就绪** ⇒ 直接注册；**未就绪** ⇒ 订阅 `UTcsDefinitionSubsystem::OnDefinitionsReady`，回调里补注册；
- **竞态窗口 MUST 填补**：订阅完成后 MUST **再次检查** `IsRuntimeReady()`——「查→订」之间若已就绪，回调永不会来，只有二次复查能救；
- **退订 MUST 有两道**：① 回调里**先退订再补注册**（`OnDefinitionsReady` 是单次信号，非每帧事件）；② `EndPlay` 与 `OnUnregister` MUST 在**未收到回调就被销毁**时退订（防御路径）。两道 MUST 共用同一份「已订阅」状态（`FDelegateHandle` 有效性）以保证不重复 Remove；
- **补注册 MUST 幂等**：同一组件实例被多次调用 MUST 只发放**一个**实体句柄——`FTcsCombatEntityHandleRegistry::Allocate()` 的 `Id` 原子递增且**永不复用**，重复发号即造出两个实体；
- **幂等哨兵 MUST 与事实同生共死**：以 `EntityHandle.IsValid()` 作哨兵时，任何使「注册事实」消失的路径 MUST 同时把句柄置回默认构造（见 `OnUnregister` 需求）。**`Id == 0` 的唯一含义是「从未调用 `RegisterEntity`」**——句柄不会「失效后自动变有效」，让句柄有效的唯一手段就是调 `RegisterEntity`。

**为什么这是缺陷而不是"时序是宿主责任"**：组件此前的 Warning 文案写着「时序是宿主责任」，但**宿主无法修复它**——定义库就绪是一个**框架内部事件**（GameInstance 级），宿主既收不到通知（广播零订阅者也未被组件订阅）也没有合法手段重新触发注册。⇒ 把义务推给一个无手段履行的主体，等于没有义务方。

#### Scenario: 未就绪时订阅而不是放弃

- **WHEN** 组件 `BeginPlay` 时 `IsRuntimeReady() == false`
- **THEN** 组件 MUST 订阅 `OnDefinitionsReady` 并返回，且**不**销毁、**不**标记永久失败

#### Scenario: 就绪回调后补注册成功

- **WHEN** 已订阅的组件在之后收到 `OnDefinitionsReady`
- **THEN** 该实体的 `EntityHandle` MUST 变为有效（`Id != 0`），且 MUST 已登记进实体查询映射（`EnumerateEntities` 能吐出该句柄）

#### Scenario: 竞态窗口由二次复查填补

- **WHEN** `BeginPlay` 查到未就绪，但在订阅动作完成之前定义库恰好就绪（回调已错过）
- **THEN** 订阅后的二次复查 MUST 发现已就绪并**立即注册**，MUST NOT 永久挂在等待态

#### Scenario: 补注册幂等

- **WHEN** `RegisterAsCombatEntity` 被连续调用两次（如多世界各触发一次就绪回调）
- **THEN** MUST 只发放一个实体句柄——第二次调用 MUST 因句柄已有效而直接返回，MUST NOT 向 `RegisterEntity` 发起第二次发号

#### Scenario: 已就绪时行为不变

- **WHEN** 组件 `BeginPlay` 时 `IsRuntimeReady() == true`
- **THEN** MUST 直接在 `BeginPlay` 内完成注册（与变更前逐字同行为），MUST NOT 订阅

### Requirement: 两条销毁路径必须共用同一份注销实现

`UTcsCombatEntityComponent` MUST 把「销毁期必须执行的逻辑」提炼成**一个私有函数**，并由 `EndPlay` 与 `OnUnregister` **共同调用**：

- **提炼的理由不是复用，是两条路径都会真实走到**：宿主项目极可能使用**对象池**（`UnregisterComponent` 后保留实例、再 `RegisterComponent` 复用）⇒ `OnUnregister` 是**正常业务路径**，不是「理论上可能有」；
- 该函数 MUST 依次做三件事：① **退订** `OnDefinitionsReady`（若仍持有）；② **四段注销级联**（查询映射 → Buff → Skill → 属性账本，顺序为契约）；③ **把 `EntityHandle` 置回默认构造**（`Id = 0`）；
- **该函数 MUST 幂等**：`UActorComponent::DestroyComponent` 的调用序列是 **`EndPlay(Destroyed)` → `UninitializeComponent()` → `UnregisterComponent()`**（`ActorComponent.cpp:2163-2179`）⇒ **两条路径在一次销毁里会连续触发**。第二次调用 MUST 因句柄已无效而安全返回（no-op），MUST NOT 重复注销、MUST NOT ensure。

**为什么第 ③ 件事不可省**（防哨兵说谎，MUST 读）：本组件的补注册用 `EntityHandle.IsValid()` 当幂等哨兵。若走「`OnUnregister`（组件被移除但 Actor 未销毁）→ 再 `RegisterComponent` → 再次 `BeginPlay`」这条路，而第 ③ 件事缺失，则属性账本已删、句柄仍有效 ⇒ 哨兵说「已注册」而**跳过补注册** ⇒ 该实体永久失去属性账本，且**无任何日志**。

#### Scenario: OnUnregister 后句柄复位

- **WHEN** 组件已注册（句柄有效）后走 `OnUnregister`（Actor 未销毁）
- **THEN** 注销级联 MUST 执行，且 `EntityHandle` MUST 变回 `Id == 0`

#### Scenario: 移除后再挂回发新句柄

- **WHEN** 组件先 `OnUnregister`、随后再次注册（`RegisterComponent`）并 `BeginPlay`
- **THEN** MUST 发放一个**新的**实体句柄（`Id` 与注销前不同）并重新登记进查询映射；MUST NOT 复用旧句柄，MUST NOT 因旧句柄残留而跳过

#### Scenario: 一次销毁里两路径连续触发不重复注销

- **WHEN** `DestroyComponent` 先调 `EndPlay(Destroyed)` 再调 `UnregisterComponent`（两者都走到提炼函数）
- **THEN** 四段注销级联 MUST **只执行一次**（第二次因句柄已无效而 no-op），MUST NOT 重复 `UnregisterEntity`、MUST NOT 触发 ensure

#### Scenario: 未订阅时退订是 no-op

- **WHEN** 组件从未订阅 `OnDefinitionsReady`（因为 `BeginPlay` 时已就绪）就走 `EndPlay`
- **THEN** 退订 MUST 是**安全的 no-op**，MUST NOT 崩溃、MUST NOT 报错

#### Scenario: 旧句柄悬空表现为查不到而非误命中

- **WHEN** 组件注销后（旧句柄已退出级联），某处仍持有旧 `EntityHandle` 并拿它查属性 / 状态 / 技能
- **THEN** 查询 MUST 返回「查不到」（空值 / 0 / nullptr），MUST NOT 命中任何其他实体的数据——这是「句柄无代际段且永不复用」（`TcsCombatEntityHandle.h:15`）提供的保证
- **THEN** 退订 MUST 是**安全的 no-op**，MUST NOT 崩溃、MUST NOT 报错
