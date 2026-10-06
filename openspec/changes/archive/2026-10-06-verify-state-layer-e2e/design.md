## Context

R5 六个状态层能力各自的证据都已归档，但没有一条证据穿过它们的耦合点。Task 7 要在**宿主装置**里
串成一条竖切，并产出可复核证据。约束来自本轮侦察的实测事实：

1. **全库 `UTcsAttrModDef` 资产数为 0**。Task 4 走的是 C++ 手搓瞬态模板
   （`TcsDevSliceRig.cpp:1432-1434`），故"内容驱动 `ModifierRows`"与
   `TcsStateModifierMaterializer.cpp:50-54` 的 `LoadSynchronous()` 真资产路径**从未被执行**。
2. **`FTcsStepApplyState` / `FTcsStepModifyAttribute` 在全部 `.uasset` 中零出现**。
   既有链资产只有 `TcsStepDamage` / `TcsStepWaitDelay` / `TcsParamSource_Literal`。
3. **`UTcsDevEntityLevelProvider` 已存在并已注入**（`TcsDevBootstrap.cpp:368-392`）。
   **但（2026-10-05 修正）这不等于"等级读口在本轮被验"**：`LevelProvider` 的消费者**只有**
   `FTcsParamSource_InstigatorLevel*`（`TcsParamSource_InstigatorLevel.h:76,155`），
   而本轮内容资产的等级行走 `FTcsParamSource_StateLevelArray`，该源**只读 `Context.EffectiveLevel`**
   （`TcsParamSource_StateLevel.h:68-72`，由 `TcsStateOps.cpp:118` 填 `Def->LevelBase`）⇒
   **`LevelProvider` 在本轮内容路径上一次都不被调用**。原"已注入 ⇒ 转为验证项"的推论作废：
   读口的实际覆盖来自既有检查 20b（`InstigatorLevelArray`，探针定义非内容资产），
   本轮只验 `StateLevelArray`，读口未消费这条如实进边界清单（见 D9 与 tasks §7.2）。
4. **C# 侧 `ITcsParamSourceHost` 已实现**且已被五槽位 GC 探针验过 ⇒ 不是 Task 7 的待办。
5. 屏显基址占用：`0x7C5D0000`（观测者）/`0x7C5D1000`（Slice 装置）/`0x7C5D2000`（GC 探针）。
6. **`UTcsAttrModDef` 全库无发现路径**：七模块 grep `DiscoverAttrModDef` / `ResolveAttrModDef` /
   `UTcsAttrModDef::StaticClass` **零命中**；定义库只有三条按类发现路径（链 / 触发定义 / 状态定义）。
   而 `UTcsAttrModDef::IsDataValid` 把空 `TemplateTag` 判为 **Error**（`TcsAttrModDef.cpp:28-32`）
   ⇒ "落一个合法真资产"与"该身份词无根"两条要求直接冲突，倒逼出 D8。

## Goals / Non-Goals

- Goals：一条内容资产驱动的竖切，穿过"施加 → 快照冻结 → 修正器物化 → 内联触发 → 行为链改属性 →
  周期 → 到期 → 属性复原 → 级联退订 → 实例回收"；双面证据（属性面 + 事件面）；可复现性由两轮逐字比对证明；
  拒绝面独立成命令；边界清单**每条标注"实测"或"未覆盖"**；**闭合 `ATTR-1`**（落地"按 tag 解析修正器模板"的路径）。
- Non-Goals：不做竞技压测；不走 Mass 路径；不做网络复制；不重复验单元级机制
  （既有 19/20/21/22/23 各段已覆盖，本变更只在竖切里引用其结果）。
  **插件侧改动严格限定于"修正器模板身份解析"这一条**（`DiscoverAttrModDefs` / `ResolveAttrModDef`）——
  不改任何既有发现路径的行为、不改状态层、不改物化器；`AttrModDef` 的**运行期取用**（按 tag 取模板供物化）
  不在本轮，因为它的调用者为零（见 D8）。

## Decisions

### D1：装置落 `TcsDevSliceRig_State.cpp`（新文件）+ 内部头，而非内联进 3663 行的既有文件

