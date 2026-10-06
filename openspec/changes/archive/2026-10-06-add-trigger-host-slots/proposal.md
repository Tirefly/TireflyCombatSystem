# Change: 触发行条件求值器与载荷读取器的宿主脚本插槽（`R-2` 后段）

## Why

两张注册表（`FTcsTriggerConditionRegistry` / `FTcsTriggerPayloadReaderRegistry`）的**动态登记入口目前是纯 C++ 面**——注册值是 `TFunction`，无 `USTRUCT` 宏、无法进 UHT 签名面，故方法无法标 `UFUNCTION()`。设计意图（D4-17 双入口之脚本可达面）要求它们可被宿主脚本层触达。

`LEDGER-reflection` 的 `R-2` 欠账由两部分组成，**寿命那一半已于 2026-09-29 随提案 `harden-registry-cross-world-lifetime` 闭环**（对象/世界弱引用、失效判据、拒绝门收窄、`Unregister` / `GetDynamicKeys`、门面 `Deinitialize` 按世界撤销，四张注册表全覆盖）。**本提案只补剩下的一半：反射入口。**

真实消费者已出现：宿主专属条件类型与宿主专属 **SkillCost** 求值（用户 2026-09-27 点名）。`PLN-R5` 与台账已两次排期"**R-2 后段 → 排到 R6 开工前**"，本轮（R6 = M5 技能层）兑现。

`effect-trigger` 现行规格已**预先约束**本次工作（两处，条件求值器与载荷读取器各一条）：

> 当 `LEDGER-reflection` R-2 为条件求值器新增**宿主脚本插槽**（UObject 基类形态）时，其登记入口 MUST 直接满足上述寿命语义——MUST NOT 先按旧口径落地再返工。

本提案**保持该约束的实质（直接满足寿命语义）、订正其括注的形态**——理由见下。

## What Changes

- **`FTcsTriggerContext` 与 `FTcsTriggerPayloadInfo` 升格 `BlueprintType`**——`BlueprintNativeEvent` 触发 UHT 蓝图参数校验，非 `BlueprintType` 的 `USTRUCT` 无法作该签名形参（同 SCRIPT-8 对 `FTcsChainRunHandle` 的处置）；
- **新增两个宿主契约接口**（`UINTERFACE(MinimalAPI, Blueprintable)` + `UFUNCTION(BlueprintNativeEvent)`）：
  - `ITcsTriggerConditionEvaluator::Test(const FInstancedStruct& ConditionData, const FTcsTriggerContext& Context, double RandomValue) -> bool`；
  - `ITcsTriggerPayloadReader::Read(const FInstancedStruct& Payload) -> FTcsTriggerPayloadInfo`；
- **门面新增两个薄转发登记口**（裸 `UFUNCTION()`，口径同门面其余方法）：`RegisterConditionEvaluator` / `RegisterPayloadReader`——内部把 `TScriptInterface` 包成 `TFunction` 转发进**既有**注册表，**键与查表逻辑零改动**；两注册表的 `Unregister` 语义对动态脚本项同样适用；
- **GC 与寿命**：宿主对象由门面 `UPROPERTY` 数组强持有（裸 C++ 注册表持不住对象引用，不持有则被静默回收 = WAIT-8 缺陷形态）；寿命校验**复用**已落地的对象/世界弱引用，不新增机制；
- **双轨并存**：内置条件（`HasAllTags` / `Chance` / `AttributeCompare`）与属主模块的领域载荷读取器**继续走静态自注册 C++ 快路径**，注册值类型与查表逻辑 MUST NOT 变动；插槽只服务宿主扩展。

**形态订正（BREAKING 于文档口径，非于代码）**：现行规格两处的括注"（**UObject 基类形态**）"与 `LEDGER-reflection` 家族表的"统一到转发器模式"**均被本提案取代**：

- **为什么不采 `UObject` 基类**：本次要让宿主**既有类**（角色 / 组件 / 任意 `UObject`）直接实现插槽——`UINTERFACE` 允许一个类同时挂多个插槽，宿主不必为每个插槽专造一个对象；且宿主面与 R-1（参数源）/ P-A（评分器）/ 选择器族**同形**。
- **为什么不采 USTRUCT 转发器**：转发器只在"内置扩展点是 `USTRUCT` 虚分派"时才需要（脚本定义的结构体无 C++ 类型 ⇒ `CppStructOps == nullptr` ⇒ vtable 位为 0 ⇒ 野调用）。**两张注册表的注册值是 `TFunction`、没有策略基类可继承** ⇒ 转发器无对象可接；要造它就必须先发明一个 `USTRUCT` 策略基类并**改掉注册值类型**——那正是 SCRIPT-8 刻意绕开的 SCRIPT-2 路线（`TcsStepExecutor.h` 自注"比换 `TFunction` 签名改动面小得多"）。
- **语言可达性不受影响**：语言可达性由"宿主面对的契约是否为 `UObject` 反射类型"决定，与转发器、注册值载体皆无关——四处已落地插槽形态各异但宿主面全是 `Blueprintable`，均已实测语言可达（C# / AS / Lua / TS / 蓝图）。

## Impact

- Affected specs: `effect-trigger`（MODIFIED ×2：条件求值器需求、载荷读取器需求）；`LEDGER-reflection`（`R-2` 行与《宿主插槽家族的耦合关系》表三处文本订正——非 OpenSpec 能力）
- Affected code:
  - `Source/TcsEffect/Public/Trigger/TcsTriggerCondition.h`（`BlueprintType` 升格 + 接口 + 门面口）
  - `Source/TcsEffect/Public/Trigger/TcsTriggerPayloadReader.h`（同上）
  - `Source/TcsEffect/Public/TcsEffectSubsystem.h`（两个登记口 + 两个 `UPROPERTY` 持有数组）
  - `Source/TcsEffect/Private/Trigger/TcsTriggerHostSlots.cpp`（新——`TFunction` 包装与注册实现）
  - 宿主装置（**LAC 仓**）：`Source/TcsDev/…/TcsDevSkillCostProbe.*` + `Script/LegendAutoChessCS/`（**复用**既有 `ATcsHostScriptingE2EProbe` 探针，MUST NOT 另写）
- **不改动**：两张注册表的键、查表逻辑、寿命语义与 `Unregister` 面；内置条件与领域读取器的静态自注册路径；总线语义。
- **风险**：`FTcsTriggerContext` 含非 `UPROPERTY` 的裸 `const UWorld* World`——升格 `BlueprintType` 前 MUST 实测 UHT 对非 `UPROPERTY` 成员的处置（若被拒则改为门面按句柄访问器取，或移入派生上下文）。该项为实施第一步，不预设结论。
