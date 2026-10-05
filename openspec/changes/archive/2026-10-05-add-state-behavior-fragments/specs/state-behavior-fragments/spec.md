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
- 运行期载荷缺失 / 类型不符 MUST NOT ensure：该条按无行为跳过并留 Warning。作者期 IsDataValid 对 Fragments 空载荷和非派生类型分别报 Error。

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
- **路由**：精确 Tag → 候选实例句柄快照 → 逐回调代际校验、重解析实例与 Def → 按下条状态实例事件过滤/外部事件扇出契约投递 → OnStateEvent。MUST NOT 跨回调持旧实例或桶指针；回调注销 Def 不得释放正在执行的片段副本。
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

- **WHEN** 两个实例兴趣同一状态事件 Tag，其一被刷新并发布带该实例 Handle 的 FTcsStateEventPayload
- **THEN** 此状态事件只唤起 Handle 匹配的实例；外部非状态事件仍按兴趣向全部在册候选实例扇出。

#### Scenario: 世界反初始化后无残留订阅

- **WHEN** 让世界（PIE 会话）反初始化后重新起一个会话
- **THEN** 订阅表为空、无来自上一会话的订阅残留（旧句柄一律判失效）

### Requirement: 状态实例事件过滤与外部事件扇出

行为派发 MUST 用单参数 Payload.GetPtr<FTcsStateEventPayload>() 识别状态载荷。识别成功时仅载荷 Handle 与候选 Handle 相等才投递；身份遵守 state-instance-lifecycle 的进程唯一代际规则，不能只比桶内 Index。非状态载荷（包括空载荷、自定义载荷）MUST 按精确兴趣 Tag 向全部存活候选实例扇出，即使其自定义读取器提供 Subject 也不泛化 Fragment 过滤。

触发行的泛型 Subject 匹配独立归 effect-trigger。MUST NOT 引入 TcsEffect 对 TcsState 的反向依赖；状态读取器的 Caster 取有效 Instigator、否则取新增 Unit，Subject 装 FTcsStateHandle。既有 Damage 读取器保持在册。

#### Scenario: 同单位不同实例不互相唤起

- **WHEN** 同单位两个状态都兴趣 Applied/Refreshed，其一发布状态载荷
- **THEN** 只有 Handle 匹配者收到回调，另一实例计数不增

#### Scenario: 不同单位相同槽位不串扰

- **WHEN** 两单位各自在 Index 0 建状态并兴趣同一状态事件
- **THEN** Generation 互异，只唤起载荷 Handle 对应实例

#### Scenario: 外部事件向全部兴趣实例扇出

- **WHEN** 两实例兴趣同一精确 Tag，发布非状态载荷或空载荷
- **THEN** 两实例都收到事件，同 Tag 行为订阅仍为一条

#### Scenario: 自定义 Subject 不泛化行为过滤

- **WHEN** 非状态的宿主载荷读取器返回有效 Subject
- **THEN** Fragment 仍向全部存活兴趣实例扇出；该 Subject 只约束 effect-trigger 绑定行
