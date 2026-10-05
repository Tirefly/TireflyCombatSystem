## ADDED Requirements

### Requirement: 行为 Fragment 契约

`TcsState` MUST 提供状态行为的作者面——**行为 Fragment**（D3-7 v2/v4 的两种 Fragment 之一；另一半"决策 Fragment"归 `state-stacking-policies`）：

- `FTcsStateBehaviorFragment`（`USTRUCT(meta = (Hidden))`）：字段 `Interests: TArray<FGameplayTag>`（订阅哪些事件）+ 单一泛化虚回调
  `virtual void OnStateEvent(const FGameplayTag& EventTag, const FInstancedStruct& Payload, const FTcsStateBehaviorContext& Ctx) const;`
- **中性默认实现 + `meta = (Hidden)`；MUST NOT 用 `= 0` 或 `PURE_VIRTUAL`**（全仓 USTRUCT 抽象手法；`PURE_VIRTUAL` 在 Development / Shipping 行为不同）；
- **回调 MUST 为 `const`**：片段是**配置**（住 Def 资产，解析出来是 `const FTcsBuffDef*`），MUST NOT 借回调改自身状态——"同一个 Def 被多个实例共享"时可变片段会让实例串味；要产生副作用一律经 `Ctx` 里的门面；
- `FTcsStateBehaviorContext`（**纯 C++ 值语义 struct，不反射**）：`UTcsStateSubsystem* Subsystem` / `FTcsStateHandle Handle` / `FTcsCombatEntityHandle Unit` / `FGameplayTag DefTag` / `int32 Stacks` / `int32 Level`——**MUST NOT** 内嵌实例指针或引用（实例会因桶内扩容搬移，跨回调持指针即悬空）；
- `Interests` 里的词 MUST 是**事件 Tag**（落既有 `TcsEvent` 根，如 `TcsEvent.State.Applied` / `TcsEvent.Damage.Pre`）；**MUST NOT** 为行为兴趣另开词根（`gameplay-tag-governance` 的「一角色一根」）；
- **框架 MUST NOT 提供任何具体行为 Fragment**（零内置策略）——宿主样本住宿主仓（与决策 Fragment 同口径）。

#### Scenario: 片段是纯配置且可被 Def 持有

- **WHEN** 检查 `FTcsStateBehaviorFragment` 的成员与 `OnStateEvent` 的签名
- **THEN** 除 `Interests` 外无数据成员、回调为 `const` 且无 `= 0` / `PURE_VIRTUAL`（默认实现可直接构造使用）

#### Scenario: 回调上下文不持实例指针

- **WHEN** 检查 `FTcsStateBehaviorContext` 的成员
- **THEN** 只有门面指针与值语义句柄/标量，无实例指针、无引用成员

### Requirement: Def 载体与实例零策略

- `FTcsBuffDef` MUST 提供行为 Fragment 载体字段 `Fragments: TArray<FInstancedStruct>`，并以**手写 `meta = (BaseStruct = "/Script/TcsState.TcsStateBehaviorFragment")`** 收窄选择器（全仓 2026-09-24 换型口径：`TInstancedStruct<T>` 字段在宿主脚本层导出为空壳 ⇒ 载具一律裸 `FInstancedStruct` + BaseStruct 元数据）；
- 载体**归 Def 资产持有**；`FTcsStateInstance` MUST NOT 持有 Fragment、事件载荷、订阅句柄或任何 `UObject` 引用（D3-7 v2 / D2-9 纪律：值语义实例是池化与操作复制的地基）；
- 载荷缺失 / 类型不符 MUST NOT `ensure`：该条按"无行为"处理并留 `Warning`（配置错误语义，与决策 Fragment 的退化口径同款）。

#### Scenario: Def 可配置多条行为片段

- **WHEN** 在 `FTcsBuffDef.Fragments` 里填两条不同兴趣的片段
- **THEN** 字段可容纳并往返保真（选择器只列出 `FTcsStateBehaviorFragment` 派生类型）

#### Scenario: 实例字段集合不含行为载体

- **WHEN** 检查一个带行为 Fragment 的在册实例
- **THEN** 实例字段里没有 Fragment 数组、没有订阅句柄（行为配置全在 Def 侧）

### Requirement: 行为订阅的生命周期接线

`TcsState` MUST 以**订阅表 + 共享 Handler** 承载行为订阅（先例 = `FTcsChainEventWaitRegistry`）：

- **每个兴趣 Tag 只订阅一条**（计数配对：首个感兴趣实例出现时订阅、最后一个离开时退订）；**MUST NOT** 每实例各订一次（总线 `Subscribe` 只收 `UTcsEventHandler*`、派发只传 `(EventTag, Payload)` ⇒ 回调无法自辨身份，收益为零而订阅表膨胀、退订易漏）；
- **共享 Handler** MUST 是 `UTcsEventHandler` 派生对象，由门面以 `UPROPERTY` 强持有（总线订阅表对 Handler 持**弱引用**，不 root 会被 GC 掉、订阅静默失效）；Handler 对门面持弱引用；
- **路由**：事件到达 ⇒ 订阅表给出"兴趣该 Tag 的在册实例句柄快照"⇒ 逐条**代际校验**后重解析实例（失效句柄静默跳过）⇒ 取该实例 Def 的 `Fragments` ⇒ 对 `Interests` 含该 Tag 的片段调 `OnStateEvent`；**匹配为精确 Tag**（总线按精确 Tag 建索引；层级展开属监听节点自己的过滤面，行为 Fragment 不做）；
- **接线时机**：`Apply` 新实例路径 MUST 在 `Applied` 广播**之前**完成订阅（订阅者能在自己的 `Applied` 回调里跑行为）；刷新 / 叠层路径 MUST NOT 重订阅（同一实例、订阅已在册）；
- **退订时机**：移除 / 到期路径 MUST 在 `Removed` / `Expired` 广播**之前**退订（不退订则该实例自己的行为会被自己的死亡事件触发）；`UnregisterUnit` 复用同一条移除链；世界反初始化 MUST 全量退订（不留跨世界残留订阅）；
- 订阅表 MUST NOT 引入 GC 引用（只持句柄与 Tag ⇒ 不需要 `AddReferencedObjects`；**判据 = 容器是否 GC 可见**，与"值语义"无关）；Handler 若持任何对象引用则 MUST 由 `UPROPERTY` 持有。

#### Scenario: 订阅在广播前完成、退订在广播前完成

- **WHEN** 施加一个带行为片段（兴趣 = `TcsEvent.State.Applied`）的状态，随后移除它
- **THEN** 行为回调在 `Applied` 广播当次即被调用（订阅先于广播），移除时**不**因自己的 `Removed` 触发回调（退订先于广播）

#### Scenario: 同一兴趣 Tag 的实例共用一个订阅

- **WHEN** 对同一单位施加两个都兴趣 `TcsEvent.State.Applied` 的不同状态
- **THEN** 总线订阅数**不随实例数增长**（该 Tag 只有一条订阅），两个实例各收到自己的事件回调

#### Scenario: 同一实例只被自己的事件唤起

- **WHEN** 两个实例都兴趣同一 Tag，其一被刷新 / 移除
- **THEN** 该事件只唤起其载荷句柄对应的那个实例的片段（路由按句柄重解析，不广播给同 Tag 的全部在册实例）

#### Scenario: 世界反初始化后无残留订阅

- **WHEN** 让世界（PIE 会话）反初始化后重新起一个会话
- **THEN** 订阅表为空、无来自上一会话的订阅残留（旧句柄一律判失效）
