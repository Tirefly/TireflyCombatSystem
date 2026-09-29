# TCS 反射未解决项清单与专项调研登记册

- **文档 ID**：`LEDGER-reflection`
- **类型**：LEDGER / 台账
- **状态**：LIVING
- **权威范围**：反射可达性未解决项 R-1~R-6 与逐项专项调研记录
- **最后更新**：2026-09-27

- 建立：2026-09-24（用户拍板：把反射相关的未解决项全部列出留档，然后**逐项专项调研**）
- 性质：**反射专项的未解决项留档 + 逐项调研登记册**——只收"反射可达性"这一类的未解决项
- 与台账 `deferred-inputs-ledger.md` 的分工：台账收**所有**"决策已拍板、未落地"的输入；本册只收**反射可达性**一类，且额外承担"逐项调研"的登记。**调研结论经用户确认后**，需实施的转台账（归 R/S 轮）、需改规格的走提案——本册不重复登记已转走的条目（仅留关联指针）
- 与 `reflection-terminology.md` 的分工：规约管"词怎么写"，本册管"哪些事没做"

## 工作流（用户 2026-09-24 定）

1. **留档**——未解决项全部列出（本册）；
2. **逐项调研**——一次一项，调研产出「现状 / 根因 / 可选解法 / 影响面 / 开放问题」；
3. **用户评审**——每项调研完，停下给用户看；
4. **讨论**——开放问题逐条拍板；
5. **落档**——结论写入本册「调研记录」，状态更新，需实施的转台账。

---

## 未解决项总表

| ID | 事项 | 根因类 | 现状 | 归属 | 状态 |
|---|---|---|---|---|---|
| **R-1** | **参数源族宿主插槽**（`FTcsParamValueSource` 虚分派脚本不可达） | ② 虚分派 | 未解决（SCRIPT-8 只覆盖选择器族，参数源族无转发器） | **SCRIPT-8 归档后独立小提案**（用户 2026-09-24 拍板） | **✅ 已调研并拍板**（2026-09-24；三项开放问题全定，待落地） |
| **R-2** | **条件求值器 / 载荷读取器注册表反射入口**（SCRIPT-2 未被 SCRIPT-8 替代的部分） | ③ 签名 | 部分被 SCRIPT-8 替代（步骤执行器有 UObject 基类，条件/载荷读取器没有）；**另含跨世界寿命缺陷**（SCRIPT-8 已落地的步骤执行器即存在） | **R-1 紧邻独立提案**（用户 2026-09-27 拍板） | **✅ 已调研并拍板**（2026-09-27；含寿命缺陷记录，待落地） |
| **R-3** | **`ITcsEntityQuery` 反射化**（`EnumerateEntities` 的 `TFunctionRef` 形参 + 门面 `SetEntityQuery` 无 `UFUNCTION`） | ③ 签名 | 并入 SCRIPT-8 但 SCRIPT-8 非目标明示"不做" | 台账 SCRIPT-5 | **⚖ 已裁决：不换**（2026-09-29，`DEC-04` 裁定 ③）——`TFunctionRef` = 宿主提供回调、框架遍历中同步调用且**不持有**；UObject 化会把契约由"拉"改"推"、把分配推进热路径，且 R0 §9 明文"蓝图不承诺"。**不再调研、不再挂待办** |
| **R-4** | **上下文整体反射化**（SCRIPT-3：`FTcsEffectContext` 可做 / `FTcsChainRun` 需包壳 / `FTcsDamageFlowContext` 依赖 OnConsumed 分层） | ① 不可反射成员 + 结构形状 | 降为可选（SCRIPT-8"传句柄"已绕过主要场景） | 台账 SCRIPT-3 + DAMAGE-4（前置） | 待调研（**前置 = R-6 落定**）——R-6 已由 `DEC-04` 裁定 ④ 定向：走"`OnConsumed` 改事件/原语语义"，**与 DAMAGE-4 同批** |
| **R-5** | **`FTcsSourceHandle` 反射化 + `FindChain` 裸指针**（SCRIPT-1 未覆盖项） | ③ 签名 | SCRIPT-1 已消费但明示"未覆盖留待后续" | 台账 SCRIPT-1 未覆盖 | 待调研 |
| **R-6** | **OnConsumed / 消耗语义**（规格欠账；`TFunction` 是 `FTcsDamageFlowContext` 反射化的唯一根因） | ① 不可反射成员 | 已登记台账 DAMAGE-4（含完整证据链） | 台账 DAMAGE-4（R5/M4a） | **⚖ 已裁决（2026-09-29，`DEC-04` 裁定 ④）**：`FTcsConsumePolicy::OnConsumed` 的 `TFunction<void()>` **改为事件/原语语义**（可反射、可复制、可脚本可达），**与 DAMAGE-4 同批**；形状待 DAMAGE-4 落定时定。`DEC-04` §3.3 补齐了本条此前缺的量化证据（三个字段全库零消费者、`BestIndex` 赋值后无使用点） |

