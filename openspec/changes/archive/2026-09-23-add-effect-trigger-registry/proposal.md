# Change: 触发行登记表与求值器（订阅生命周期 + 四道门）

## Why

`effect-trigger` 今天只有**数据形状**与**条件注册表**（Task 1，2026-09-23 归档），**零调用方**——没有任何东西能把一行触发行挂到事件总线上，也没有任何东西在事件到达时求值。`plan3` 的定位是"事件 → 触发行 → 效果链"闭环，缺的正是这一环：**订阅侧**。

本提案落地 `plan3` Task 2：登记表（含订阅计数配对）+ 求值器（四道门）+ 点灯 API + 载荷读取器。

### 三处必须交代的事（实施前必读）

**1. `plan3` 的一处内部矛盾（本提案修正）**：Task 2 注记写"`Caster` 的解析规则：从载荷内已知类型取（流程收集事件 → `Context->Attacker`）"——**这不可实现**。`TcsEffect` MUST NOT 认识任何领域载荷类型（依赖铁律 `Core←Attribute←Effect←{Damage,…}`，04 §1），它不可能写 `GetPtr<FTcsDamageFlowCollectEvent>()`。而 `FTcsTriggerContext.ClassificationTags` 若无来源，则 R4 随规格交付的 `FTcsTriggerCondition_HasAllTags` **恒不过**（空集上匹配非空 Tag 数组）——**一个出厂即不可用的条件**。故本提案引入**触发载荷读取器注册表**：由**载荷类型的属主模块**（TcsDamage 属主 = TcsDamage）登记"怎么从我的载荷里读主体信息"，TcsEffect 全程不认识具体类型。

**2. 事件订阅通道 = 立即（不是选择，是约束）**：`TcsDamage` 的收集事件走 `PublishImmediate`（`TcsDamageSubsystem.cpp:217`），而 `plan3` Task 3 的 `ModifyFlow` 依赖"**修正提交落在 `PublishCollectEvent` 返回之前**"。故触发行 MUST 订阅**立即通道**——若订帧末通道，修正会晚一拍，伤害已结算完。

**3. 槽位复用 MUST 配代际校验 + GC 补引用（对已归档规格的两处收紧）**：
- **代际**：`effect-trigger` 现规格说持有形态为 `TArray` 值语义 + "代际校验不适用"。**代际段不适用是对的（不引池类型），但"槽位复用无需代际校验"是错的**：登记表用空闲链表复用槽位后，**陈旧句柄会静默改指另一行**（`UnregisterTriggerRow(旧句柄)` 摘掉无辜的行）。句柄类型 `TTcsInstanceHandle` 本就携带 `Generation` 段——恒填 0 等于让类型对自身语义说谎。故收紧为：`TArray` + 空闲链表 + 代际计数（**仍不引入 `TTcsInstancePool` 类型**）。
- **GC**：`plan3` 注记以"值语义 `TArray` → 无需 ARO 覆写"判断不需要补引用——**判据错了**。是否需要 ARO 与"值语义/指针语义"**无关**，只取决于**容器是否 GC 可见**：`FTcsTriggerRegistry` 是门面的普通 C++ 成员（非 `UPROPERTY`），GC 的 `RefLink` **走不到它**，而行内 `FInstancedStruct`（`Conditions` / `EventPayloadFilter`）的**内层内存可放宿主自定义 struct 的 `UPROPERTY` 对象引用**（D4-16 类型不设限）→ 静默回收。这与 T-8 是同一类缺口（**缺口在容器，不在载荷**），故门面 MUST 逐行补引用。

## What Changes

- `effect-trigger` 的「触发行实例与级联退订」需求 → **MODIFIED**：持有形态收紧为"值语义 `TArray` + 空闲链表 + 代际计数"，并补"陈旧句柄被拒"场景。
- 新增「触发行登记表与订阅生命周期」需求：`RegisterTriggerRow` / `UnregisterTriggerRow` / `UnregisterTriggerRowsBySource` / 点灯 API；**同一 `EventTag` 多行共用一个订阅**（计数配对，归零即退订）；求值器由门面创建并 `UPROPERTY` 持有（总线订阅表持弱引用）；`Deinitialize` 全量退订。
- 新增「触发求值器与四道门」需求：`UTcsTriggerEvaluator : UTcsEventHandler`；门序 `事件 Tag 路由 → ExecutionGate → GateTags → Conditions → 起链`；`Priority` 大者先、同序按登记序（稳定排序，不得依赖哈希序）；起链装配（原载荷进 `EventPayload`）；条件未过的日志级别纪律。
- 新增「触发载荷读取器」需求：`FTcsTriggerPayloadReaderRegistry`（按载荷反射类型分派 + 自注册宏对）；未注册载荷类型**不 ensure**（默认构造 + Verbose）。