`TcsDevSliceRig.cpp` 已 **3663 行**（`.cpp` 风格上限 300 行，此处属既有"风格例外 5"的验收装置）。
新增一个完整命令对会让它继续膨胀。`TcsDevSliceRig_Behavior.cpp` 已确立"同装置族拆文件"的先例。

代价：需把 `FindSliceRigWorld` / `SpawnSliceUnit` / `TeardownSliceRigActors` / `FTcsDevRigScreenSection` /
`GTcsDevRigActors` / `GTcsDevRigSourceRegistry` 提升进内部头，触碰既有文件一次。
- 备选：全部内联进 `TcsDevSliceRig.cpp` —— 零触碰既有文件，但把 3663 行推到 4300+ 行。
- 备选：三处复制粘贴 —— 拒绝：`GTcsDevRigActors` 是 `TcsDevSliceRig.cpp:94` 的 file-static，
  复制出的第二份数组会导致**两套夹具各自以为清理干净**，是真实缺陷而非风格问题。

### D2（本轮最重要的发现）：内容资产 MUST NOT 引用切片侧类型 ⇒ buff 的 `Fragments` 留空

`FTcsSelSelf` 的文档把这条纪律写死了（`TcsSelSelf.h:19-22`）：

> `FInstancedStruct` 存的是**类型身份**（包 import 表索引，非名字字符串）——
> 链资产若引用切片侧的类型，切片退役后该步骤会**静默变空**（降级路径只剩一条 `LogCore` Warning）。
> 纪律：**内容引用的每个类型都必须长于内容**。

`FTcsDevBehaviorSample` 住在 **`Source/TcsDev/`（切片侧）**，生命周期短于内容资产。因此：

- **可以做**（类型全部长于内容）：内联 `Triggers` 行引用 `TcsEvent.State.Applied`（原生 tag）+ 一个链资产 id；
  链步骤用 `FTcsStepSelectTargets` / `FTcsSelSelf` / `FTcsStepModifyAttribute`（皆框架类型）。
  ⇒ "buff 自己的内联触发行起行为链改属性"这条**完全内容驱动**。
- **不可以做**：把 `FTcsDevBehaviorSample` 塞进内容 buff 的 `Fragments`。这会造出一个
  "切片退役即静默失效"的资产，正是该纪律要防的事。
- 故 `Fragments` 留空，**并在证据文档边界清单里如实登记**：行为片段路径本轮不走在内容路径上，
  由既有检查 23i–23l（瞬态定义）承担，理由 = 类型稳定性纪律。

**这条不是规避，是纪律的推论**：它同时解释了为什么框架要提供 `FTcsSelSelf`
（"选自己"是任何项目都会用的通用语义，故必须长于内容），而切片样例片段不必。

### D3：内容步骤的 `Target` 留空，靠 `SelectTargets(FTcsSelSelf)` 解析

实测（`TcsTriggerEvaluator.cpp:114-118`）：触发行起链时
**`ChainContext.Targets` 留空**，只填 `Caster` / `EventPayload` / `CausedBy`。
而状态事件的载荷读取器把 `Caster` 解析为 `Instigator.IsValid() ? Instigator : Unit`
（`TcsStateEvents.cpp:29`）⇒ 状态事件起的链，`Caster` 就是**持有该状态的那个单位**。

`FTcsStepModifyAttribute.cpp:35-39` 与 `TcsStepApplyState.cpp:40-44` 的目标解析是
"步骤字段优先，无效则取 `Context.Targets[0]`"。**`FTcsCombatEntityHandle` 是运行时句柄，
在资产里写不出来**（写死的数字在新世界里必然指错单位）。

故正确的内容写法是：链首步 `FTcsStepSelectTargets` + `Selector = FTcsSelSelf`
（执行器先清空再填 `Context.Targets`，`TcsStepSelectTargets.h:20-23`），后续步骤 `Target` 留空。
这既是内容可表达的，也是唯一在资产里正确的。

