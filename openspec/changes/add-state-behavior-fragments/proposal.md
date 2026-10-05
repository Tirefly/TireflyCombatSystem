# Change: 状态行为 Fragment（订阅挂接与来源级联退订）

## Why

`D3-7 v2` 把状态的可扩展面切成两种 Fragment：**决策 Fragment**（Task 5 已落：五轴共存决策）与**行为 Fragment**（本轮落）。行为 Fragment 是"状态想对世界里发生的事做点什么"的作者面——兴趣 Tag 列表 + 一个泛化回调，作者零订阅管理代码。今天它**完全不存在**（全仓 `FTcsStateBehaviorFragment` / `OnStateEvent` / `Fragments` 零命中），Def 侧也没有载体字段（Task 1 明文"本轮 MUST NOT 预建：行为 Fragment 的兴趣 Tag 已是声明面…等真实消费者"——**本轮就是那个消费者**）。

本变更把行为 Fragment 的三件事一次落齐：**契约**（基类 + 上下文）、**载体**（Def 持有、实例零策略）、**接线**（施加订阅 / 移除退订，与内联触发行同处同序）。

## What Changes

- **`FTcsStateBehaviorFragment`（新，`USTRUCT(meta = (Hidden))`）**：`Interests: TArray<FGameplayTag>` + `virtual void OnStateEvent(const FGameplayTag& EventTag, const FInstancedStruct& Payload, const FTcsStateBehaviorContext& Ctx) const`——**中性默认实现（空实现）+ 禁 `= 0` 与 `PURE_VIRTUAL`**（与 Task 5 决策 Fragment 同款纪律）；回调 **MUST 为 `const`**（片段住 Def 资产，解析出来是 `const FTcsBuffDef*`）。
- **`FTcsStateBehaviorContext`（新，纯 C++ 值语义）**：`Subsystem` / `Handle` / `Unit` / `DefTag` / `Stacks` / `Level`——回调上下文一屏可读，**不含指针到实例本身**（实例会因池扩容搬移）。
- **Def 载体**：`FTcsBuffDef::Fragments: TArray<FInstancedStruct>` + 手写 `meta = (BaseStruct = "/Script/TcsState.TcsStateBehaviorFragment")`（全仓 2026-09-24 换型口径；`TInstancedStruct<T>` 字段在脚本层导出为空壳）。
- **订阅表 + 共享 Handler**：`FTcsStateBehaviorRegistry`（纯逻辑类）**每个兴趣 Tag 只订阅一条**（计数配对：首个实例订阅、末个实例退订）+ 一个共享 `UTcsStateBehaviorHandler`（`UTcsEventHandler` 派生，弱引用，强引用归门面 `UPROPERTY`）——先例 = `FTcsChainEventWaitRegistry`（总线派发只传 `(EventTag, Payload)`、不传订阅句柄 ⇒ 每实例各订一次收益为零、成本为正）。
- **接线时机**：`Apply` 新实例路径在 `Applied` 广播**之前**订阅；`Remove` / 到期在广播**之前**退订；刷新 / 叠层路径**不重订阅**（同一实例）；世界反初始化全量退订。
- **实例零策略不破**：`FTcsStateInstance` MUST NOT 存 Fragment / 载荷 / 订阅句柄（D3-7 v2）——订阅句柄住注册表、路由靠"事件 Tag → 兴趣该 Tag 的在册实例"的旁表。

## 待评审的裁定

