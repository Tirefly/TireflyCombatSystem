## 1. 契约与载体

- [ ] 1.1 `TcsState/Public/State/TcsStateBehaviorFragment.h`（新）：`FTcsStateBehaviorFragment`（`USTRUCT(meta = (Hidden))` + `Interests` + `virtual void OnStateEvent(const FGameplayTag&, const FInstancedStruct&, const FTcsStateBehaviorContext&) const` + 中性空实现，**禁 `= 0` / `PURE_VIRTUAL`**）+ `FTcsStateBehaviorContext`（纯 C++ 值语义：`Subsystem` / `Handle` / `Unit` / `DefTag` / `Stacks` / `Level`）
- [ ] 1.2 `TcsBuffDef.h`：补 `Fragments: TArray<FInstancedStruct>`（`UPROPERTY(EditAnywhere, BlueprintReadOnly, ...)` + 手写 `meta = (BaseStruct = "/Script/TcsState.TcsStateBehaviorFragment")`）；头注释写明"载具换型口径 2026-09-24"
- [ ] 1.3 `TcsBuffDefAsset::IsDataValid`：补两条作者期规则——`Fragments` 内**载荷空**或**类型不符**（`IsChildOf` 判定）⇒ Error；文案与既有六类错误同风格
- [ ] 1.4 编译（Development Editor + Shipping；本变更新增 `UPROPERTY` ⇒ Shipping 必跑）

## 2. 订阅表与共享 Handler

- [ ] 2.1 `TcsState/Public/State/TcsStateBehaviorHandler.h` + `Private/State/TcsStateBehaviorHandler.cpp`（新）：`UTcsStateBehaviorHandler : UTcsEventHandler`，唯一成员 = 门面弱引用；`HandleEvent_Implementation` 只转注册表路由（**不在 Handler 里做匹配**——回调拿不到订阅句柄，身份只能靠旁表认领）
- [ ] 2.2 `TcsState/Public/State/TcsStateBehaviorRegistry.h` + `Private/State/TcsStateBehaviorRegistry.cpp`（新）：`SetHandler` / `Reset(Bus)` / `AddInstance(Handle, Def, Bus)` / `RemoveInstance(Handle, Bus)` / `CollectInterests(Tag, OutHandles)` / `GetSubscriptionCount()` / `GetInstanceCount()`；**每 Tag 一条订阅 + 计数配对**（首个订阅、末个退订）；头注释写明"本表零 GC 引用 ⇒ 不需要 ARO；Handler 强引用归门面 `UPROPERTY`"
- [ ] 2.3 `UTcsStateSubsystem`：+`FTcsStateBehaviorRegistry BehaviorRegistry`（非 `UPROPERTY`）+ `UPROPERTY() TObjectPtr<UTcsStateBehaviorHandler> BehaviorHandler`（懒建，与触发行求值器的懒建同款）+ 私有接线入口（`friend class FTcsStateOps` 已有的可见性口径）

## 3. 接线两挂点

- [ ] 3.1 `TcsState/Private/State/TcsStateOps_Behavior.cpp`（新）：`WireBehaviors(Subsystem, Instance, Def)` / `UnwireBehaviors(Subsystem, Instance)`——`Def.Fragments` 为空时**直接返回**（不建 Handler、不订阅、零开销）
- [ ] 3.2 `Apply` 新实例路径：接线排在 `Broadcast(Applied)` **之前**（与 6a 的触发行登记同处）
- [ ] 3.3 `Remove`：退订排在广播**之前**（与 6a 的触发行退订同处）；`RefreshStacked` 路径**不重订阅**
- [ ] 3.4 `Deinitialize`：`BehaviorRegistry.Reset(GetEventBus())` 全量退订（跨世界零残留）

## 4. 宿主装置检查（LAC 仓，跨仓交付）

- [ ] 4.1 `TcsDevBehaviorSample.h/.cpp`（新）：宿主行为 Fragment 样本（兴趣 `TcsEvent.State.Applied`，回调留一行 `Display` 作"被真实调用"的证据）——"框架零具体行为 Fragment"的现场证明
- [ ] 4.2 `TcsDevSliceRig.cpp` 追加检查 **23i–23l**：23i 自己的 `Applied` 唤起行为一次；23j 移除时**不**因自己的 `Removed` 唤起（退订先于广播）；23k 同 Tag 两实例共用一条订阅（订阅数不随实例数增长）；23l 世界拆解后零残留（重开会话再跑一遍）
- [ ] 4.3 双配置编译 + 整轮 PIE：`Tcs.Test.Slice.Run`（即时 / 延迟段）与 `.Reject` 面读数留档；**头部预期红字不变**（本轮不加故意红字）

## 5. 规格与文档

- [ ] 5.1 归档 `add-state-behavior-fragments`（**MUST 排在 6a `add-state-chain-primitives` 归档之后**——6b 的规格文本以 6a 归档后的主规格为准）；`openspec validate --all --strict --no-interactive` 全绿
- [ ] 5.2 归档后**手工补**新能力 `state-behavior-fragments` 的 `## Purpose`（占位句必须换掉）
- [ ] 5.3 证据文档 `Documents/combat-system-design/evidence/2026-10-05-state-behavior-fragments.md`（区段行号 / 字节 / SHA-256 / 复算脚本 / 如实边界清单）
- [ ] 5.4 计划 Task 6 Step 5–7 勾选 + 《Task 6 落地结果》块；两册日志、台账、`INDEX`、`SPEC-02-states` §3.3 的 Fragment 机制条目状态同步（行为 Fragment 从"设计"改为"已落地（宿主样本证明）"）