- 备选：步骤里硬编码句柄 —— 拒绝：资产里的句柄跨世界失效，是"看起来配好了、跑起来静默打错人"的缺陷形态。
- 备选：宿主在运行时给链资产补 `Target` —— 拒绝：那等于内容资产不自洽，且违背 D4-4 v2
  "目标集 = 链上显式数据流"。

### D4：新建 4 个内容资产，**不动** `DA_Check_BuffDef`

检查 18 断言**全部** `UTcsBuffDefAsset` 的 `IsDataValid == Valid`（`TcsDevSliceRig.cpp:1005`）⇒
新资产只让分母 +1，安全。但若把 `Period` 加到 `DA_Check_BuffDef` 上，其周期回调会落入既有检查 20l
的采样窗口（该检查断言窗口内周期计数**不增**，`TcsDevSliceRig.cpp:3119`）⇒ **既有验收转红**。
这是我实测得出的理由，不是洁癖。故新建而非改造。

### D5：拒绝面独立命令，全部预期红字集中于此

`Tcs.Test.State.Run` 要求零**非预期**红字；`Tcs.Test.State.Reject` 承载 6 项拒绝
（未登记 `DefTag` / 无效 `Target` / 悬空句柄 / 库未就绪 / `EDP_Infinite` 上 `ExtendDuration` /
`SetRemaining`）。拒绝面的留痕级别已实测各异（`Warning` vs `Log`），断言按实测级别写，
**不把 `Log` 当 `Warning` 断言**。

### D6：可复现性 = 两轮 PIE 逐字比对，不是"跑两次都对"

第二轮的读数行与第一轮做**字符串相等**比对。仅"两次都 PASS"不能证明可复现
（不同的随机/时序都可能各自 PASS）。计数器与摘要行因此设计成稳定格式串。

### D7：屏显段基址 `0x7C5D3000`

与观测者（`0x7C5D0000`）/ Slice 装置（`0x7C5D1000`）/ GC 探针（`0x7C5D2000`）错开。
已知风险：新命令若在 `Tcs.Test.Slice.Run` 后约 2 秒内执行，会被旧延时尾段交叠；
证据文档按**独立 PIE 轮次**取证以规避。

### D8（本变更性质变更的由来）：真资产倒逼出解析路径 ⇒ 新立 `AttrModDef` 根

**因果链（不是设计偏好，是被校验规则逼出来的）**：

1. 甲案（真内容资产）要求落一个**真的** `UTcsAttrModDef` → 它必须 `IsDataValid == Valid`；
2. `TcsAttrModDef.cpp:28-32` 把空 `TemplateTag` 判为 **Error**，且 `GetPrimaryAssetId()` 用它作主身份名
   ⇒ 真资产必须有**有效的** `TemplateTag`；
3. 有效 tag 需要有根，而根段注册表要求"消费角色 = 哪条代码路径解析它"⇒ 需要有**解析方**；
4. 解析方全库**不存在**（已核：七模块 grep `DiscoverAttrModDef` / `ResolveAttrModDef` / `UTcsAttrModDef::StaticClass` **零命中**）；
5. 台账 `ATTR-1` 正是记这件事："模板身份词今天零解析消费者 ⇒ **不为它开根**，归属 = 触发条件：
   出现'按 tag 解析修正器模板'的真实需求时"。

⇒ 两条路：**留空 `TemplateTag`**（资产编辑器里显示 Invalid，边界先记着）或**补上解析方**（触发条件成立、根落地）。
用户裁定**后者**，故本变更从"纯验证"扩为"验证 + 一条实现路径 + 一个新根"。

**为什么这不是"预建根"**：`ATTR-1` 的判据是"零消费者不预建根"，其**触发条件**明写为"出现按 tag 解析的真实需求"。
本变更**制造**了那个需求（要真资产就必须能按身份寻址），并**同时**交付了消费者（`Initialize` 调发现、就绪日志出计数、
失败清单拦重复身份）——同 `StateDef` 根 9 → 10 的先例（Task 1 落地时它也只有缓存、无消费方）。