1. **载体字段落 `FTcsBuffDef`（派生）而不是 `FTcsStateDefBase`（基类）**：判据 = 既有同层先例（`Triggers` 与 `StackPolicy` 都住派生）+ "零消费者不预建"（R6 的技能要不要行为 Fragment 今天无人知道；将来要，抬到基类是纯加法）。基类那条"无时值 / 无堆叠 = 基类"的判据不受影响。
2. **每 Tag 一条订阅（不搞每实例一个 Handler）**：判据 = 先例 `FTcsChainEventWaitRegistry` 的两条硬依据（总线 `Subscribe` 只收 `UTcsEventHandler*`、派发不传订阅句柄 ⇒ 回调无法自辨身份；每实例各订一次会让订阅表随节点数膨胀且退订易漏）。
3. **`Interests` 只收事件 Tag**：词 MUST 落既有 `TcsEvent` 根（如 `TcsEvent.State.Applied` / `TcsEvent.Damage.Pre`），**MUST NOT** 为行为兴趣另开词根（`gameplay-tag-governance` 的"一角色一根"）。
4. **`OnStateEvent` 的 `const` + 中性空实现**：片段是**配置**不是运行态；要改状态/属性一律经 `Ctx.Subsystem` 的门面（副作用有门面兜纪律），片段自身不留可变状态（否则"同一 Def 被多个实例共享"会让状态串味）。
5. **兴趣匹配 = 精确 Tag**（不做层级展开）：总线路由本身按精确 Tag 建索引（`TagIndex`），层级匹配是监听节点自己的过滤面（`UTcsAsyncAction_ListenForCombatEvent`）；行为 Fragment 走精确匹配并如实写进注释，避免"配了父 Tag 以为会命中"的沉默。
6. **回调内重入同一实例 = 未定义面（如实登记）**：回调里调用会改动在册表的状态操作（移除自己 / 注销单位）时，注册表以"快照 + 代际校验"接住失效句柄（先例同总线派发），但**同一回调内对自己重复增删**不做额外保证；登记台账。

## Impact

- **Affected specs**（插件仓 TCS `openspec/`）：
  - `state-behavior-fragments`（**ADDED**，新能力 3 条：「行为 Fragment 契约」、「Def 载体与实例零策略」、「行为订阅的生命周期接线」）
  - `state-def-asset`（**MODIFIED** 1 条：「状态 Def 数据形状」——`FTcsBuffDef` 补 `Fragments` 字段，并订正"本轮 MUST NOT 预建"那句里已过期的理由）
  - `state-instance-lifecycle`（**不改**：撤销 / 接线顺序的权威文本住「广播点与阶段迁移」，其中已留指针指向本能力的生命周期接线需求）
- **Affected code**（插件仓 TCS）：
  - 新：`TcsState/Public/State/TcsStateBehaviorFragment.h`（Fragment 基类 + 上下文）、`TcsState/Public/State/TcsStateBehaviorRegistry.h` + `Private/State/TcsStateBehaviorRegistry.cpp`（订阅表）、`TcsState/Public/State/TcsStateBehaviorHandler.h` + `Private/State/TcsStateBehaviorHandler.cpp`（共享 Handler）、`TcsState/Private/State/TcsStateOps_Behavior.cpp`（接线两挂点）
  - 改：`TcsState/Public/Def/TcsBuffDef.h`（+`Fragments`）、`TcsState/Public/TcsStateSubsystem.h`（+注册表与 Handler 成员、私有接线入口）、`TcsState/Private/State/TcsStateOps.cpp`（两处接线调用）、`TcsState/Private/TcsStateSubsystem.cpp`（`Deinitialize` 全量退订）
- **Affected code**（宿主仓 LAC `Source/TcsDev/`）：`TcsDevSliceRig.cpp` 追加检查 **23i–23l**（行为 Fragment 收事件 / 退订后不再收 / 订阅数按 Tag 不随实例数膨胀 / 世界拆解后零残留）；`TcsDevBehaviorSample.h/.cpp`（新：宿主行为 Fragment 样本，与 Task 5 的决策样本同款——"框架零具体策略"的现场证明）
- **资产迁移**：无（新字段默认空 ⇒ 既有定义资产行为不变）

## 非目标

- **内置行为策略**：框架零具体行为 Fragment（宿主样本住 LAC `TcsDev`）——与 Task 5"框架零具体决策策略"同口径。
- 行为 Fragment 的**脚本可达面**（C# 派生）：虚分派在脚本侧物理不可达（`TcsEffect` 条件注册表处已实证），归 `LEDGER-reflection` R-2 一族。
- 兴趣 Tag 的**层级匹配**（见裁定 5）。
- 片段的**实例 Variables 通道**（D3-7 v2 的 per-instance 扩展状态）：今天零消费者。