**根因类**（沿用 S 系列判据表）：
- ① **不可反射成员**——`TFunction`/闭包作 struct 成员 ⇒ 递归传染，宿主类型无法加 `USTRUCT()`；
- ② **虚分派不可达**——脚本 struct 无 C++ 类型 ⇒ 无 vtable ⇒ 野调用，引擎层面无解 ⇒ 只能"转发器"；
- ③ **签名/形参不合规**——`TFunction`/`TFunctionRef`/非反射 struct 作 `UFUNCTION` 形参 ⇒ UHT 拒绝 ⇒ 需换签名或加替代入口。

---

## 明细

### R-1 参数源族宿主插槽（`FTcsParamValueSource` 虚分派脚本不可达）

**事项**：`FTcsParamValueSource` 走 C++ 虚分派（D3-7 v3 策略形态：`USTRUCT` 基类 + `virtual double Evaluate(...)`），脚本定义的 struct **物理不可达**（无 C++ 类型 ⇒ `CppStructOps == nullptr` ⇒ vtable 位为 0 ⇒ 野调用，与选择器族同根因）⇒ 宿主无法用脚本定义"新数值来源"。**选择器族已有 SCRIPT-8 转发器**（`FTcsSelHostDelegate` / `FTcsFilterHostDelegate`），**参数源族没有对应物**。

**影响面**：`FTcsParamValue.Source` 是**全插件数值载体**——链步骤 `DamageBase`、修正器 `Operand`、流程 `Operand`、`FormulaParams` 全走它。所有需要"数值来源策略"的地方，脚本都配不了新源类型。

**现状证据（已核）**：
- `Source/TcsCore/Public/Parameter/` 只有 `TcsParamSource_Literal` / `TcsParamSource_ParamRef`（+ `TcsParamTableReader` 接口），**无任何 Host/转发类型**；全库 grep `ParamSourceHost` 零命中；
- 台账 SCRIPT-8 原文自认："PV-5（'delegate/接口插槽（宿主 C++ 绑定）'——当时只规划到参数源，**未延伸到选择器**，本条补上）"——即**参数源的插槽在 PV-5 已规划、但从未落地**；SCRIPT-8 补的是选择器，参数源反而留在原地；
- `TcsParamValueSource.h:14`（上下文 MUST 反射可见的注释）与 `:62`（基类声明）确认当前形态。

**归属**：建议 R5（与 `ApplyState` / `ModifyAttribute` 同批——这些步骤都要配数值源，是真实消费者出现之时）。

**状态**：**✅ 已调研并拍板**（2026-09-24 启动、同日拍板；三项开放问题全定，见「调研记录 · 调研 R-1：参数源族宿主插槽（2026-09-24，**✅ 已拍板**）」）——与总表 R-1 行同口径；**待落地**（独立小提案，紧邻 R-2）。

---

### R-2 条件求值器 / 载荷读取器注册表反射入口（SCRIPT-2 未被 SCRIPT-8 替代的部分）

**事项**：`FTcsTriggerConditionRegistry::Register` / `FTcsTriggerPayloadReaderRegistry::Register` 的形参是 `TFunction`（不可反射）⇒ 脚本层无法登记**条件求值器**与**载荷读取器**。步骤执行器已被 SCRIPT-8 的 `UTcsStepExecutor` / `UTcsFlowStepExecutor`（UObject 基类）覆盖，但**条件 / 载荷读取器没有对应的 UObject 基类替代**。

**现状证据（已核）**：
- `TcsTriggerCondition.h:136` / `TcsTriggerPayloadReader.h:122` 自注"反射面欠账（明示，非遗漏）：Register 目前是纯 C++ 面（TFunction 不可反射）"；
- 台账 SCRIPT-2 状态"**定位降级（2026-09-24）**：SCRIPT-8 的 UObject 执行器基类路线可替代**多数**场景……本条仅对'必须用纯 struct 执行器'仍有意义"——**"多数"≠全部**，条件/载荷读取器无替代；
- SCRIPT-8 落地清单只有两个执行器基类（`UTcsStepExecutor` / `UTcsFlowStepExecutor`），无条件/载荷读取器对应物；
- **调研补充（2026-09-27）**：①两张注册表**不在任何门面上**（`TcsEffectSubsystem.h` 零命中）；②**载荷读取器宏零调用**（`UE_DEFINE_TRIGGER_PAYLOAD_READER` 全库只有定义、无登记）；③条件求值器**热路径**（每候选行每条件一次）、载荷读取器**每事件一次**（各行共用）；④`FTcsTriggerContext` / `FTcsTriggerPayloadInfo` glue 产物均为**真字段读写**（可直接作 `BlueprintNativeEvent` 形参），但都是 `USTRUCT()` 非 `BlueprintType`——须升格（SCRIPT-8 已确立的插槽硬约束）；⑤**发现跨世界寿命缺陷**（SCRIPT-8 已落地的步骤执行器即存在——注册表进程级单例 vs 门面 `UWorldSubsystem` 持有，PIE 切换后 stale 指针）。

**归属**：**R-1 紧邻独立提案**（用户 2026-09-27 拍板）。

**状态**：✅ 已调研并拍板（2026-09-27；含跨世界寿命缺陷记录，见「调研记录 · 调研 R-2」；待落地）。

---

### R-3 `ITcsEntityQuery` 反射化（SCRIPT-5）