**残留边界（MUST 如实登记，不得过度声称）**：`ResolveAttrModDef` 的**运行期调用者为零**——
`ModifierRows` 仍走资产直引用，物化器只 `Get()` / `LoadSynchronous()` 取对象、读 `->Def`，**从不读 `TemplateTag`**。
故本变更交付的是**身份解析与索引**（可寻址 / 可去重 / 可作者侧校验），
**不是**"按 tag 取模板供运行期物化"；后者的真实调用者仍待 R6（技能侧）或 R8（内容侧），`ATTR-1` 的闭合语据此写。

**为什么不做成 `UPROPERTY TMap<FGameplayTag, TObjectPtr<UTcsAttrModDef>>`**：全库无该形态先例（已核），
而本类已确立两容器模式（`TUniquePtr` 缓存 + tag 键 `TMap` + `UPROPERTY` 数组锚定）。
照 `StateDefAssets` 形状落 `AttrModDefAssets`（`UPROPERTY` 锚定）最为一致；
一致性优先于"少一个成员"——两容器模式的语义（内容缓存 vs 资产锚定分开）在本类已反复使用。

**`AttrModDef` 根名为何不撞车**：与宿主根 `Attribute` 前 4 字符同为 `Attr`，但**互不为真前缀**
（第 5 字符 `M` vs `i`）⇒ 不命中"互不为前缀"禁令，也无"订阅 `Attribute` 命中 `AttrModDef.*`"的部分匹配风险。
该回查已写进 `gameplay-tag-governance` 的根名判据段（**同前缀段的两个根是未来最可能撞车的一对**，留痕以备重查）。

### D9（实施期发现并修正）：模板引用键、`Overrides` 键、`Params` 行键**必须三者同键**

**原稿的缺口**：tasks §3.2 让模板的 `ParamRef.Key` = `TcsStateParam.Check.Modifier`，
而 §3.4 只让 `Params` 写两行 `TcsStateParam.Check.Override` + `TcsStateParam.Check.Level`
⇒ 被引用的键**不在参数表里**。这不是措辞问题，是**接线不成立**，会让 §4.7 的断言必然失败。

**机制（已核，非偏好）**：
- `BuildSnapshot` 的覆盖判定是 `Overrides.Find(Row.Key)`（`TcsStateOps_Snapshot.cpp:57`）
  ⇒ `Overrides` 的键**必须**是某个 `Params` 行的 `Key`，否则该次 `Overrides` 是空操作；
- 物化器把**实例快照**装成 `Ctx.ParamTable`（`TcsStateModifierMaterializer.cpp:35-42` 的
  `FTcsStateSnapshotScope`，注释明写"期间任何引用类操作数读到的都是这个实例的快照"），
  于是模板的 `FTcsParamSource_ParamRef::Evaluate` 查的是**快照的键空间**；
- ⇒ 模板 `ParamRef.Key` ∈ `Params` 行的键，**且**该键被 `Overrides` 命中，Y 才可能到达护甲；
  三者任一断裂 ⇒ 恒 miss ⇒ 落 `Fallback` = -999（`TcsParamSource_ParamRef.h:40-51`）。

**裁定（甲案）**：`Params` 写**三行**，三个键各有互不相同的真实消费者，并因此**多拿到一条负对照**：

| 行 | 键 | 值 | 消费者 / 证据角色 |
|---|---|---|---|
| ① | `TcsStateParam.Check.Modifier` | 字面量 10 | 模板 `ParamRef.Key` 指向它 + rig 的 `Overrides` 也用它 ⇒ **跨资产引用通道**；护甲变化 = Y（若落兜底会是 -999、若忽略覆盖会是 10，三值互不相等 ⇒ 判据能区分） |
| ② | `TcsStateParam.Check.Override` | 字面量 10 | **不进 `Overrides`** ⇒ **负对照**：未被列名覆盖就应保持定义值 10（若"覆盖"实现成整体替换，它会变成 Y ⇒ 判据能抓住） |
| ③ | `TcsStateParam.Check.Level` | `FTcsParamSource_StateLevelArray`，`Values` = [30, 60]，`Fallback` = -1 | 等级数组源；`EffectiveLevel` = `LevelBase` = 1 ⇒ 取 `Values[0]` = **30** |

