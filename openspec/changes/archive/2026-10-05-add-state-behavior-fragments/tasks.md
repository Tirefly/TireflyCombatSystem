## 0. 前置：TRIG-6 自筛、身份修复与 STAT-5 有限重入

- [x] 0.1 登记首个状态生命周期读取器（已有 Damage collect 在先）；状态载荷新增反射 Unit，从 Instance.Unit 填入。Caster 取有效 Instigator，否则取 Unit；Subject 装 FTcsStateHandle。
- [x] 0.2 载荷信息与行实例各持反射 FInstancedStruct Subject，状态行登记时绑定实例 Handle；两侧 Subject 均有效时须精确类型与反射值相等。全局未绑定行、外部无 Subject 事件保持原路由。Fragment 只直接识别状态载荷自筛，非状态载荷按兴趣扇出。
- [x] 0.3 同步四能力 delta：behavior 4 ADDED、state-def 1 MODIFIED、effect-trigger 3 MODIFIED、state-instance-lifecycle 4 MODIFIED；TRIG-6 验收后就地改判。
- [x] 0.4 同 Tag 两实例互不引爆；23g 恢复先主后次而期望仍为 +25/一次。行为读数覆盖跨单位隔离与外部事件扇出；全局触发行兼容以代码评审与既有整轮回归核验，未单独实测场景须如实留边界。
- [x] 0.5 用户甲案身份修复：StateRegistry 出线进程唯一正奇数代际，每次分配取新号、释放 +1 偶数、不随世界复位、int32 空间耗尽不回绕；保留两字段 API。读数验证不同单位同槽身份互异，跨 PIE 旧句柄不命中。
- [x] 0.6 STAT-5 有限守卫：InRemovalBroadcastHandles 只覆盖真正移除广播；同句柄递归 Remove false 静默，不重复广播/释放；Applied/Refreshed 自移除后按身份重查。保留既有清理顺序，不以任意 Expiring 判拒。装置覆盖三路回调；其它反复增删组合不外推。

## 1. 契约与载体

- [x] 1.1 `TcsState/Public/State/TcsStateBehaviorFragment.h`（新）：`FTcsStateBehaviorFragment`（`USTRUCT(meta = (Hidden))` + `Interests` + `virtual void OnStateEvent(const FGameplayTag&, const FInstancedStruct&, const FTcsStateBehaviorContext&) const` + 中性空实现，**禁 `= 0` / `PURE_VIRTUAL`**）+ `FTcsStateBehaviorContext`（纯 C++ 值语义：`Subsystem` / `Handle` / `Unit` / `DefTag` / `Stacks` / `Level`）
- [x] 1.2 `TcsBuffDef.h`：补 `Fragments: TArray<FInstancedStruct>`（`UPROPERTY(EditAnywhere, BlueprintReadOnly, ...)` + 手写 `meta = (BaseStruct = "/Script/TcsState.TcsStateBehaviorFragment")`）；头注释写明"载具换型口径 2026-09-24"
- [x] 1.3 `TcsBuffDefAsset::IsDataValid`：补两条作者期规则——`Fragments` 内**载荷空**或**类型不符**（`IsChildOf` 判定）⇒ Error；文案与既有六类错误同风格
- [x] 1.4 编译（Development Editor + Shipping；本变更新增 `UPROPERTY` ⇒ Shipping 必跑）

## 2. 订阅表与共享 Handler

- [x] 2.1 `TcsState/Public/State/TcsStateBehaviorHandler.h` + `Private/State/TcsStateBehaviorHandler.cpp`（新）：`UTcsStateBehaviorHandler : UTcsEventHandler`，唯一成员 = 门面弱引用；`HandleEvent_Implementation` 只转注册表路由（**不在 Handler 里做匹配**——回调拿不到订阅句柄，身份只能靠旁表认领）
- [x] 2.2 `TcsState/Public/State/TcsStateBehaviorRegistry.h` + `Private/State/TcsStateBehaviorRegistry.cpp`（新）：`SetHandler` / `Reset(Bus)` / `AddInstance(Handle, Def, Bus)` / `RemoveInstance(Handle, Bus)` / `CollectInterests(Tag, OutHandles)` / `GetSubscriptionCount()` / `GetInstanceCount()`；**每 Tag 一条订阅 + 计数配对**（首个订阅、末个退订）；头注释写明"本表零 GC 引用 ⇒ 不需要 ARO；Handler 强引用归门面 `UPROPERTY`"
- [x] 2.3 `UTcsStateSubsystem`：+`FTcsStateBehaviorRegistry BehaviorRegistry`（非 `UPROPERTY`）+ `UPROPERTY() TObjectPtr<UTcsStateBehaviorHandler> BehaviorHandler`（懒建，与触发行求值器的懒建同款）+ 私有接线入口（`friend class FTcsStateOps` 已有的可见性口径）

## 3. 接线两挂点

- [x] 3.1 `TcsState/Private/State/TcsStateOps_Behavior.cpp`（新）：`WireBehaviors(Subsystem, Instance, Def)` / `UnwireBehaviors(Subsystem, Instance)`——`Def.Fragments` 为空时**直接返回**（不建 Handler、不订阅、零开销）
- [x] 3.2 `Apply` 新实例路径：接线排在 `Broadcast(Applied)` **之前**（与 6a 的触发行登记同处）
- [x] 3.3 `Remove`：退订排在广播**之前**（与 6a 的触发行退订同处）；`RefreshStacked` 路径**不重订阅**
- [x] 3.4 `Deinitialize`：`BehaviorRegistry.Reset(GetEventBus())` 全量退订（跨世界零残留）

## 4. 宿主装置检查（LAC 仓，跨仓交付）

- [x] 4.1 `TcsDevBehaviorSample.h/.cpp`（新）：宿主行为 Fragment 样本（兴趣 `TcsEvent.State.Applied`，回调留一行 `Display` 作"被真实调用"的证据）——"框架零具体行为 Fragment"的现场证明
- [x] 4.2 `TcsDevSliceRig.cpp` 追加检查 **23i–23l**：23i 自身 Applied 与状态读取器/反射；23j 跨实例、跨单位隔离，刷新不重挂与外部事件扇出；23k Removed 前退订、三路有限重入与作者期校验；23l 真实哨兵两连 PIE 验证零残留（首轮不冒充跨世界已验）
- [x] 4.3 双配置编译 + 整轮 PIE：`Tcs.Test.Slice.Run`（即时 / 延迟段）与 `.Reject` 面读数留档；**头部预期红字不变**（本轮不加故意红字）

## 5. 规格与文档

- [x] 5.1 归档 `add-state-behavior-fragments`（**MUST 排在 6a `add-state-chain-primitives` 归档之后**——6b 的规格文本以 6a 归档后的主规格为准）；`openspec validate --all --strict --no-interactive` 全绿
- [x] 5.2 归档后**手工补**新能力 `state-behavior-fragments` 的 `## Purpose`（占位句必须换掉）
- [x] 5.3 证据文档 `Documents/combat-system-design/evidence/2026-10-05-state-behavior-fragments.md`（区段行号 / 字节 / SHA-256 / 复算脚本 / 如实边界清单）
- [x] 5.4 计划 Task 6 Step 5–7 勾选 + 《Task 6 落地结果》块；两册日志、台账、`INDEX`、`SPEC-02-states` §3.3 的 Fragment 机制条目状态同步（行为 Fragment 从"设计"改为"已落地（宿主样本证明）"）