**事项**：`ITcsEntityQuery::EnumerateEntities(TFunctionRef<void(FTcsCombatEntityHandle)>)` 含 `TFunctionRef` 形参（不可反射）⇒ 脚本层无法实现**实体遍历**；且门面 `SetEntityQuery` / `GetEntityQuery` 无 `UFUNCTION`。SCRIPT-8 的 6 个按句柄访问器覆盖了"读写目标集 / 变量 / 主体"，但**实体遍历无替代**（"找所有敌人"仍需实体查询）。

**现状证据（已核）**：
- `TcsEntityQuery.h:32` 自注"C++ 专用面：参数含 TFunctionRef，蓝图不可表达"；
- SCRIPT-8 proposal 非目标明文"**不做 `ITcsEntityQuery` 换签名**（台账 SCRIPT-5）——本提案的 `GetRunTargets` 等访问器已覆盖'脚本读/写目标集'这一主要诉求；实体遍历（`EnumerateEntities`）的 `TFunctionRef` 换型另轮"；
- 台账 SCRIPT-5 状态"并入 SCRIPT-8（2026-09-24 定位）"——但 SCRIPT-8 落地清单**不含它**，实际仍待办。

**归属**：台账 SCRIPT-5。

**状态**：待调研。

---

### R-4 上下文整体反射化（SCRIPT-3）

**事项**：三个上下文的三档难度——
- `FTcsEffectContext`：字段全反射（句柄×2 + `FInstancedStruct` + `TArray<句柄>` + `TMap<tag,double>`），**可做**（但规格禁令已拆捆绑，见 `reflection-terminology.md` 与 `effect-chain/spec.md` 修订）；
- `FTcsChainRun`：含 `TWeakObjectPtr` + 裸模板句柄 `TTcsInstanceHandle` + `FTcsTimeEntryHandle`——第二道坎，包 USTRUCT 可解（先例 `FTcsCombatEntityHandle` 展平）；
- `FTcsDamageFlowContext`：**依赖 R-6 分层**（`OnConsumed` 摘出前物理不可反射）。

**现状证据（已核）**：台账 SCRIPT-3 状态"**降为可选（2026-09-24）**：SCRIPT-8 的插槽签名传句柄而非上下文 struct……对'宿主专属语义脚本化'目标，本条不再是必经之路。仅在'脚本必须直接持有/构造上下文 struct'时才有必要"。

**归属**：台账 SCRIPT-3 + DAMAGE-4（`FTcsDamageFlowContext` 侧前置）。

**状态**：待调研（`FTcsDamageFlowContext` 侧前置 = R-6 落定）。

---

### R-5 `FTcsSourceHandle` 反射化 + `FindChain` 裸指针（SCRIPT-1 未覆盖项）

**事项**：两个 SCRIPT-1 明示未覆盖的缺口——
- `UnregisterTriggerRowsBySource(const FTcsSourceHandle&)`：形参 `FTcsSourceHandle` 是**无 `USTRUCT()` 宏的纯 C++ struct** ⇒ 该方法脚本不可达；
- `FindChain`：返回**裸 struct 指针**（`const FTcsEffectChain*`）⇒ 反射返回类型表达不了。

**现状证据（已核）**：`effect-interpreter/spec.md:108`（SCRIPT-1 未覆盖清单）；台账 SCRIPT-1 已消费但明示"**未覆盖（留待后续）**：`UnregisterTriggerRowsBySource`（形参 `FTcsSourceHandle` 非反射）、`FindChain`（裸 struct 指针不可反射）"。

**归属**：SCRIPT-1 未覆盖项。

**状态**：待调研。

---

### R-6 OnConsumed / 消耗语义（关联项）

**事项**：消耗语义整体未实现（规格欠账——归档提案明文要求"成功才消费（调用命中候选的 `OnConsumed`）"，归档时被裁）；`OnConsumed`（`TFunction<void()>`）是 **`FTcsDamageFlowContext` 物理不可反射化的唯一根因**。

**现状证据（已核）**：台账 **DAMAGE-4**（2026-09-24 登记，含完整证据链：归档提案原文 / 生效规格被裁 / 实现忠于规格 / 无消费者未暴露 / 逐字段消费者计数）。

**归属**：台账 DAMAGE-4（R5/M4a，与 `ModifyFlow` 同批）。

**状态**：已登记待办；**形状待 DAMAGE-4 落定时定**（拍板前不动 `FTcsConsumePolicy`）。本册收录为 **R-4 的前置关联项**。

---

### 调研 R-2：条件求值器 / 载荷读取器注册表反射入口（2026-09-27，**✅ 已拍板**）

> 本调研为第二项专项调研。结论未经用户确认前**不落档**——开放问题见文末。

#### 现状确认（已核）