## 影响面

- **Affected specs**: `effect-trigger`（MODIFIED × 1 + ADDED × 3）
- **Affected code**:
  - 新：`Source/TcsEffect/Public/Trigger/TcsTriggerEvaluator.h` + `Private/Trigger/TcsTriggerEvaluator.cpp`（总线适配 + 四道门求值）
  - 新：`Source/TcsEffect/Public/Trigger/TcsTriggerPayloadReader.h` + `Private/Trigger/TcsTriggerPayloadReader.cpp`（载荷读取器注册表 + 宏）
  - 改：`Source/TcsEffect/Public/TcsEffectSubsystem.h` + `Private/TcsEffectSubsystem.cpp`（登记表 + 订阅生命周期 + 点灯 API + 求值器持有）
- **不改**：`TcsEffect.Build.cs`（零新依赖边——载荷读取器靠属主模块自登记，不靠 include）；既有链/解释器/步骤执行器注册表；条件注册表（Task 1 产物）。
- **新增跨模块消费面**：登记表 API 供宿主调用 → MUST 带导出宏（台账 T-9）。

## 关键决策与依据

| 决策 | 依据 |
|---|---|
| 同 `EventTag` 多行**共用一个订阅** | 每行各订一次会让总线订阅表随行数膨胀且退订易漏；计数配对（归零退订）保证不留死订阅 |
| 求值器是**单个共享对象**（`UPROPERTY` 持有） | 总线订阅表持**弱引用**（`FTcsEventSubscription::Handler` 是 `TWeakObjectPtr`，`TcsEventBus.h:66`）——不 root 会被 GC 掉、订阅静默失效（宿主 `UTcsDevScreenObserver` 的既有注释即实证） |
| 门序**固定**为 Tag 路由 → 闸 → 点灯 → 条件 | `ExecutionGate` 最廉价先判、`Conditions` 最贵（可含宿主自定义求值）最后判 |
| 同 `Priority` 按**登记序** | 确定性纪律——遍历顺序 MUST NOT 依赖容器哈希序（`TSet` 哈希序不确定的坑见 2026-09-22 plan2 Task 7 Step 1） |
| 订阅**立即通道** | 收集协议要求"修正落在发布返回前"（见上文第 2 点） |
| 载荷读取器走**注册表**（而非 TcsEffect 内建解析） | 依赖铁律；且与步骤执行器/条件注册表同款"属主自登记"惯例——不加深 TCS 内部两套分派机制的分裂 |
| 载荷读取器未注册时**不 ensure** | "载荷类型未知"不是契约违规（手动发布一条自定义事件是合法用法），故默认构造 + Verbose |
| 代际计数**自持**（不用 `TTcsInstancePool`） | 池的挂起锚/占用统计在触发行上零收益；但代际段本身必需（见上文第 3 点） |
| 门面 `AddReferencedObjects` **逐行补引用** | 登记表非 `UPROPERTY` 成员 → GC 的 `RefLink` 走不到；行内 `FInstancedStruct` 内层可放对象引用（T-8 同类缺口，见上文第 3 点） |

## 非目标

- **不做反射注册入口**（`UFUNCTION(BlueprintCallable)` 包装）：`FTcsStepExecute` / `FTcsTriggerConditionTest` / `FTcsTriggerPayloadReader` 都是 `TFunction`，不可反射。这是与步骤执行器注册表**共享的欠账**（CS 调研 §7.6 的 G-2），MUST **同批**解决——Task 1 已把该口径写进规格，本提案**沿用不破**（两处口径必须一致）。
- **不做** `EventPayloadFilter` 的裁剪语义（R4 只存不裁——载荷类型尚无字段可筛）。
- **不做** `InterruptPriority` 的链打断语义（只存不裁）。
- **不做** `TEG_AuthorityOnly` 的判别实现（单机下本地即权威，与 `TEG_Always` 同判；判别逻辑随网络姿态轮）。
- **不做** TcsDamage 侧的载荷读取器登记（属主模块的登记随 Task 3 同批——那时 `TcsDamage` 本来就要新开文件）。
- **不做** `Targets` 的事件目标装填（台账 R5-1：完整形态需真实带目标的载荷类型；R4 的消费者 `ModifyFlow` 不需要目标）。
