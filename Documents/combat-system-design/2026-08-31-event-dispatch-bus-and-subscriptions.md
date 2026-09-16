# 事件分发形态：总线 + 订阅表 + 共享 Handler（裁决 2 子文档）

> 性质：设计调研记录——**裁决 2（Struct vs UObject 载体）进行中**，本篇为其子结论：**事件分发形态已定**（2026-08-31）。
> 证据标注：【源码调研】= UE 5.8.0 (Release-5.8, CL 55116800) 本机源码取证；【通识】= Lyra/GAS/Mass 等公开已知做法；【设计判断】= 架构推理。
> 关联：讨论总日志 `../combat-system-state-tree-research/2026-08-31-combat-skill-system-carrier-discussion.md` §6.4（裁决 2 主体结论：类做词汇、struct 做状态、系统做流转）；裁决 1 文档 `2026-08-31-unit-state-relation-and-state-slots.md`。

## 1. 背景与问题

裁决 2 选定 RuntimeInstance = USTRUCT + 句柄 + 容器后，浮现硬质疑：**UStruct 无法监听 UE Delegate**（`AddUObject`/`AddSP` 需要对象身份）。若用 struct，似乎只能让 Subsystem/Component 收到事件后，找到订阅该事件的技能/Buff，通过函数调用传递信息——这是否为退化方案？

## 2. 裁决：不是降级，是正确形态——订阅与"对象身份"解耦

**"Subsystem 收事件 → 查订阅索引 → 直接函数调用"就是数据导向系统对 UE delegate 的标准替代。**delegate 自绑定是"对象是主角"世界的事件机制；struct 容器世界里，主角是总线，订阅表就是事件机制。

## 3. 关键转念：监听的是共享 Handler 类，实例作为视图参数被路由

class/struct 分离后，逻辑住在**共享、永生**的 Handler UClass 上（可被 C++/BP/C# 继承——与裁决 2 的词汇扩展面契约一致），状态住在**短命** struct 实例里。因此：

- **订阅注册**：Buff 被 Apply 时，容器向事件总线登记 `{Unit句柄, Buff句柄, Handler类, 触发参数}`；
- **事件分发**：总线查订阅索引，**句柄校验后直接调用 `Handler->Execute(实例视图, 事件)`**；
- 短命实例永远不做 AddUObject——delegate 的"绑定"职责上移到永生词汇对象。

## 4. 骨架

```cpp
struct FBuffEventSubscription {
    FUnitHandle      Unit;
    FBuffHandle      Buff;        // generation 校验防悬挂
    TObjectPtr<UBuffTriggerHandler> Handler; // 共享词汇类，可 BP/C# 子类
    FInstancedStruct TriggerParams;
};
// 键 = 事件 Tag（Event.Combat.Damaged…），Tag 层级匹配做订阅降维：
// 订阅 Event.Combat.* 一条命中一族事件——与状态关系表同一降维技巧
TMultiMap<FGameplayTag, FBuffEventSubscription> Subscriptions;

// 每帧 Flush：注册序 + 优先级；实例内产生的新事件重新入队（防重入/迭代失效）
for (const FCombatEvent& E : PendingEvents)
    for (FBuffEventSubscription& Sub : Subscriptions.Matching(E.EventTag))
        if (auto* B = Containers.Resolve(Sub.Buff))      // generation 校验
            Sub.Handler->Execute(B->GetView(), E);       // 直接函数调用，O(订阅数)
```

## 5. 三条纪律

1. **生命周期闭合**：Apply/Remove 时同步注册/注销订阅。UE delegate 自绑定最常见的"忘 Unbind → 悬挂崩溃"坑在此结构性消失。
2. **事件分级**：查询类事件（能否施法）立即同步返回；状态类事件（伤害/死亡）帧末统一泵，Buff 连锁产生的新事件下一轮处理——**重入与无限循环被结构性切断**。
3. **溢出策略自定**：队列长度与泵次序是自己的代码——对照 StateTree 事件队列 64 上限、溢出丢新事件的教训【源码调研：StateTreeEvents.cpp L45-49】，这里不再有引擎强加的丢弃语义。

## 6. 先例

- **Lyra `UGameplayMessageSubsystem`**【通识】：Tag 为键的消息总线 + `FGameplayMessageListenerHandle` 句柄式订阅——监听者不需要是 UObject，回调+句柄即全部。
- **TGFS Tag 消息枢纽**：内嵌、同步分发的 Tag 路由（MEM-20260826-06 设计）——同一构件，战斗系统事件层可直接复用或对齐。
- **Mass**【设计判断】：Processor 收事件按 chunk 分发，实例永不自绑。

## 7. 自绑定的合法残留区

不是全面禁令——**与具体资源生命周期绑死的一次性监听**（某次 Montage 的 AnimNotify 委托、某投射物的 OnHit）用 lambda 捕获 `{总线, 句柄}` 校验后路由，是干净的。禁的是把它当**通用事件机制**：每个 Buff 自绑 N 个 delegate，等于把分发顺序、生命周期、可观测性打散回对象森林。

## 8. 这笔 rent 换回什么

1. **生命周期安全**：无悬挂绑定，注销即闭合。
2. **顺序与确定性**：分发序 = 注册序 + 优先级；暂停/变速/回放在单点可控（自走棋硬需求）。
3. **单点可观测**：全部战斗事件流经一个泵——调试 overlay、日志、未来复制路由（服务端事件→选择性广播）都在一处插桩。
4. **同构红利**：与 TGFS 消息枢纽、Lyra 消息总线、Mass 同一条队伍，概念零翻译。

## 9. 决策状态

**已定**（2026-08-31）：
- 事件分发形态 = 总线 + 订阅表 + 共享 Handler 类；实例不自绑 delegate（§2~§4）。
- 三条纪律（§5）与自绑定残留区边界（§7）。

**待定**：
- [ ] 事件分级清单（哪些事件同步立即、哪些帧末泵——按语义逐个裁决）。
- [ ] 订阅索引数据结构选型（TMultiMap vs 按 Tag 分桶数组 vs 层级树匹配的缓存）。
- [ ] Tag 层级匹配的性能档位（匹配缓存/失效策略，规模到 200 单位×高频事件时验证）。
- [ ] 与 TGFS Tag 消息枢纽的关系：直接复用同一构件，还是战斗系统内建、仅语义对齐。
- [ ] Handler 基类接口形状（Execute 签名、参数打包方式、BP/C# 可见性设计）。