| 项 | 事实 |
|---|---|
| 两张注册表的值 | `FTcsTriggerConditionTest = TFunction<bool(const FInstancedStruct&, const FTcsTriggerContext&, double)>`（`TcsTriggerCondition.h:64`）；`FTcsTriggerPayloadRead = TFunction<FTcsTriggerPayloadInfo(const FInstancedStruct&)>`（`TcsTriggerPayloadReader.h:61`） |
| 两个 `Register` | 均无 `UFUNCTION`；两处头注释自认欠账（"与步骤执行器注册表同批解决，MUST NOT 单独开"） |
| **两张注册表不在任何门面上** | `grep TriggerConditionRegistry / TriggerPayloadReaderRegistry` 于 `TcsEffectSubsystem.h` → **零命中**。脚本层不但登不了记，连入口都没有 |
| 条件求值器热路径 | `EvaluateTriggerConditions` 每候选行、每条件调用一次（`TcsTriggerEvaluator.cpp:169`） |
| 载荷读取器热路径 | `ReadPayloadInfo` **每事件一次**（各行共用，已优化） |
| 内置条件登记 | 2 个（`HasAllTags` / `Chance`），全走同一注册表 ✅ |
| **载荷读取器登记数** | **0**——`UE_DEFINE_TRIGGER_PAYLOAD_READER` 宏全库只有定义、零调用；TcsDamage 未为自己发布的收集事件登记读取器 |
| glue 产物（真往返判据） | `TcsTriggerContext.generated.cs` 三字段 / `TcsTriggerPayloadInfo.generated.cs` 二字段，**全有真实 `ToNative`/`FromNative`** ✅ |

#### ★ 关键发现（两条，超出原登记范围）

**发现 1：两个上下文类型可直接作 `BlueprintNativeEvent` 形参——但需升格 `BlueprintType`**

两个类型的 glue 产物都是真字段读写 ⇒ **形参不需要** SCRIPT-8 选择器那种"传句柄 + 门面访问器"绕行（与 R-1 的 `FTcsParamEvaluateContext` 同型）。**但两者都是 `USTRUCT()` 非 `BlueprintType`** —— 按 SCRIPT-8 已确立的约束 2（`BlueprintNativeEvent` 触发 UHT 蓝图参数校验，非 `BlueprintType` 的 USTRUCT 拿不到 blueprint cap），**必须升格 `BlueprintType`**，否则插槽编译不过。这与 SCRIPT-8 对 `FTcsChainRunHandle` 做的事完全同款（那次也是"MUST NOT 加 → 放宽"，承诺面代价为零：消费它们的门面方法仍是 `UFUNCTION()` 无 specifier）。

**发现 2：★ 跨世界寿命缺陷（SCRIPT-8 已落地的代码里就有）**

```
注册表 = static 函数局部单例（进程级，跨 PIE 存活）
        ↑ 无 Unregister / Reset / Clear（三张注册表全无注销入口，已 grep 确认）

SCRIPT-8 的持有 = UPROPERTY TArray<TObjectPtr<...>> 挂在 UWorldSubsystem 上
        ↑ Deinitialize 里清了 ChainDefs / TriggerRegistry / RunPool，但没碰注册表

转发器 = 捕获 Executor 裸指针（代码注释自陈"生命周期不短于本子系统"——
        注意是"不短于子系统"，而注册表比子系统活得久）
```

推理链：PIE 结束 → Subsystem 销毁 → `UPROPERTY` 数组释放唯一强引用 → 执行器对象被 GC 回收；**但进程级注册表里的 `TFunction` 仍捕获着它的裸指针**。第二次 PIE 时宿主重新登记 → 注册表命中同键 → **拒绝 + 保留首个** → 拿到的是 stale 指针 → 步骤执行 = 野调用。

**置信度声明**：**静态阅读推断，未实测**（SCRIPT-8 Task 6 PIE 实证还没做）。是否真的炸取决于宿主把执行器持有在哪里——若宿主另外持有（GameInstance 级、C# 静态引用），对象不回收，侥幸不炸。**这是巧合不是设计**，且注释里"生命周期不短于本子系统"这个措辞本身就说明作者只想到子系统这一层。

**对 R-2 的直接含义**：R-2 若照抄 SCRIPT-8 模式，会把同一缺陷复制到两张新注册表上。故 R-2 的解法必须同时回答"注销/寿命"。

#### 可选解法

| 方案 | 内容 | 取舍 |
|---|---|---|
| **A. 照抄 SCRIPT-8（UObject 基类替代 `TFunction` 作注册值）** | 新增 `UTcsTriggerConditionEvaluator : UObject`（`BlueprintNativeEvent bool Test(const FInstancedStruct&, const FTcsTriggerContext&, double)`）+ `UTcsTriggerPayloadReader : UObject`（`BlueprintNativeEvent FTcsTriggerPayloadInfo Read(const FInstancedStruct&)`）；门面加两个 `RegisterXxx(UScriptStruct*, UObject*)`，内部包 `TFunction` 转发进既有注册表；GC 持有数组 | **键与查表逻辑零改动**、C++ 快路径原样、内置条件不动。与 R-1/SCRIPT-8 同一模式，验证方法现成。**代价**：两个上下文类型要升格 `BlueprintType`；**且必须连带解决寿命问题** |
| **B. 换 `Register` 的 `TFunction` 形参为反射可见委托**（台账 SCRIPT-2 原方案） | 动三张注册表的签名 | SCRIPT-8 已判"改动面大"；且注册值仍是委托、寿命问题一样存在。**不推荐** |
| **C. 不做** | 维持 C++ 专用面 | 但**条件/载荷读取器没有替代物**——SCRIPT-8 的 `UTcsStepExecutor` 到不了触发行门禁的位置（条件必须在起链**前**判） |
| **D. 近似替代（记录备查）** | 宿主用"总是通过的条件 + 链首步骤自判 + 中止"绕过 | 语义不等价：链已起（运行态已分配），且"不通过 = 不起链"这个门禁语义表达不了。**降低紧迫性但不消除需求** |

