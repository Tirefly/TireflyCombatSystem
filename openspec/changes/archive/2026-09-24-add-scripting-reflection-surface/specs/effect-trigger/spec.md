## MODIFIED Requirements

### Requirement: 触发行登记表与订阅生命周期

`TcsEffect` MUST 以门面 API 承载触发行的登记与摘除，并按 `EventTag` 装配总线订阅：

- `RegisterTriggerRow(const FTcsEffectTriggerInstance&) -> FTcsEffectTriggerHandle`：登记表持有（值语义）+ **按 `Def.EventTag` 装配总线订阅**。`EventTag` 或 `EffectChainId` 无效时 ensure + 返回无效句柄（配置错误，不静默接受）；
- `UnregisterTriggerRow(FTcsEffectTriggerHandle) -> bool`：摘除单行（先校验代际——失配即拒 + 返回 false，不 ensure）；
- `UnregisterTriggerRowsBySource(const FTcsSourceHandle&) -> int32`：按来源全量摘除（级联退订锚点，与 M2 `RemoveBySource` 同款语义）；
- **订阅计数配对（核心纪律）**：同一 `EventTag` 的多行**共用一条订阅**——首次出现该 Tag 时订阅一次，该 Tag 的行数归零时才退订。**MUST NOT** 每行各订一次（那会让总线订阅表随行数膨胀且退订易漏）；
- **订阅通道 = 立即**（`ETcsEventDispatch::EED_Immediate`）：收集协议要求"修正提交落在事件发布返回之前"（`TcsDamage` 的收集事件走立即通道），订帧末会让修正晚一拍；
- **点灯 API**：`SetTriggerGateTag(FGameplayTag, bool)` / `IsTriggerGateTagLit(FGameplayTag) const`——行级开关（`GateTags` 全部点亮才通过该道门）；
- **观测 API**：`GetTriggerRowCount() const`（装置断言用）；
- **求值器生命周期**：由门面在首次登记时创建并 **`UPROPERTY` 持有**——总线订阅表持**弱引用**（`FTcsEventSubscription::Handler` 是 `TWeakObjectPtr`），不 root 会被 GC 掉、订阅静默失效；
- `Deinitialize` MUST 清空登记表 + **全量退订**（不留跨世界残留订阅）；
- **脚本层可达（2026-09-24 补）**：上列方法中形参全为反射类型的部分（`RegisterTriggerRow` / `UnregisterTriggerRow` / `SetTriggerGateTag` / `IsTriggerGateTagLit` / `GetTriggerRowCount` / `SetTriggerRandomSeed`）MUST 标记 `UFUNCTION()`（无 specifier，口径见 `effect-interpreter` 的「门面反射面」需求）——这是"宿主用 C# 写技能/Buff 逻辑"的登记入口。**形参含非反射裸 struct 的 `UnregisterTriggerRowsBySource(const FTcsSourceHandle&)` 不在其列**（需先反射化 `FTcsSourceHandle`）。

#### Scenario: 同一事件 Tag 的多行共用一个订阅

- **WHEN** 连续登记 3 行，`EventTag` 均为 T
- **THEN** 总线订阅表中 Tag = T 的订阅恰有 1 条（而非 3 条）

#### Scenario: 末行摘除才退订

- **WHEN** 上述 3 行先摘 1 行、再摘 1 行
- **THEN** Tag = T 的订阅仍在（还有 1 行）；摘除最后 1 行后该订阅被退订

#### Scenario: 无效配置被拒

- **WHEN** 以空 `EventTag`（或空 `EffectChainId`）调 `RegisterTriggerRow`
- **THEN** 返回无效句柄 + 留 ensure 提示，登记表行数不变

#### Scenario: 求值器被 GC 持有

- **WHEN** 登记过至少一行后检查门面的持有面
- **THEN** 求值器对象被 `UPROPERTY` 持有（不因无强引用而被回收——否则订阅静默失效）

#### Scenario: 反初始化全量退订

- **WHEN** 门面 `Deinitialize`（世界销毁 / PIE 结束）
- **THEN** 登记表清空且全部订阅被退订

#### Scenario: 脚本层可登记触发行

- **WHEN** 宿主脚本层（C#）调 `RegisterTriggerRow` 并传入一个 `FTcsEffectTriggerInstance`
- **THEN** 调用成立（方法反射可见、形参类型 `FTcsEffectTriggerInstance` 反射可见），登记表行数 +1

## ADDED Requirements

### Requirement: 触发行句柄的反射性

`FTcsEffectTriggerHandle` MUST 是**反射可见类型**（`USTRUCT()`）且**值可跨语言往返**——脚本层调 `RegisterTriggerRow` 接住行句柄、再把它传回 `UnregisterTriggerRow` 摘除该行（2026-09-24 随门面反射面同批落地）。

- **字段 MUST 展平**（2026-09-24 实测修正，与 `FTcsChainRunHandle` 同根因同修法）：句柄 MUST 直接持有 `Index`/`Generation` 两个 `UPROPERTY int32` 字段，**MUST NOT** 内嵌 `TTcsInstanceHandle<T>`——后者是模板类型、无法作 `UPROPERTY`，会让绑定产物生成**空壳**（`ToNative`/`FromNative` 函数体为空）⇒ 脚本层接住句柄时读不到值、传回时写全零 ⇒ **代际失配、往返失效**（该缺陷在门面反射面打通前不可见——C# 此前根本调不到这些方法）；
- `Index` MUST 为 `int32`（UHT 不支持 `uint32` 作属性类型）；**无效值 `-1` 与 `TTcsInstanceHandle::InvalidIndex(0xFFFFFFFF)` 位模式相同**——MUST 经唯一转换点（`GetInner`/`SetInner`）与登记表内句柄互转，保证往返无损；
- **MUST NOT** 加 `BlueprintType`（口径同 `FTcsChainRunHandle`：运行期身份词、非配置数据；R0 §9 蓝图不承诺）。

#### Scenario: 行句柄值可跨语言往返

- **WHEN** 脚本层调 `RegisterTriggerRow` 接住返回的行句柄，随后原样传回 `UnregisterTriggerRow`
- **THEN** 摘除成功（句柄值完整往返：`Index`/`Generation` 均保持，代际校验通过）

#### Scenario: 行句柄字段可被脚本层读出

- **WHEN** 脚本层读取接住的行句柄的 `Index` / `Generation`
- **THEN** 读到的是登记表的真实值——绑定产物 MUST 为这两个字段生成真实的读写代码（非空壳）