**为什么不选"改模板键为 `.Override`"**：那样 §3.1 的 `TcsStateParam.Check.Modifier` 本轮就没有消费者，
须从 7 个 tag 里删掉（否则与"每词写明谁消费"相抵），且失去负对照 ⇒ 证据更弱而改动更多。

**这不是"为验收面加内容"**：三行键全部是宿主的**检查词**（`Check` 段，非正式内容词），
且不作弊——`Overrides` 传的 Y 与定义里写的 10 不同，模板若没真读到快照就拿不到 Y。

### D10（实施期发现并修正）：行为链的落点改 `Attribute.Attack`，状态修正器留 `Attribute.Armor`

**原稿的缺口**：tasks §3.8 让行为链的 `FTcsStepModifyAttribute` 也改 `Attribute.Armor`，
而 §4.13 断言到期后"属性复原（回到 X）"——**两者算术上不可能同时成立**。

**机制（已核，非偏好）——不是级联摘除失效，是两条条目的来源锚点不同**：

| 条目 | 挂载时的 `Source` | 落点 |
|---|---|---|
| 状态修正器（`ModifierRows`） | `Instance.CascadeAnchor` | `TcsStateModifierMaterializer.cpp:108` |
| 行为链的 `ModifyAttribute` | `Context.RunSource`（每次运行新发号） | `TcsStepModifyAttribute.cpp:80`；发号见 `TcsEffectSubsystem_Run.cpp:81-84` |

到期时 `StripModifiers` 只按**前者**摘（`TcsStateOps_Modifier.cpp:101`
`Access.RemoveBySource(Instance.Unit, Instance.CascadeAnchor)`）⇒ 链那条按设计**常驻**。
`RemoveBySource` 本身按 `Source` 精确匹配（`TcsAttributePipeline_Cascade.cpp:22-35`），行为完全正确。

**三条替代路径都已堵死**（故不修机制）：
- **运行结束回收**：全即时链的 `ExecuteChain` 返回前运行态即释放（`TcsEffectSubsystem_Run.cpp:104-105`）
  ⇒ 连时间窗口都没有；
- **按因果边（`CausedBy`）级联撤销**：**已被裁定③明文禁止**（`decisions-log.md:249`；同口径
  `03-module-states.md:258` 引原始版否决理由"父级 SourceHandle 的生命周期可能先于子级结束"；
  `effect-interpreter` 规格有 `Scenario: 因果边不参与撤销`）；
- **框架自动回收**：`FTcsStepModifyAttribute` **故意没有回收者**——其语义是"即时步骤……
  本原语表达的是链直接改一次账本"（`TcsStepModifyAttribute.h:26-27`），既有检查 23c 注释同款表述
  （"行为链的 +5 是**常驻条目**，**不随状态实例的锚点级联摘除**"）。

**裁定（甲案，用户拍板）**：改落点，不改契约——行为链改 `Attribute.Attack`，状态修正器留 `Attribute.Armor`。
于是两个通道分属两个属性，"会回退的"（护甲回到 X）与"永久留的"（攻击停在 X−5）**各自有干净读数**，
两份 spec scenario 逐字成立、一字不改。**代价如实记**：§4.12 的断言对象由 `Armor` 改 `Attack`。

**为什么不选"同改 Armor、改断言 + 改 spec"**（乙案）：那要修改已批准的 MUST 级规范文本
（`state-layer-e2e-validation` 的 `到期回收与级联摘除` scenario 那句"属性复原到施加前的读数"），
把一次验收做成一次契约弱化；而分属性既能保住规范原文，又**多出一条互证读数**。

**这不是"逃避问题"**：真正待裁定的是"要不要给因果链加回收语义"，那是改契约，须另开提案；
本变更把它连同"链原语缺直写基础值"一起登记为延后项（`CHAIN-7` / `CHAIN-8`，见 tasks §7.4）。

### D11：`§5` 的回归判据由"与改动前同值"改为"增量核对表"