**寿命问题的解法**（与 A 配套，也可独立用于修 SCRIPT-8）：

| 选项 | 内容 |
|---|---|
| **A1. 注册表补 `Unregister` + 门面 `Deinitialize` 按来源批量注销** | 最小改动、符合既有口径（注册表仍进程级；"动态登记"那部分由门面负责撤销，静态自注册的不动）。**我倾向这个** |
| A2. 注册值改弱引用 + 调用前校验 | 每次执行多一次校验；且"键在但值失效"要定义语义（按未注册处理？） |
| A3. 注册表改世界级 | 与"步骤是代码而非世界状态"的既有口径冲突，且三张注册表全要改 |

#### 影响面

| 面 | 影响 |
|---|---|
| 新增文件 | 2 个 UObject 基类（各 +1 `.cpp`）+ 门面 2 个注册方法 |
| 既有类型改动 | `FTcsTriggerContext` / `FTcsTriggerPayloadInfo` **升格 `BlueprintType`**（要改 `effect-trigger/spec.md` 两处描述） |
| 既有注册表改动 | 补 `Unregister`（三张注册表同批，含 SCRIPT-8 的步骤执行器）——**这条是修缺陷，不只是新功能** |
| 门面改动 | `Deinitialize` 加注销逻辑 |
| 规格 | `effect-trigger` 两条需求（条件注册表 / 载荷读取器）的"脚本可达面欠账"段改为已解决 + 新增插槽需求 |
| 脚本层 | C# 可定义纯数据条件 struct + 实现求值器；可为自己发布的自定义事件载荷登记读取器 |

#### 拍板结论（2026-09-27，用户确认）

| # | 开放问题 | 拍板 |
|---|---|---|
| ① | **需求边界**：宿主脚本需要"新条件类型"吗？ | **需要**——用户原话："宿主肯定是需要专属条件类型的，甚至专属的 SkillCost 都是很有可能的" ⇒ **R-2 确立为必需项**。宿主脚本对"新条件类型"与"新求值语义"（如 SkillCost 判定）都是真实需求 |
| ② | **载荷读取器的"零登记"要不要一起处理？** | **并入 plan3 Task 3**——用户裁定把载荷读取器的属主登记（TcsDamage 的收集事件读取器）放进 plan3 Task 3 一并落地。这是**功能未做**（TcsDamage 该登记没登记），与反射无关 |
| ③ | **归属轮次**：R-2 与 R-1 的关系？ | **R-2 与 R-1 紧邻、拆成独立提案**——用户裁定"R2 与 R1 可以紧邻，可以拆成独立提案" |
| ④ | **寿命缺陷要不要连带修（含 SCRIPT-8 已落地的步骤执行器）？** | **先实测，但只做记录**——用户裁定"先实测，但是我现在思路有了变化，所以就算实测验证，也只是做记录"。**不因实测结果改变设计**（TCS 插件应直接兜底，不交给宿主项目——用户明确"TCS 插件应该直接兜底，而不是交给宿主项目"） |
| ⑤ | **载荷读取器键冲突语义** | **撤掉（伪问题）**——核清：宿主自定义载荷类型与领域载荷类型键不同，永不冲突；维持"重复即拒绝 + ensure"的既有语义，**不加"宿主覆盖"概念** |

**★ 用户对解法的两条裁定（2026-09-27）**：

1. **解法 A 通过**，且用户确认其形态"有些像我之前在初版 TCS 里使用的 UObject-CDO 策略模式"。**回答**：对，骨架同一族（策略 = UObject 子类、取行为 = 反射分派），但**两处关键差异**——①**不替换现有结构，只替代 `TFunction` 作注册表的值**（键、查表、C++ 快路径原样保留，双轨并存）；②**补了初版缺的三样**：GC 可见持有、寿命注销、空 Host 守卫。**且须写明边界**：`FInstancedStruct` 只能装纯数据 USTRUCT、装不了 UObject ⇒ **资产里的策略（纯数据）走虚分派/注册表；宿主的脚本行为（UObject）走插槽**——插槽不能统一替代虚分派，两种载体各管一摊。
2. **解法 B 否**（用户："这个方式确实不太好，总体而言，我个人也倾向可选解法 A"）。

**★ 用户对寿命问题的裁定（2026-09-27）**：A1（补 `Unregister` + 门面 `Deinitialize` 按来源批量注销）**通过**——"看起来 A1 方法像是提供了注销接口，然后把注销时机和选择权交给宿主项目，倒是可以，没什么问题"。**但优先级低于兜底**——"如果 TCS 本身确实没有办法提供兜底，确实可以这样，TCS 如果能提供兜底，最好还是提供兜底"。**⇒ 落地顺序**：先实测确认缺陷（只记录），同时**优先设计 TCS 侧兜底**（若可行），A1 作为兜底不可行时的退路。

#### 落地清单（待执行；R-2 与 R-1 紧邻、独立提案）

| # | 产物 | 说明 |
|---|---|---|
| 1 | `UTcsTriggerConditionEvaluator : UObject` | `BlueprintNativeEvent bool Test(const FInstancedStruct&, const FTcsTriggerContext&, double)`；门面 `RegisterConditionEvaluator(UScriptStruct*, UObject*)` |
| 2 | `UTcsTriggerPayloadReader : UObject` | `BlueprintNativeEvent FTcsTriggerPayloadInfo Read(const FInstancedStruct&)`；门面 `RegisterPayloadReader(UScriptStruct*, UObject*)` |
| 3 | `FTcsTriggerContext` / `FTcsTriggerPayloadInfo` 升格 `BlueprintType` | 与 SCRIPT-8 对 `FTcsChainRunHandle` 同款；`effect-trigger/spec.md` 两处描述同步 |
| 4 | 三张注册表补 `Unregister` + 门面 `Deinitialize` 按来源注销 | **修跨世界寿命缺陷**（含 SCRIPT-8 已落地的步骤执行器）；优先 TCS 侧兜底设计 |
| 5 | plan3 Task 3：TcsDamage 登记收集事件读取器 | 用户裁定并入（载荷读取器零登记 = 功能未做，随 Task 3 落地） |
| 6 | 提案 + 规格 delta + UBT 编译 + glue 核验 + C# 实现实测 | 同 R-1 的验证流程（"能导出 ≠ 能往返"判据） |

#### ★ 落地时的关键判据（调研已确认，供实施者直接引用）

- **形参可直接用 `FTcsTriggerContext` / `FTcsTriggerPayloadInfo`**——两者 glue 产物都是真字段读写，**不需要** SCRIPT-8 选择器那种"传句柄 + 门面访问器"绕行（这是 R-2 比 SCRIPT-8 简单的原因）。唯一前提是升格 `BlueprintType`。
- **载荷读取器的"每事件一次"语义要保住**——`ReadPayloadInfo` 已在求值器里优化为各行共用（只读一次载荷）；插槽转发只替换"读取器"这一层，**不得**把读取点挪进每行循环。
- **双轨并存**——内置条件（`HasAllTags` / `Chance`）仍走 C++ 快路径（静态自注册宏，零反射开销）；插槽只服务宿主扩展（多一次 `UFunction::Invoke`，按 SCRIPT-8 判据"客制化/非热路径"可接受）。条件求值器虽是热路径，但**内置路径不动**，插槽的额外开销只在宿主条件上发生。
- **TCS 侧兜底优先于 A1**（用户裁定）——落地时先评估"注册值改持有对象引用（`TObjectPtr` + 生命周期校验）"是否可行，A1 作为退路。

---

## 调研记录

### 调研 R-1：参数源族宿主插槽（2026-09-24，**✅ 已拍板**）

> 本调研为第一项专项调研。结论未经用户确认前**不落档**——开放问题见文末。

#### 现状确认

- `FTcsParamValueSource`（`TcsCore/Public/Parameter/TcsParamValueSource.h:62`）：`USTRUCT(meta=(Hidden))` 基类 + 两个虚函数——`virtual double Evaluate(const FTcsParamEvaluateContext&) const`（`:77`）与 `virtual bool AllowsValueConvention() const`（`:99`）；
- 载体 `FTcsParamValue{ FInstancedStruct Source }`（SCRIPT-6 换型后），调用点 `Evaluate(Ctx)` → `Source.GetPtr<FTcsParamValueSource>()` → `->Evaluate(Ctx)`（`TcsParamValue.h:63-67`）；
- 内置源：`FTcsParamSource_Literal` / `FTcsParamSource_ParamRef`（TcsCore）、`FTcsParamSource_AttributeScaled`（TcsAttribute）——全部 C++ 虚分派；
- 选择器族的解法（SCRIPT-8 已落地）：`ITcsTargetSelectorHost`（`UINTERFACE(MinimalAPI, Blueprintable)` + `UFUNCTION(BlueprintNativeEvent)`）+ `FTcsSelHostDelegate`（USTRUCT 策略子类，持 `UPROPERTY TScriptInterface<ITcsTargetSelectorHost> Host`，`Resolve` 纯转发 + 空 Host 守卫）——**两层结构**：宿主实现 UObject 接口，转发器把它接进虚分派体系。

#### ★ 关键发现：参数源比选择器**更简单**——Evaluate 的形参已经反射

选择器被迫"传句柄"是因为 `FTcsEffectContext` 是**非反射纯 C++ struct**（无法作 `UFUNCTION` 形参，UHT 报错）。但参数源的求值上下文 **`FTcsParamEvaluateContext` 是 `USTRUCT(BlueprintType)`**（字段 `TScriptInterface<ITcsParamTableReader> ParamTable`，反射可承载）——**它可以直接作 `UFUNCTION(BlueprintNativeEvent)` 的形参**。

⇒ 宿主插槽接口不需要"传句柄 + 门面访问器"那套绕行，直接：

```cpp
UINTERFACE(MinimalAPI, Blueprintable)
class U... /* ITcsParamSourceHost */ : public UInterface { ... };

UFUNCTION(BlueprintNativeEvent)
double Evaluate(const FTcsParamEvaluateContext& Context);   // 形参已反射，直接可用

UFUNCTION(BlueprintNativeEvent)
bool AllowsValueConvention();                                // 第二个虚函数，一并转发
```

比 `FTcsSelHostDelegate` 简单——后者要 `TScriptInterface` + 句柄 + 访问器三层，这里一层接口 + 一个转发器 struct 就够。