**原稿的缺口**：tasks §5.5 要求就绪日志三计数与改动前**同值**，§5.4 要求各段读数与改动前**一致**——
但本变更新增 2 个链资产 + 1 个 buff 资产，链定义 9→11、状态定义 1→2 是**必然**，
检查 18 的 `Valid 1/1` → `2/2`、检查 19a 的 `门面内 1 条` → `2 条` 同样是**必然**。
照原稿判会把"预期内的分母增长"记成回归失败。

**裁定（甲案，用户拍板）**：把"同值"改为**逐项增量核对表**（tasks §5.5 已落表），
§5.4 改为按**通过/失败结论**回归、并点名两处分母的已知变化。**失败清单仍须为空**（非空即判失败）。

## Risks / Trade-offs

- **MCP 写 `TSoftObjectPtr<UTcsAttrModDef>` 数组（`ModifierRows`）的 JSON 形态未经实测** →
  若 `set_properties` 无法写入该字段，回退方案：该一行改用瞬态模板挂载，并在边界清单登记
  "内容驱动 `ModifierRows` 未闭合"。**这是本变更最可能失败的一步，故排在最前（任务 2.5）。**
- **`FTcsStepSelectTargets` 的 `Selector` 是裸 `FInstancedStruct`**（非 `TInstancedStruct`）→
  MCP 写入需 `{"_structType": "/Script/TcsTargeting.TcsSelSelf"}` 形态；失败则回退为
  行为链改由瞬态链承担，并登记边界。
- **属性账本未就绪时 `MountModifiers` 只 `Log` 不 `Warning`**（`TcsStateOps_Modifier.cpp:78-85`）⇒
  若夹具顺序写错（未先 `ApplySliceAttributes`），症状是"静默无修正器"而非红字。
  故 3.5 把属性账本准备排在施加之前，且 3.7 的读数带"修正器槽位数"作为独立交叉验证。
- **`GTcsDevRigActors` 为进程级共享**：新命令自持完整 setup/teardown，不与 Slice 装置争用；
  若用户交替运行两条命令，后跑者会销毁前者的存活夹具 —— 证据文档要求分轮取证。
- 并发/时序：周期回调是帧驱动的 ⇒ 断言一律用**下界**（≥1），MUST NOT 用等号（记忆卡 MEM-20261004-23）。
- **新增：插件侧改动会把本次验收从"零插件改动"变成"带插件改动的验收"** ⇒ 回归面必须相应加宽：
  除三命令与既有状态面检查（19–23 段）外，MUST 额外确认定义库就绪行的**前三计数与改动前同值**
  （见 tasks §5.5）——只加第四个计数、失败清单仍为空，才证明新路径没污染既有发现路径。
- **新增：新根下的词覆盖极薄**（只有 `AttrModDef.Check.StateLayer` 一个，且是 `Check` 形态）
  ⇒ "根的代表词已验证"这句话**本轮不成立**（正式内容词为零）。证据文档 MUST 如实标注，
  MUST NOT 用"根已落地并可寻址"暗示"该根已被真实内容使用"。

## Migration Plan

无迁移：纯新增宿主装置 + 新增内容资产 + 新增文档，外加**一条新增的插件发现路径**（纯增量，不改既有路径行为）。
回滚 = 删除新增文件（含 `TcsDefinitionSubsystem_AttrModDef.cpp`）与 4 个资产、还原
`DefaultGameplayTags.ini` 的 7 行、还原 `TcsDefinitionSubsystem.h` / `.cpp` 的四处改动、`git checkout` 既有装置文件；
`AttrModDef` 根随之撤回（根下无其它词，删除无副作用）。

## Open Questions

- 无阻塞项。若 D2/D3 的纪律判定与后续 R5.5 的关系表族结论冲突，以本证据为当轮记录、由 R5.5 复议。
- **留给 R6/R8 的一问**：`ResolveAttrModDef` 的真实调用者出现时，"按 tag 取模板"与"资产直引用"两条取用路径
  是否要收敛为一条？本轮不裁定（零调用者时裁定等于猜）。