#### 照搬选择器解法的形状

- 新增 `ITcsParamSourceHost`（UINTERFACE + `BlueprintNativeEvent`，两个方法对应两个虚函数）；
- 新增 `FTcsParamSource_HostDelegate : FTcsParamValueSource`（USTRUCT 策略子类，持 `UPROPERTY TScriptInterface<ITcsParamSourceHost> Host`；`Evaluate` / `AllowsValueConvention` 纯转发 + 空 Host 守卫 = 返回 0 / true）；
- 载体 `FTcsParamValue.Source` **零改动**（`FInstancedStruct` 照装）；`meta=(BaseStruct)` picker 限定不变；
- 依赖方向：接口放 `TcsCore/Public/Parameter/`（`ITcsParamTableReader` 已在此——TcsCore 是零战斗词汇底座，参数接口归它；或对齐选择器放 `Public/Host/` 子目录，见开放问题 ②）。

#### 与 `ITcsParamTableReader` 的重叠（必须先澄清的需求边界）

`ITcsParamTableReader::TryGetNumericParam`（UINTERFACE + `BlueprintNativeEvent`）**已能让宿主脚本提供自定义参数查询**——`ParamRef` 源消费它。所以一部分"自定义数值"场景**今天已可脚本化**。

- **已覆盖**：宿主脚本实现参数表接口 → `ParamRef` 源按名读到自定义值（"读宿主专有的某张表"）；
- **未覆盖**：**全新求值语义**——不是查表，而是"读任意宿主状态做计算/组合/转换"（如"攻击力 = 基础 + 等级×成长 + 装备加成"这种需要写逻辑的源）。

**⇒ 开放问题 ①：宿主脚本需要的到底是哪一类？** 如果只要"查表"，`ITcsParamTableReader` 已够，R-1 可能不必做；如果要"新求值语义"，R-1 才是必需的。这个判断只有你能拍——它决定 R-1 是"必需"还是"锦上添花"。

#### 热路径考量

`Evaluate` 被属性聚合折叠 / 链步骤求值 / 流程 Operand 多处调用——若宿主用插槽，每次 `Evaluate` 多一次 `UFunction::Invoke`。但按 SCRIPT-8 判据**双轨并存**：内置源（Literal/ParamRef/AttributeScaled）仍走 C++ 快路径（`GetPtr` + 虚表直调零反射开销），插槽只服务宿主扩展（客制化、非热路径）。与选择器插槽的取舍完全同构。

#### 影响面

| 面 | 影响 |
|---|---|
| 新增文件 | `ITcsParamSourceHost`（接口）+ `FTcsParamSource_HostDelegate`（转发器）≈ 2 文件（+ 各 1 .cpp） |
| 既有代码 | **零改动**（载体不变、内置源不动、调用点不动）——纯增量，与 SCRIPT-8 对选择器的处理同型 |
| 资产 | 无影响（不改任何序列化字段） |
| 脚本层 | C# 可实现 `ITcsParamSourceHost` 并配到任意 `FTcsParamValue.Source`（SCRIPT-6 换型后字段已可配） |

#### 拍板结论（2026-09-24，用户确认）

| # | 开放问题 | 拍板 |
|---|---|---|
| ① | **需求边界**：宿主脚本要"新求值语义"还是"查表"已够？ | **要"新求值语义"**——用户原话："我希望宿主脚本有权利定制**新的求值语义**" ⇒ **R-1 确立为必需项**（非锦上添花）。`ITcsParamTableReader` + `ParamRef` 只能覆盖"查表"，表达不了"读任意宿主状态做计算/组合/转换"（如"攻击力 = 基础 + 等级×成长 + 装备加成"这种要写逻辑的源） |
| ② | **接口归属**：`TcsCore/Public/Parameter/` 还是 `Public/Host/`？ | **A 方案 = 都放 `TcsCore/Public/Parameter/`**。理由：①TcsCore 的参数域接口本来就是**平铺**的（`ITcsParamTableReader` 同为"宿主可实现的接口"且就在 `Parameter/`）；②**转发器必须**放这里（继承 `FTcsParamValueSource`）；③接口与转发器是同一机制的两半，分开放会多一跳；④TcsCore 尚未建立 `Host/` 约定，为一个文件新建目录反而制造内部矛盾。（TcsEffect/TcsTargeting 的 `Host/` 约定不适用于 TcsCore——那两个模块的 `Host/` 目录装的是领域契约，非参数域。） |
| ③ | **归属轮次**：R5 内 / SCRIPT-8 后独立 / 触发条件型？ | **SCRIPT-8 归档后作为独立小提案做**。四条理由：①**同一模式、验证方法现成**——SCRIPT-8 刚走通"转发器 + glue 核验 + C# 实现实测"完整路径，趁热做成本最低、验证脚本可复用；②**补全插槽家族**——参数源是同类第四项（选择器/过滤器、步骤执行器、伤害流程、**参数源**），拖久会让"为什么参数源不一样"变成长期疑问；③**零依赖纯增量**——不依赖 R5 任何功能，两个新文件、既有代码零改动；④**不并入 SCRIPT-8**——SCRIPT-8 已近收尾（代码/编译/glue 核验全过，仅剩 PIE 实证与文档），此时扩容会拖慢它且其规格 delta 已全绿、加内容要重跑验证。<br>**反面理由（已记录，供将来复盘）**：R5 的 `ApplyState`/`ModifyAttribute` 都要配数值源，那里会出现**第一个真实消费者**，放 R5 能用真实场景验证；但 R5 是重轮（M4a 触发行 + 修改器通道 + M4c 目标选择），小而正交的项有被挤掉的风险，且"等消费者"省下的成本有限、反而可能在 R5/R6 落地时逼人中断手上工作去补。 |

**落地清单（待执行，SCRIPT-8 归档后）**：

| # | 产物 | 位置（A 方案） |
|---|---|---|
| 1 | `ITcsParamSourceHost`（`UINTERFACE(MinimalAPI, Blueprintable)` + 2 个 `UFUNCTION(BlueprintNativeEvent)`：`Evaluate(const FTcsParamEvaluateContext&)` / `AllowsValueConvention()`） | `Source/TcsCore/Public/Parameter/TcsParamSourceHost.h` |
| 2 | `FTcsParamSource_HostDelegate : FTcsParamValueSource`（USTRUCT 策略子类，持 `UPROPERTY TScriptInterface<ITcsParamSourceHost> Host`；两个虚函数纯转发 + 空 Host 守卫 = 返回 0 / true） | `Source/TcsCore/Public/Parameter/TcsParamSource_HostDelegate.h`（+ `.cpp`） |
| 3 | 提案 + 规格 delta（`param-value` 能力：ADDED 一条"宿主参数源插槽"需求 + 场景） | `openspec/changes/<id>/` |
| 4 | 验证 | UBT 编译 + glue 产物核验（**"能导出 ≠ 能往返"**——确认 `Evaluate` 生成真实方法体）+ C# 实现实测（配到 `FTcsParamValue.Source` 后求值走脚本） |

**★ 落地时的关键判据（调研已确认，供实施者直接引用）**：

- **形参可直接用 `FTcsParamEvaluateContext`**——它是 `USTRUCT(BlueprintType)`、字段全反射、**无 `TFunction`** ⇒ 可直接作 `BlueprintNativeEvent` 形参。**这是 R-1 比 SCRIPT-8 选择器插槽更简单的原因**：选择器被迫"传句柄 + 门面访问器"是因为 `FTcsEffectContext` 是非反射纯 C++ struct（不能作 `UFUNCTION` 形参，UHT 报错）；参数源没有这个约束。
- **两个虚函数都要转发**——`Evaluate`（求值）与 `AllowsValueConvention`（约定能力位，D5-18 v3 白名单的判据）。漏掉后者会让宿主新源在"是否允许配置行级值约定"上拿到基类默认值 `true`，而契约要求"能力探测走虚分派，不得建立中心名单"（`TcsParamValueSource.h:95`）。
- **载体与调用点零改动**——`FTcsParamValue.Source` 是裸 `FInstancedStruct`（SCRIPT-6 换型后），照装转发器；`Evaluate` 调用点走 `GetPtr<FTcsParamValueSource>()` + 判空，转发器作为子类自然命中。
- **双轨并存**——内置源（Literal/ParamRef/AttributeScaled）仍走 C++ 快路径（`GetPtr` + 虚表直调，零反射开销），插槽只服务宿主扩展（多一次 `UFunction::Invoke`，按 SCRIPT-8 判据"客制化/非热路径"可接受）。

---

## 变更记录

- **2026-09-24 建立**：用户拍板"未解决项留档 + 逐项专项调研，每项调研完评审、讨论完再落档"。收录 **R-1 ~ R-6** 六项（R-1 参数源族为本次讨论新发现；R-2~R-6 为既有台账条目的反射专项视图）。启动**调研 R-1**（待评审）。
- **2026-09-27 R-2 评审完成（用户确认）**：需求边界 = 必需项（宿主需专属条件类型，甚至专属 SkillCost）；载荷读取器零登记并入 plan3 Task 3；R-2 与 R-1 紧邻独立提案；寿命缺陷先实测只记录 + TCS 侧兜底优先；开放问题 ⑤ 撤掉（伪问题）。解法 A 通过（用户点出其与初版 UObject-CDO 策略模式同族）、解法 B 否。**发现跨世界寿命缺陷**（SCRIPT-8 已落地代码中即存在，注册表进程级 vs 持有世界级）——已记录待实测。
- **2026-09-29 `DEC-04` 裁定落定（用户拍板 §7 五项全接受）**：本册 **3 行改判**——**R-3 由"待调研"改为"⚖ 已裁决：不换"**（`TFunctionRef` 的语义/热路径/蓝图不承诺三条判据不随时间改变，不再挂待办）；**R-6 由"已登记待办"改为"⚖ 已裁决"**（`OnConsumed` 改事件/原语语义，与 `DAMAGE-4` 同批）；**R-4 的前置获定向**（R-6 已裁定，待 `DAMAGE-4` 落定后启动）。**R-1 / R-2 状态不变**（仍"已调研并拍板，待落地"），但落地顺序获确认：**先做 A 类值语义改造**（`DEC-04` 裁定 ⑤），它是 R-2 跨世界寿命兜底的前置护栏。条目总数不变（6 项）。
